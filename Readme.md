# Ion

A simple shell built in C from scratch. It's a CLI interpreter that sits between you and the kernel — no extra layers, no magic.

## Features

- **Tab completion** — completes command names from `$PATH` and filenames under the current directory
- **Directory jumping** — `j <name>` jumps to the most visited directory matching that name, no full path needed (fuzzy match using frecency)
- **Built-in tree view** — `tree <depth>` prints a directory tree without needing the `tree` package installed
- **Pipes** — `cmd1 | cmd2` connects stdout of one process to stdin of another
- **Redirections** — `>` overwrite, `>>` append, `<` input redirection
- **Signal handling** — Ctrl+C kills the running command, not the shell. Ctrl+D exits cleanly
- **Raw mode input** — reads keypresses directly using `termios`, no line buffering. This is what makes tab completion and custom keybindings possible
- **CWD in prompt** — shows current directory with `~` shortening, updates on every command

## Building

```bash
git clone https://github.com/Whitfrost21/Ion
cd ion
make
./ion
```

## Usage

```bash
# pipes
ls | grep .c

# redirections
echo hello > out.txt
cat < out.txt
echo world >> out.txt

# directory jump (visits are tracked automatically on cd)
cd ~/workspace/syswork/shell
j shell        # jumps back to shell next time

# tree
tree 2         # print directory tree 2 levels deep

# tab completion
pw<tab>        # completes to pwd
cat io<tab>    # completes to ion.c
```

## How it works

Every command goes through the same path: read → parse → execute.

**Execution** uses the Unix `fork`+`execvp` model. The shell forks a child process, the child calls `execvp` which replaces itself with the requested program, and the parent waits. Built-in commands like `cd` and `exit` skip this entirely and run directly in the shell process — a child process changing its directory would die immediately leaving the parent unchanged.

**Pipes** use `pipe()` + `dup2()` to connect the stdout of one child to the stdin of another. `dup2` redirects file descriptors — the same trick powers redirections, just with files instead of pipe ends.

**Directory jumping** stores visited paths and their frequency in `~/.ion_history`. `j <partial>` matches against the last component of each stored path using `strstr()`, picks the highest frequency match, and `chdir()`s there.

**Tab completion** scans every directory in `$PATH` for command matches, and the current directory for filename matches. All of this is possible because of raw mode — without `termios` the shell would never see the Tab keypress, the terminal would just insert a tab character.

## Setting ion as your default shell(testing case)

```bash
# add ion to the list of valid shells
echo $(pwd)/ion | sudo tee -a /etc/shells

# set as default (replace /path/to/ion with the actual path)
chsh -s /path/to/ion
```

## Roadmap

- Command history with arrow keys
- Left/right cursor navigation
- `tree -a` for hidden files
- Multiple pipes (`cmd1 | cmd2 | cmd3`)
- Prompt showing current git branch
- Config file (`~/.config/ion/config`)
- Flag completion per command

## Contibutions

 Contibutions are Welcome, feel free to start Discussions, Issues and send PRs.
