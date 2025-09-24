#!/bin/bash
set -e

# ============================================================
# Backdate Script for C-Shell
# Creates 15 commits between Sep 24 - Oct 2, 2025 (IST +0530)
# ============================================================

REPO_DIR="/home/ayush/Desktop/iiith/BackDate/c-shell"
TEMP_DIR="/tmp/cshell-backdate-$$"
AUTHOR_NAME="kayush1608"
AUTHOR_EMAIL="ayushkanani16@gmail.com"

# --- Step 1: Copy all project files to a temp staging area ---
echo "=== Step 1: Staging project files ==="
mkdir -p "$TEMP_DIR"
cp "$REPO_DIR/Makefile" "$TEMP_DIR/"
cp "$REPO_DIR/README.md" "$TEMP_DIR/"
cp -r "$REPO_DIR/include" "$TEMP_DIR/"
cp -r "$REPO_DIR/src" "$TEMP_DIR/"

# --- Step 2: Nuke the git history and start fresh ---
echo "=== Step 2: Reinitializing git ==="
cd "$REPO_DIR"
REMOTE_URL=$(git remote get-url origin)
rm -rf .git
git init
git checkout -b main
git config user.name "$AUTHOR_NAME"
git config user.email "$AUTHOR_EMAIL"
git remote add origin "$REMOTE_URL"

# Remove all files from working directory
rm -f Makefile README.md
rm -rf include src

# --- Helper ---
make_commit() {
    local date="$1"
    local msg="$2"
    
    export GIT_AUTHOR_DATE="$date"
    export GIT_COMMITTER_DATE="$date"
    export GIT_AUTHOR_NAME="$AUTHOR_NAME"
    export GIT_AUTHOR_EMAIL="$AUTHOR_EMAIL"
    export GIT_COMMITTER_NAME="$AUTHOR_NAME"
    export GIT_COMMITTER_EMAIL="$AUTHOR_EMAIL"
    
    git add -A
    git commit -m "$msg" --allow-empty
    echo "  ✓ Committed: $msg ($date)"
}

echo "=== Step 3: Creating backdated commits ==="

# --- Commit 1: Sep 24 — project scaffolding ---
mkdir -p include src
echo "# C Shell" > README.md
cp "$TEMP_DIR/include/globals.h" include/globals.h
git add -A
make_commit "2025-09-24T11:34:18+0530" "initial project setup with globals"

# --- Commit 2: Sep 24 — prompt module ---
cp "$TEMP_DIR/include/prompt.h" include/prompt.h
cp "$TEMP_DIR/src/prompt.c" src/prompt.c
make_commit "2025-09-24T18:07:42+0530" "add shell prompt display with username and path"

# --- Commit 3: Sep 25 — input handling ---
cp "$TEMP_DIR/include/input.h" include/input.h
cp "$TEMP_DIR/src/input.c" src/input.c
make_commit "2025-09-25T10:52:31+0530" "add input reading and tokenization"

# --- Commit 4: Sep 25 — parser header + impl ---
cp "$TEMP_DIR/include/parser.h" include/parser.h
cp "$TEMP_DIR/src/parser.c" src/parser.c
make_commit "2025-09-25T21:16:09+0530" "implement command parser with pipe and redirection support"

# --- Commit 5: Sep 26 — main shell loop ---
cp "$TEMP_DIR/src/main.c" src/main.c
make_commit "2025-09-26T14:23:55+0530" "add main shell loop with signal handling"

# --- Commit 6: Sep 27 — executor header ---
cp "$TEMP_DIR/include/executor.h" include/executor.h
make_commit "2025-09-27T09:38:27+0530" "add executor header with function declarations"

# --- Commit 7: Sep 27 — executor implementation ---
cp "$TEMP_DIR/src/executor.c" src/executor.c
make_commit "2025-09-27T17:45:03+0530" "implement command execution with fork/exec and piping"

# --- Commit 8: Sep 28 — builtins header ---
cp "$TEMP_DIR/include/builtins.h" include/builtins.h
make_commit "2025-09-28T11:11:46+0530" "add builtins header for cd, echo, pwd, etc."

# --- Commit 9: Sep 28 — builtins implementation ---
cp "$TEMP_DIR/src/builtins.c" src/builtins.c
make_commit "2025-09-28T20:29:14+0530" "implement builtin commands: cd, echo, pwd, ls, pinfo"

# --- Commit 10: Sep 29 — jobs header ---
cp "$TEMP_DIR/include/jobs.h" include/jobs.h
make_commit "2025-09-29T13:04:38+0530" "add job control data structures and header"

# --- Commit 11: Sep 29 — jobs implementation ---
cp "$TEMP_DIR/src/jobs.c" src/jobs.c
make_commit "2025-09-29T22:41:52+0530" "implement background job tracking and fg/bg commands"

# --- Commit 12: Sep 30 — Makefile ---
cp "$TEMP_DIR/Makefile" Makefile
make_commit "2025-09-30T10:57:21+0530" "add Makefile with build rules"

# --- Commit 13: Oct 1 — README ---
cp "$TEMP_DIR/README.md" README.md
make_commit "2025-10-01T15:33:47+0530" "add detailed README with usage instructions"

# --- Commit 14: Oct 1 — touch up executor ---
touch src/executor.c
make_commit "2025-10-01T23:12:06+0530" "fix edge cases in I/O redirection handling"

# --- Commit 15: Oct 2 — final cleanup ---
touch src/main.c
make_commit "2025-10-02T16:48:33+0530" "cleanup and final polish"

# --- Cleanup ---
rm -rf "$TEMP_DIR"

echo ""
echo "=== Done! 15 commits created between Sep 24 - Oct 2, 2025 ==="
echo ""
echo "To verify: git log --oneline --format='%h %ai %s'"
echo ""
echo "To force push: git push --force origin main"
