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

# This file contains common function definitions for various other shell
# scripts in this directory. It is not meant to be executed by itself.

# Important: before including this file, the following variables must be set:
# - BRANCHNAME
# - PERSISTFILE_BASENAME

TEMP_BRANCH=$BRANCHNAME-temporary-branch-created-by-script
VERSION_FILE="src/version.cc"
CHANGELOG_ENTRY_FILE="$PERSISTFILE_BASENAME-changelog-entry"
PATCH_FILE="$PERSISTFILE_BASENAME-patch"
PATCH_OUTPUT_FILE="$PERSISTFILE_BASENAME-patch-output"
COMMITMSG_FILE="$PERSISTFILE_BASENAME-commitmsg"
TOUCHED_FILES_FILE="$PERSISTFILE_BASENAME-touched-files"
TRUNK_REVISION_FILE="$PERSISTFILE_BASENAME-trunkrevision"
START_STEP=0
CURRENT_STEP=0

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
  local MATCH=$(git branch | grep "$1" | awk '{print $NF}' | grep -x $1)
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
  local VALUE="${!VARNAME}"
  if [ -z "$VALUE" ] ; then
    VALUE="__EMPTY__"
  fi
  echo "$VALUE" > $FILE
}

restore() {
  local VARNAME=$1
  local FILE="$PERSISTFILE_BASENAME-$VARNAME"
  local VALUE="$(cat $FILE)"
  [[ -z "$VALUE" ]] && die "Variable '$VARNAME' could not be restored."
  if [ "$VALUE" == "__EMPTY__" ] ; then
    VALUE=""
  fi
  eval "$VARNAME=\"$VALUE\""
}

restore_if_unset() {
  local VARNAME=$1
  [[ -z "${!VARNAME}" ]] && restore "$VARNAME"
}

initial_environment_checks() {
  # Cancel if this is not a git checkout.
  [[ -d .git ]] \
    || die "This is not a git checkout, this script won't work for you."

  # Cancel if EDITOR is unset or not executable.
  [[ -n "$EDITOR" && -x "$(which $EDITOR)" ]] \
    || die "Please set your EDITOR environment variable, you'll need it."
}

common_prepare() {
  # Check for a clean workdir.
  [[ -z "$(git status -s -uno)" ]] \
    || die "Workspace is not clean. Please commit or undo your changes."

  # Persist current branch.
  CURRENT_BRANCH=$(git status -s -b -uno | grep "^##" | awk '{print $2}')
  persist "CURRENT_BRANCH"

  # Fetch unfetched revisions.
  git svn fetch || die "'git svn fetch' failed."

  # Get ahold of a safe temporary branch and check it out.
  if [ "$CURRENT_BRANCH" != "$TEMP_BRANCH" ] ; then
    delete_branch $TEMP_BRANCH
    git checkout -b $TEMP_BRANCH
  fi

  # Delete the branch that will be created later if it exists already.
  delete_branch $BRANCHNAME
}

common_cleanup() {
  restore_if_unset "CURRENT_BRANCH"
  git checkout -f $CURRENT_BRANCH
  [[ "$TEMP_BRANCH" != "$CURRENT_BRANCH" ]] && git branch -D $TEMP_BRANCH
  [[ "$BRANCHNAME" != "$CURRENT_BRANCH" ]] && git branch -D $BRANCHNAME
  # Clean up all temporary files.
  rm -f "$PERSISTFILE_BASENAME"*
}

# These two functions take a prefix for the variable names as first argument.
read_and_persist_version() {
  for v in MAJOR_VERSION MINOR_VERSION BUILD_NUMBER PATCH_LEVEL; do
    VARNAME="$1${v%%_*}"
    VALUE=$(grep "#define $v" "$VERSION_FILE" | awk '{print $NF}')
    eval "$VARNAME=\"$VALUE\""
    persist "$VARNAME"
  done
}
restore_version_if_unset() {
  for v in MAJOR MINOR BUILD PATCH; do
    restore_if_unset "$1$v"
  done
}

upload_step() {
  let CURRENT_STEP+=1
  if [ $START_STEP -le $CURRENT_STEP ] ; then
    echo ">>> Step $CURRENT_STEP: Upload for code review."
    echo -n "Please enter the email address of a V8 reviewer for your patch: "
    read REVIEWER
    git cl upload -r "$REVIEWER" --send-mail \
      || die "'git cl upload' failed, please try again."
  fi
}

wait_for_lgtm() {
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
}

# Takes a file containing the patch to apply as first argument.
apply_patch() {
  patch $REVERSE_PATCH -p1 < "$1" > "$PATCH_OUTPUT_FILE" || \
    { cat "$PATCH_OUTPUT_FILE" && die "Applying the patch failed."; }
  tee < "$PATCH_OUTPUT_FILE" >(grep "patching file" \
                               | awk '{print $NF}' >> "$TOUCHED_FILES_FILE")
  rm "$PATCH_OUTPUT_FILE"
}

stage_files() {
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
}
