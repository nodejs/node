#!/usr/bin/env python
# Copyright (C) 2017 The Android Open Source Project
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

from __future__ import print_function
import sys
import os
import subprocess
import tempfile


def test_command(*args):
  subprocess.check_call(args, stdout=sys.stdout, stderr=sys.stderr)


if __name__ == '__main__':
  if len(sys.argv) != 4:
    print('Usage: test_proto_gen.py to/ftrace_proto_gen to/protoc to/format')
    exit(1)
  ftrace_proto_gen_path = sys.argv[1]
  protoc_path = sys.argv[2]
  format_path = sys.argv[3]
  tmpdir = tempfile.mkdtemp()
  proto_path = os.path.join(tmpdir, 'format.proto')
  test_command(ftrace_proto_gen_path, format_path, proto_path)
  test_command(protoc_path, proto_path, '--proto_path=' + tmpdir,
               '--cpp_out=' + tmpdir)
