# Uatu: WakaTime for your Terminal

I just wanted to track time on how much I spend in a project from start to completion.

It automatically detects:
- **Project Context:** Groups time by Git repositories (e.g. `~/work/backend` counts towards `backend`).
- **Idle Time:** Smartly ignores lunch breaks or inactive windows.
- **Deep Work:** Tracks time spent running builds, tests, SSH sessions, and file operations.

## Quick Start

### 1. Install
Run the installer script. It will compile the tool and add it to your shell configuration.

```bash
chmod +x install.sh
./install.sh
```

### 2. Activate
Restart your terminal or run:
```bash
source ~/.zshrc
```

**That's it.** Uatu is now running in the background.

## Usage

You don't need to do anything. Uatu watches your shell navigation and commands automatically.

### View Your Stats

To see where your time went:
```bash
uatu report
```

Example Output:
```text
Project / Directory                                          Time Spent      Sessions
-------------------                                          ----------      --------
/Users/JhonDoe/Costco/uatu                                  04:12:05    12
/Users/JohnDoe/work/legacy-api                                01:45:30    5
/Users/JohnDoe/.config/nvim                                   00:20:15    3
```

### Clean Up Noise
If you have accidental directory hops (< 1 second), you can clean them:
```bash
uatu cleanup
```

## How It Works

- **Zero Config:** It auto-initializes its local database at `~/.uatu/uatu.db`.
- **Privacy First:** All data is stored locally on your machine. Nothing is sent to the cloud.
- **Performance:** Written in C for sub-millisecond overhead.

## Uninstall

To remove Uatu:
1. Delete `~/.uatu` directory.
2. Remove the `source .../uaterc.zsh` line from your `~/.zshrc`.
