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
    echo "Limitations: This tools only supports PRs that landed as single commit, e.g. with 'commit-queue-squash' label." >&2

    exit 1
}

SED_CMD='s/^index [a-f0-9]\+..[a-f0-9]\+ \([0-7]\{6\}\)$/index eeeeeeeeee..eeeeeeeeee \1/g;s/^@@ -[0-9]\+,[0-9]\+ +[0-9]\+,[0-9]\+ @@/@@ -111,1 +111,1 @@/'

set -ex

BACKPORT_PR_URL=$(gh pr view "$BACKPORT_PR" --json url --jq .url)

ORIGINAL=$(mktemp)
BACKPORT=$(mktemp)
trap 'set -x; rm -f "$ORIGINAL" "$BACKPORT"; set +x; trap - EXIT; exit' EXIT INT HUP

gh pr view "$BACKPORT_PR" --json commits --jq '.[] | map([ .oid, (.messageBody | match("(?:^|\\n)PR-URL: (https?://.+/pull/\\d+)(?:\\n|$)", "g") | .captures | last | .string)] | @tsv) | .[]' \
| while read -r LINE; do
  COMMIT_SHA=$(echo "$LINE" | cut -f1)
  PR_URL=$(echo "$LINE" | cut -f2)

  curl -fsL "$PR_URL.diff" | sed "$SED_CMD" >> "$ORIGINAL"
  curl -fsL "$BACKPORT_PR_URL/commits/$COMMIT_SHA.diff" | sed "$SED_CMD" >> "$BACKPORT"
done

${DIFF_CMD:-diff} "$ORIGINAL" "$BACKPORT" || echo "diff command exited with $?" >&2

set +x

printf "Approve the PR using gh? [y/N] "
read -r r
[ "$r" != "y" ] || {
  set -x
  gh pr review "$BACKPORT_PR" --approve
}
