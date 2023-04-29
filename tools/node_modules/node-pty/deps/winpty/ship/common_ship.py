import os
import sys

if os.name != "nt":
    sys.exit("Error: ship scripts require native Python 2.7. (wrong os.name)")
if sys.version_info[0:2] != (2,7):
    sys.exit("Error: ship scripts require native Python 2.7. (wrong version)")

import glob
import shutil
import subprocess
from distutils.spawn import find_executable

topDir = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

with open(topDir + "/VERSION.txt", "rt") as f:
    winptyVersion = f.read().strip()

def rmrf(patterns):
    for pattern in patterns:
        for path in glob.glob(pattern):
            if os.path.isdir(path) and not os.path.islink(path):
                print "+ rm -r " + path
                sys.stdout.flush()
                shutil.rmtree(path)
            elif os.path.isfile(path):
                print "+ rm " + path
                sys.stdout.flush()
                os.remove(path)

def mkdir(path):
    if not os.path.isdir(path):
        os.makedirs(path)

def requireExe(name, guesses):
    if find_executable(name) is None:
        for guess in guesses:
            if os.path.exists(guess):
                newDir = os.path.dirname(guess)
                print "Adding " + newDir + " to Path to provide " + name
                os.environ["Path"] = newDir + ";" + os.environ["Path"]
    ret = find_executable(name)
    if ret is None:
        sys.exit("Error: required EXE is missing from Path: " + name)
    return ret

requireExe("git.exe", [
    "C:\\Program Files\\Git\\cmd\\git.exe",
    "C:\\Program Files (x86)\\Git\\cmd\\git.exe"
])

commitHash = subprocess.check_output(["git.exe", "rev-parse", "HEAD"]).decode().strip()
defaultPathEnviron = "C:\\Windows\\System32;C:\\Windows"
