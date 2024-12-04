#!/bin/sh

set -xe

RELEASE_DATE=$1
RELEASE_LINE=$2
RELEASER=$3

if [ -z "$RELEASE_DATE" ] || [ -z "$RELEASE_LINE" ]; then
  echo "Usage: $0 <RELEASE_DATE> <RELEASE_LINE>"
  exit 1
fi

git node release --prepare --skipBranchDiff --yes --releaseDate "$RELEASE_DATE"
# We use it to not specify the branch name as it changes based on
# the commit list (semver-minor/semver-patch)
git config push.default current
git push

TITLE=$(awk "/^## ${RELEASE_DATE}/ { print substr(\$0, 4) }" "doc/changelogs/CHANGELOG_V${RELEASE_LINE}.md")

# Use a temporary file for the PR body
TEMP_BODY="$(awk "/## ${RELEASE_DATE}/,/^<a id=/{ if (!/^<a id=/) print }" "doc/changelogs/CHANGELOG_V${RELEASE_LINE}.md")"

PR_URL="$(gh pr create --draft --title "$TITLE" --body "$TEMP_BODY" --base "v$RELEASE_LINE.x" --assignee "$RELEASER" --label release)"

# Amend commit message so it contains the correct PR-URL trailer.
AMENDED_COMMIT_MSG="$(git log -1 --pretty=%B | sed "s|PR-URL: TODO|PR-URL: $PR_URL|")"

# Replace "TODO" with the PR URL in the last commit
git commit --amend --no-edit -m "$AMENDED_COMMIT_MSG" || true

# Force-push the amended commit
git push --force
