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

body="$(curl -fsS "http://127.0.0.1:8080/month/$month")"
grep -q 'No entries yet this month' <<<"$body"

post() { curl -sS -D "$tmp/headers.txt" -o "$tmp/out.html" -w '%{http_code}' -X POST http://127.0.0.1:8080/entries "$@"; }

code="$(post -d "title=Test purchase&amount=12,50&kind=expense&category=alimentacion&date=$month-15&month=$month")"
[[ "$code" == 200 ]] || { echo "Valid POST returned $code"; exit 1; }
grep -q 'Test purchase' "$tmp/out.html"
grep -q '12,50' "$tmp/out.html"

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

echo "OK: dashboard, entry list, add, validation and statistics"
