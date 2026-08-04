#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import subprocess
import sys


def GetArgs():
  parser = argparse.ArgumentParser(
      description="Finds a commit that a given patch can be applied to. "
                  "Does not actually apply the patch or modify your checkout "
                  "in any way.")
  parser.add_argument("patch_file", help="Patch file to match")
  parser.add_argument(
      "--branch", "-b", default="origin/master", type=str,
      help="Git tree-ish where to start searching for commits, "
           "default: %(default)s")
  parser.add_argument(
      "--limit", "-l", default=500, type=int,
      help="Maximum number of commits to search, default: %(default)s")
  parser.add_argument(
      "--verbose", "-v", default=False, action="store_true",
      help="Print verbose output for your entertainment")
  return parser.parse_args()


def FindFilesInPatch(patch_file):
  files = {}
  next_file = ""
  with open(patch_file) as patch:
    for line in patch:
      if line.startswith("diff --git "):
        # diff --git a/src/objects.cc b/src/objects.cc
        words = line.split()
        assert words[2].startswith("a/") and len(words[2]) > 2
        next_file = words[2][2:]
      elif line.startswith("index "):
        # index add3e61..d1bbf6a 100644
        hashes = line.split()[1]
        old_hash = hashes.split("..")[0]
        if old_hash.startswith("0000000"): continue  # Ignore new files.
        files[next_file] = old_hash
  return files


def GetGitCommitHash(treeish):
  cmd = ["git", "log", "-1", "--format=%H", treeish]
  return subprocess.check_output(cmd).strip()


def CountMatchingFiles(commit, files):
  matched_files = 0
  # Calling out to git once and parsing the result Python-side is faster
  # than calling 'git ls-tree' for every file.
  cmd = ["git", "ls-tree", "-r", commit] + [f for f in files]
  output = subprocess.check_output(cmd)
  for line in output.splitlines():
    # 100644 blob c6d5daaa7d42e49a653f9861224aad0a0244b944      src/objects.cc
    _, _, actual_hash, filename = line.split()
    expected_hash = files[filename]
    if actual_hash.startswith(expected_hash): matched_files += 1
  return matched_files


def FindFirstMatchingCommit(start, files, limit, verbose):
  commit = GetGitCommitHash(start)
  num_files = len(files)
  if verbose: print(">>> Found %d files modified by patch." % num_files)
  for _ in range(limit):
    matched_files = CountMatchingFiles(commit, files)
    if verbose: print("Commit %s matched %d files" % (commit, matched_files))
    if matched_files == num_files:
      return commit
    commit = GetGitCommitHash("%s^" % commit)
  print("Sorry, no matching commit found. "
        "Try running 'git fetch', specifying the correct --branch, "
        "and/or setting a higher --limit.")
  sys.exit(1)


if __name__ == "__main__":
  args = GetArgs()
  files = FindFilesInPatch(args.patch_file)
  commit = FindFirstMatchingCommit(args.branch, files, args.limit, args.verbose)
  if args.verbose:
    print(">>> Matching commit: %s" % commit)
    print(subprocess.check_output(["git", "log", "-1", commit]))
    print(">>> Kthxbai.")
  else:
    print(commit)
