#!/bin/sh

BACKPORT_PR=$1

[ -n "$BACKPORT_PR" ] || {
    echo "Usage:"
    echo 'tools/actions/review_backport.sh https://github.com/nodejs/node/pull/<backport-PR-number> | less'
    echo 'DIFF_CMD="codium --wait --diff" tools/actions/review_backport.sh https://github.com/nodejs/node/pull/<backport-PR-number>'
    echo "Limitations: This tools only supports PRs that landed as single commit, e.g. with 'commit-queue-squash' label."

    exit 1
}

SED_CMD='s/^index [a-f0-9]\+..[a-f0-9]\+ \([0-7]\{6\}\)$/index eeeeeeeeee..eeeeeeeeee \1/g;s/^@@ -[0-9]\+,[0-9]\+ +[0-9]\+,[0-9]\+ @@/@@ -111,1 +111,1 @@/'

set -ex

ORIGINAL=$(mktemp)
BACKPORT=$(mktemp)
gh pr view "$BACKPORT_PR" --json commits --jq '.[] | map([ .oid, (.messageBody | match("(?:^|\\n)PR-URL: (https?://.+/pull/\\d+)(?:\\n|$)", "g") | .captures | last | .string)] | @tsv) | .[]' \
| while read -r LINE; do
  COMMIT_SHA=$(echo "$LINE" | cut -f1)
  PR_URL=$(echo "$LINE" | cut -f2)

  curl -fsL "$PR_URL.diff" | sed "$SED_CMD" >> "$ORIGINAL"
  curl -fsL "$BACKPORT_PR/commits/$COMMIT_SHA.diff" | sed "$SED_CMD" >> "$BACKPORT"
done

${DIFF_CMD:-diff} "$ORIGINAL" "$BACKPORT"
rm "$ORIGINAL" "$BACKPORT"
