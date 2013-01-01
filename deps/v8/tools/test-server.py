#!/usr/bin/env python
#
# Copyright 2012 the V8 project authors. All rights reserved.
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


import os
import subprocess
import sys


PIDFILE = "/tmp/v8-distributed-testing-server.pid"
ROOT = os.path.abspath(os.path.dirname(sys.argv[0]))


def _PrintUsage():
  print("""Usage: python %s COMMAND

Where COMMAND can be any of:
  start     Starts the server. Forks to the background.
  stop      Stops the server.
  restart   Stops, then restarts the server.
  setup     Creates or updates the environment for the server to run.
  update    Alias for "setup".
  trust <keyfile>  Adds the given public key to the list of trusted keys.
  help      Displays this help text.
  """ % sys.argv[0])


def _IsDaemonRunning():
  return os.path.exists(PIDFILE)


def _Cmd(cmd):
  code = subprocess.call(cmd, shell=True)
  if code != 0:
    print("Command '%s' returned error code %d" % (cmd, code))
    sys.exit(code)


def Update():
  # Create directory for private data storage.
  data_dir = os.path.join(ROOT, "data")
  if not os.path.exists(data_dir):
    os.makedirs(data_dir)

  # Create directory for trusted public keys of peers (and self).
  trusted_dir = os.path.join(ROOT, "trusted")
  if not os.path.exists(trusted_dir):
    os.makedirs(trusted_dir)

  # Install UltraJSON. It is much faster than Python's builtin json.
  try:
    import ujson  #@UnusedImport
  except ImportError:
    # Install pip if it doesn't exist.
    code = subprocess.call("which pip > /dev/null", shell=True)
    if code != 0:
      apt_get_code = subprocess.call("which apt-get > /dev/null", shell=True)
      if apt_get_code == 0:
        print("Installing pip...")
        _Cmd("sudo apt-get install python-pip")
      else:
        print("Please install pip on your machine. You can get it at: "
              "http://www.pip-installer.org/en/latest/installing.html "
              "or via your distro's package manager.")
        sys.exit(1)
    print("Using pip to install UltraJSON...")
    _Cmd("sudo pip install ujson")

  # Make sure we have a key pair for signing binaries.
  privkeyfile = os.path.expanduser("~/.ssh/v8_dtest")
  if not os.path.exists(privkeyfile):
    _Cmd("ssh-keygen -t rsa -f %s -N '' -q" % privkeyfile)
  fingerprint = subprocess.check_output("ssh-keygen -lf %s" % privkeyfile,
                                        shell=True)
  fingerprint = fingerprint.split(" ")[1].replace(":", "")[:16]
  pubkeyfile = os.path.join(trusted_dir, "%s.pem" % fingerprint)
  if (not os.path.exists(pubkeyfile) or
      os.path.getmtime(pubkeyfile) < os.path.getmtime(privkeyfile)):
    _Cmd("openssl rsa -in %s -out %s -pubout" % (privkeyfile, pubkeyfile))
    with open(pubkeyfile, "a") as f:
      f.write(fingerprint + "\n")
    datafile = os.path.join(data_dir, "mypubkey")
    with open(datafile, "w") as f:
      f.write(fingerprint + "\n")

  # Check out or update the server implementation in the current directory.
  testrunner_dir = os.path.join(ROOT, "testrunner")
  if os.path.exists(os.path.join(testrunner_dir, "server/daemon.py")):
    _Cmd("cd %s; svn up" % testrunner_dir)
  else:
    path = ("http://v8.googlecode.com/svn/branches/bleeding_edge/"
            "tools/testrunner")
    _Cmd("svn checkout --force %s %s" % (path, testrunner_dir))

  # Update this very script.
  path = ("http://v8.googlecode.com/svn/branches/bleeding_edge/"
          "tools/test-server.py")
  scriptname = os.path.abspath(sys.argv[0])
  _Cmd("svn cat %s > %s" % (path, scriptname))

  # The testcfg.py files currently need to be able to import the old test.py
  # script, so we temporarily need to make that available.
  # TODO(jkummerow): Remove this when removing test.py.
  for filename in ("test.py", "utils.py"):
    url = ("http://v8.googlecode.com/svn/branches/bleeding_edge/"
           "tools/%s" % filename)
    filepath = os.path.join(os.path.dirname(scriptname), filename)
    _Cmd("svn cat %s > %s" % (url, filepath))

  # Check out or update V8.
  v8_dir = os.path.join(ROOT, "v8")
  if os.path.exists(v8_dir):
    _Cmd("cd %s; git fetch" % v8_dir)
  else:
    _Cmd("git clone git://github.com/v8/v8.git %s" % v8_dir)

  print("Finished.")


# Handle "setup" here, because when executing that we can't import anything
# else yet.
if __name__ == "__main__" and len(sys.argv) == 2:
  if sys.argv[1] in ("setup", "update"):
    if _IsDaemonRunning():
      print("Please stop the server before updating. Exiting.")
      sys.exit(1)
    Update()
    sys.exit(0)
  # Other parameters are handled below.


#==========================================================
# At this point we can assume that the implementation is available,
# so we can import it.
try:
  from testrunner.server import constants
  from testrunner.server import local_handler
  from testrunner.server import main
except Exception, e:
  print(e)
  print("Failed to import implementation. Have you run 'setup'?")
  sys.exit(1)


def _StartDaemon(daemon):
  if not os.path.isdir(os.path.join(ROOT, "v8")):
    print("No 'v8' working directory found. Have you run 'setup'?")
    sys.exit(1)
  daemon.start()


if __name__ == "__main__":
  if len(sys.argv) == 2:
    arg = sys.argv[1]
    if arg == "start":
      daemon = main.Server(PIDFILE, ROOT)
      _StartDaemon(daemon)
    elif arg == "stop":
      daemon = main.Server(PIDFILE, ROOT)
      daemon.stop()
    elif arg == "restart":
      daemon = main.Server(PIDFILE, ROOT)
      daemon.stop()
      _StartDaemon(daemon)
    elif arg in ("help", "-h", "--help"):
      _PrintUsage()
    elif arg == "status":
      if not _IsDaemonRunning():
        print("Server not running.")
      else:
        print(local_handler.LocalQuery([constants.REQUEST_STATUS]))
    else:
      print("Unknown command")
      _PrintUsage()
      sys.exit(2)
  elif len(sys.argv) == 3:
    arg = sys.argv[1]
    if arg == "approve":
      filename = sys.argv[2]
      if not os.path.exists(filename):
        print("%s does not exist.")
        sys.exit(1)
      filename = os.path.abspath(filename)
      if _IsDaemonRunning():
        response = local_handler.LocalQuery([constants.ADD_TRUSTED, filename])
      else:
        daemon = main.Server(PIDFILE, ROOT)
        response = daemon.CopyToTrusted(filename)
      print("Added certificate %s to trusted certificates." % response)
    else:
      print("Unknown command")
      _PrintUsage()
      sys.exit(2)
  else:
    print("Unknown command")
    _PrintUsage()
    sys.exit(2)
  sys.exit(0)
