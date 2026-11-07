#!/bin/sh

BACKPORT_PR=$1

[ -x "$(command -v gh || true)" ] || {
    # shellcheck disable=SC2016
    echo 'Missing required `gh` dependency' >&2
    exit 1
}
[ -n "$BACKPORT_PR" ] || {
    echo "Usage:" >&2
    echo 'tools/actions/review_backport.sh https://github.com/nodejs/node/pull/<backport-PR-number> | less' >&2
    echo 'DIFF_CMD="codium --wait --diff" tools/actions/review_backport.sh https://github.com/nodejs/node/pull/<backport-PR-number>' >&2

    exit 1
}

SED_CMD=
SED_CMD="${SED_CMD};"'s/^index [a-f0-9]\+..[a-f0-9]\+\( [0-7]\{6\}\)\?$/index eeeeeeeeee..eeeeeeeeee\1/g'
SED_CMD="${SED_CMD};"'s/^From [a-f0-9]\{40\} /From 0000000000000000000000000000000000000000 /'
SED_CMD="${SED_CMD};"'s/^@@ -[0-9]\+,[0-9]\+ +[0-9]\+,[0-9]\+ @@/@@ -111,1 +111,1 @@/'

set -ex

REMOTE=${GIT_REMOTE:-https://github.com/${GH_REPO:-nodejs/node}.git}

BACKPORT_PR_URL=$(gh pr view "$BACKPORT_PR" --json url --jq .url)

ORIGINAL=$(mktemp)
BACKPORT=$(mktemp)
trap 'set -x; rm -f "$ORIGINAL" "$BACKPORT"; set +x; trap - EXIT; exit' EXIT INT HUP

${DIFF_CMD_NO_WAIT:-diff} "$ORIGINAL" "$BACKPORT"

gh pr view "$BACKPORT_PR" --json commits --jq '.[] | map([ .oid, .messageHeadline, (.messageBody | match("(?:^|\\n)PR-URL: (https?://.+/pull/\\d+)(?:\\n|$)", "g") | .captures | last | .string)] | @tsv) | .[]' \
| while read -r LINE; do
  COMMIT_SHA=$(echo "$LINE" | cut -f1)
  COMMIT_TITLE=$(echo "$LINE" | cut -f2)
  PR_URL=$(echo "$LINE" | cut -f3)

  [ -n "$PR_URL" ] || {
      echo "Missing PR-URL in commit message for $COMMIT_SHA" >&2
      exit 1
  }

  ORIGINAL_BASE=$(gh pr view "$PR_URL" --json baseRefName -q '.baseRefName')
  git fetch "$REMOTE" "$ORIGINAL_BASE"
  ORIGINAL_COMMIT=$(
    git log FETCH_HEAD --fixed-strings --grep="$COMMIT_TITLE" --format="%H%x09%s%x09%(trailers:key=PR-URL,only,valueonly)" \
    | awk -F'\t' -v t="$COMMIT_TITLE" -v p="$PR_URL" '$2 == t && $3 == p { print $1; exit }'
  )

  curl -fsL "$BACKPORT_PR_URL/commits/$COMMIT_SHA.patch" | sed "$SED_CMD" >> "$BACKPORT"
  [ -n "$ORIGINAL_COMMIT" ] || {
      echo "Error: Could not locate landed commit with title '$COMMIT_TITLE' in $REMOTE/$ORIGINAL_BASE" >&2
      continue
  }
  git format-patch --no-signature --stdout "${ORIGINAL_COMMIT}^!" | sed "$SED_CMD" >> "$ORIGINAL"
done

${DIFF_CMD:-diff} "$ORIGINAL" "$BACKPORT" || echo "diff command exited with $?" >&2

set +x

printf "Approve the PR using gh? [y/N] "
read -r r
[ "$r" != "y" ] || {
  set -x
  gh pr review "$BACKPORT_PR" --approve
}
