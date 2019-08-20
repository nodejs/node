#!/usr/bin/env python

# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Runs an android build of d8 over adb, with any given arguments. Files
# requested by d8 are transferred on-demand from the caller, by reverse port
# forwarding a simple TCP file server from the computer to the android device.
#
# Usage:
#    adb-d8.py <build_dir> [<d8_args>...]
#
# Options:
#    <build_dir>    The directory containing the android build of d8.
#    <d8_args>...   The arguments passed through to d8.
#
# Run adb-d8.py --help for complete usage information.

from __future__ import print_function

import os
import sys
import struct
import threading
import subprocess
import SocketServer # TODO(leszeks): python 3 compatibility

def CreateFileHandlerClass(root_dirs, verbose):
  class FileHandler(SocketServer.BaseRequestHandler):
    def handle(self):
      data = self.request.recv(1024);
      while data[-1] != "\0":
        data += self.request.recv(1024);

      filename = data[0:-1]

      try:
        filename = os.path.abspath(filename)

        if not any(filename.startswith(root) for root in root_dirs):
          raise Exception("{} not in roots {}".format(filename, root_dirs))
        if not os.path.isfile(filename):
          raise Exception("{} is not a file".format(filename))

        if verbose:
          sys.stdout.write("Serving {}\r\n".format(os.path.relpath(filename)))

        with open(filename) as f:
          contents = f.read();
          self.request.sendall(struct.pack("!i", len(contents)))
          self.request.sendall(contents)

      except Exception as e:
        if verbose:
          sys.stderr.write(
            "Request failed ({})\n".format(e).replace('\n','\r\n'))
        self.request.sendall(struct.pack("!i", -1))

  return FileHandler


def TransferD8ToDevice(adb, build_dir, device_d8_dir, verbose):
  files_to_copy = ["d8", "natives_blob.bin", "snapshot_blob.bin"]

  # Pipe the output of md5sum from the local computer to the device, checking
  # the md5 hashes on the device.
  local_md5_sum_proc = subprocess.Popen(
    ["md5sum"] + files_to_copy,
    cwd=build_dir,
    stdout=subprocess.PIPE
  )
  device_md5_check_proc = subprocess.Popen(
    [
      adb, "shell",
      "mkdir -p '{0}' ; cd '{0}' ; md5sum -c -".format(device_d8_dir)
    ],
    stdin=local_md5_sum_proc.stdout,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE
  )

  # Push any files which failed the md5 check.
  (stdoutdata, stderrdata) = device_md5_check_proc.communicate()
  for line in stdoutdata.split('\n'):
    if line.endswith(": FAILED"):
      filename = line[:-len(": FAILED")]
      if verbose:
        print("Updating {}...".format(filename))
      subprocess.check_call([
        adb, "push",
        os.path.join(build_dir, filename),
        device_d8_dir
      ], stdout=sys.stdout if verbose else open(os.devnull, 'wb'))


def AdbForwardDeviceToLocal(adb, device_port, server_port, verbose):
  if verbose:
    print("Forwarding device:{} to localhost:{}...".format(
      device_port, server_port))

  subprocess.check_call([
    adb, "reverse",
    "tcp:{}".format(device_port),
    "tcp:{}".format(server_port)
  ])


def AdbRunD8(adb, device_d8_dir, device_port, d8_args, verbose):
  # Single-quote the arguments to d8, and concatenate them into a string.
  d8_arg_str = " ".join("'{}'".format(a) for a in d8_args)
  d8_arg_str = "--read-from-tcp-port='{}' ".format(device_port) + d8_arg_str

  # Don't use os.path.join for d8 because we care about the device's os, not
  # the host os.
  d8_str = "{}/d8 {}".format(device_d8_dir, d8_arg_str)

  if sys.stdout.isatty():
    # Run adb shell with -t to have a tty if we run d8 without a script.
    cmd = [adb, "shell", "-t", d8_str]
  else:
    cmd = [adb, "shell", d8_str]

  if verbose:
    print("Running {}".format(" ".join(cmd)))
  return subprocess.call(cmd)


def PrintUsage(file=sys.stdout):
  print("Usage: adb-d8.py [-v|--verbose] [--] <build_dir> [<d8 args>...]",
    file=file)


def PrintHelp(file=sys.stdout):
  print("""Usage:
   adb-d8.py [options] [--] <build_dir> [<d8_args>...]
   adb-d8.py -h|--help

Options:
   -h|--help             Show this help message and exit.
   -v|--verbose          Print verbose output.
   --device-dir=DIR      Specify which directory on the device should be used
                         for the d8 binary. [default: /data/local/tmp/v8]
   --extra-root-dir=DIR  In addition to the current directory, allow d8 to
                         access files inside DIR. Multiple additional roots
                         can be specified.
   <build_dir>           The directory containing the android build of d8.
   <d8_args>...          The arguments passed through to d8.""", file=file)


def Main():
  if len(sys.argv) < 2:
    PrintUsage(sys.stderr)
    return 1

  script_dir = os.path.dirname(sys.argv[0])
  # Use the platform-tools version of adb so that we know it has the reverse
  # command.
  adb = os.path.join(
    script_dir,
    "../third_party/android_sdk/public/platform-tools/adb"
  )

  # Read off any command line flags before build_dir (or --). Do this
  # manually, rather than using something like argparse, to be able to split
  # the adb-d8 options from the passthrough d8 options.
  verbose = False
  device_d8_dir = '/data/local/tmp/v8'
  root_dirs = []
  arg_index = 1
  while arg_index < len(sys.argv):
    arg = sys.argv[arg_index]
    if not arg.startswith("-"):
      break
    elif arg == "--":
      arg_index += 1
      break
    elif arg == "-h" or arg == "--help":
      PrintHelp(sys.stdout)
      return 0
    elif arg == "-v" or arg == "--verbose":
      verbose = True

    elif arg == "--device-dir":
      arg_index += 1
      device_d8_dir = sys.argv[arg_index]
    elif arg.startswith("--device-dir="):
      device_d8_dir = arg[len("--device-dir="):]

    elif arg == "--extra-root-dir":
      arg_index += 1
      root_dirs.append(sys.argv[arg_index])
    elif arg.startswith("--extra-root-dir="):
      root_dirs.append(arg[len("--extra-root-dir="):])

    else:
      print("ERROR: Unrecognised option: {}".format(arg))
      PrintUsage(sys.stderr)
      return 1

    arg_index += 1

  # Transfer d8 (and dependencies) to the device.
  build_dir = os.path.abspath(sys.argv[arg_index])

  TransferD8ToDevice(adb, build_dir, device_d8_dir, verbose)

  # Start a file server for the files d8 might need.
  script_root_dir = os.path.abspath(os.curdir)
  root_dirs.append(script_root_dir)
  server = SocketServer.TCPServer(
    ("localhost", 0), # 0 means an arbitrary unused port.
    CreateFileHandlerClass(root_dirs, verbose)
  )

  try:
    # Start the file server in its own thread.
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True
    server_thread.start()

    # Port-forward the given device port to the file server.
    # TODO(leszeks): Pick an unused device port.
    # TODO(leszeks): Remove the port forwarding on exit.
    server_ip, server_port = server.server_address
    device_port = 4444
    AdbForwardDeviceToLocal(adb, device_port, server_port, verbose)

    # Run d8 over adb with the remaining arguments, using the given device
    # port to forward file reads.
    return AdbRunD8(
      adb, device_d8_dir, device_port, sys.argv[arg_index+1:], verbose)

  finally:
    if verbose:
      print("Shutting down file server...")
    server.shutdown()
    server.server_close()

if __name__ == '__main__':
  sys.exit(Main())
