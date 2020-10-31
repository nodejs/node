#!/bin/bash
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

read -p "Name of SQL file to create (in test/trace_processor): " sqlfile
read -p "Name to trace file (in test/): " tracefile

ROOTDIR=$(dirname $(dirname $(readlink -f $0)))
TEST_PATH=$ROOTDIR/test
TRACE_PROC_PATH=$TEST_PATH/trace_processor

SQL_FILE_NAME=${sqlfile%.*}

echo "Creating $TRACE_PROC_PATH/$sqlfile"
touch $TRACE_PROC_PATH/$sqlfile

TRACE_PATH=$TEST_PATH/$tracefile
TRACE_BASE=$(basename $tracefile)
TRACE_FILE_NAME=${TRACE_BASE%.*}
OUT_FILE="$SQL_FILE_NAME""_$TRACE_FILE_NAME.out"

echo "Creating $TRACE_PROC_PATH/$OUT_FILE"
touch $TRACE_PROC_PATH/$OUT_FILE

RELTRACE=$(realpath -s $TRACE_PATH --relative-to=$TRACE_PROC_PATH --relative-base=$ROOTDIR)
echo "Adding index line to $TRACE_PROC_PATH/index"
echo >> $TRACE_PROC_PATH/index
echo "$RELTRACE $sqlfile $OUT_FILE" >> $TRACE_PROC_PATH/index
