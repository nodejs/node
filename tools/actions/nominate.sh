#!/bin/sh

set -ex

ORG=nodejs
REPO=collaborators

prepare_collaborator_nomination() {
  HANDLE="$1"

  [ -n "$HANDLE" ] || {
    echo "Missing handle" >&2
    return 1
  }

  BODY_FILE=$(mktemp)
  cat -> "$BODY_FILE" <<EOF
<!-- Consider writing a small intro on the contributor and their contributions -->

* Commits in the nodejs/node repository: https://github.com/nodejs/node/commits?author=$HANDLE
* Pull requests and issues opened in the nodejs/node repository: https://github.com/nodejs/node/issues?q=author:$HANDLE
* Comments on pull requests and issues in the nodejs/node repository: https://github.com/nodejs/node/issues?q=commenter:$HANDLE
* Reviews on pull requests in the nodejs/node repository: https://github.com/nodejs/node/pulls?q=reviewed-by:$HANDLE
* Pull requests and issues opened throughout the Node.js organization: https://github.com/search?q=author:$HANDLE+org:$ORG
* Comments on pull requests and issues throughout the Node.js organization: https://github.com/search?q=commenter:$HANDLE+org:$ORG

<!-- You can add more item to that lists, such as: -->
<!--
* Help provided to end-users and novice contributors
* Participation in other projects, teams, and working groups of the Node.js
  organization
* Other participation in the wider Node.js community
-->
EOF
  ${EDITOR:-nano} "$BODY_FILE"
  BODY="$(cat "$BODY_FILE")"
  rm "$BODY_FILE"

  [ -n "$BODY" ] || {
    echo "Empty body" >&2
    return 1
  }

  read -p 'Open the discussion using `gh`? (y/N)' -n 1 -r
  echo
  if ! ([ "$REPLY" = "y" ] || [ "$REPLY" = "Y" ]); then
    set +x
    echo "Here's the body that you set:" >&2
    echo "$BODY"
    echo "Open https://github.com/nodejs/collaborators/discussions/new/choose in your browser to create the discussion manually" >&2
    return 0
  fi

  echo "Getting repo ID and discussion category" >&2
  # shellcheck disable=SC2016
  REPO_ID_AND_DISCUSSION_CATEGORY_ID="$(gh api graphql -f query='
  query($owner:String!,$repo:String!){
    repository(owner:$owner,name:$repo){
      id
      discussionCategories(first:100){
        nodes{ id name }
      }
    }
  }' -F owner="$ORG" -F repo="$REPO" --jq '[
    .data.repository.id,
    (.data.repository.discussionCategories.nodes[] | select(.name=="Collaborator nominations") | .id)
  ] | @tsv')"
  REPO_ID=$(echo "$REPO_ID_AND_DISCUSSION_CATEGORY_ID" | cut -f1)
  [ -n "$REPO_ID" ] || {
    echo "Cannot find repo ID" >&2
    return 1
  }
  CATEGORY_ID=$(echo "$REPO_ID_AND_DISCUSSION_CATEGORY_ID" | cut -f2)
  [ -n "$CATEGORY_ID" ] || {
    echo "Missing discussion category ID" >&2
    return 1
  }

  # shellcheck disable=SC2016
  gh api graphql -f query='
    mutation($repo: ID!,$cat: ID!,$title: String!,$body: String!){
        createDiscussion(input: {
        repositoryId: $repo
        categoryId: $cat
        title: $title
        body: $body
        }){ discussion { url } }
    }' \
  -F repo="$REPO_ID" -F cat="$CATEGORY_ID" -F title="Nominating ${HANDLE}?" -F body="$BODY"
}

prepare_collaborator_nomination "$1"
