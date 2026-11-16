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
SED_CMD="${SED_CMD};"'s#^\(Subject: \[PATCH [0-9]\+\)/[0-9]\+]#\1]#'

set -ex

REMOTE=${GIT_REMOTE:-https://github.com/${GH_REPO:-nodejs/node}.git}

case "$BACKPORT_PR" in
  (https://github.com/*/pull/[0-9]*) BACKPORT_PR_URL=$BACKPORT_PR ;;
  (*) BACKPORT_PR_URL=$(gh pr view "$BACKPORT_PR" --json url --jq .url) ;;
esac

WORKDIR=$(mktemp -d)
trap 'set -x; rm -rf "$WORKDIR"; set +x; trap - EXIT; exit' EXIT INT HUP
ORIGINAL=$WORKDIR/original.patch
BACKPORT=$WORKDIR/backport.patch
BUFFER=$WORKDIR/buffer.patch

curl -fsSLo "$BUFFER" "$BACKPORT_PR_URL.patch?full_index=1"
sed "$SED_CMD" "$BUFFER" > "$BACKPORT"

ORIGINAL_COMMITS=$(
  LAST_FETCHED=
  COMMIT_COUNT=$(git mailsplit -o"$WORKDIR" "$BUFFER")
  i=${COMMIT_COUNT:?No commits found in PR $BACKPORT_PR}
  while [ "$i" -gt 0 ]; do
    MAIL=$WORKDIR/$(printf '%04d' "$i")
    i=$((i - 1))

    read -r _ COMMIT_SHA _ < "$MAIL"
    PR_URL=$(git interpret-trailers --parse "$MAIL" | sed -n '/^PR-URL: /h;$ {x;s/^PR-URL: //p;}')
    [ -n "$PR_URL" ] || {
      echo "Missing PR-URL in commit message for $COMMIT_SHA" >&2
      continue
    }

    ORIGINAL_BASE=$(gh pr view "$PR_URL" --json baseRefName -q '.baseRefName')
    [ "$ORIGINAL_BASE" = "$LAST_FETCHED" ] || {
      git fetch "$REMOTE" "$ORIGINAL_BASE"
      LAST_FETCHED="$ORIGINAL_BASE"
    }
    COMMIT_TITLE=$(git mailinfo -b --no-scissors /dev/null /dev/null < "$MAIL" | sed -n 's/^Subject: //p')
    GIT_LOG_OUT=$(
      git log FETCH_HEAD --fixed-strings --grep="$COMMIT_TITLE" --format="%H%x09%s%x09%(trailers:key=PR-URL,only,valueonly)"
    )
    [ -n "$GIT_LOG_OUT" ] || {
        echo "Error: Could not locate landed commit with title '$COMMIT_TITLE' in $REMOTE/$ORIGINAL_BASE" >&2
        continue
    }
    T=$COMMIT_TITLE P=$PR_URL awk -F'\t' '$2 == ENVIRON["T"] && $3 == ENVIRON["P"] { print $1; exit }' <<EOF
$GIT_LOG_OUT
EOF
  done
)

xargs git format-patch --no-signature --stdout --root --no-walk=unsorted > "$BUFFER" <<EOF
${ORIGINAL_COMMITS:?Could not find any commit being backported}
EOF
sed "$SED_CMD" "$BUFFER" > "$ORIGINAL"
${DIFF_CMD:-diff} "$ORIGINAL" "$BACKPORT" || echo "diff command exited with $?" >&2

set +x

printf "Approve the PR using gh? [y/N] "
read -r r
[ "$r" != "y" ] || {
  set -x
  gh pr review "$BACKPORT_PR" --approve
}
