#!/bin/sh

# Requires [gh](https://cli.github.com/), [jq](https://jqlang.github.io), git, and grep. Also awk if you pass a URL.

# This script can be used to "purple-merge" PRs that are supposed to land as a single commit, using the "Squash and Merge" feature of GitHub.
# To land a PR with this tool:
# 1. Run `git node land <pr-id-or-url> --fixupAll`
# 2. Copy the hash of the commit at the top of the PR branch.
# 3. Run `tools/actions/merge.sh <pr-id-or-url> <commit-hash>` or `tools/actions/merge.sh <url-to-PR-commit>`.

set -xe

pr=$1
commit_head=$2

OWNER=nodejs
REPOSITORY=node

if expr "X$pr" : 'Xhttps://github.com/[^/]\{1,\}/[^/]\{1,\}/pull/[0-9]\{1,\}' >/dev/null; then
  OWNER="$(echo "$pr" | awk 'BEGIN { FS = "/" } ; { print $4 }')"
  REPOSITORY="$(echo "$pr" | awk 'BEGIN { FS = "/" } ; { print $5 }')"
  [ -n "$commit_head" ] || commit_head="$(echo "$pr" | awk 'BEGIN { FS = "/" } ; { print $9 }')"
  pr="$(echo "$pr" | awk 'BEGIN { FS = "/" } ; { print $7 }')"
fi

validation_error=
if ! expr "X$pr" : 'X[0-9]\{1,\}' >/dev/null; then
  set +x
  echo "Invalid PR ID: $pr"
  validation_error=1
fi
if ! expr "X$commit_head" : 'X[a-f0-9]\{40\}' >/dev/null; then
  set +x
  echo "Invalid PR head: $commit_head"
  validation_error=1
fi
[ -z "$validation_error" ] || {
  echo 'Usage:'
  echo "\t$0 <pr-id-or-url> <commit-hash>"
  echo 'or:'
  echo "\t$0 <url-to-PR-commit>"
  echo 'Examples:'
  echo "\t$0 12345 aaaaabbbbbcccccdddddeeeeefffff1111122222"
  echo "\t$0 https://github.com/$OWNER/$REPOSITORY/pull/12345 aaaaabbbbbcccccdddddeeeeefffff1111122222"
  echo "\t$0 https://github.com/$OWNER/$REPOSITORY/pull/12345/commit/aaaaabbbbbcccccdddddeeeeefffff1111122222"
  exit 1
}

git log -1 HEAD  --pretty='format:%B' | git interpret-trailers --parse --no-divider | \
  grep -q -x "^PR-URL: https://github.com/$OWNER/$REPOSITORY/pull/$pr$" || {
    echo "Invalid PR-URL trailer"
    exit 1
  }
git log -1 HEAD^ --pretty='format:%B' | git interpret-trailers --parse --no-divider | \
  grep -q -x "^PR-URL: https://github.com/$OWNER/$REPOSITORY/pull/$pr$" && {
    echo "Refuse to squash and merge a PR landing in more than one commit"
    exit 1
  }

commit_title=$(git log -1 --pretty='format:%s')
commit_body=$(git log -1 --pretty='format:%b')

commitSHA="$(
  jq -cn \
    --arg title "${commit_title}" \
    --arg body "${commit_body}" \
    --arg head "${commit_head}" \
    '{merge_method:"squash",commit_title:$title,commit_message:$body,sha:$head}' |\
  gh api -X PUT "repos/${OWNER}/${REPOSITORY}/pulls/${pr}/merge" --input -\
    --jq 'if .merged then .sha else halt_error end'
)"

gh pr comment "$pr" --repo "$OWNER/$REPOSITORY" --body "Landed in $commitSHA"
