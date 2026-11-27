#!/usr/bin/env bash
# Smoke test: start the server with an empty DB in a temporary directory.
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
bin="${1:-$root/build/finances}"
[[ -x "$bin" ]] || {
  echo "$bin does not exist; build with CMake first."
  exit 1
}

tmp="$(mktemp -d)"
pid=""
trap '[[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true; rm -rf "$tmp"' EXIT
ln -s "$root/templates" "$tmp/templates"
ln -s "$root/static" "$tmp/static"
cd "$tmp"
port=$((20000 + RANDOM % 20000)) # random: never hit a real instance on 8080
export FINANCES_PORT=$port
"$bin" >server.log 2>&1 &
pid=$!

for _ in $(seq 1 100); do
  kill -0 "$pid" 2>/dev/null || {
    echo "Server exited:"
    cat server.log
    exit 1
  }
  curl -fs -o /dev/null http://127.0.0.1:$port/ && break
  sleep 0.2
done

month="$(date +%Y-%m)"
body="$(curl -fsS http://127.0.0.1:$port/)"
grep -q 'Your finances at a glance' <<<"$body"
grep -q 'dashCategoryChartData' <<<"$body"
grep -q 'dashCurrentChartData' <<<"$body"
grep -q 'currentCategoryChartData' <<<"$body"

body="$(curl -fsS "http://127.0.0.1:$port/month/$month")"
grep -q 'No entries yet this month' <<<"$body"

post() { curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST http://127.0.0.1:$port/entries "$@"; }

code="$(post -d "title=Test purchase&amount=12,50&kind=expense&category=alimentacion&date=$month-15&month=$month")"
[[ "$code" == 200 ]] || {
  echo "Valid POST returned $code"
  exit 1
}
grep -q 'Test purchase' "$tmp/out.html"
grep -q '12,50' "$tmp/out.html"
grep -q 'hx-delete=' "$tmp/out.html"
! grep -q '<th>Added</th>' "$tmp/out.html"

code="$(post -d "title=Nominal&amount=1000&kind=income&category=otros&date=$month-01&month=$month")"
[[ "$code" == 200 ]] || {
  echo "Income POST returned $code"
  exit 1
}

code="$(post -d "title=Bad&amount=abc&kind=expense&category=ocio&date=$month-01&month=$month")"
[[ "$code" == 200 ]] || {
  echo "Invalid amount returned $code"
  exit 1
}
grep -q 'The amount is not valid.' "$tmp/out.html"
grep -qi 'HX-Retarget: #form-error' "$tmp/headers.txt"

other="$(date -d "$month-01 -1 month" +%Y-%m 2>/dev/null || echo 2026-10)"
code="$(post -d "title=Other month&amount=5&kind=expense&category=ocio&date=$other-01&month=$month")"
[[ "$code" == 200 ]] || {
  echo "Date from another month returned $code"
  exit 1
}
grep -q 'The date must belong to the displayed month' "$tmp/out.html"

count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entries')"
[[ "$count" == 2 ]] || {
  echo "Expected 2 rows, got $count"
  exit 1
}
cents="$(sqlite3 "$tmp/finances.db" "SELECT amount_cents FROM entries WHERE title='Test purchase'")"
[[ "$cents" == 1250 ]] || {
  echo "Amount saved incorrectly: $cents"
  exit 1
}

body="$(curl -fsS http://127.0.0.1:$port/)"
grep -q '1.000,00' <<<"$body"
grep -q '12,50' <<<"$body"

id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM entries WHERE title='Test purchase'")"
code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X DELETE "http://127.0.0.1:$port/entries/$id")"
[[ "$code" == 200 ]] || {
  echo "DELETE returned $code"
  exit 1
}
grep -q 'Nominal' "$tmp/out.html"
! grep -q 'Test purchase' "$tmp/out.html"
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entries')"
[[ "$count" == 1 ]] || {
  echo "Expected 1 row after delete, got $count"
  exit 1
}

code="$(post -d "title=Newest date&amount=1&kind=expense&category=ocio&date=$month-25&month=$month")"
[[ "$code" == 200 ]] || {
  echo "Late POST returned $code"
  exit 1
}
newest="$(grep -n 'Newest date' "$tmp/out.html" | cut -d: -f1 | head -1)"
older="$(grep -n 'Nominal' "$tmp/out.html" | cut -d: -f1 | head -1)"
[[ "$newest" -lt "$older" ]] || {
  echo "Entries are not ordered by date"
  exit 1
}

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST http://127.0.0.1:$port/people -d 'name=Una+persona')"
[[ "$code" == 200 ]] || {
  echo "Add person returned $code"
  exit 1
}
people_count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM people')"
[[ "$people_count" == 1 ]] || {
  echo "Person was not saved"
  exit 1
}
body="$(curl -fsS http://127.0.0.1:$port/)"
grep -q '<h2>People</h2>' <<<"$body"
grep -q 'Una persona' <<<"$body"

code="$(curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST http://127.0.0.1:$port/people -d 'name=')"
[[ "$code" == 200 ]] || {
  echo "Empty person returned $code"
  exit 1
}
grep -qi 'HX-Reswap: innerHTML' "$tmp/headers.txt"
grep -q 'The name is required' "$tmp/out.html"

person_id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM people WHERE name='Una persona'")"
entry_id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM entries WHERE title='Newest date'")"

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' "http://127.0.0.1:$port/entries/$entry_id/shares")"
[[ "$code" == 200 ]] || {
  echo "Shares dialog returned $code"
  exit 1
}
grep -q 'This entry is not shared' "$tmp/out.html"
grep -q 'value="new"' "$tmp/out.html"
grep -q 'Add share' "$tmp/out.html"

code="$(curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST "http://127.0.0.1:$port/entries/$entry_id/shares" -d "person_id=$person_id&amount=0,50")"
[[ "$code" == 200 ]] || {
  echo "Add share returned $code"
  exit 1
}
grep -qi 'HX-Refresh: true' "$tmp/headers.txt"
shared="$(sqlite3 "$tmp/finances.db" "SELECT amount_cents FROM entry_shares WHERE entry_id=$entry_id AND person_id=$person_id")"
[[ "$shared" == 50 ]] || {
  echo "Share saved incorrectly: $shared"
  exit 1
}
body="$(curl -fsS "http://127.0.0.1:$port/entries/$entry_id/shares")"
grep -q '<th>Person</th>' <<<"$body"

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST "http://127.0.0.1:$port/entries/$entry_id/shares" -d "person_id=$person_id&amount=2")"
[[ "$code" == 200 ]] || {
  echo "Oversized share returned $code"
  exit 1
}
grep -q 'The shared amount exceeds the entry total' "$tmp/out.html"

body="$(curl -fsS http://127.0.0.1:$port/)"
grep -q 'Una persona' <<<"$body"
grep -q '0,50' <<<"$body"
body="$(curl -fsS "http://127.0.0.1:$port/month/$month")"
grep -q '1 person' <<<"$body"

code="$(curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X DELETE "http://127.0.0.1:$port/entries/$entry_id/shares/$person_id")"
[[ "$code" == 200 ]] || {
  echo "Delete share returned $code"
  exit 1
}
grep -qi 'HX-Refresh: true' "$tmp/headers.txt"
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entry_shares')"
[[ "$count" == 0 ]] || {
  echo "Expected no shares after delete, got $count"
  exit 1
}

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST "http://127.0.0.1:$port/entries/$entry_id/shares" -d "person_id=new&new_person=Bob&amount=0,25")"
[[ "$code" == 200 ]] || {
  echo "New person share returned $code"
  exit 1
}
shared="$(sqlite3 "$tmp/finances.db" "SELECT s.amount_cents FROM entry_shares s JOIN people p ON p.id=s.person_id WHERE p.name='Bob'")"
[[ "$shared" == 25 ]] || {
  echo "Share for new person saved incorrectly: $shared"
  exit 1
}
body="$(curl -fsS http://127.0.0.1:$port/)"
grep -q 'Bob' <<<"$body"

bob_id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM people WHERE name='Bob'")"

# Share/person error paths: clear messages, and nothing is left half-created.
share_post() { curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST "http://127.0.0.1:$port/entries/$1/shares" -d "$2"; }
expect_share_error() { # entry, form, message
  [[ "$(share_post "$1" "$2")" == 200 ]] || {
    echo "Share POST failed: $2"
    exit 1
  }
  grep -q "$3" "$tmp/out.html" || {
    echo "Expected '$3' for: $2"
    cat "$tmp/out.html"
    exit 1
  }
}
expect_share_error "$entry_id" "person_id=$bob_id&amount=0,10" 'already has a share'
expect_share_error "$entry_id" "person_id=&amount=0,10" 'Select a person'
expect_share_error "$entry_id" "person_id=new&new_person=Ghost&amount=5" 'exceeds the entry total'
expect_share_error "$entry_id" "person_id=new&new_person=Ghost&amount=abc" 'The amount is not valid'
expect_share_error "$entry_id" "person_id=new&new_person=Bob&amount=0,01" 'already exists'
expect_share_error 999999 "person_id=$bob_id&amount=0,01" 'Entry not found'
count="$(sqlite3 "$tmp/finances.db" "SELECT COUNT(*) FROM people WHERE name='Ghost'")"
[[ "$count" == 0 ]] || {
  echo "Failed share left an orphan person"
  exit 1
}
code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X POST http://127.0.0.1:$port/people -d 'name=Bob')"
grep -q 'That person already exists' "$tmp/out.html"
code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -H "HX-Request: true" -X DELETE "http://127.0.0.1:$port/people/$bob_id")"
[[ "$code" == 200 ]] || {
  echo "Delete person returned $code"
  exit 1
}
! grep -q 'Bob' "$tmp/out.html"
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entry_shares')"
[[ "$count" == 0 ]] || {
  echo "Shares were not cascaded, got $count"
  exit 1
}
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM people')"
[[ "$count" == 1 ]] || {
  echo "Expected 1 person after delete, got $count"
  exit 1
}

# Input rules: titles count characters (not bytes), amounts accept thousands separators.
nn="$(printf 'ñ%.0s' $(seq 1 60))" # 60 chars, 120 bytes
code="$(post --data-urlencode "title=$nn" -d "amount=1.234,56&kind=expense&category=ocio&date=$month-10&month=$month")"
[[ "$code" == 200 ]] || { echo "Multibyte title returned $code"; exit 1; }
cents="$(sqlite3 "$tmp/finances.db" "SELECT amount_cents FROM entries WHERE title LIKE 'ññññ%'")"
[[ "$cents" == 123456 ]] || { echo "1.234,56 saved as: $cents"; exit 1; }
code="$(post --data-urlencode "title=$(printf 'ñ%.0s' $(seq 1 101))" -d "amount=1&kind=expense&category=ocio&date=$month-10&month=$month")"
grep -q 'maximum 100 characters' "$tmp/out.html" || { echo "101-char title accepted"; exit 1; }
code="$(post --data-urlencode "title=a"$'\t'"b" -d "amount=1&kind=expense&category=ocio&date=$month-10&month=$month")"
grep -q 'invalid characters' "$tmp/out.html" || { echo "Control char in title accepted"; exit 1; }
code="$(post -d "title=Ambiguous&amount=1.234&kind=expense&category=ocio&date=$month-10&month=$month")"
grep -q 'The amount is not valid' "$tmp/out.html" || { echo "1.234 accepted"; exit 1; }
code="$(post -d "title=Millions&amount=1.234.567&kind=expense&category=ocio&date=$month-11&month=$month")"
cents="$(sqlite3 "$tmp/finances.db" "SELECT amount_cents FROM entries WHERE title='Millions'")"
[[ "$cents" == 123456700 ]] || { echo "1.234.567 saved as: $cents"; exit 1; }

# The category <select> is generated from kCategories.
body="$(curl -fsS "http://127.0.0.1:$port/month/$month")"
for key in alimentacion transporte vivienda ocio otros; do
  grep -q "<option value=\"$key\">" <<<"$body" || { echo "Missing category option $key"; exit 1; }
done

# Month navigation stops at the supported years instead of linking to 404s.
body="$(curl -fsS "http://127.0.0.1:$port/month/9999-12")"
! grep -q '10000-01' <<<"$body" || { echo "Link past year 9999"; exit 1; }
grep -q '/month/9999-11' <<<"$body"
body="$(curl -fsS "http://127.0.0.1:$port/month/0000-01")"
! grep -q -- '-001' <<<"$body" || { echo "Link before year 0"; exit 1; }
grep -q '/month/0000-02' <<<"$body"

# Guard: mutating requests need HX-Request; Host must be loopback:port.
code="$(curl -sS -o /dev/null -w '%{http_code}' -X POST http://127.0.0.1:$port/people -d 'name=Evil')"
[[ "$code" == 403 ]] || {
  echo "POST without HX-Request returned $code"
  exit 1
}
code="$(curl -sS -o /dev/null -w '%{http_code}' -H 'Host: evil.example:80' http://127.0.0.1:$port/)"
[[ "$code" == 403 ]] || {
  echo "Foreign Host returned $code"
  exit 1
}
count="$(sqlite3 "$tmp/finances.db" "SELECT COUNT(*) FROM people WHERE name='Evil'")"
[[ "$count" == 0 ]] || {
  echo "Blocked POST was executed"
  exit 1
}
curl -sSI http://127.0.0.1:$port/ | grep -qi 'X-Frame-Options: DENY'

# Fail fast when templates are missing (no blank pages, no stray DB).
empty="$(mktemp -d)"
if (cd "$empty" && "$bin" >out.log 2>&1); then
  echo "Started without templates"
  exit 1
fi
grep -q 'Missing templates/' "$empty/out.log"
[[ ! -e "$empty/finances.db" ]] || {
  echo "DB created without templates"
  exit 1
}
rm -rf "$empty"

echo "OK: dashboard, entries, deletion, people, shares, validation, date order and statistics"
