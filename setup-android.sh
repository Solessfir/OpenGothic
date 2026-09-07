#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if ! command -v python3 >/dev/null || ! python3 -c 'import sys; sys.exit(sys.version_info < (3, 10))'; then
    read -r -p 'Install Python 3 using your system package manager (requires sudo)? [Y/n] ' answer
    case "${answer,,}" in ''|y|yes) ;; *) echo 'Python 3.10 or newer is required.'; exit 1 ;; esac
    if command -v apt-get >/dev/null; then
        sudo apt-get update
        sudo apt-get install python3
    elif command -v dnf >/dev/null; then
        sudo dnf install python3
    elif command -v pacman >/dev/null; then
        # Do not refresh Arch databases without upgrading the system.
        read -r -p 'Arch needs a full system upgrade with refreshed packages. Upgrade the system and install Python? [Y/n] ' answer
        case "${answer,,}" in ''|y|yes) ;; *) echo 'Upgrade declined. Install Python yourself and rerun.'; exit 1 ;; esac
        sudo pacman -Syu python
    else
        echo 'Install Python 3.10 or newer with your distribution package manager, then rerun.'
        exit 1
    fi
fi
exec python3 android/tools/setup_android.py "$@"
