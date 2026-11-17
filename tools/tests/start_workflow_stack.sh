#!/usr/bin/env bash
# Start isolated local dependencies for workflow_integration.py; never uses a shared database.
set -euo pipefail
crossdo_tools_dir="${CROSSDO_TOOLS:-/workspace/.crossdo-tools}"
if [[ -f "$crossdo_tools_dir/activate.sh" ]]; then
    # Optional development toolchain supplied by the workspace.
    source "$crossdo_tools_dir/activate.sh"
fi
exec python3 "$(dirname "$0")/workflow_stack.py" "$@"
