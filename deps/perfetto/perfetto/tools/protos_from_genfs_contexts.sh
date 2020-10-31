#!/bin/bash
# Copyright (C) 2018 The Android Open Source Project
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

for f in $(cat $1 | grep u:object_r:debugfs_tracing:s0 | grep tracing/events | awk '{ print $3 }' | xargs -n1 basename);do
  if [ -f "protos/perfetto/trace/ftrace/$f.proto" ]; then
    echo "'protos/perfetto/trace/ftrace/$f.proto',";
  else
    for x in $(find src/traced/probes/ftrace/test/data/*/events -wholename '*/'"$f"'/format' -or -wholename '*/'"$f"'/*/format'); do
      event=$(echo $x | awk -F / '{print $(NF - 1)}')
      n="protos/perfetto/trace/ftrace/$event.proto";
      if [ -f $n ]; then
        echo "'$n',"
      else
          echo "# MISSING $event"
      fi
    done
  fi
done | sort
