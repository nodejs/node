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

from collections import OrderedDict
import optparse
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


class MergeToBranchOptions(CommonOptions):
  def __init__(self, options, args):
    super(MergeToBranchOptions, self).__init__(options, True)
    self.requires_editor = True
    self.wait_for_lgtm = True
    self.delete_sentinel = options.f
    self.message = getattr(options, "message", "")
    self.revert = "--reverse" if getattr(options, "r", None) else ""
    self.revert_bleeding_edge = getattr(options, "revert_bleeding_edge", False)
    self.patch = getattr(options, "p", "")
    self.args = args


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    if os.path.exists(self.Config(ALREADY_MERGING_SENTINEL_FILE)):
      if self._options.delete_sentinel:
        os.remove(self.Config(ALREADY_MERGING_SENTINEL_FILE))
      elif self._options.s == 0:
        self.Die("A merge is already in progress")
    open(self.Config(ALREADY_MERGING_SENTINEL_FILE), "a").close()

    self.InitialEnvironmentChecks()
    if self._options.revert_bleeding_edge:
      self.Persist("merge_to_branch", "bleeding_edge")
    elif self._options.args[0]:
      self.Persist("merge_to_branch", self._options.args[0])
      self._options.args = self._options.args[1:]
    else:
      self.Die("Please specify a branch to merge to")

    self.CommonPrepare()
    self.PrepareBranch()


class CreateBranch(Step):
  MESSAGE = "Create a fresh branch for the patch."

  def RunStep(self):
    self.RestoreIfUnset("merge_to_branch")
    args = "checkout -b %s svn/%s" % (self.Config(BRANCHNAME),
                                      self._state["merge_to_branch"])
    if self.Git(args) is None:
      self.die("Creating branch %s failed." % self.Config(BRANCHNAME))


class SearchArchitecturePorts(Step):
  MESSAGE = "Search for corresponding architecture ports."

  def RunStep(self):
    full_revision_list = list(OrderedDict.fromkeys(self._options.args))
    port_revision_list = []
    for revision in full_revision_list:
      # Search for commits which matches the "Port rXXX" pattern.
      args = ("log svn/bleeding_edge --reverse "
              "--format=%%H --grep=\"Port r%d\"" % int(revision))
      git_hashes = self.Git(args) or ""
      for git_hash in git_hashes.strip().splitlines():
        args = "svn find-rev %s svn/bleeding_edge" % git_hash
        svn_revision = self.Git(args).strip()
        if not svn_revision:
          self.Die("Cannot determine svn revision for %s" % git_hash)
        revision_title = self.Git("log -1 --format=%%s %s" % git_hash)

        # Is this revision included in the original revision list?
        if svn_revision in full_revision_list:
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
        full_revision_list.extend(port_revision_list)
    self.Persist("full_revision_list", ",".join(full_revision_list))


class FindGitRevisions(Step):
  MESSAGE = "Find the git revisions associated with the patches."

  def RunStep(self):
    self.RestoreIfUnset("full_revision_list")
    self.RestoreIfUnset("merge_to_branch")
    full_revision_list = self._state["full_revision_list"].split(",")
    patch_commit_hashes = []
    for revision in full_revision_list:
      next_hash = self.Git("svn find-rev \"r%s\" svn/bleeding_edge" % revision)
      if not next_hash:
        self.Die("Cannot determine git hash for r%s" % revision)
      patch_commit_hashes.append(next_hash)

    # Stringify: [123, 234] -> "r123, r234"
    revision_list = ", ".join(map(lambda s: "r%s" % s, full_revision_list))

    if not revision_list:
      self.Die("Revision list is empty.")

    if self._options.revert:
      if not self._options.revert_bleeding_edge:
        new_commit_msg = ("Rollback of %s in %s branch."
                          % (revision_list, self._state["merge_to_branch"]))
      else:
        new_commit_msg = "Revert %s ." % revision_list
    else:
      new_commit_msg = ("Merged %s into %s branch."
                        % (revision_list, self._state["merge_to_branch"]))
    new_commit_msg += "\n\n"

    for commit_hash in patch_commit_hashes:
      patch_merge_desc = self.Git("log -1 --format=%%s %s" % commit_hash)
      new_commit_msg += "%s\n\n" % patch_merge_desc.strip()

    bugs = []
    for commit_hash in patch_commit_hashes:
      msg = self.Git("log -1 %s" % commit_hash)
      for bug in re.findall(r"^[ \t]*BUG[ \t]*=[ \t]*(.*?)[ \t]*$", msg,
                            re.M):
        bugs.extend(map(lambda s: s.strip(), bug.split(",")))
    bug_aggregate = ",".join(sorted(bugs))
    if bug_aggregate:
      new_commit_msg += "BUG=%s\nLOG=N\n" % bug_aggregate
    TextToFile(new_commit_msg, self.Config(COMMITMSG_FILE))
    self.Persist("new_commit_msg", new_commit_msg)
    self.Persist("revision_list", revision_list)
    self._state["patch_commit_hashes"] = patch_commit_hashes
    self.Persist("patch_commit_hashes_list", " ".join(patch_commit_hashes))


class ApplyPatches(Step):
  MESSAGE = "Apply patches for selected revisions."

  def RunStep(self):
    self.RestoreIfUnset("merge_to_branch")
    self.RestoreIfUnset("patch_commit_hashes_list")
    patch_commit_hashes = self._state.get("patch_commit_hashes")
    if not patch_commit_hashes:
      patch_commit_hashes = (
          self._state.get("patch_commit_hashes_list").strip().split(" "))
    if not patch_commit_hashes and not options.patch:
      self.Die("Variable patch_commit_hashes could not be restored.")
    for commit_hash in patch_commit_hashes:
      print("Applying patch for %s to %s..."
            % (commit_hash, self._state["merge_to_branch"]))
      patch = self.Git("log -1 -p %s" % commit_hash)
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
    self.RestoreIfUnset("patch")
    new_patch = str(int(self._state["patch"]) + 1)
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
    if self.Git("commit -a -F \"%s\"" % self.Config(COMMITMSG_FILE)) is None:
      self.Die("'git commit -a' failed.")


class CommitRepository(Step):
  MESSAGE = "Commit to the repository."

  def RunStep(self):
    self.RestoreIfUnset("merge_to_branch")
    if self.Git("checkout %s" % self.Config(BRANCHNAME)) is None:
      self.Die("Cannot ensure that the current branch is %s"
               % self.Config(BRANCHNAME))
    self.WaitForLGTM()
    if self.Git("cl presubmit", "PRESUBMIT_TREE_CHECK=\"skip\"") is None:
      self.Die("Presubmit failed.")

    if self.Git("cl dcommit -f --bypass-hooks",
                retry_on=lambda x: x is None) is None:
      self.Die("Failed to commit to %s" % self._status["merge_to_branch"])


class PrepareSVN(Step):
  MESSAGE = "Determine svn commit revision."

  def RunStep(self):
    if self._options.revert_bleeding_edge:
      return
    self.RestoreIfUnset("new_commit_msg")
    self.RestoreIfUnset("merge_to_branch")
    if self.Git("svn fetch") is None:
      self.Die("'git svn fetch' failed.")
    args = ("log -1 --format=%%H --grep=\"%s\" svn/%s"
            % (self._state["new_commit_msg"], self._state["merge_to_branch"]))
    commit_hash = self.Git(args).strip()
    if not commit_hash:
      self.Die("Unable to map git commit to svn revision.")
    svn_revision = self.Git("svn find-rev %s" % commit_hash).strip()
    print "subversion revision number is r%s" % svn_revision
    self.Persist("svn_revision", svn_revision)


class TagRevision(Step):
  MESSAGE = "Create the tag."

  def RunStep(self):
    if self._options.revert_bleeding_edge:
      return
    self.RestoreVersionIfUnset("new_")
    self.RestoreIfUnset("svn_revision")
    self.RestoreIfUnset("merge_to_branch")
    ver = "%s.%s.%s.%s" % (self._state["new_major"],
                           self._state["new_minor"],
                           self._state["new_build"],
                           self._state["new_patch"])
    print "Creating tag svn/tags/%s" % ver
    if self._state["merge_to_branch"] == "trunk":
      to_url = "trunk"
    else:
      to_url = "branches/%s" % self._state["merge_to_branch"]
    self.SVN("copy -r %s https://v8.googlecode.com/svn/%s "
             "https://v8.googlecode.com/svn/tags/%s -m "
             "\"Tagging version %s\""
             % (self._state["svn_revision"], to_url, ver, ver))
    self.Persist("to_url", to_url)


class CleanUp(Step):
  MESSAGE = "Cleanup."

  def RunStep(self):
    self.RestoreIfUnset("svn_revision")
    self.RestoreIfUnset("to_url")
    self.RestoreIfUnset("revision_list")
    self.RestoreVersionIfUnset("new_")
    ver = "%s.%s.%s.%s" % (self._state["new_major"],
                           self._state["new_minor"],
                           self._state["new_build"],
                           self._state["new_patch"])
    self.CommonCleanup()
    if not self._options.revert_bleeding_edge:
      print "*** SUMMARY ***"
      print "version: %s" % ver
      print "branch: %s" % self._state["to_url"]
      print "svn revision: %s" % self._state["svn_revision"]
      if self._state["revision_list"]:
        print "patches: %s" % self._state["revision_list"]


def RunMergeToBranch(config,
                     options,
                     side_effect_handler=DEFAULT_SIDE_EFFECT_HANDLER):
  step_classes = [
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

  RunScript(step_classes, config, options, side_effect_handler)


def BuildOptions():
  result = optparse.OptionParser()
  result.set_usage("""%prog [OPTIONS]... [BRANCH] [REVISION]...

Performs the necessary steps to merge revisions from bleeding_edge
to other branches, including trunk.""")
  result.add_option("-f",
                    help="Delete sentinel file.",
                    default=False, action="store_true")
  result.add_option("-m", "--message",
                    help="Specify a commit message for the patch.")
  result.add_option("-r", "--revert",
                    help="Revert specified patches.",
                    default=False, action="store_true")
  result.add_option("-R", "--revert-bleeding-edge",
                    help="Revert specified patches from bleeding edge.",
                    default=False, action="store_true")
  result.add_option("-p", "--patch", dest="p",
                    help="Specify a patch file to apply as part of the merge.")
  result.add_option("-s", "--step", dest="s",
                    help="Specify the step where to start work. Default: 0.",
                    default=0, type="int")
  return result


def ProcessOptions(options, args):
  revert_from_bleeding_edge = 1 if options.revert_bleeding_edge else 0
  min_exp_args = 2 - revert_from_bleeding_edge
  if len(args) < min_exp_args:
    if not options.p:
      print "Either a patch file or revision numbers must be specified"
      return False
    if not options.message:
      print "You must specify a merge comment if no patches are specified"
      return False
  if options.s < 0:
    print "Bad step number %d" % options.s
    return False
  return True


def Main():
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options, args):
    parser.print_help()
    return 1
  RunMergeToBranch(CONFIG, MergeToBranchOptions(options, args))

if __name__ == "__main__":
  sys.exit(Main())
