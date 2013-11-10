#!/bin/bash
# Copyright 2013 the V8 project authors. All rights reserved.
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

# Tests the push-to-trunk.sh script. Needs to be run in V8 base dir:
# ./tools/test-push-to-trunk.sh

# TODO(machenbach): Check automatically if expectations match.
# TODO(machenbach): Mock out version number retrieval.
# TODO(machenbach): Allow multiple different test cases.
# TODO(machenbach): Allow multi line mock output.
# TODO(machenbach): Represent test expectations/mock output without an array
# index increment.

########## Stdin for push-to-trunk.sh

# Confirm push to trunk commit ID
INPUT[0]="Y"
# Open editor
INPUT[1]=""
# Confirm increment version number
INPUT[2]="Y"
# Reviewer for V8 CL
INPUT[3]="reviewer@chromium.org"
# Enter LGTM for V8 CL
INPUT[4]="LGTM"
# Confirm checkout sanity
INPUT[5]="Y"
# Manually type in trunk revision
INPUT[6]="12345"
# Reviewer for Chromium CL
INPUT[7]="reviewer@chromium.org"

########## Expected commands and mock output

EXP[0]="git status -s -uno"
OUT[0]=""
EXP[1]="git status -s -b -uno"
OUT[1]="## some_branch"
EXP[2]="git svn fetch"
OUT[2]=""
EXP[3]="git branch"
OUT[3]="not the temp branch"
EXP[4]="git checkout -b prepare-push-temporary-branch-created-by-script"
OUT[4]=""
EXP[5]="git branch"
OUT[5]="not the branch"
EXP[6]="git branch"
OUT[6]="not the trunk branch"
EXP[7]="git checkout -b prepare-push svn/bleeding_edge"
OUT[7]=""
EXP[8]="git log -1 --format=%H ChangeLog"
OUT[8]="hash1"
EXP[9]="git log -1 hash1"
OUT[9]=""
EXP[10]="git log hash1..HEAD --format=%H"
OUT[10]="hash2"
EXP[11]="git log -1 hash2 --format=\"%w(80,8,8)%s\""
OUT[11]="Log line..."
EXP[12]="git log -1 hash2 --format=\"%B\""
OUT[12]="BUG=6789"
EXP[13]="git log -1 hash2 --format=\"%w(80,8,8)(%an)\""
OUT[13]="   (author@chromium.org)"
EXP[14]="git commit -a -m \"Prepare push to trunk.  Now working on version 3.4.5.\""
OUT[14]=""
EXP[15]="git cl upload -r reviewer@chromium.org --send-mail"
OUT[15]=""
EXP[16]="git cl dcommit"
OUT[16]=""
EXP[17]="git svn fetch"
OUT[17]=""
EXP[18]="git checkout svn/bleeding_edge"
OUT[18]=""
EXP[19]="git log -1 --format=%H --grep=Prepare push to trunk.  Now working on version 3.4.5."
OUT[19]="hash3"
EXP[20]="git diff svn/trunk"
OUT[20]="patch1"
EXP[21]="git checkout -b trunk-push svn/trunk"
OUT[21]=""
EXP[22]="git apply --index --reject /tmp/v8-push-to-trunk-tempfile-patch"
OUT[22]=""
EXP[23]="git add src/version.cc"
OUT[23]=""
EXP[24]="git commit -F /tmp/v8-push-to-trunk-tempfile-commitmsg"
OUT[24]=""
EXP[25]="git svn dcommit"
OUT[25]="r1234"
EXP[26]="git svn tag 3.4.5 -m \"Tagging version 3.4.5\""
OUT[26]=""
EXP[27]="git status -s -uno"
OUT[27]=""
EXP[28]="git checkout master"
OUT[28]=""
EXP[29]="git pull"
OUT[29]=""
EXP[30]="git checkout -b v8-roll-12345"
OUT[30]=""
EXP[31]="git commit -am Update V8 to version 3.4.5."
OUT[31]=""
EXP[32]="git cl upload --send-mail"
OUT[32]=""
EXP[33]="git checkout -f some_branch"
OUT[33]=""
EXP[34]="git branch -D prepare-push-temporary-branch-created-by-script"
OUT[34]=""
EXP[35]="git branch -D prepare-push"
OUT[35]=""
EXP[36]="git branch -D trunk-push"
OUT[36]=""

########## Global temp files for test input/output

export TEST_OUTPUT=$(mktemp)
export INDEX=$(mktemp)
export MOCK_OUTPUT=$(mktemp)
export EXPECTED_COMMANDS=$(mktemp)

########## Command index

inc_index() {
  local I="$(command cat $INDEX)"
  let "I+=1"
  echo "$I" > $INDEX
  echo $I
}

echo "-1" > $INDEX
export -f inc_index

########## Mock output accessor

get_mock_output() {
  local I=$1
  let "I+=1"
  command sed "${I}q;d" $MOCK_OUTPUT
}

export -f get_mock_output

for E in "${OUT[@]}"; do
  echo $E
done > $MOCK_OUTPUT

########## Expected commands accessor

get_expected_command() {
  local I=$1
  let "I+=1"
  command sed "${I}q;d" $EXPECTED_COMMANDS
}

export -f get_expected_command

for E in "${EXP[@]}"; do
  echo $E
done > $EXPECTED_COMMANDS

########## Mock commands

git() {
  # All calls to git are mocked out. Expected calls and mock output are stored
  # in the EXP/OUT arrays above.
  local I=$(inc_index)
  local OUT=$(get_mock_output $I)
  local EXP=$(get_expected_command $I)
  echo "#############################" >> $TEST_OUTPUT
  echo "Com. Index:  $I" >> $TEST_OUTPUT
  echo "Expected:    ${EXP}" >> $TEST_OUTPUT
  echo "Actual:      git $@" >> $TEST_OUTPUT
  echo "Mock Output: ${OUT}" >> $TEST_OUTPUT
  echo "${OUT}"
}

mv() {
  echo "#############################" >> $TEST_OUTPUT
  echo "mv $@" >> $TEST_OUTPUT
}

sed() {
  # Only calls to sed * -i * are mocked out.
  echo "#############################" >> $TEST_OUTPUT
  local arr=$@
  if [[ "${arr[@]}" =~ "-i" || "${arr[${#arr[@]}-1]}" == "-i" ]]; then
    echo "sed $@" >> $TEST_OUTPUT
  else
    echo "sed $@" >> $TEST_OUTPUT
    command sed "$@"
  fi
}

editor() {
  echo "#############################" >> $TEST_OUTPUT
  echo "editor $@" >> $TEST_OUTPUT
}

cd() {
  echo "#############################" >> $TEST_OUTPUT
  echo "cd $@" >> $TEST_OUTPUT
}

export -f git
export -f mv
export -f sed
export -f cd
export -f editor
export EDITOR=editor

########## Invoke script with test stdin

for i in "${INPUT[@]}"; do 
    echo $i
done | tools/push-to-trunk.sh -c "path/to/chromium"

echo "Collected output:"
command cat $TEST_OUTPUT

########## Clean up

rm -rf $TEST_OUTPUT
rm -rf $INDEX
rm -rf $MOCK_OUTPUT
rm -rf $EXPECTED_COMMANDS
