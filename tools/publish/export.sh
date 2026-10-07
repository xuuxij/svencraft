#!/bin/sh
# export.sh [DEST] [--list]: build or refresh the publishing staging folder (default: <project>/../Svencraft-publish)
# from tools/publish/manifest.txt. Writes and deletes only inside DEST. See export.py for the details.
HERE=$(cd "$(dirname "$0")" && pwd)
exec python "$HERE/export.py" "$@"
