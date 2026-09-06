#!/bin/sh
# Point git at the versioned hooks in this directory.
#
#   .githooks/install.sh
#
# core.hooksPath lives in the shared repository config, so this covers every
# worktree of the repo at once. To undo: git config --unset core.hooksPath
set -eu

root=$(git rev-parse --show-toplevel)
cd "$root"
chmod +x .githooks/pre-commit
git config core.hooksPath .githooks
echo "Installed git hooks from .githooks (core.hooksPath)."
echo "  pre-commit: builds fh and runs ./run_tests.sh"
echo "Uninstall with: git config --unset core.hooksPath"
