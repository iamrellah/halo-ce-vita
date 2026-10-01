#!/bin/bash
# snapshot.sh DIR REMOTE BRANCH: commit DIR's whole working tree (tracked + untracked, .gitignore honoured,
# no game data, nothing over 50 MB) onto BRANCH via a temporary index, and push it; the tree and index are untouched
set -e
d=$1; remote=$2; branch=$3
cd "$d"
export GIT_INDEX_FILE=$(mktemp -u /tmp/claude-1000/snapidx.XXXXXX)
git read-tree HEAD
git add -A .
git ls-files -z | while IFS= read -r -d '' f; do
  if [[ "$f" =~ \.(map|xbe|iso|bik|xex)$ ]] || [[ "$f" == *code_*.c && "$f" == recomp/* ]] || { [ -f "$f" ] && [ $(stat -c %s "$f") -gt 50000000 ]; }; then
    git rm -q --cached -- "$f" && echo "  skipped $f"; fi
done
tree=$(git write-tree)
parent="-p HEAD"; [ -n "$ORPHAN" ] && parent=""
commit=$(git commit-tree $tree $parent -m "Backup snapshot of the working tree, $(date '+%Y-%m-%d %H:%M') ($(git branch --show-current || true) at $(git rev-parse --short HEAD))")
rm -f "$GIT_INDEX_FILE"
echo "  $d -> $commit: $(git diff --stat HEAD $commit | tail -1)"
git push -q "$remote" $commit:refs/heads/$branch
echo "  pushed $branch"
