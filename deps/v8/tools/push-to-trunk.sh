#!/bin/bash
# Copyright 2011 the V8 project authors. All rights reserved.
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
TEMP_BRANCH=v8-push-to-trunk-script-temporary-branch
VERSION_FILE="src/version.cc"
PERSISTFILE_BASENAME=/tmp/v8-push-to-trunk-tempfile
CHANGELOG_ENTRY_FILE="$PERSISTFILE_BASENAME-changelog-entry"
PATCH_FILE="$PERSISTFILE_BASENAME-patch"
COMMITMSG_FILE="$PERSISTFILE_BASENAME-commitmsg"
TOUCHED_FILES_FILE="$PERSISTFILE_BASENAME-touched-files"
STEP=0


########## Function definitions

usage() {
cat << EOF
usage: $0 OPTIONS

Performs the necessary steps for a V8 push to trunk. Only works for \
git checkouts.

OPTIONS:
  -h    Show this message
  -s    Specify the step where to start work. Default: 0.
  -l    Manually specify the git commit ID of the last push to trunk.
EOF
}

die() {
  [[ -n "$1" ]] && echo "Error: $1"
  echo "Exiting."
  exit 1
}

confirm() {
  echo -n "$1 [Y/n] "
  read ANSWER
  if [[ -z "$ANSWER" || "$ANSWER" == "Y" || "$ANSWER" == "y" ]] ; then
    return 0
  else
    return 1
  fi
}

delete_branch() {
  local MATCH=$(git branch | grep $1 | awk '{print $NF}' )
  if [ "$MATCH" == "$1" ] ; then
    confirm "Branch $1 exists, do you want to delete it?"
    if [ $? -eq 0 ] ; then
      git branch -D $1 || die "Deleting branch '$1' failed."
      echo "Branch $1 deleted."
    else
      die "Can't continue. Please delete branch $1 and try again."
    fi
  fi
}

# Persist and restore variables to support canceling/resuming execution
# of this script.
persist() {
  local VARNAME=$1
  local FILE="$PERSISTFILE_BASENAME-$VARNAME"
  echo "${!VARNAME}" > $FILE
}

restore() {
  local VARNAME=$1
  local FILE="$PERSISTFILE_BASENAME-$VARNAME"
  local VALUE="$(cat $FILE)"
  eval "$VARNAME=\"$VALUE\""
}

restore_if_unset() {
  local VARNAME=$1
  [[ -z "${!VARNAME}" ]] && restore "$VARNAME"
  [[ -z "${!VARNAME}" ]] && die "Variable '$VARNAME' could not be restored."
}


########## Option parsing

while getopts ":hs:l:" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    s)  STEP=$OPTARG
        ;;
    l)  LASTPUSH=$OPTARG
        ;;
    ?)  echo "Illegal option: -$OPTARG"
        usage
        exit 1
        ;;
  esac
done


########## Regular workflow

# Cancel if this is not a git checkout.
[[ -d .git ]] \
  || die "This is not a git checkout, this script won't work for you."

# Cancel if EDITOR is unset or not executable.
[[ -n "$EDITOR" && -x "$(which $EDITOR)" ]] \
  || die "Please set your EDITOR environment variable, you'll need it."

if [ $STEP -le 0 ] ; then
  echo ">>> Step 0: Preparation"
  # Check for a clean workdir.
  [[ -z "$(git status -s -uno)" ]] \
    || die "Workspace is not clean. Please commit or undo your changes."

  # Persist current branch.
  CURRENT_BRANCH=$(git status -s -b -uno | grep "^##" | awk '{print $2}')
  persist "CURRENT_BRANCH"
  # Get ahold of a safe temporary branch and check it out.
  if [ "$CURRENT_BRANCH" != "$TEMP_BRANCH" ] ; then
    delete_branch $TEMP_BRANCH
    git checkout -b $TEMP_BRANCH
  fi
  # Delete branches if they exist.
  delete_branch $BRANCHNAME
  delete_branch $TRUNKBRANCH
fi

if [ $STEP -le 1 ] ; then
  echo ">>> Step 1: Fetch unfetched revisions."
  git svn fetch || die "'git svn fetch' failed."
fi

if [ $STEP -le 2 ] ; then
  echo ">>> Step 2: Create a fresh branch."
  git checkout -b $BRANCHNAME svn/bleeding_edge \
    || die "Creating branch $BRANCHNAME failed."
fi

if [ $STEP -le 3 ] ; then
  echo ">>> Step 3: Detect commit ID of last push to trunk."
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

if [ $STEP -le 4 ] ; then
  echo ">>> Step 4: Prepare raw ChangeLog entry."
# These version numbers are used again later for the trunk commit.
  MAJOR=$(grep "#define MAJOR_VERSION" "$VERSION_FILE" | awk '{print $NF}')
  persist "MAJOR"
  MINOR=$(grep "#define MINOR_VERSION" "$VERSION_FILE" | awk '{print $NF}')
  persist "MINOR"
  BUILD=$(grep "#define BUILD_NUMBER" "$VERSION_FILE" | awk '{print $NF}')
  persist "BUILD"

  DATE=$(date +%Y-%m-%d)
  persist "DATE"
  echo "$DATE: Version $MAJOR.$MINOR.$BUILD" > "$CHANGELOG_ENTRY_FILE"
  echo "" >> "$CHANGELOG_ENTRY_FILE"
  COMMITS=$(git log $LASTPUSH..HEAD --format=%H)
  for commit in $COMMITS ; do
    # Get the commit's title line.
    git log -1 $commit --format="%w(80,8,8)%s" >> "$CHANGELOG_ENTRY_FILE"
    # Grep for "BUG=xxxx" lines in the commit message.
    git log -1 $commit --format="%b" | grep BUG= | grep -v "BUG=$" \
                                     | sed -e 's/^/        /' \
                                     >> "$CHANGELOG_ENTRY_FILE"
    # Append the commit's author for reference.
    git log -1 $commit --format="%w(80,8,8)(%an)" >> "$CHANGELOG_ENTRY_FILE"
    echo "" >> "$CHANGELOG_ENTRY_FILE"
  done
fi

if [ $STEP -le 5 ] ; then
  echo ">>> Step 5: Edit ChangeLog entry."
  echo -n "Please press <Return> to have your EDITOR open the ChangeLog entry, \
then edit its contents to your liking. When you're done, save the file and \
exit your EDITOR. "
  read ANSWER
  $EDITOR "$CHANGELOG_ENTRY_FILE"
  NEWCHANGELOG=$(mktemp)
  # Eliminate any trailing newlines by going through a shell variable.
  CHANGELOGENTRY=$(cat "$CHANGELOG_ENTRY_FILE")
  [[ -n "$CHANGELOGENTRY" ]] || die "Empty ChangeLog entry."
  echo "$CHANGELOGENTRY" > "$NEWCHANGELOG"
  echo "" >> "$NEWCHANGELOG" # Explicitly insert two empty lines.
  echo "" >> "$NEWCHANGELOG"
  cat ChangeLog >> "$NEWCHANGELOG"
  mv "$NEWCHANGELOG" ChangeLog
fi

if [ $STEP -le 6 ] ; then
  echo ">>> Step 6: Increment version number."
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
  NEWMAJOR=$(grep "#define MAJOR_VERSION" "$VERSION_FILE" | awk '{print $NF}')
  persist "NEWMAJOR"
  NEWMINOR=$(grep "#define MINOR_VERSION" "$VERSION_FILE" | awk '{print $NF}')
  persist "NEWMINOR"
  NEWBUILD=$(grep "#define BUILD_NUMBER" "$VERSION_FILE" | awk '{print $NF}')
  persist "NEWBUILD"
fi

if [ $STEP -le 7 ] ; then
  echo ">>> Step 7: Commit to local branch."
  restore_if_unset "NEWMAJOR"
  restore_if_unset "NEWMINOR"
  restore_if_unset "NEWBUILD"
  git commit -a -m "Prepare push to trunk.  \
Now working on version $NEWMAJOR.$NEWMINOR.$NEWBUILD." \
    || die "'git commit -a' failed."
fi

if [ $STEP -le 8 ] ; then
  echo ">>> Step 8: Upload for code review."
  echo -n "Please enter the email address of a V8 reviewer for your patch: "
  read REVIEWER
  git cl upload -r $REVIEWER --send-mail \
    || die "'git cl upload' failed, please try again."
fi

if [ $STEP -le 9 ] ; then
  echo ">>> Step 9: Commit to the repository."
  echo "Please wait for an LGTM, then type \"LGTM<Return>\" to commit your \
change. (If you need to iterate on the patch, do so in another shell.)"
  unset ANSWER
  while [ "$ANSWER" != "LGTM" ] ; do
    [[ -n "$ANSWER" ]] && echo "That was not 'LGTM'."
    echo -n "> "
    read ANSWER
  done
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

if [ $STEP -le 10 ] ; then
  echo ">>> Step 10: NOP"
  # Present in the manual guide, not necessary (even harmful!) for this script.
fi

if [ $STEP -le 11 ] ; then
  echo ">>> Step 11: Squash commits into one."
  # Instead of relying on "git rebase -i", we'll just create a diff, because
  # that's easier to automate.
  git diff svn/trunk > "$PATCH_FILE"
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
  LOOP=1
  while [ $LOOP -eq 1 ] ; do
    echo "This is the trunk commit message:"
    echo "--------------------"
    cat "$COMMITMSG_FILE"
    echo -e "\n--------------------"
    confirm "Does this look good to you? (Saying 'n' will fire up your \
EDITOR so you can change the commit message. When you're done, save the \
file and exit your EDITOR.)"
    if [ $? -eq 0 ] ; then
      LOOP=0
    else
      $EDITOR "$COMMITMSG_FILE"
    fi
  done
  rm -f "$CHANGELOG_ENTRY_FILE"
fi

if [ $STEP -le 12 ] ; then
  echo ">>> Step 12: Create a new branch from trunk."
  git checkout -b $TRUNKBRANCH svn/trunk \
    || die "Checking out a new branch '$TRUNKBRANCH' failed."
fi

if [ $STEP -le 13 ] ; then
  echo ">>> Step 13: Apply squashed changes."
  patch -p1 < "$PATCH_FILE" | tee >(awk '{print $NF}' >> "$TOUCHED_FILES_FILE")
  [[ $? -eq 0 ]] || die "Applying the patch to trunk failed."
  # Stage added and modified files.
  TOUCHED_FILES=$(cat "$TOUCHED_FILES_FILE")
  for FILE in $TOUCHED_FILES ; do
    git add "$FILE"
  done
  # Stage deleted files.
  DELETED_FILES=$(git status -s -uno --porcelain | grep "^ D" \
                                                 | awk '{print $NF}')
  for FILE in $DELETED_FILES ; do
    git rm "$FILE"
  done
  rm -f "$PATCH_FILE"
  rm -f "$TOUCHED_FILES_FILE"
fi

if [ $STEP -le 14 ] ; then
  echo ">>> Step 14: Set correct version for trunk."
  restore_if_unset "MAJOR"
  restore_if_unset "MINOR"
  restore_if_unset "BUILD"
  sed -e "/#define MAJOR_VERSION/s/[0-9]*$/$MAJOR/" \
      -e "/#define MINOR_VERSION/s/[0-9]*$/$MINOR/" \
      -e "/#define BUILD_NUMBER/s/[0-9]*$/$BUILD/" \
      -e "/#define PATCH_LEVEL/s/[0-9]*$/0/" \
      -e "/#define IS_CANDIDATE_VERSION/s/[0-9]*$/0/" \
      -i "$VERSION_FILE" || die "Patching $VERSION_FILE failed."
fi

if [ $STEP -le 15 ] ; then
  echo ">>> Step 15: Commit to local trunk branch."
  git add "$VERSION_FILE"
  git commit -F "$COMMITMSG_FILE" || die "'git commit' failed."
  rm -f "$COMMITMSG_FILE"
fi

if [ $STEP -le 16 ] ; then
  echo ">>> Step 16: Sanity check."
  confirm "Please check if your local checkout is sane: Inspect $VERSION_FILE, \
compile, run tests. Do you want to commit this new trunk revision to the \
repository?"
  [[ $? -eq 0 ]] || die "Execution canceled."
fi

if [ $STEP -le 17 ] ; then
  echo ">>> Step 17. Commit to SVN."
  git svn dcommit || die "'git svn dcommit' failed."
fi

if [ $STEP -le 18 ] ; then
  echo ">>> Step 18: Tag the new revision."
  restore_if_unset "MAJOR"
  restore_if_unset "MINOR"
  restore_if_unset "BUILD"
  git svn tag $MAJOR.$MINOR.$BUILD -m "Tagging version $MAJOR.$MINOR.$BUILD" \
    || die "'git svn tag' failed."
fi

if [ $STEP -le 19 ] ; then
  echo ">>> Step 19: Cleanup."
  restore_if_unset "CURRENT_BRANCH"
  git checkout -f $CURRENT_BRANCH
  [[ "$TEMP_BRANCH" != "$CURRENT_BRANCH" ]] && git branch -D $TEMP_BRANCH
  [[ "$BRANCHNAME" != "$CURRENT_BRANCH" ]] && git branch -D $BRANCHNAME
  [[ "$TRUNKBRANCH" != "$CURRENT_BRANCH" ]] && git branch -D $TRUNKBRANCH
fi

if [ $STEP -le 20 ] ; then
  echo ">>> Step 20: Done!"
  restore_if_unset "MAJOR"
  restore_if_unset "MINOR"
  restore_if_unset "BUILD"
  echo "Congratulations, you have successfully created the trunk revision \
$MAJOR.$MINOR.$BUILD. Please don't forget to update the v8rel spreadsheet, \
and to roll this new version into Chromium."
  # Clean up all temporary files.
  rm -f "$PERSISTFILE_BASENAME"*
fi
