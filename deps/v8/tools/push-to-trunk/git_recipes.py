#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
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

import re


class GitFailedException(Exception):
  pass


def Strip(f):
  def new_f(*args, **kwargs):
    return f(*args, **kwargs).strip()
  return new_f


def MakeArgs(l):
  """['-a', '', 'abc', ''] -> '-a abc'"""
  return " ".join(filter(None, l))


def Quoted(s):
  return "\"%s\"" % s


class GitRecipesMixin(object):
  def GitIsWorkdirClean(self):
    return self.Git("status -s -uno").strip() == ""

  @Strip
  def GitBranch(self):
    return self.Git("branch")

  def GitCreateBranch(self, name, branch=""):
    assert name
    self.Git(MakeArgs(["checkout -b", name, branch]))

  def GitDeleteBranch(self, name):
    assert name
    self.Git(MakeArgs(["branch -D", name]))

  def GitReset(self, name):
    assert name
    self.Git(MakeArgs(["reset --hard", name]))

  def GitStash(self):
    self.Git(MakeArgs(["stash"]))

  def GitRemotes(self):
    return map(str.strip, self.Git(MakeArgs(["branch -r"])).splitlines())

  def GitCheckout(self, name):
    assert name
    self.Git(MakeArgs(["checkout -f", name]))

  def GitCheckoutFile(self, name, branch_or_hash):
    assert name
    assert branch_or_hash
    self.Git(MakeArgs(["checkout -f", branch_or_hash, "--", name]))

  def GitCheckoutFileSafe(self, name, branch_or_hash):
    try:
      self.GitCheckoutFile(name, branch_or_hash)
    except GitFailedException:  # pragma: no cover
      # The file doesn't exist in that revision.
      return False
    return True

  def GitChangedFiles(self, git_hash):
    assert git_hash
    try:
      files = self.Git(MakeArgs(["diff --name-only",
                                 git_hash,
                                 "%s^" % git_hash]))
      return map(str.strip, files.splitlines())
    except GitFailedException:  # pragma: no cover
      # Git fails using "^" at branch roots.
      return []


  @Strip
  def GitCurrentBranch(self):
    for line in self.Git("status -s -b -uno").strip().splitlines():
      match = re.match(r"^## (.+)", line)
      if match: return match.group(1)
    raise Exception("Couldn't find curent branch.")  # pragma: no cover

  @Strip
  def GitLog(self, n=0, format="", grep="", git_hash="", parent_hash="",
             branch="", reverse=False):
    assert not (git_hash and parent_hash)
    args = ["log"]
    if n > 0:
      args.append("-%d" % n)
    if format:
      args.append("--format=%s" % format)
    if grep:
      args.append("--grep=\"%s\"" % grep.replace("\"", "\\\""))
    if reverse:
      args.append("--reverse")
    if git_hash:
      args.append(git_hash)
    if parent_hash:
      args.append("%s^" % parent_hash)
    args.append(branch)
    return self.Git(MakeArgs(args))

  def GitGetPatch(self, git_hash):
    assert git_hash
    return self.Git(MakeArgs(["log", "-1", "-p", git_hash]))

  # TODO(machenbach): Unused? Remove.
  def GitAdd(self, name):
    assert name
    self.Git(MakeArgs(["add", Quoted(name)]))

  def GitApplyPatch(self, patch_file, reverse=False):
    assert patch_file
    args = ["apply --index --reject"]
    if reverse:
      args.append("--reverse")
    args.append(Quoted(patch_file))
    self.Git(MakeArgs(args))

  def GitUpload(self, reviewer="", author="", force=False, cq=False,
                bypass_hooks=False):
    args = ["cl upload --send-mail"]
    if author:
      args += ["--email", Quoted(author)]
    if reviewer:
      args += ["-r", Quoted(reviewer)]
    if force:
      args.append("-f")
    if cq:
      args.append("--use-commit-queue")
    if bypass_hooks:
      args.append("--bypass-hooks")
    # TODO(machenbach): Check output in forced mode. Verify that all required
    # base files were uploaded, if not retry.
    self.Git(MakeArgs(args), pipe=False)

  def GitCommit(self, message="", file_name=""):
    assert message or file_name
    args = ["commit"]
    if file_name:
      args += ["-aF", Quoted(file_name)]
    if message:
      args += ["-am", Quoted(message)]
    self.Git(MakeArgs(args))

  def GitPresubmit(self):
    self.Git("cl presubmit", "PRESUBMIT_TREE_CHECK=\"skip\"")

  def GitDCommit(self):
    self.Git("cl dcommit -f --bypass-hooks", retry_on=lambda x: x is None)

  def GitDiff(self, loc1, loc2):
    return self.Git(MakeArgs(["diff", loc1, loc2]))

  def GitPull(self):
    self.Git("pull")

  def GitSVNFetch(self):
    self.Git("svn fetch")

  def GitSVNRebase(self):
    self.Git("svn rebase")

  # TODO(machenbach): Unused? Remove.
  @Strip
  def GitSVNLog(self):
    return self.Git("svn log -1 --oneline")

  @Strip
  def GitSVNFindGitHash(self, revision, branch=""):
    assert revision
    return self.Git(MakeArgs(["svn find-rev", "r%s" % revision, branch]))

  @Strip
  def GitSVNFindSVNRev(self, git_hash, branch=""):
    return self.Git(MakeArgs(["svn find-rev", git_hash, branch]))

  def GitSVNDCommit(self):
    return self.Git("svn dcommit 2>&1", retry_on=lambda x: x is None)

  def GitSVNTag(self, version):
    self.Git(("svn tag %s -m \"Tagging version %s\"" % (version, version)),
             retry_on=lambda x: x is None)
