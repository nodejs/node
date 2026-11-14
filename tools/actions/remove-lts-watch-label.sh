#!/bin/sh

set -ex

[ -n "$GITHUB_SERVER_URL" ] || {
  echo "Required environment variable GITHUB_SERVER_URL not found" >&2
  exit 1
}
[ -n "$GITHUB_REPOSITORY_OWNER" ] || {
  echo "Required environment variable GITHUB_REPOSITORY_OWNER not found" >&2
  exit 1
}
[ -n "$GITHUB_REPOSITORY" ] || {
  echo "Required environment variable GITHUB_REPOSITORY not found" >&2
  exit 1
}
[ -n "$GITHUB_REF_NAME" ] || {
  echo "Required environment variable GITHUB_REF_NAME not found" >&2
  exit 1
}
[ -n "$GITHUB_EVENT_PATH" ] || {
  echo "Required environment variable GITHUB_EVENT_PATH not found" >&2
  exit 1
}

owner=$GITHUB_REPOSITORY_OWNER
repo=$(echo "$GITHUB_REPOSITORY" | cut -d/ -f2)
label="lts-watch-$GITHUB_REF_NAME"

# Lazy-get label node_id once
label_id=
fetch_label_id () {
  # shellcheck disable=SC2016
  label_id="$(gh api graphql -f query='
    query($owner:String!, $repo:String!, $name:String!) {
      repository(owner:$owner, name:$repo) {
        label(name:$name) { id }
      }
    }' -f owner="$owner" -f repo="$repo" -f name="$label" --jq '.data.repository.label.id')"

  if [ -z "$label_id" ] || [ "$label_id" = "null" ]; then
    echo "Could not resolve label id for '$label' in $owner/$repo" >&2
    exit 1
  fi
}

jq --raw-output0 '.commits[].message' < "$GITHUB_EVENT_PATH" \
| while IFS= read -r -d '' msg; do
  pr_url=$(echo "$msg" | git interpret-trailers --parse | awk -F': ' '$1 == "PR-URL" { print $2; exit }')
  pr_number=${pr_url##"$GITHUB_SERVER_URL/$GITHUB_REPOSITORY/pull/"}

  # Filter out pr_number that are not numbers (e.g. if PR-URL points to a different repo)
  case "$pr_number" in
    ''|*[!0-9]*) continue ;;
    *) ;;
  esac

  # shellcheck disable=SC2016
  pr_id="$(LTS_WATCH_LABEL="$label" gh api graphql -f query='
    query($owner:String!, $repo:String!, $number:Int!) {
      repository(owner:$owner, name:$repo) {
        pullRequest(number:$number) {
          id
          labels(first: 100) { nodes { name } }
        }
      }
    }' -f owner="$owner" -f repo="$repo" -F number="$pr_number" \
    --jq 'if (.data.repository.pullRequest.labels.nodes | any(.name == env.LTS_WATCH_LABEL)) then .data.repository.pullRequest.id else null end')"

  # Skip PRs that do not have the label
  [ -n "$pr_id" ] || continue

  # Lazy-load the label ID if not done already
  [ -n "$label_id" ] || fetch_label_id

  # shellcheck disable=SC2016
  gh api graphql -f query='
    mutation($labelableId:ID!, $labelId:ID!) {
      removeLabelsFromLabelable(input:{labelableId:$labelableId, labelIds:[$labelId]}) {
        clientMutationId
      }
    }' -f labelableId="$pr_id" -f labelId="$label_id"

  # gentle throttle
  sleep 0.2
done
