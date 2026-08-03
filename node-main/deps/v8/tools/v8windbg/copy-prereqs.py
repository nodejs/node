#!/usr/bin/env python3
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program copies dbgeng.dll from the Windows SDK to the output directory,
so that we can test v8windbg. (The version of dbgeng.dll in system32, which
would be loaded otherwise, is insufficient.)
Arguments:
1. The directory that contains vs_toolchain.py
2. The directory to copy to
3. The cpu type for this build
"""

import sys
import os

vs_toolchain_dir, target_dir, target_cpu = sys.argv[1:]

sys.path.insert(0, vs_toolchain_dir)
import vs_toolchain

def CopyDebuggerFile(debug_file):
  win_sdk_dir = vs_toolchain.SetEnvironmentAndGetSDKDir()
  if not win_sdk_dir:
    return

  full_path = os.path.join(win_sdk_dir, 'Debuggers', target_cpu, debug_file)
  if not os.path.exists(full_path):
    return

  target_path = os.path.join(target_dir, debug_file)
  vs_toolchain._CopyRuntimeImpl(target_path, full_path, verbose=False)

  # Ninja expects the file's timestamp to be newer than this script.
  os.utime(target_path, None)

CopyDebuggerFile('dbgeng.dll')
