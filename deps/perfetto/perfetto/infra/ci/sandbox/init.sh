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

# Performs initialization that requires root, then runs the test as non-root.

set -eux
chmod 777 /ci/cache /ci/artifacts
chown perfetto.perfetto /ci/ramdisk
cd /ci/ramdisk
exec sudo -u perfetto -g perfetto -EH bash /ci/testrunner.sh
