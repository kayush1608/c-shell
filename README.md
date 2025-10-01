# C-shell

A small, POSIX-style shell written in C, featuring pipelines, I/O redirection, background jobs, and a handful of custom built-ins. It uses process groups for job control and provides a minimal yet practical command experience in Linux terminals.

## Overview

This shell implements:

- Prompt with user, host, and current directory: `<user@host:~relative-path>`
- Command separators: `;` for sequential and `&` for background
- Pipelines: `cmd1 | cmd2 | cmd3`
- I/O redirection: `< infile`, `> outfile`, `>> outfile`
- Quoted arguments: single `'` and double `"` quotes
- Foreground and background job control using process groups
- Command logging (last 15 commands) with replay
- Custom built-ins: `hop`, `reveal`, `jobs`, `activities`, `fg`, `bg`, `log`, `ping`, `exit`

Linux is required (uses `/proc` for the `activities` builtin and POSIX APIs).

## Build

The project uses a `Makefile` and standard GCC flags.

```bash
make        # builds shell.out
./shell.out # runs the shell
```

Clean build artifacts:

```bash
make clean
```

## Prompt

The prompt prints as:

```
<user@host:~path>
```

Where `~` is the shell’s startup directory (the directory where you launched `./shell.out`). Paths under this startup directory are displayed as `~/<subpath>`.

## Usage

- Sequential commands: `cmd1; cmd2; cmd3`
- Background command: `long_running_cmd &`
- Pipelines: `producer | filter | consumer`
- Redirection:
	- Input: `cmd < infile` (fails if `infile` doesn’t exist)
	- Output: `cmd > outfile`
	- Append: `cmd >> outfile`

Quotes around arguments are supported: `cmd "arg with spaces" 'another arg'`.

### Multiple output redirections

This shell supports multiple output redirections in a single command. Files are processed left-to-right; only the last redirection receives the command’s stdout, while earlier ones are created/truncated/appended as specified.

Example:

```
echo hello > a.txt >> b.txt > final.txt
# a.txt created/truncated, b.txt created/appended, final.txt gets the output
```

## Built-ins

Built-ins run without spawning external processes and are available inside pipelines where needed.

- `hop [path ...]`
	- Change directory (similar to `cd`).
	- Supports `~` (shell startup directory) and `-` (previous directory via `OLDPWD`).
	- Multiple paths are applied sequentially.
	- On success, updates `PWD` and `OLDPWD`.
	- Examples:
		- `hop /tmp`
		- `hop ~`
		- `hop -`

- `reveal [-a] [-l] [path|-]`
	- List directory contents (similar to `ls`).
	- `-a`: include hidden files. `-l`: print one entry per line.
	- `-` uses `OLDPWD` (only if set by this shell in the current session).
	- Entries are sorted lexicographically.
	- Examples:
		- `reveal`
		- `reveal -a /var/log`
		- `reveal -l -`

- `jobs`
	- Show active jobs with `[job_id] Running|Stopped <cmd>`.

- `activities`
	- Show process group IDs (PGIDs) and state by reading `/proc/<pgid>/stat`.
	- Output: `[#pgid]: <cmd> - Running|Stopped`.

- `fg <job_id>`
	- Bring a stopped/running background job to the foreground and attach the terminal.

- `bg <job_id>`
	- Resume a stopped job in the background.

- `log [execute N]`
	- Print the last 15 commands (oldest to newest).
	- `log execute N` replays a previous command by index where `1` is the newest command.
	- Prevents recursive `log execute` loops.

- `ping <pid> <signal_number>`
	- Send a signal to a process. The signal used is `signal_number % 32`.
	- Example: `ping 12345 9` (sends SIGKILL).

- `exit`
	- Exit the shell immediately.

## Job control and signals

- Foreground jobs are given the terminal; `Ctrl+C` (SIGINT) and `Ctrl+Z` (SIGTSTP) are forwarded to the foreground process group.
- Background jobs print a line like: `[<job_id>] <pgid>` when started with `&`.
- On EOF (`Ctrl+D`) in an interactive terminal, the shell prints `logout`, kills all tracked jobs, and exits.

## Pipelines and redirection details

- Pipelines connect stdout of each command to stdin of the next: `a | b | c`.
- Built-ins inside pipelines are executed in forked children with proper redirection.
- Input redirection `< file` must reference an existing file.
- Output redirection respects append (`>>`) vs truncate (`>`).

## Examples

```bash
# List then filter via grep
reveal -a | grep ".c$"

# Run a command in the background
sleep 10 &
jobs

# Stop then resume a job
Ctrl+Z        # in the foreground job
bg 1          # resume job 1 in background
fg 1          # bring job 1 to foreground

# Use hop (cd-like) and reveal (ls-like)
hop /tmp
reveal -l

# Redirection
echo "hello" > out.txt
cat < out.txt | tr a-z A-Z >> upper.txt

# Logging and replay
log
log execute 1
```

## Project structure

```
include/
	builtins.h   # builtin interfaces
	executor.h   # executor interfaces
	globals.h    # extern global declarations
	input.h      # input interface
	jobs.h       # job control data and functions
	parser.h     # parsing interfaces and constants
	prompt.h     # prompt helpers
src/
	builtins.c   # implementations of built-ins
	executor.c   # pipelines, redirection, logging, execution
	input.c      # alternate input helper (not used by main loop)
	jobs.c       # job registry, state checks, PGID handling
	main.c       # main loop, signals, command dispatch
	parser.c     # tokenizer, syntax validation, quotes & redirs
	prompt.c     # prompt rendering and ~path mapping
Makefile       # build rules (produces shell.out)
README.md      # this file
```

## Notes and limitations

- `~` maps to the shell startup directory, not `$HOME`.
- `reveal -l` prints one entry per line (not full long listing like `ls -l`).
- No variable expansion (`$VAR`) or globbing (`*`, `?`).
- No subshells, conditionals, or advanced shell syntax.
- `activities` relies on `/proc` and Linux-specific behavior.
- Multiple output redirections create/truncate/append earlier files but only the final redirection receives output.
