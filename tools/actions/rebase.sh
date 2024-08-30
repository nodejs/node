#!/bin/sh

gh api graphql -F "prID=$(gh pr view "$1" --json id --jq .id)" -f query='
mutation RebasePR($prID: ID!) {
    updatePullRequestBranch(input:{pullRequestId:$prID,updateMethod:REBASE}) {
        clientMutationId
    }
}'
