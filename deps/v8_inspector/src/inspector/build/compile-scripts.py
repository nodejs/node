#!/usr/bin/env python
#
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path as path
import generate_protocol_externs
import re
import subprocess
import sys

if len(sys.argv) == 2 and sys.argv[1] == '--help':
  print("Usage: %s" % path.basename(sys.argv[0]))
  sys.exit(0)

java_required_major = 1
java_required_minor = 7

v8_inspector_path = path.dirname(path.dirname(path.abspath(__file__)))

protocol_externs_file = path.join(v8_inspector_path, 'protocol_externs.js')
injected_script_source_name = path.join(v8_inspector_path,
  'injected-script-source.js')
injected_script_externs_file = path.join(v8_inspector_path,
  'injected_script_externs.js')
debugger_script_source_name = path.join(v8_inspector_path,
  'debugger-script.js')
debugger_script_externs_file = path.join(v8_inspector_path,
  'debugger_script_externs.js')

generate_protocol_externs.generate_protocol_externs(protocol_externs_file,
  path.join(v8_inspector_path, 'js_protocol.json'))

error_warning_regex = re.compile(r'WARNING|ERROR')

closure_compiler_jar = path.join(v8_inspector_path, 'build',
  'closure-compiler', 'closure-compiler.jar')

common_closure_args = [
  '--checks_only',
  '--warning_level', 'VERBOSE'
]

# Error reporting and checking.
errors_found = False

def popen(arguments):
  return subprocess.Popen(arguments, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT)

def error_excepthook(exctype, value, traceback):
  print 'ERROR:'
  sys.__excepthook__(exctype, value, traceback)
sys.excepthook = error_excepthook

def has_errors(output):
  return re.search(error_warning_regex, output) != None

# Find java. Based on
# http://stackoverflow.com/questions/377017/test-if-executable-exists-in-python.
def which(program):
  def is_exe(fpath):
    return path.isfile(fpath) and os.access(fpath, os.X_OK)

  fpath, fname = path.split(program)
  if fpath:
    if is_exe(program):
      return program
  else:
    for part in os.environ['PATH'].split(os.pathsep):
      part = part.strip('"')
      exe_file = path.join(part, program)
      if is_exe(exe_file):
        return exe_file
  return None

def find_java():
  exec_command = None
  has_server_jvm = True
  java_path = which('java')
  if not java_path:
    java_path = which('java.exe')

  if not java_path:
    print 'NOTE: No Java executable found in $PATH.'
    sys.exit(0)

  is_ok = False
  java_version_out, _ = popen([java_path, '-version']).communicate()
  java_build_regex = re.compile(r'^\w+ version "(\d+)\.(\d+)')
  # pylint: disable=E1103
  match = re.search(java_build_regex, java_version_out)
  if match:
    major = int(match.group(1))
    minor = int(match.group(2))
    is_ok = major >= java_required_major and minor >= java_required_minor
  if is_ok:
    exec_command = [java_path, '-Xms1024m', '-server',
      '-XX:+TieredCompilation']
    check_server_proc = popen(exec_command + ['-version'])
    check_server_proc.communicate()
    if check_server_proc.returncode != 0:
      # Not all Java installs have server JVMs.
      exec_command = exec_command.remove('-server')
      has_server_jvm = False

  if not is_ok:
    print 'NOTE: Java executable version %d.%d or above not found in $PATH.' % (java_required_major, java_required_minor)
    sys.exit(0)
  print 'Java executable: %s%s' % (java_path, '' if has_server_jvm else ' (no server JVM)')
  return exec_command

java_exec = find_java()

spawned_compiler_command = java_exec + [
  '-jar',
  closure_compiler_jar
] + common_closure_args

print 'Compiling injected-script-source.js...'

command = spawned_compiler_command + [
  '--externs', injected_script_externs_file,
  '--externs', protocol_externs_file,
  '--js', injected_script_source_name
]

injected_script_compile_proc = popen(command)

print 'Compiling debugger-script.js...'

command = spawned_compiler_command + [
  '--externs', debugger_script_externs_file,
  '--js', debugger_script_source_name,
  '--new_type_inf'
]

debugger_script_compile_proc = popen(command)

print 'Validating injected-script-source.js...'
injectedscript_check_script_path = path.join(v8_inspector_path, 'build',
  'check_injected_script_source.py')
validate_injected_script_proc = popen([sys.executable,
  injectedscript_check_script_path, injected_script_source_name])

print

(injected_script_compile_out, _) = injected_script_compile_proc.communicate()
print 'injected-script-source.js compilation output:%s' % os.linesep
print injected_script_compile_out
errors_found |= has_errors(injected_script_compile_out)

(debugger_script_compiler_out, _) = debugger_script_compile_proc.communicate()
print 'debugger-script.js compilation output:%s' % os.linesep
print debugger_script_compiler_out
errors_found |= has_errors(debugger_script_compiler_out)

(validate_injected_script_out, _) = validate_injected_script_proc.communicate()
print 'Validate injected-script-source.js output:%s' % os.linesep
print validate_injected_script_out if validate_injected_script_out else '<empty>'
errors_found |= has_errors(validate_injected_script_out)

os.remove(protocol_externs_file)

if errors_found:
  print 'ERRORS DETECTED'
  sys.exit(1)
