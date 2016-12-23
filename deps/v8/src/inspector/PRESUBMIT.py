#!/usr/bin/env python
#
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""v8_inspect presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

compile_note = "Be sure to run your patch by the compile-scripts.py script prior to committing!"


def _CompileScripts(input_api, output_api):
  local_paths = [f.LocalPath() for f in input_api.AffectedFiles()]

  compilation_related_files = [
    "js_protocol.json"
    "compile-scripts.js",
    "injected-script-source.js",
    "debugger_script_externs.js",
    "injected_script_externs.js",
    "check_injected_script_source.js",
    "debugger-script.js"
  ]

  for file in compilation_related_files:
    if (any(file in path for path in local_paths)):
      script_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        "build", "compile-scripts.py")
      proc = input_api.subprocess.Popen(
        [input_api.python_executable, script_path],
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.STDOUT)
      out, _ = proc.communicate()
      if "ERROR" in out or "WARNING" in out or proc.returncode:
        return [output_api.PresubmitError(out)]
      if "NOTE" in out:
        return [output_api.PresubmitPromptWarning(out + compile_note)]
      return []
  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CompileScripts(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CompileScripts(input_api, output_api))
  return results
