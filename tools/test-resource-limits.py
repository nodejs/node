import argparse
import os
import resource

parser = argparse.ArgumentParser()
parser.add_argument('--disable-core-files', action='store_true')
parser.add_argument('--max-virtual-memory', type=int)
parser.add_argument('command', nargs=argparse.REMAINDER)
options = parser.parse_args()
command = options.command
if command and command[0] == '--':
  command = command[1:]
if not command:
  parser.error('a test command is required')

if options.disable_core_files or options.max_virtual_memory is not None:
  resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
if options.max_virtual_memory is not None:
  resource.setrlimit(resource.RLIMIT_AS,
                     (options.max_virtual_memory, options.max_virtual_memory + 1))

# Replace this process so test signals, timeouts and exit codes reach the runner.
os.execvpe(command[0], command, os.environ)
