#!/bin/bash
# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


########## Global variable definitions

BRANCHNAME=prepare-push
TRUNKBRANCH=trunk-push
PERSISTFILE_BASENAME=/tmp/v8-push-to-trunk-tempfile
CHROME_PATH=

########## Function definitions

source $(dirname $BASH_SOURCE)/common-includes.sh

usage() {
cat << EOF
usage: $0 OPTIONS

Performs the necessary steps for a V8 push to trunk. Only works for \
git checkouts.

OPTIONS:
  -h    Show this message
  -s    Specify the step where to start work. Default: 0.
  -l    Manually specify the git commit ID of the last push to trunk.
  -c    Specify the path to your Chromium src/ directory to automate the
        V8 roll.
EOF
}

########## Option parsing

while getopts ":hs:l:c:" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    s)  START_STEP=$OPTARG
        ;;
    l)  LASTPUSH=$OPTARG
        ;;
    c)  CHROME_PATH=$OPTARG
        ;;
    ?)  echo "Illegal option: -$OPTARG"
        usage
        exit 1
        ;;
  esac
done


########## Regular workflow

initial_environment_checks

if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Preparation"
  common_prepare
  delete_branch $TRUNKBRANCH
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Create a fresh branch."
  git checkout -b $BRANCHNAME svn/bleeding_edge \
    || die "Creating branch $BRANCHNAME failed."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Detect commit ID of last push to trunk."
  [[ -n "$LASTPUSH" ]] || LASTPUSH=$(git log -1 --format=%H ChangeLog)
  LOOP=1
  while [ $LOOP -eq 1 ] ; do
    # Print assumed commit, circumventing git's pager.
    git log -1 $LASTPUSH | cat
    confirm "Is the commit printed above the last push to trunk?"
    if [ $? -eq 0 ] ; then
      LOOP=0
    else
      LASTPUSH=$(git log -1 --format=%H $LASTPUSH^ ChangeLog)
    fi
  done
  persist "LASTPUSH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Prepare raw ChangeLog entry."
  # These version numbers are used again later for the trunk commit.
  read_and_persist_version

  DATE=$(date +%Y-%m-%d)
  persist "DATE"
  echo "$DATE: Version $MAJOR.$MINOR.$BUILD" > "$CHANGELOG_ENTRY_FILE"
  echo "" >> "$CHANGELOG_ENTRY_FILE"
  COMMITS=$(git log $LASTPUSH..HEAD --format=%H)
  for commit in $COMMITS ; do
    # Get the commit's title line.
    git log -1 $commit --format="%w(80,8,8)%s" >> "$CHANGELOG_ENTRY_FILE"
    # Grep for "BUG=xxxx" lines in the commit message and convert them to
    # "(issue xxxx)".
    git log -1 $commit --format="%B" \
        | grep "^BUG=" | grep -v "BUG=$" | grep -v "BUG=none$" \
        | sed -e 's/^/        /' \
        | sed -e 's/BUG=v8:\(.*\)$/(issue \1)/' \
        | sed -e 's/BUG=\(.*\)$/(Chromium issue \1)/' \
        >> "$CHANGELOG_ENTRY_FILE"
    # Append the commit's author for reference.
    git log -1 $commit --format="%w(80,8,8)(%an)" >> "$CHANGELOG_ENTRY_FILE"
    echo "" >> "$CHANGELOG_ENTRY_FILE"
  done
  echo "        Performance and stability improvements on all platforms." \
    >> "$CHANGELOG_ENTRY_FILE"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Edit ChangeLog entry."
  echo -n "Please press <Return> to have your EDITOR open the ChangeLog entry, \
then edit its contents to your liking. When you're done, save the file and \
exit your EDITOR. "
  read ANSWER
  $EDITOR "$CHANGELOG_ENTRY_FILE"
  NEWCHANGELOG=$(mktemp)
  # Eliminate any trailing newlines by going through a shell variable.
  # Also (1) eliminate tabs, (2) fix too little and (3) too much indentation,
  # and (4) eliminate trailing whitespace.
  CHANGELOGENTRY=$(cat "$CHANGELOG_ENTRY_FILE" \
                   | sed -e 's/\t/        /g' \
                   | sed -e 's/^ \{1,7\}\([^ ]\)/        \1/g' \
                   | sed -e 's/^ \{9,80\}\([^ ]\)/        \1/g' \
                   | sed -e 's/ \+$//')
  [[ -n "$CHANGELOGENTRY" ]] || die "Empty ChangeLog entry."
  echo "$CHANGELOGENTRY" > "$NEWCHANGELOG"
  echo "" >> "$NEWCHANGELOG" # Explicitly insert two empty lines.
  echo "" >> "$NEWCHANGELOG"
  cat ChangeLog >> "$NEWCHANGELOG"
  mv "$NEWCHANGELOG" ChangeLog
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Increment version number."
  restore_if_unset "BUILD"
  NEWBUILD=$(($BUILD + 1))
  confirm "Automatically increment BUILD_NUMBER? (Saying 'n' will fire up \
your EDITOR on $VERSION_FILE so you can make arbitrary changes. When \
you're done, save the file and exit your EDITOR.)"
  if [ $? -eq 0 ] ; then
    sed -e "/#define BUILD_NUMBER/s/[0-9]*$/$NEWBUILD/" \
        -i "$VERSION_FILE"
  else
    $EDITOR "$VERSION_FILE"
  fi
  read_and_persist_version "NEW"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to local branch."
  restore_version_if_unset "NEW"
  PREPARE_COMMIT_MSG="Prepare push to trunk.  \
Now working on version $NEWMAJOR.$NEWMINOR.$NEWBUILD."
  persist "PREPARE_COMMIT_MSG"
  git commit -a -m "$PREPARE_COMMIT_MSG" \
    || die "'git commit -a' failed."
fi

upload_step

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to the repository."
  wait_for_lgtm
  # Re-read the ChangeLog entry (to pick up possible changes).
  cat ChangeLog | awk --posix '{
    if ($0 ~ /^[0-9]{4}-[0-9]{2}-[0-9]{2}:/) {
      if (in_firstblock == 1) {
        exit 0;
      } else {
        in_firstblock = 1;
      }
    };
    print $0;
  }' > "$CHANGELOG_ENTRY_FILE"
  git cl dcommit || die "'git cl dcommit' failed, please try again."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Fetch straggler commits that sneaked in \
since this script was started."
  git svn fetch || die "'git svn fetch' failed."
  git checkout svn/bleeding_edge
  restore_if_unset "PREPARE_COMMIT_MSG"
  PREPARE_COMMIT_HASH=$(git log -1 --format=%H --grep="$PREPARE_COMMIT_MSG")
  persist "PREPARE_COMMIT_HASH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Squash commits into one."
  # Instead of relying on "git rebase -i", we'll just create a diff, because
  # that's easier to automate.
  restore_if_unset "PREPARE_COMMIT_HASH"
  git diff svn/trunk $PREPARE_COMMIT_HASH > "$PATCH_FILE"
  # Convert the ChangeLog entry to commit message format:
  # - remove date
  # - remove indentation
  # - merge paragraphs into single long lines, keeping empty lines between them.
  restore_if_unset "DATE"
  CHANGELOGENTRY=$(cat "$CHANGELOG_ENTRY_FILE")
  echo "$CHANGELOGENTRY" \
    | sed -e "s/^$DATE: //" \
    | sed -e 's/^ *//' \
    | awk '{
        if (need_space == 1) {
          printf(" ");
        };
        printf("%s", $0);
        if ($0 ~ /^$/) {
          printf("\n\n");
          need_space = 0;
        } else {
          need_space = 1;
        }
      }' > "$COMMITMSG_FILE" || die "Commit message editing failed."
  rm -f "$CHANGELOG_ENTRY_FILE"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Create a new branch from trunk."
  git checkout -b $TRUNKBRANCH svn/trunk \
    || die "Checking out a new branch '$TRUNKBRANCH' failed."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Apply squashed changes."
  rm -f "$TOUCHED_FILES_FILE"
  apply_patch "$PATCH_FILE"
  stage_files
  rm -f "$PATCH_FILE"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Set correct version for trunk."
  restore_version_if_unset
  sed -e "/#define MAJOR_VERSION/s/[0-9]*$/$MAJOR/" \
      -e "/#define MINOR_VERSION/s/[0-9]*$/$MINOR/" \
      -e "/#define BUILD_NUMBER/s/[0-9]*$/$BUILD/" \
      -e "/#define PATCH_LEVEL/s/[0-9]*$/0/" \
      -e "/#define IS_CANDIDATE_VERSION/s/[0-9]*$/0/" \
      -i "$VERSION_FILE" || die "Patching $VERSION_FILE failed."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to local trunk branch."
  git add "$VERSION_FILE"
  git commit -F "$COMMITMSG_FILE" || die "'git commit' failed."
  rm -f "$COMMITMSG_FILE"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Sanity check."
  confirm "Please check if your local checkout is sane: Inspect $VERSION_FILE, \
compile, run tests. Do you want to commit this new trunk revision to the \
repository?"
  [[ $? -eq 0 ]] || die "Execution canceled."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to SVN."
  git svn dcommit | tee >(grep -E "^Committed r[0-9]+" \
                          | sed -e 's/^Committed r\([0-9]\+\)/\1/' \
                          > "$TRUNK_REVISION_FILE") \
    || die "'git svn dcommit' failed."
  TRUNK_REVISION=$(cat "$TRUNK_REVISION_FILE")
  persist "TRUNK_REVISION"
  rm -f "$TRUNK_REVISION_FILE"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Tag the new revision."
  restore_version_if_unset
  git svn tag $MAJOR.$MINOR.$BUILD -m "Tagging version $MAJOR.$MINOR.$BUILD" \
    || die "'git svn tag' failed."
fi

if [ -n "$CHROME_PATH" ] ; then

  let CURRENT_STEP+=1
  if [ $START_STEP -le $CURRENT_STEP ] ; then
    echo ">>> Step $CURRENT_STEP: Switch to Chromium checkout."
    V8_PATH=$(pwd)
    persist "V8_PATH"
    cd "$CHROME_PATH"
    initial_environment_checks
    # Check for a clean workdir.
    [[ -z "$(git status -s -uno)" ]] \
      || die "Workspace is not clean. Please commit or undo your changes."
    # Assert that the DEPS file is there.
    [[ -w "DEPS" ]] || die "DEPS file not present or not writable; \
current directory is: $(pwd)."
  fi

  let CURRENT_STEP+=1
  if [ $START_STEP -le $CURRENT_STEP ] ; then
    echo ">>> Step $CURRENT_STEP: Update the checkout and create a new branch."
    git checkout master || die "'git checkout master' failed."
    git pull || die "'git pull' failed, please try again."
    restore_if_unset "TRUNK_REVISION"
    git checkout -b "v8-roll-$TRUNK_REVISION" \
      || die "Failed to checkout a new branch."
  fi

  let CURRENT_STEP+=1
  if [ $START_STEP -le $CURRENT_STEP ] ; then
    echo ">>> Step $CURRENT_STEP: Create and upload CL."
    # Patch DEPS file.
    sed -r -e "/\"v8_revision\": /s/\"[0-9]+\"/\"$TRUNK_REVISION\"/" \
        -i DEPS
    restore_version_if_unset
    echo -n "Please enter the email address of a reviewer for the roll CL: "
    read REVIEWER
    git commit -am "Update V8 to version $MAJOR.$MINOR.$BUILD.

TBR=$REVIEWER" || die "'git commit' failed."
    git cl upload --send-mail \
      || die "'git cl upload' failed, please try again."
    echo "CL uploaded."
  fi

  let CURRENT_STEP+=1
  if [ $START_STEP -le $CURRENT_STEP ] ; then
    echo ">>> Step $CURRENT_STEP: Returning to V8 checkout."
    restore_if_unset "V8_PATH"
    cd "$V8_PATH"
  fi
fi  # if [ -n "$CHROME_PATH" ]

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Done!"
  restore_version_if_unset
  restore_if_unset "TRUNK_REVISION"
  if [ -n "$CHROME_PATH" ] ; then
    echo "Congratulations, you have successfully created the trunk revision \
$MAJOR.$MINOR.$BUILD and rolled it into Chromium. Please don't forget to \
update the v8rel spreadsheet:"
  else
    echo "Congratulations, you have successfully created the trunk revision \
$MAJOR.$MINOR.$BUILD. Please don't forget to roll this new version into \
Chromium, and to update the v8rel spreadsheet:"
  fi
  echo -e "$MAJOR.$MINOR.$BUILD\ttrunk\t$TRUNK_REVISION"
  common_cleanup
  [[ "$TRUNKBRANCH" != "$CURRENT_BRANCH" ]] && git branch -D $TRUNKBRANCH
fi
