# Windows WSL Setup Guide

This guide covers setting up and working with this repo on Windows using WSL.
If you are on Windows and not using WSL, ignore this file — clone normally in VS Code.

---

## When You Need This

- You want to use Claude Code in a Linux environment on Windows
- You need native ESP-IDF toolchain support
- You prefer WSL2 for development

---

## One-Time Setup

### 1. Install WSL2
In PowerShell as admin:
```powershell
wsl --install
```
Reboot when prompted. Default distro is Ubuntu.

### 2. Install usbipd (for USB device passthrough to WSL)
In PowerShell as admin:
```powershell
winget install usbipd
```
Close and reopen PowerShell after install.

If `usbipd` is not recognized, find it manually:
```powershell
& "C:\Program Files\usbipd-win\usbipd.exe" list
```
Add `C:\Program Files\usbipd-win` to your PATH via:
Windows Search → "Edit environment variables for your account" → Path → Edit → New

### 3. Set Git Identity in WSL
Open WSL terminal and run:
```bash
git config --global user.name "your-github-username"
git config --global user.email "your@email.com"
```

### 4. Set Git Credential Manager
So WSL uses your existing Windows GitHub credentials:
```bash
git config --global credential.helper "/mnt/c/Program\ Files/Git/mingw64/bin/git-credential-manager.exe"
```

### 5. Install Claude Code in WSL
```bash
curl -fsSL https://claude.ai/install.sh | bash
```

### 6. Add dialout group permission (for serial/USB access)
```bash
sudo usermod -a -G dialout $USER
```
Close and reopen WSL for this to take effect.

---

## Why Repos Go on the Windows Drive

Keep repos on a Windows drive (e.g. `J:\ReposWSL\`) rather than WSL home (`~/`):

- **Windows Explorer** can browse, copy, and open the files normally
- **Backups** — OneDrive, robocopy, or any standard Windows tool can see them
- **No performance penalty** — VS Code Remote WSL reads from the Windows filesystem efficiently
- **One copy** — no syncing between Windows and Linux; both sides see the same files

WSL home (`~/`) is only reachable from Windows via `\\wsl.localhost\Ubuntu\home\username\`, which is slower and easy to lose track of.

---

## Creating a New Project from the GitHub Template

1. Go to the template repo on GitHub
2. Click **"Use this template"** → **"Create a new repository"**
3. Name it, set visibility, click **"Create repository"**
4. Copy the clone URL

Then in WSL:

```bash
# Navigate to your Windows repos folder
cd /mnt/j/ReposWSL

# Clone into it (replace URL and folder name)
git clone https://github.com/your-org/your-project.git your-project

# Enter the project
cd your-project

# Open in VS Code (connected to WSL)
code .
```

VS Code will open with `[WSL: Ubuntu]` in the title bar. All builds, flashing, and Claude Code run inside WSL. The files remain on the Windows drive so you can also browse them in Explorer at `J:\ReposWSL\your-project\`.

---

## Cloning This Repo Directly

**Always specify the full target path when cloning — do not clone into WSL home.**

Decide where you keep repos on your Windows drive, for example `J:\ReposWSL\`.
That maps to `/mnt/j/ReposWSL/` in WSL.

```bash
git clone https://github.com/l8fordinner/esp32-base-template.git /mnt/j/ReposWSL/esp32-base-template
```

Replace `/mnt/j/ReposWSL/` with your actual repos path.

---

## Opening the Repo in VS Code

From WSL terminal, cd into the repo and run:
```bash
cd /mnt/j/WSLRepos/esp32-base-template
code .
```

This opens VS Code connected to WSL via the Remote WSL extension.
The title bar will show `[WSL: Ubuntu]` confirming you are in WSL.
All git operations (commit, push, pull) work normally through VS Code's Source Control panel.

---

## Attaching USB Device Each Session

When you plug in your ESP32 board, run this in PowerShell as admin:
```powershell
usbipd list
```

Find your device (e.g. `Silicon Labs CP210x` for Feather HUZZAH32) and note its BUSID (e.g. `7-2`).

Then attach it to WSL:
```powershell
usbipd bind --busid 7-2
usbipd attach --wsl --busid 7-2
```

Verify in WSL:
```bash
ls /dev/ttyUSB*
```

Should show `/dev/ttyUSB0`.

To avoid typing the full path each session, add usbipd to your Windows user PATH:
Windows Search → "Edit environment variables for your account" → Path → Edit → New → `C:\Program Files\usbipd-win`

---

## Finding Your WSL Home in Windows Explorer

WSL home is not directly visible as a drive letter. To access it in Windows Explorer:
Type this in the Explorer address bar:
```
\\wsl.localhost\Ubuntu\home\your-username
```

**Note:** Keep repos on a Windows drive (e.g. J:\WSLRepos\) not in WSL home.
Repos in WSL home are not easily accessible from Windows and can be confusing.

---

## Starting Claude Code

From inside the repo directory in WSL:
```bash
claude
```

First launch will open a browser for authentication. Sign in with your Claude account.
Credentials are stored locally — you will not need to log in every session.
