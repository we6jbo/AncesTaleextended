#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$HOME/Projects/AncesTaleextended"
STATE="$HOME/.local/state/AncesTaleextended"
[[ -d "$ROOT" ]] || { echo "Missing project: $ROOT" >&2;exit 2; }
command -v vibe >/dev/null || { echo 'Vibe is missing from PATH' >&2;exit 3; }
command -v flock >/dev/null || { echo 'flock command missing' >&2;exit 3; }
for file in user-options.md what-oneal-wants.md vibe-init.md chatgpt-report.md nov23.md memory.md runner-report.md daily.md; do
  [[ -f "$ROOT/$file" ]] || { echo "Missing $ROOT/$file" >&2; exit 4; }
done
mkdir -p "$STATE"
chmod 700 "$STATE" 2>/dev/null || true
exec 9>"$STATE/vibe-launcher.lock"
if ! flock -n 9; then
  echo 'Another Vibe session launched by this startup script is already running.'
  echo 'Use the existing Vibe terminal instead of starting a duplicate.'
  read -r -p 'Press Enter to close this terminal. ' _ </dev/tty || true
  exit 5
fi
cd "$ROOT"
PROMPT=$(cat <<'PROMPTEND'
You are starting interactively through AncesTaleextended -> 2menus. Start by reading the following PROJECT-LOCAL files in exactly this order and follow the user's wishes and recorded constraints:
1. user-options.md
2. what-oneal-wants.md (if nonempty)
3. vibe-init.md
4. chatgpt-report.md
5. nov23.md (one-time checklist; do not repeat completed items without a reason)
6. memory.md
7. runner-report.md
8. daily.md

Check whether ~/.buildsq/AncesTaleextended/AncesTaleextended-program is ALREADY running before launching it. Do not open a second genealogy GUI instance or overwrite its running-hoeu.running marker. If not running, set ANCESTALEEXTENDED_ROOT to /home/we6jbo/Projects/AncesTaleextended and set the working directory to that project before launching the genealogy GUI. Use only the already-documented authenticated controller on 127.0.0.1:53141; never print or copy its private token into a report. The controller is not a shell; follow its own protocol.

Perform initial read-only checks including JSON validity and Continued Research file resolution, Family Connection Quest status, and the real application process state. Avoid reinstalling packages, deploying website files, committing/pushing, modifying genealogical records, or changing source code without approval. Do not claim success without observed verification. For a request to share diagnostics with ChatGPT, write a sanitized report in ~/.local/state/AncesTaleextended/reports/vibe-report.md and tell the user the path; do not require terminal output to be pasted. Keep the Vibe interface interactive for ongoing instructions.
PROMPTEND
)
printf 'Opening interactive Mistral Vibe with the AncesTaleextended startup instructions.\n'
# IMPORTANT: positional initial prompt remains interactive; --prompt is programmatic.
exec vibe "$PROMPT"
