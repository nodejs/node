#!/bin/bash

# Requirements:
# The release-helper script requires having branch-diff, changelog-maker and
# the GitHub CLI installed and properly authorized in the current machine.
if [[ -z "${NODEJS_RELEASE_LINE}" ]]; then
  printf '%s\n' "NODEJS_RELEASE_LINE env var needs to be defined." >&2
  exit 1
else
  CURRENT="${NODEJS_RELEASE_LINE}"
fi

# startcherrypick will paginate and parse through the list of commits in
# chunks of 20, so that it's more convenient for releasers to work in batches
# having time to update the staging branch in between working sessions.
function startcherrypick() {
  head -n 20 .commit_list | xargs git cherry-pick 2>&1 | node ./tools/release/watch-cherry-pick.js
  tail -n +21 .commit_list > .commit_list_next
  mv .commit_list_next .commit_list
}

function endcherrypick() {
  rm .commit_list
}

function helpmsg() {
  cat <<EOF
Usage: release-helper <cmd>

To start working on a new release begin with either of the startup commands:

release-helper cherry-pick
OR
release-helper prepare


Commands:
release-helper cherry-pick     Starts cherry-picking from branch-diff without using a local cache file
release-helper prepare         Uses branch-diff to cache a local file with a list of commits to work with
release-helper start           Starts cherry picking commits from main into the release line branch
release-helper backport        During cherry pick, asks original PR author for a backport using gh cli
release-helper skip            During cherry pick, skips a commit that has conflicts
release-helper continue        During cherry pick, after amending a commit resume the cherry picking
release-helper end             Stop cherry picking commits
release-helper notable         Retrieves notable changes, requires being in the proposal branch
release-helper notable-md      Markdown notable changes, requires being in the proposal branch
release-helper changelog       When in a proposal branch generates the changelog using changelog-maker
EOF
}

case $1 in
  help|--help|-h)
    helpmsg
    ;;
  changelog)
    changelog-maker --start-ref=$1 --group --filter-release --markdown
    ;;
  notable)
    branch-diff upstream/$CURRENT.x $(git cb) --require-label=notable-change --plaintext
    ;;
  notable-md)
    branch-diff upstream/$CURRENT.x $(git cb) --require-label=notable-change --markdown
    ;;
  cherry-pick)
    branch-diff $CURRENT.x-staging upstream/main --exclude-label=semver-major,dont-land-on-$CURRENT.x,backport-requested-$CURRENT.x,backported-to-$CURRENT.x,backport-blocked-$CURRENT.x,backport-open-$CURRENT.x --filter-release --format=sha --reverse --cache | head | xargs git cherry-pick 2>&1 | node ./tools/release/watch-cherry-pick.js
    ;;
  start)
    startcherrypick
    ;;
  end)
    endcherrypick; git cherry-pick --quit
    ;;
# prepare will run branch-diff only once and store the retrieved metadata in
# a `.commit_list` file, which is a pratical way to avoid hitting GitHub API
# rate limits.
  prepare)
    branch-diff $CURRENT.x-staging upstream/main --exclude-label=semver-major,dont-land-on-$CURRENT.x,backport-requested-$CURRENT.x,backported-to-$CURRENT.x,backport-blocked-$CURRENT.x,backport-open-$CURRENT.x --filter-release --format=sha --reverse > .commit_list
    ;;
  backport)
    node ./tools/release/backport.js
    ;;
  continue)
    git -c core.editor=true cherry-pick --continue 2>&1 | node ./tools/release/watch-cherry-pick.js
    ;;
  skip)
    git cherry-pick --skip 2>&1 | node ./tools/release/watch-cherry-pick.js
    ;;
  * )
    helpmsg
    ;;
esac
