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

BRANCHNAME=prepare-merge
VERSION_FILE="src/version.cc"
PERSISTFILE_BASENAME=/tmp/v8-merge-to-branch-tempfile
ALREADY_MERGING_SENTINEL_FILE="$PERSISTFILE_BASENAME-already-merging"
CHANGELOG_ENTRY_FILE="$PERSISTFILE_BASENAME-changelog-entry"
PATCH_FILE="$PERSISTFILE_BASENAME-patch"
COMMITMSG_FILE="$PERSISTFILE_BASENAME-commitmsg"
COMMITMSG_FILE_COPY="$PERSISTFILE_BASENAME-commitmsg-copy"
TOUCHED_FILES_FILE="$PERSISTFILE_BASENAME-touched-files"
TRUNK_REVISION_FILE="$PERSISTFILE_BASENAME-trunkrevision"
START_STEP=0
CURRENT_STEP=0

usage() {
cat << EOF
usage: $0 [OPTIONS]... [BRANCH] [REVISION]...

Performs the necessary steps to merge revisions from bleeding_edge
to other branches, including trunk.

OPTIONS:
  -h    Show this message
  -s    Specify the step where to start work. Default: 0.
EOF
}

########## Function definitions

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

persist_patch_commit_hashes() {
  local FILE="$PERSISTFILE_BASENAME-PATCH_COMMIT_HASHES"
  echo "PATCH_COMMIT_HASHES=( ${PATCH_COMMIT_HASHES[@]} )" > $FILE
}

restore_patch_commit_hashes() {
  local FILE="$PERSISTFILE_BASENAME-PATCH_COMMIT_HASHES"
  source $FILE
}

restore_patch_commit_hashes_if_unset() {
  [[ "${#PATCH_COMMIT_HASHES[@]}" == 0 ]] && restore_patch_commit_hashes
  [[ "${#PATCH_COMMIT_HASHES[@]}" == 0 ]] && \
      die "Variable PATCH_COMMIT_HASHES could not be restored."
}

########## Option parsing

while getopts ":hs:f" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    f)  rm -f "$ALREADY_MERGING_SENTINEL_FILE"
        ;;
    s)  START_STEP=$OPTARG
        ;;
    ?)  echo "Illegal option: -$OPTARG"
        usage
        exit 1
        ;;
  esac
done
let OPTION_COUNT=$OPTIND-1
shift $OPTION_COUNT

########## Regular workflow

# If there is a merge in progress, abort.
[[ -e "$ALREADY_MERGING_SENTINEL_FILE" ]] && [[ -z "$START_STEP" ]] \
   && die "A merge is already in progress"
touch "$ALREADY_MERGING_SENTINEL_FILE"

# Cancel if this is not a git checkout.
[[ -d .git ]] \
  || die "This is not a git checkout, this script won't work for you."

# Cancel if EDITOR is unset or not executable.
[[ -n "$EDITOR" && -x "$(which $EDITOR)" ]] \
  || die "Please set your EDITOR environment variable, you'll need it."

if [ $START_STEP -le $CURRENT_STEP ] ; then
  MERGE_TO_BRANCH=$1
  [[ -n "$MERGE_TO_BRANCH" ]] \
    || die "Please specify a branch to merge to"
  shift
  persist "MERGE_TO_BRANCH"

  echo ">>> Step $CURRENT_STEP: Preparation"
  # Check for a clean workdir.
  [[ -z "$(git status -s -uno)" ]] \
    || die "Workspace is not clean. Please commit or undo your changes."

  # Persist current branch.
  CURRENT_BRANCH=$(git status -s -b -uno | grep "^##" | awk '{print $2}')
  persist "CURRENT_BRANCH"
  delete_branch $BRANCHNAME
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Fetch unfetched revisions."
  git svn fetch || die "'git svn fetch' failed."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  restore_if_unset "MERGE_TO_BRANCH"
  echo ">>> Step $CURRENT_STEP: Create a fresh branch for the patch."
  git checkout -b $BRANCHNAME svn/$MERGE_TO_BRANCH \
    || die "Creating branch $BRANCHNAME failed."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Find the git \
revisions associated with the patches."
  current=0
  for REVISION in "$@" ; do
    NEXT_HASH=$(git svn find-rev "r$REVISION" svn/bleeding_edge)
    [[ -n "$NEXT_HASH" ]] \
      || die "Cannot determine git hash for r$REVISION"
    PATCH_COMMIT_HASHES[$current]="$NEXT_HASH"
    [[ -n "$NEW_COMMIT_MSG" ]] && NEW_COMMIT_MSG="$NEW_COMMIT_MSG,"
    NEW_COMMIT_MSG="$NEW_COMMIT_MSG r$REVISION"
    let current+=1
  done
  NEW_COMMIT_MSG="Merged$NEW_COMMIT_MSG into $MERGE_TO_BRANCH branch."
  
  echo "$NEW_COMMIT_MSG" > $COMMITMSG_FILE
  echo >> $COMMITMSG_FILE
  for HASH in ${PATCH_COMMIT_HASHES[@]} ; do
    PATCH_MERGE_DESCRIPTION=$(git log -1 --format=%s $HASH)
    echo "$PATCH_MERGE_DESCRIPTION" >> $COMMITMSG_FILE
    echo >> $COMMITMSG_FILE
  done
  for HASH in ${PATCH_COMMIT_HASHES[@]} ; do
    BUG=$(git log -1 $HASH | grep "BUG=" | awk -F '=' '{print $NF}')
    if [ $BUG ] ; then
      if [ "$BUG_AGGREGATE" ] ; then
        BUG_AGGREGATE="$BUG_AGGREGATE,"
      fi
      BUG_AGGREGATE="$BUG_AGGREGATE$BUG"
    fi
  done
  if [ "$BUG_AGGREGATE" ] ; then
    echo "BUG=$BUG_AGGREGATE" >> $COMMITMSG_FILE
  fi
  persist "NEW_COMMIT_MSG"
  persist_patch_commit_hashes
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  restore_if_unset "MERGE_TO_BRANCH"
  restore_patch_commit_hashes_if_unset "PATCH_COMMIT_HASHES"
  echo "${PATCH_COMMIT_HASHES[@]}"
  echo ">>> Step $CURRENT_STEP: Apply patches for selected revisions."
  rm -f "$TOUCHED_FILES_FILE"
  for HASH in ${PATCH_COMMIT_HASHES[@]} ; do
    git log -1 -p $HASH | patch -p1 \
      | tee >(awk '{print $NF}' >> "$TOUCHED_FILES_FILE")
    [[ $? -eq 0 ]] \
      || die "Cannot apply the patch for $HASH to $MERGE_TO_BRANCH."
  done
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
  rm -f "$TOUCHED_FILES_FILE"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Prepare version.cc"
# These version numbers are used again for creating the tag
  PATCH=$(grep "#define PATCH_LEVEL" "$VERSION_FILE" | awk '{print $NF}')
  persist "PATCH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Increment version number."
  restore_if_unset "PATCH"
  NEWPATCH=$(($PATCH + 1))
  confirm "Automatically increment PATCH_LEVEL? (Saying 'n' will fire up \
your EDITOR on $VERSION_FILE so you can make arbitrary changes. When \
you're done, save the file and exit your EDITOR.)"
  if [ $? -eq 0 ] ; then
    sed -e "/#define PATCH_LEVEL/s/[0-9]*$/$NEWPATCH/" \
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
  NEWPATCH=$(grep "#define PATCH_LEVEL" "$VERSION_FILE" | awk '{print $NF}')
  persist "NEWPATCH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to local branch."
  git commit -a -F "$COMMITMSG_FILE" \
    || die "'git commit -a' failed."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Upload for code review."
  echo -n "Please enter the email address of a V8 reviewer for your patch: "
  read REVIEWER
  git cl upload -r "$REVIEWER" --send-mail \
    || die "'git cl upload' failed, please try again."
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  restore_if_unset "MERGE_TO_BRANCH"
  git checkout $BRANCHNAME \
    || die "cannot ensure that the current branch is $BRANCHNAME" 
  echo ">>> Step $CURRENT_STEP: Commit to the repository."
  echo "Please wait for an LGTM, then type \"LGTM<Return>\" to commit your \
change. (If you need to iterate on the patch or double check that it's \
sane, do so in another shell, but remember to not change the headline of \
the uploaded CL."
  unset ANSWER
  while [ "$ANSWER" != "LGTM" ] ; do
    [[ -n "$ANSWER" ]] && echo "That was not 'LGTM'."
    echo -n "> "
    read ANSWER
  done
  git cl dcommit || die "failed to commit to $MERGE_TO_BRANCH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  restore_if_unset "NEW_COMMIT_MSG"
  restore_if_unset "MERGE_TO_BRANCH"
  echo ">>> Step $CURRENT_STEP: Determine svn commit revision"
  git svn fetch || die "'git svn fetch' failed."
  COMMIT_HASH=$(git log -1 --format=%H --grep="$NEW_COMMIT_MSG" \
svn/$MERGE_TO_BRANCH)
  [[ -z "$COMMIT_HASH" ]] && die "Unable to map git commit to svn revision"
  SVN_REVISION=$(git svn find-rev $COMMIT_HASH)
  echo "subversion revision number is r$SVN_REVISION"
  persist "SVN_REVISION"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  restore_if_unset "SVN_REVISION"
  restore_if_unset "NEWMAJOR"
  restore_if_unset "NEWMINOR"
  restore_if_unset "NEWBUILD"
  restore_if_unset "NEWPATCH"
  echo ">>> Step $CURRENT_STEP: Create the tag."
  echo "Creating tag svn/tags/$NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH"
  svn copy -r $SVN_REVISION \
https://v8.googlecode.com/svn/branches/$MERGE_TO_BRANCH \
https://v8.googlecode.com/svn/tags/$NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH \
-m "Tagging version $NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Cleanup."
  restore_if_unset "CURRENT_BRANCH"
  git checkout -f $CURRENT_BRANCH
  [[ "$BRANCHNAME" != "$CURRENT_BRANCH" ]] && git branch -D $BRANCHNAME
  rm -f "$ALREADY_MERGING_SENTINEL_FILE"
fi
