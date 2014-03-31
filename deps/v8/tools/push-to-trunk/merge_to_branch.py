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

import argparse
from collections import OrderedDict
import sys

from common_includes import *

ALREADY_MERGING_SENTINEL_FILE = "ALREADY_MERGING_SENTINEL_FILE"
COMMIT_HASHES_FILE = "COMMIT_HASHES_FILE"
TEMPORARY_PATCH_FILE = "TEMPORARY_PATCH_FILE"

CONFIG = {
  BRANCHNAME: "prepare-merge",
  PERSISTFILE_BASENAME: "/tmp/v8-merge-to-branch-tempfile",
  ALREADY_MERGING_SENTINEL_FILE:
      "/tmp/v8-merge-to-branch-tempfile-already-merging",
  TEMP_BRANCH: "prepare-merge-temporary-branch-created-by-script",
  DOT_GIT_LOCATION: ".git",
  VERSION_FILE: "src/version.cc",
  TEMPORARY_PATCH_FILE: "/tmp/v8-prepare-merge-tempfile-temporary-patch",
  COMMITMSG_FILE: "/tmp/v8-prepare-merge-tempfile-commitmsg",
  COMMIT_HASHES_FILE: "/tmp/v8-merge-to-branch-tempfile-PATCH_COMMIT_HASHES",
}


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    if os.path.exists(self.Config(ALREADY_MERGING_SENTINEL_FILE)):
      if self._options.force:
        os.remove(self.Config(ALREADY_MERGING_SENTINEL_FILE))
      elif self._options.step == 0:  # pragma: no cover
        self.Die("A merge is already in progress")
    open(self.Config(ALREADY_MERGING_SENTINEL_FILE), "a").close()

    self.InitialEnvironmentChecks()
    if self._options.revert_bleeding_edge:
      self["merge_to_branch"] = "bleeding_edge"
    elif self._options.branch:
      self["merge_to_branch"] = self._options.branch
    else:  # pragma: no cover
      self.Die("Please specify a branch to merge to")

    self.CommonPrepare()
    self.PrepareBranch()


class CreateBranch(Step):
  MESSAGE = "Create a fresh branch for the patch."

  def RunStep(self):
    self.GitCreateBranch(self.Config(BRANCHNAME),
                         "svn/%s" % self["merge_to_branch"])


class SearchArchitecturePorts(Step):
  MESSAGE = "Search for corresponding architecture ports."

  def RunStep(self):
    self["full_revision_list"] = list(OrderedDict.fromkeys(
        self._options.revisions))
    port_revision_list = []
    for revision in self["full_revision_list"]:
      # Search for commits which matches the "Port rXXX" pattern.
      git_hashes = self.GitLog(reverse=True, format="%H",
                               grep="Port r%d" % int(revision),
                               branch="svn/bleeding_edge")
      for git_hash in git_hashes.splitlines():
        svn_revision = self.GitSVNFindSVNRev(git_hash, "svn/bleeding_edge")
        if not svn_revision:  # pragma: no cover
          self.Die("Cannot determine svn revision for %s" % git_hash)
        revision_title = self.GitLog(n=1, format="%s", git_hash=git_hash)

        # Is this revision included in the original revision list?
        if svn_revision in self["full_revision_list"]:
          print("Found port of r%s -> r%s (already included): %s"
                % (revision, svn_revision, revision_title))
        else:
          print("Found port of r%s -> r%s: %s"
                % (revision, svn_revision, revision_title))
          port_revision_list.append(svn_revision)

    # Do we find any port?
    if len(port_revision_list) > 0:
      if self.Confirm("Automatically add corresponding ports (%s)?"
                      % ", ".join(port_revision_list)):
        #: 'y': Add ports to revision list.
        self["full_revision_list"].extend(port_revision_list)


class FindGitRevisions(Step):
  MESSAGE = "Find the git revisions associated with the patches."

  def RunStep(self):
    self["patch_commit_hashes"] = []
    for revision in self["full_revision_list"]:
      next_hash = self.GitSVNFindGitHash(revision, "svn/bleeding_edge")
      if not next_hash:  # pragma: no cover
        self.Die("Cannot determine git hash for r%s" % revision)
      self["patch_commit_hashes"].append(next_hash)

    # Stringify: [123, 234] -> "r123, r234"
    self["revision_list"] = ", ".join(map(lambda s: "r%s" % s,
                                      self["full_revision_list"]))

    if not self["revision_list"]:  # pragma: no cover
      self.Die("Revision list is empty.")

    if self._options.revert:
      if not self._options.revert_bleeding_edge:
        self["new_commit_msg"] = ("Rollback of %s in %s branch."
            % (self["revision_list"], self["merge_to_branch"]))
      else:
        self["new_commit_msg"] = "Revert %s." % self["revision_list"]
    else:
      self["new_commit_msg"] = ("Merged %s into %s branch."
          % (self["revision_list"], self["merge_to_branch"]))
    self["new_commit_msg"] += "\n\n"

    for commit_hash in self["patch_commit_hashes"]:
      patch_merge_desc = self.GitLog(n=1, format="%s", git_hash=commit_hash)
      self["new_commit_msg"] += "%s\n\n" % patch_merge_desc

    bugs = []
    for commit_hash in self["patch_commit_hashes"]:
      msg = self.GitLog(n=1, git_hash=commit_hash)
      for bug in re.findall(r"^[ \t]*BUG[ \t]*=[ \t]*(.*?)[ \t]*$", msg,
                            re.M):
        bugs.extend(map(lambda s: s.strip(), bug.split(",")))
    bug_aggregate = ",".join(sorted(bugs))
    if bug_aggregate:
      self["new_commit_msg"] += "BUG=%s\nLOG=N\n" % bug_aggregate
    TextToFile(self["new_commit_msg"], self.Config(COMMITMSG_FILE))


class ApplyPatches(Step):
  MESSAGE = "Apply patches for selected revisions."

  def RunStep(self):
    for commit_hash in self["patch_commit_hashes"]:
      print("Applying patch for %s to %s..."
            % (commit_hash, self["merge_to_branch"]))
      patch = self.GitGetPatch(commit_hash)
      TextToFile(patch, self.Config(TEMPORARY_PATCH_FILE))
      self.ApplyPatch(self.Config(TEMPORARY_PATCH_FILE), self._options.revert)
    if self._options.patch:
      self.ApplyPatch(self._options.patch, self._options.revert)


class PrepareVersion(Step):
  MESSAGE = "Prepare version file."

  def RunStep(self):
    if self._options.revert_bleeding_edge:
      return
    # These version numbers are used again for creating the tag
    self.ReadAndPersistVersion()


class IncrementVersion(Step):
  MESSAGE = "Increment version number."

  def RunStep(self):
    if self._options.revert_bleeding_edge:
      return
    new_patch = str(int(self["patch"]) + 1)
    if self.Confirm("Automatically increment PATCH_LEVEL? (Saying 'n' will "
                    "fire up your EDITOR on %s so you can make arbitrary "
                    "changes. When you're done, save the file and exit your "
                    "EDITOR.)" % self.Config(VERSION_FILE)):
      text = FileToText(self.Config(VERSION_FILE))
      text = MSub(r"(?<=#define PATCH_LEVEL)(?P<space>\s+)\d*$",
                  r"\g<space>%s" % new_patch,
                  text)
      TextToFile(text, self.Config(VERSION_FILE))
    else:
      self.Editor(self.Config(VERSION_FILE))
    self.ReadAndPersistVersion("new_")


class CommitLocal(Step):
  MESSAGE = "Commit to local branch."

  def RunStep(self):
    self.GitCommit(file_name=self.Config(COMMITMSG_FILE))


class CommitRepository(Step):
  MESSAGE = "Commit to the repository."

  def RunStep(self):
    self.GitCheckout(self.Config(BRANCHNAME))
    self.WaitForLGTM()
    self.GitPresubmit()
    self.GitDCommit()


class PrepareSVN(Step):
  MESSAGE = "Determine svn commit revision."

  def RunStep(self):
    if self._options.revert_bleeding_edge:
      return
    self.GitSVNFetch()
    commit_hash = self.GitLog(n=1, format="%H", grep=self["new_commit_msg"],
                              branch="svn/%s" % self["merge_to_branch"])
    if not commit_hash:  # pragma: no cover
      self.Die("Unable to map git commit to svn revision.")
    self["svn_revision"] = self.GitSVNFindSVNRev(commit_hash)
    print "subversion revision number is r%s" % self["svn_revision"]


class TagRevision(Step):
  MESSAGE = "Create the tag."

  def RunStep(self):
    if self._options.revert_bleeding_edge:
      return
    self["version"] = "%s.%s.%s.%s" % (self["new_major"],
                                       self["new_minor"],
                                       self["new_build"],
                                       self["new_patch"])
    print "Creating tag svn/tags/%s" % self["version"]
    if self["merge_to_branch"] == "trunk":
      self["to_url"] = "trunk"
    else:
      self["to_url"] = "branches/%s" % self["merge_to_branch"]
    self.SVN("copy -r %s https://v8.googlecode.com/svn/%s "
             "https://v8.googlecode.com/svn/tags/%s -m "
             "\"Tagging version %s\""
             % (self["svn_revision"], self["to_url"],
                self["version"], self["version"]))


class CleanUp(Step):
  MESSAGE = "Cleanup."

  def RunStep(self):
    self.CommonCleanup()
    if not self._options.revert_bleeding_edge:
      print "*** SUMMARY ***"
      print "version: %s" % self["version"]
      print "branch: %s" % self["to_url"]
      print "svn revision: %s" % self["svn_revision"]
      if self["revision_list"]:
        print "patches: %s" % self["revision_list"]


class MergeToBranch(ScriptsBase):
  def _Description(self):
    return ("Performs the necessary steps to merge revisions from "
            "bleeding_edge to other branches, including trunk.")

  def _PrepareOptions(self, parser):
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--branch", help="The branch to merge to.")
    group.add_argument("-R", "--revert-bleeding-edge",
                       help="Revert specified patches from bleeding edge.",
                       default=False, action="store_true")
    parser.add_argument("revisions", nargs="*",
                        help="The revisions to merge.")
    parser.add_argument("-f", "--force",
                        help="Delete sentinel file.",
                        default=False, action="store_true")
    parser.add_argument("-m", "--message",
                        help="A commit message for the patch.")
    parser.add_argument("--revert",
                        help="Revert specified patches.",
                        default=False, action="store_true")
    parser.add_argument("-p", "--patch",
                        help="A patch file to apply as part of the merge.")

  def _ProcessOptions(self, options):
    # TODO(machenbach): Add a test that covers revert from bleeding_edge
    if len(options.revisions) < 1:
      if not options.patch:
        print "Either a patch file or revision numbers must be specified"
        return False
      if not options.message:
        print "You must specify a merge comment if no patches are specified"
        return False
    return True

  def _Steps(self):
    return [
      Preparation,
      CreateBranch,
      SearchArchitecturePorts,
      FindGitRevisions,
      ApplyPatches,
      PrepareVersion,
      IncrementVersion,
      CommitLocal,
      UploadStep,
      CommitRepository,
      PrepareSVN,
      TagRevision,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(MergeToBranch(CONFIG).Run())
