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
PERSISTFILE_BASENAME=/tmp/v8-merge-to-branch-tempfile
ALREADY_MERGING_SENTINEL_FILE="$PERSISTFILE_BASENAME-already-merging"
COMMIT_HASHES_FILE="$PERSISTFILE_BASENAME-PATCH_COMMIT_HASHES"
TEMPORARY_PATCH_FILE="$PERSISTFILE_BASENAME-temporary-patch"

########## Function definitions

source $(dirname $BASH_SOURCE)/common-includes.sh

usage() {
cat << EOF
usage: $0 [OPTIONS]... [BRANCH] [REVISION]...

Performs the necessary steps to merge revisions from bleeding_edge
to other branches, including trunk.

OPTIONS:
  -h    Show this message
  -s    Specify the step where to start work. Default: 0.
  -p    Specify a patch file to apply as part of the merge
EOF
}

persist_patch_commit_hashes() {
  echo "PATCH_COMMIT_HASHES=( ${PATCH_COMMIT_HASHES[@]} )" > $COMMIT_HASHES_FILE
}

restore_patch_commit_hashes() {
  source $COMMIT_HASHES_FILE
}

restore_patch_commit_hashes_if_unset() {
  [[ "${#PATCH_COMMIT_HASHES[@]}" == 0 ]] && restore_patch_commit_hashes
  [[ "${#PATCH_COMMIT_HASHES[@]}" == 0 ]] && [[ -z "$EXTRA_PATCH" ]] && \
      die "Variable PATCH_COMMIT_HASHES could not be restored."
}

########## Option parsing

while getopts ":hs:fp:" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    p)  EXTRA_PATCH=$OPTARG
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
[[ -e "$ALREADY_MERGING_SENTINEL_FILE" ]] && [[ $START_STEP -eq 0 ]] \
   && die "A merge is already in progress"
touch "$ALREADY_MERGING_SENTINEL_FILE"

initial_environment_checks

if [ $START_STEP -le $CURRENT_STEP ] ; then
  if [ ${#@} -lt 2 ] && [ -z "$EXTRA_PATCH" ] ; then
    die "Either a patch file or revision numbers must be specified"
  fi
  echo ">>> Step $CURRENT_STEP: Preparation"
  MERGE_TO_BRANCH=$1
  [[ -n "$MERGE_TO_BRANCH" ]] || die "Please specify a branch to merge to"
  shift
  persist "MERGE_TO_BRANCH"
  common_prepare
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Create a fresh branch for the patch."
  restore_if_unset "MERGE_TO_BRANCH"
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
    [[ -n "$REVISION_LIST" ]] && REVISION_LIST="$REVISION_LIST,"
    REVISION_LIST="$REVISION_LIST r$REVISION"
    let current+=1
  done
  if [ -z "$REVISION_LIST" ] ; then
    NEW_COMMIT_MSG="Applied patch to $MERGE_TO_BRANCH branch."
  else
    NEW_COMMIT_MSG="Merged$REVISION_LIST into $MERGE_TO_BRANCH branch."
  fi;

  echo "$NEW_COMMIT_MSG" > $COMMITMSG_FILE
  echo "" >> $COMMITMSG_FILE
  for HASH in ${PATCH_COMMIT_HASHES[@]} ; do
    PATCH_MERGE_DESCRIPTION=$(git log -1 --format=%s $HASH)
    echo "$PATCH_MERGE_DESCRIPTION" >> $COMMITMSG_FILE
    echo "" >> $COMMITMSG_FILE
  done
  for HASH in ${PATCH_COMMIT_HASHES[@]} ; do
    BUG=$(git log -1 $HASH | grep "BUG=" | awk -F '=' '{print $NF}')
    if [ -n "$BUG" ] ; then
      [[ -n "$BUG_AGGREGATE" ]] && BUG_AGGREGATE="$BUG_AGGREGATE,"
      BUG_AGGREGATE="$BUG_AGGREGATE$BUG"
    fi
  done
  if [ -n "$BUG_AGGREGATE" ] ; then
    echo "BUG=$BUG_AGGREGATE" >> $COMMITMSG_FILE
  fi
  persist "NEW_COMMIT_MSG"
  persist "REVISION_LIST"
  persist_patch_commit_hashes
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Apply patches for selected revisions."
  restore_if_unset "MERGE_TO_BRANCH"
  restore_patch_commit_hashes_if_unset "PATCH_COMMIT_HASHES"
  rm -f "$TOUCHED_FILES_FILE"
  for HASH in ${PATCH_COMMIT_HASHES[@]} ; do
    echo "Applying patch for $HASH to $MERGE_TO_BRANCH..."
    git log -1 -p $HASH > "$TEMPORARY_PATCH_FILE"
    apply_patch "$TEMPORARY_PATCH_FILE"
  done
  if [ -n "$EXTRA_PATCH" ] ; then
    apply_patch "$EXTRA_PATCH"
  fi
  stage_files
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Prepare $VERSION_FILE."
  # These version numbers are used again for creating the tag
  read_and_persist_version
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
  read_and_persist_version "NEW"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to local branch."
  git commit -a -F "$COMMITMSG_FILE" \
    || die "'git commit -a' failed."
fi

upload_step

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Commit to the repository."
  restore_if_unset "MERGE_TO_BRANCH"
  git checkout $BRANCHNAME \
    || die "cannot ensure that the current branch is $BRANCHNAME"
  wait_for_lgtm
  git cl dcommit || die "failed to commit to $MERGE_TO_BRANCH"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Determine svn commit revision"
  restore_if_unset "NEW_COMMIT_MSG"
  restore_if_unset "MERGE_TO_BRANCH"
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
  echo ">>> Step $CURRENT_STEP: Create the tag."
  restore_if_unset "SVN_REVISION"
  restore_version_if_unset "NEW"
  echo "Creating tag svn/tags/$NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH"
  if [ "$MERGE_TO_BRANCH" == "trunk" ] ; then
    TO_URL="$MERGE_TO_BRANCH"
  else
    TO_URL="branches/$MERGE_TO_BRANCH"
  fi
  svn copy -r $SVN_REVISION \
    https://v8.googlecode.com/svn/$TO_URL \
    https://v8.googlecode.com/svn/tags/$NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH \
    -m "Tagging version $NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH"
  persist "TO_URL"
fi

let CURRENT_STEP+=1
if [ $START_STEP -le $CURRENT_STEP ] ; then
  echo ">>> Step $CURRENT_STEP: Cleanup."
  restore_if_unset "SVN_REVISION"
  restore_if_unset "TO_URL"
  restore_if_unset "REVISION_LIST"
  restore_version_if_unset "NEW"
  common_cleanup
  echo "*** SUMMARY ***"
  echo "version: $NEWMAJOR.$NEWMINOR.$NEWBUILD.$NEWPATCH"
  echo "branch: $TO_URL"
  echo "svn revision: $SVN_REVISION"
  [[ -n "$REVISION_LIST" ]] && echo "patches:$REVISION_LIST"
fi
