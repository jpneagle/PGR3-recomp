#!/bin/sh
# tools/run.sh <name> [seconds] [extra args...]  -> titles/pgr3/run/<name>.log, prints a summary
cd "$(dirname "$0")/.."
name=$1; secs=${2:-60}; shift; shift
log="$PWD/titles/pgr3/run/$name.log"
mkdir -p titles/pgr3/run; rm -f "$log"
timeout "$secs" out/build/win-amd64-release/pgr3.exe --game_data_root="$PWD/titles/pgr3/game" \
  --log_file="$log" "$@" > "titles/pgr3/run/$name.stdout" 2>&1
echo "exit=$?  lines=$(wc -l < "$log")"
grep -E "\[(warning|error|critical)\]" "$log" | sed -E 's/^\[[^]]*\] //; s/\[t[0-9a-f]+\] //' | sort | uniq -c | sort -rn | head -40
