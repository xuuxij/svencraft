#!/bin/sh
# publish.sh --yes [-m "commit message"] [DEST]
# Publish Svencraft to the PRIVATE GitHub repo xuuxij/svencraft. Run it only when the owner asks to publish.
#   1. export.sh  -> refresh the staging folder (default <project>/../Svencraft-publish) from manifest.txt
#   2. audit.py   -> stop on any personal-data / content hit
#   3. git init (first run), stage everything, pin the submodule gitlinks, commit as the noreply identity with a
#      UTC date (nothing to commit = no commit)
#   4. audit.py again (now also checks every commit's identity and UTC dates)
#   5. first run: gh repo create xuuxij/svencraft --private --source . --remote origin --push; later: git push
# Without --yes it does nothing.
set -eu
YES=0; MSG=""; DEST=""
while [ $# -gt 0 ]; do
  case $1 in
    --yes) YES=1 ;;
    -m) shift; MSG=$1 ;;
    -*) echo "unknown option $1" >&2; exit 2 ;;
    *) DEST=$1 ;;
  esac
  shift
done
if [ $YES -ne 1 ]; then
  echo "publish.sh: refusing to run without --yes (it commits and pushes to GitHub)." >&2
  echo "  dry run instead: sh tools/publish/export.sh && python tools/publish/audit.py" >&2
  exit 2
fi
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
DEST=${DEST:-$(dirname "$ROOT")/Svencraft-publish}
NAME=xuuxij
EMAIL=20761916+xuuxij@users.noreply.github.com
REPO=xuuxij/svencraft

# GitHub CLI must be logged in as the repo's owner
LOGIN=$(gh api user -q .login 2>/dev/null || true)
if [ "$LOGIN" != "$NAME" ]; then echo "gh is logged in as '$LOGIN', not $NAME: stop" >&2; exit 1; fi

python "$HERE/export.py" "$DEST"
python "$HERE/audit.py" "$DEST"

cd "$DEST"
[ -d .git ] || git init -q -b main
# this repo's identity and settings (never the global ones: the global e-mail is personal)
git config user.name "$NAME"
git config user.email "$EMAIL"
git config core.autocrlf false
git config core.safecrlf false
git config commit.gpgsign false
export GIT_AUTHOR_NAME="$NAME" GIT_AUTHOR_EMAIL="$EMAIL" GIT_COMMITTER_NAME="$NAME" GIT_COMMITTER_EMAIL="$EMAIL"
export TZ=UTC
NOW="$(date -u +%s) +0000"
export GIT_AUTHOR_DATE="$NOW" GIT_COMMITTER_DATE="$NOW"

# stage exactly the export (-f: upstream .gitignore files must not drop tracked upstream files; .export/ is bookkeeping)
git add -A -f -- . ':(exclude).export'
# the engine's and game's submodules as gitlinks pinned to the commits export.py recorded
while read -r SHA SUBPATH URL; do
  [ -n "$SHA" ] && git update-index --add --cacheinfo "160000,$SHA,$SUBPATH"
done < .export/submodules.txt

if git diff --cached --quiet; then
  echo "nothing changed since the last commit"
else
  [ -n "$MSG" ] || MSG="Svencraft snapshot $(date -u +%Y-%m-%d)"
  git commit -q -m "$MSG"
  echo "committed: $(git log -1 --format='%h %an <%ae> %ad' --date=iso-strict)"
fi

python "$HERE/audit.py" "$DEST"

if git remote get-url origin > /dev/null 2>&1; then
  git push origin HEAD:main
else
  gh repo create "$REPO" --private --source . --remote origin --push
fi
echo "published: https://github.com/$REPO (private)"
