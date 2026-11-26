#!/usr/bin/env bash
# Smoke test: start the server with an empty DB in a temporary directory.
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
bin="${1:-$root/build/finances}"
[[ -x "$bin" ]] || { echo "$bin does not exist; build with CMake first."; exit 1; }

tmp="$(mktemp -d)"
pid=""
trap '[[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true; rm -rf "$tmp"' EXIT
ln -s "$root/templates" "$tmp/templates"
ln -s "$root/static" "$tmp/static"
cd "$tmp"
"$bin" >server.log 2>&1 &
pid=$!

for _ in $(seq 1 100); do
  curl -fsS -o /dev/null http://127.0.0.1:8080/ && break
  sleep 0.2
done

month="$(date +%Y-%m)"
body="$(curl -fsS http://127.0.0.1:8080/)"
grep -q 'Your finances at a glance' <<<"$body"
grep -q 'dashCategoryChartData' <<<"$body"
grep -q 'dashCurrentChartData' <<<"$body"
grep -q 'currentCategoryChartData' <<<"$body"

body="$(curl -fsS "http://127.0.0.1:8080/month/$month")"
grep -q 'No entries yet this month' <<<"$body"

post() { curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -X POST http://127.0.0.1:8080/entries "$@"; }

code="$(post -d "title=Test purchase&amount=12,50&kind=expense&category=alimentacion&date=$month-15&month=$month")"
[[ "$code" == 200 ]] || { echo "Valid POST returned $code"; exit 1; }
grep -q 'Test purchase' "$tmp/out.html"
grep -q '12,50' "$tmp/out.html"
grep -q 'hx-delete=' "$tmp/out.html"
! grep -q '<th>Added</th>' "$tmp/out.html"

code="$(post -d "title=Nominal&amount=1000&kind=income&category=otros&date=$month-01&month=$month")"
[[ "$code" == 200 ]] || { echo "Income POST returned $code"; exit 1; }

code="$(post -d "title=Bad&amount=abc&kind=expense&category=ocio&date=$month-01&month=$month")"
[[ "$code" == 200 ]] || { echo "Invalid amount returned $code"; exit 1; }
grep -q 'The amount is not valid.' "$tmp/out.html"
grep -qi 'HX-Retarget: #form-error' "$tmp/headers.txt"

other="$(date -d "$month-01 -1 month" +%Y-%m 2>/dev/null || echo 2026-10)"
code="$(post -d "title=Other month&amount=5&kind=expense&category=ocio&date=$other-01&month=$month")"
[[ "$code" == 200 ]] || { echo "Date from another month returned $code"; exit 1; }
grep -q 'The date must belong to the displayed month' "$tmp/out.html"

count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entries')"
[[ "$count" == 2 ]] || { echo "Expected 2 rows, got $count"; exit 1; }
cents="$(sqlite3 "$tmp/finances.db" "SELECT amount_cents FROM entries WHERE title='Test purchase'")"
[[ "$cents" == 1250 ]] || { echo "Amount saved incorrectly: $cents"; exit 1; }

body="$(curl -fsS http://127.0.0.1:8080/)"
grep -q '1.000,00' <<<"$body"
grep -q '12,50' <<<"$body"

id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM entries WHERE title='Test purchase'")"
code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -X DELETE "http://127.0.0.1:8080/entries/$id")"
[[ "$code" == 200 ]] || { echo "DELETE returned $code"; exit 1; }
grep -q 'Nominal' "$tmp/out.html"
! grep -q 'Test purchase' "$tmp/out.html"
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entries')"
[[ "$count" == 1 ]] || { echo "Expected 1 row after delete, got $count"; exit 1; }

code="$(post -d "title=Newest date&amount=1&kind=expense&category=ocio&date=$month-25&month=$month")"
[[ "$code" == 200 ]] || { echo "Late POST returned $code"; exit 1; }
newest="$(grep -n 'Newest date' "$tmp/out.html" | cut -d: -f1 | head -1)"
older="$(grep -n 'Nominal' "$tmp/out.html" | cut -d: -f1 | head -1)"
[[ "$newest" -lt "$older" ]] || { echo "Entries are not ordered by date"; exit 1; }

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -X POST http://127.0.0.1:8080/people -d 'name=Una+persona')"
[[ "$code" == 200 ]] || { echo "Add person returned $code"; exit 1; }
people_count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM people')"
[[ "$people_count" == 1 ]] || { echo "Person was not saved"; exit 1; }
body="$(curl -fsS http://127.0.0.1:8080/)"
grep -q '<h2>People</h2>' <<<"$body"
grep -q 'Una persona' <<<"$body"

code="$(curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -X POST http://127.0.0.1:8080/people -d 'name=')"
[[ "$code" == 200 ]] || { echo "Empty person returned $code"; exit 1; }
grep -qi 'HX-Reswap: innerHTML' "$tmp/headers.txt"
grep -q 'The name is required' "$tmp/out.html"

person_id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM people WHERE name='Una persona'")"
entry_id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM entries WHERE title='Newest date'")"

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' "http://127.0.0.1:8080/entries/$entry_id/shares")"
[[ "$code" == 200 ]] || { echo "Shares dialog returned $code"; exit 1; }
grep -q 'This entry is not shared' "$tmp/out.html"
grep -q 'value="new"' "$tmp/out.html"
grep -q 'Add share' "$tmp/out.html"

code="$(curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -X POST "http://127.0.0.1:8080/entries/$entry_id/shares" -d "person_id=$person_id&amount=0,50")"
[[ "$code" == 200 ]] || { echo "Add share returned $code"; exit 1; }
grep -qi 'HX-Refresh: true' "$tmp/headers.txt"
shared="$(sqlite3 "$tmp/finances.db" "SELECT amount_cents FROM entry_shares WHERE entry_id=$entry_id AND person_id=$person_id")"
[[ "$shared" == 50 ]] || { echo "Share saved incorrectly: $shared"; exit 1; }
body="$(curl -fsS "http://127.0.0.1:8080/entries/$entry_id/shares")"
grep -q '<th>Person</th>' <<<"$body"

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -X POST "http://127.0.0.1:8080/entries/$entry_id/shares" -d "person_id=$person_id&amount=2")"
[[ "$code" == 200 ]] || { echo "Oversized share returned $code"; exit 1; }
grep -q 'The shared amount exceeds the entry total' "$tmp/out.html"

body="$(curl -fsS http://127.0.0.1:8080/)"
grep -q 'Una persona' <<<"$body"
grep -q '0,50' <<<"$body"
body="$(curl -fsS "http://127.0.0.1:8080/month/$month")"
grep -q '1 person' <<<"$body"

code="$(curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -X DELETE "http://127.0.0.1:8080/entries/$entry_id/shares/$person_id")"
[[ "$code" == 200 ]] || { echo "Delete share returned $code"; exit 1; }
grep -qi 'HX-Refresh: true' "$tmp/headers.txt"
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entry_shares')"
[[ "$count" == 0 ]] || { echo "Expected no shares after delete, got $count"; exit 1; }

code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -X POST "http://127.0.0.1:8080/entries/$entry_id/shares" -d "person_id=new&new_person=Bob&amount=0,25")"
[[ "$code" == 200 ]] || { echo "New person share returned $code"; exit 1; }
shared="$(sqlite3 "$tmp/finances.db" "SELECT s.amount_cents FROM entry_shares s JOIN people p ON p.id=s.person_id WHERE p.name='Bob'")"
[[ "$shared" == 25 ]] || { echo "Share for new person saved incorrectly: $shared"; exit 1; }
body="$(curl -fsS http://127.0.0.1:8080/)"
grep -q 'Bob' <<<"$body"

bob_id="$(sqlite3 "$tmp/finances.db" "SELECT id FROM people WHERE name='Bob'")"
code="$(curl -sS -o "$tmp/out.html" -w '%{http_code}' -X DELETE "http://127.0.0.1:8080/people/$bob_id")"
[[ "$code" == 200 ]] || { echo "Delete person returned $code"; exit 1; }
! grep -q 'Bob' "$tmp/out.html"
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM entry_shares')"
[[ "$count" == 0 ]] || { echo "Shares were not cascaded, got $count"; exit 1; }
count="$(sqlite3 "$tmp/finances.db" 'SELECT COUNT(*) FROM people')"
[[ "$count" == 1 ]] || { echo "Expected 1 person after delete, got $count"; exit 1; }

echo "OK: dashboard, entries, deletion, people, shares, validation, date order and statistics"
