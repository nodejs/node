#!/bin/sh

set -xe

RELEASE_DATE=$1
RELEASE_LINE=$2

if [ -z "$RELEASE_DATE" ] || [ -z "$RELEASE_LINE" ]; then
  echo "Usage: $0 <RELEASE_DATE> <RELEASE_LINE>"
  exit 1
fi

createCommitAPICall() {
  commit="${1:-HEAD}"
  cat - <<'EOF'
mutation ($repo: String! $branch: String!, $parent: GitObjectID!, $commit_title: String!, $commit_body: String) {
  createCommitOnBranch(input: {
    branch: {
      repositoryNameWithOwner: $repo,
      branchName: $branch
    },
    message: {
      headline: $commit_title,
      body: $commit_body
    },
    expectedHeadOid: $parent,
    fileChanges: {
      additions: [
EOF
  git show "$commit" --diff-filter=d --name-only --format= | while read -r FILE; do
    printf "          { path: "
    node -p 'JSON.stringify(process.argv[1])' "$FILE"
    printf "          , contents: \""
    base64 -w 0 -i "$FILE"
    echo "\"},"
  done
  echo '      ], deletions: ['
  git show "$commit" --diff-filter=D --name-only --format= | while read -r FILE; do
    echo "        $(node -p 'JSON.stringify(process.argv[1])' "$FILE"),"
  done
  cat - <<'EOF'
      ]
    }
  }) {
    commit {
      url
    }
  }
}
EOF
}

git node release --prepare --skipBranchDiff --yes --releaseDate "$RELEASE_DATE"

HEAD_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
HEAD_SHA="$(git rev-parse HEAD^)"

TITLE=$(awk "/^## ${RELEASE_DATE}/ { print substr(\$0, 4) }" "doc/changelogs/CHANGELOG_V${RELEASE_LINE}.md")

# Use a temporary file for the PR body
TEMP_BODY="$(awk "/## ${RELEASE_DATE}/,/^<a id=/{ if (!/^<a id=/) print }" "doc/changelogs/CHANGELOG_V${RELEASE_LINE}.md")"

# Create the proposal branch
gh api \
  --method POST \
  -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  "/repos/${GITHUB_REPOSITORY}/git/refs" \
   -f "ref=refs/heads/$HEAD_BRANCH" -f "sha=$HEAD_SHA"

# Create the proposal PR
PR_URL="$(gh api \
  --method POST \
  --jq .html_url \
  -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  "/repos/${GITHUB_REPOSITORY}/pulls" \
   -f "title=$TITLE" -f "body=$TEMP_BODY" -f "head=$HEAD_BRANCH" -f "base=v$RELEASE_LINE.x")"

# Push the release commit to the proposal branch
createCommitAPICall | node --input-type=module -e 'console.log(JSON.stringify({
  query: Buffer.concat(await process.stdin.toArray()).toString(),
  variables: {
    repo: process.argv[1],
    branch: process.argv[2],
    parent: process.argv[3],
    commit_title: process.argv[4],
    commit_body: process.argv[5]
  }
}))' \
    "$GITHUB_REPOSITORY" \
    "$HEAD_BRANCH" \
    "$HEAD_SHA" \
    "$(git log -1 HEAD --format=%s)" \
    "$(git log -1 HEAD --format=%b | sed "s|PR-URL: TODO|PR-URL: $PR_URL|")" \
| curl -fS -H "Authorization: bearer ${BOT_TOKEN}" -X POST --data @- https://api.github.com/graphql
