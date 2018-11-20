#!/usr/bin/env python
# Copyright 2013 the V8 project authors. All rights reserved.
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
import os
import sys
import tempfile
import urllib2

from common_includes import *

PUSH_MSG_GIT_SUFFIX = " (based on %s)"


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.InitialEnvironmentChecks(self.default_cwd)
    self.CommonPrepare()

    if(self["current_branch"] == self.Config("CANDIDATESBRANCH")
       or self["current_branch"] == self.Config("BRANCHNAME")):
      print "Warning: Script started on branch %s" % self["current_branch"]

    self.PrepareBranch()
    self.DeleteBranch(self.Config("CANDIDATESBRANCH"))


class FreshBranch(Step):
  MESSAGE = "Create a fresh branch."

  def RunStep(self):
    self.GitCreateBranch(self.Config("BRANCHNAME"),
                         self.vc.RemoteMasterBranch())


class PreparePushRevision(Step):
  MESSAGE = "Check which revision to push."

  def RunStep(self):
    if self._options.revision:
      self["push_hash"] = self._options.revision
    else:
      self["push_hash"] = self.GitLog(n=1, format="%H", git_hash="HEAD")
    if not self["push_hash"]:  # pragma: no cover
      self.Die("Could not determine the git hash for the push.")


class IncrementVersion(Step):
  MESSAGE = "Increment version number."

  def RunStep(self):
    latest_version = self.GetLatestVersion()

    # The version file on master can be used to bump up major/minor at
    # branch time.
    self.GitCheckoutFile(VERSION_FILE, self.vc.RemoteMasterBranch())
    self.ReadAndPersistVersion("master_")
    master_version = self.ArrayToVersion("master_")

    # Use the highest version from master or from tags to determine the new
    # version.
    authoritative_version = sorted(
        [master_version, latest_version], key=SortingKey)[1]
    self.StoreVersion(authoritative_version, "authoritative_")

    # Variables prefixed with 'new_' contain the new version numbers for the
    # ongoing candidates push.
    self["new_major"] = self["authoritative_major"]
    self["new_minor"] = self["authoritative_minor"]
    self["new_build"] = str(int(self["authoritative_build"]) + 1)

    # Make sure patch level is 0 in a new push.
    self["new_patch"] = "0"

    self["version"] = "%s.%s.%s" % (self["new_major"],
                                    self["new_minor"],
                                    self["new_build"])

    print ("Incremented version to %s" % self["version"])


class DetectLastRelease(Step):
  MESSAGE = "Detect commit ID of last release base."

  def RunStep(self):
    if self._options.last_master:
      self["last_push_master"] = self._options.last_master
    else:
      self["last_push_master"] = self.GetLatestReleaseBase()


class PrepareChangeLog(Step):
  MESSAGE = "Prepare raw ChangeLog entry."

  def RunStep(self):
    self["date"] = self.GetDate()
    output = "%s: Version %s\n\n" % (self["date"], self["version"])
    TextToFile(output, self.Config("CHANGELOG_ENTRY_FILE"))
    commits = self.GitLog(format="%H",
        git_hash="%s..%s" % (self["last_push_master"],
                             self["push_hash"]))

    # Cache raw commit messages.
    commit_messages = [
      [
        self.GitLog(n=1, format="%s", git_hash=commit),
        self.GitLog(n=1, format="%B", git_hash=commit),
        self.GitLog(n=1, format="%an", git_hash=commit),
      ] for commit in commits.splitlines()
    ]

    # Auto-format commit messages.
    body = MakeChangeLogBody(commit_messages, auto_format=True)
    AppendToFile(body, self.Config("CHANGELOG_ENTRY_FILE"))

    msg = ("        Performance and stability improvements on all platforms."
           "\n#\n# The change log above is auto-generated. Please review if "
           "all relevant\n# commit messages from the list below are included."
           "\n# All lines starting with # will be stripped.\n#\n")
    AppendToFile(msg, self.Config("CHANGELOG_ENTRY_FILE"))

    # Include unformatted commit messages as a reference in a comment.
    comment_body = MakeComment(MakeChangeLogBody(commit_messages))
    AppendToFile(comment_body, self.Config("CHANGELOG_ENTRY_FILE"))


class EditChangeLog(Step):
  MESSAGE = "Edit ChangeLog entry."

  def RunStep(self):
    print ("Please press <Return> to have your EDITOR open the ChangeLog "
           "entry, then edit its contents to your liking. When you're done, "
           "save the file and exit your EDITOR. ")
    self.ReadLine(default="")
    self.Editor(self.Config("CHANGELOG_ENTRY_FILE"))

    # Strip comments and reformat with correct indentation.
    changelog_entry = FileToText(self.Config("CHANGELOG_ENTRY_FILE")).rstrip()
    changelog_entry = StripComments(changelog_entry)
    changelog_entry = "\n".join(map(Fill80, changelog_entry.splitlines()))
    changelog_entry = changelog_entry.lstrip()

    if changelog_entry == "":  # pragma: no cover
      self.Die("Empty ChangeLog entry.")

    # Safe new change log for adding it later to the candidates patch.
    TextToFile(changelog_entry, self.Config("CHANGELOG_ENTRY_FILE"))


class StragglerCommits(Step):
  MESSAGE = ("Fetch straggler commits that sneaked in since this script was "
             "started.")

  def RunStep(self):
    self.vc.Fetch()
    self.GitCheckout(self.vc.RemoteMasterBranch())


class SquashCommits(Step):
  MESSAGE = "Squash commits into one."

  def RunStep(self):
    # Instead of relying on "git rebase -i", we'll just create a diff, because
    # that's easier to automate.
    TextToFile(self.GitDiff(self.vc.RemoteCandidateBranch(),
                            self["push_hash"]),
               self.Config("PATCH_FILE"))

    # Convert the ChangeLog entry to commit message format.
    text = FileToText(self.Config("CHANGELOG_ENTRY_FILE"))

    # Remove date and trailing white space.
    text = re.sub(r"^%s: " % self["date"], "", text.rstrip())

    # Show the used master hash in the commit message.
    suffix = PUSH_MSG_GIT_SUFFIX % self["push_hash"]
    text = MSub(r"^(Version \d+\.\d+\.\d+)$", "\\1%s" % suffix, text)

    # Remove indentation and merge paragraphs into single long lines, keeping
    # empty lines between them.
    def SplitMapJoin(split_text, fun, join_text):
      return lambda text: join_text.join(map(fun, text.split(split_text)))
    strip = lambda line: line.strip()
    text = SplitMapJoin("\n\n", SplitMapJoin("\n", strip, " "), "\n\n")(text)

    if not text:  # pragma: no cover
      self.Die("Commit message editing failed.")
    self["commit_title"] = text.splitlines()[0]
    TextToFile(text, self.Config("COMMITMSG_FILE"))


class NewBranch(Step):
  MESSAGE = "Create a new branch from candidates."

  def RunStep(self):
    self.GitCreateBranch(self.Config("CANDIDATESBRANCH"),
                         self.vc.RemoteCandidateBranch())


class ApplyChanges(Step):
  MESSAGE = "Apply squashed changes."

  def RunStep(self):
    self.ApplyPatch(self.Config("PATCH_FILE"))
    os.remove(self.Config("PATCH_FILE"))
    # The change log has been modified by the patch. Reset it to the version
    # on candidates and apply the exact changes determined by this
    # PrepareChangeLog step above.
    self.GitCheckoutFile(CHANGELOG_FILE, self.vc.RemoteCandidateBranch())
    # The version file has been modified by the patch. Reset it to the version
    # on candidates.
    self.GitCheckoutFile(VERSION_FILE, self.vc.RemoteCandidateBranch())


class CommitSquash(Step):
  MESSAGE = "Commit to local candidates branch."

  def RunStep(self):
    # Make a first commit with a slightly different title to not confuse
    # the tagging.
    msg = FileToText(self.Config("COMMITMSG_FILE")).splitlines()
    msg[0] = msg[0].replace("(based on", "(squashed - based on")
    self.GitCommit(message = "\n".join(msg))


class PrepareVersionBranch(Step):
  MESSAGE = "Prepare new branch to commit version and changelog file."

  def RunStep(self):
    self.GitCheckout("master")
    self.Git("fetch")
    self.GitDeleteBranch(self.Config("CANDIDATESBRANCH"))
    self.GitCreateBranch(self.Config("CANDIDATESBRANCH"),
                         self.vc.RemoteCandidateBranch())


class AddChangeLog(Step):
  MESSAGE = "Add ChangeLog changes to candidates branch."

  def RunStep(self):
    changelog_entry = FileToText(self.Config("CHANGELOG_ENTRY_FILE"))
    old_change_log = FileToText(os.path.join(self.default_cwd, CHANGELOG_FILE))
    new_change_log = "%s\n\n\n%s" % (changelog_entry, old_change_log)
    TextToFile(new_change_log, os.path.join(self.default_cwd, CHANGELOG_FILE))
    os.remove(self.Config("CHANGELOG_ENTRY_FILE"))


class SetVersion(Step):
  MESSAGE = "Set correct version for candidates."

  def RunStep(self):
    self.SetVersion(os.path.join(self.default_cwd, VERSION_FILE), "new_")


class CommitCandidate(Step):
  MESSAGE = "Commit version and changelog to local candidates branch."

  def RunStep(self):
    self.GitCommit(file_name = self.Config("COMMITMSG_FILE"))
    os.remove(self.Config("COMMITMSG_FILE"))


class SanityCheck(Step):
  MESSAGE = "Sanity check."

  def RunStep(self):
    # TODO(machenbach): Run presubmit script here as it is now missing in the
    # prepare push process.
    if not self.Confirm("Please check if your local checkout is sane: Inspect "
        "%s, compile, run tests. Do you want to commit this new candidates "
        "revision to the repository?" % VERSION_FILE):
      self.Die("Execution canceled.")  # pragma: no cover


class Land(Step):
  MESSAGE = "Land the patch."

  def RunStep(self):
    self.vc.CLLand()


class TagRevision(Step):
  MESSAGE = "Tag the new revision."

  def RunStep(self):
    self.vc.Tag(
        self["version"], self.vc.RemoteCandidateBranch(), self["commit_title"])


class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    print("Congratulations, you have successfully created the candidates "
          "revision %s."
          % self["version"])

    self.CommonCleanup()
    if self.Config("CANDIDATESBRANCH") != self["current_branch"]:
      self.GitDeleteBranch(self.Config("CANDIDATESBRANCH"))


class PushToCandidates(ScriptsBase):
  def _PrepareOptions(self, parser):
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-f", "--force",
                      help="Don't prompt the user.",
                      default=False, action="store_true")
    group.add_argument("-m", "--manual",
                      help="Prompt the user at every important step.",
                      default=False, action="store_true")
    parser.add_argument("-b", "--last-master",
                        help=("The git commit ID of the last master "
                              "revision that was pushed to candidates. This is"
                              " used for the auto-generated ChangeLog entry."))
    parser.add_argument("-l", "--last-push",
                        help="The git commit ID of the last candidates push.")
    parser.add_argument("-R", "--revision",
                        help="The git commit ID to push (defaults to HEAD).")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.manual and not options.reviewer:
      print "A reviewer (-r) is required in (semi-)automatic mode."
      return False
    if not options.manual and not options.author:
      print "Specify your chromium.org email with -a in (semi-)automatic mode."
      return False

    options.tbr_commit = not options.manual
    return True

  def _Config(self):
    return {
      "BRANCHNAME": "prepare-push",
      "CANDIDATESBRANCH": "candidates-push",
      "PERSISTFILE_BASENAME": "/tmp/v8-push-to-candidates-tempfile",
      "CHANGELOG_ENTRY_FILE":
          "/tmp/v8-push-to-candidates-tempfile-changelog-entry",
      "PATCH_FILE": "/tmp/v8-push-to-candidates-tempfile-patch-file",
      "COMMITMSG_FILE": "/tmp/v8-push-to-candidates-tempfile-commitmsg",
    }

  def _Steps(self):
    return [
      Preparation,
      FreshBranch,
      PreparePushRevision,
      IncrementVersion,
      DetectLastRelease,
      PrepareChangeLog,
      EditChangeLog,
      StragglerCommits,
      SquashCommits,
      NewBranch,
      ApplyChanges,
      CommitSquash,
      SanityCheck,
      Land,
      PrepareVersionBranch,
      AddChangeLog,
      SetVersion,
      CommitCandidate,
      Land,
      TagRevision,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(PushToCandidates().Run())
