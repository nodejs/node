#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import tempfile
import urllib2

from common_includes import *

class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.Git("fetch origin +refs/heads/*:refs/heads/*")
    self.GitCheckout("origin/master")
    self.DeleteBranch("work-branch")


class PrepareBranchRevision(Step):
  MESSAGE = "Check from which revision to branch off."

  def RunStep(self):
    self["push_hash"] = (self._options.revision or
                         self.GitLog(n=1, format="%H", branch="origin/master"))
    assert self["push_hash"]
    print "Release revision %s" % self["push_hash"]


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

    # The new version is not a candidate.
    self["new_candidate"] = "0"

    self["version"] = "%s.%s.%s" % (self["new_major"],
                                    self["new_minor"],
                                    self["new_build"])

    print ("Incremented version to %s" % self["version"])


class DetectLastRelease(Step):
  MESSAGE = "Detect commit ID of last release base."

  def RunStep(self):
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


class PushBranchRef(Step):
  MESSAGE = "Create branch ref."

  def RunStep(self):
    cmd = "push origin %s:refs/heads/%s" % (self["push_hash"], self["version"])
    if self._options.dry_run:
      print "Dry run. Command:\ngit %s" % cmd
    else:
      self.Git(cmd)


class MakeBranch(Step):
  MESSAGE = "Create the branch."

  def RunStep(self):
    self.Git("reset --hard origin/master")
    self.Git("new-branch work-branch --upstream origin/%s" % self["version"])
    self.GitCheckoutFile(CHANGELOG_FILE, self["latest_version"])
    self.GitCheckoutFile(VERSION_FILE, self["latest_version"])
    self.GitCheckoutFile(WATCHLISTS_FILE, self["latest_version"])


class AddChangeLog(Step):
  MESSAGE = "Add ChangeLog changes to release branch."

  def RunStep(self):
    changelog_entry = FileToText(self.Config("CHANGELOG_ENTRY_FILE"))
    old_change_log = FileToText(os.path.join(self.default_cwd, CHANGELOG_FILE))
    new_change_log = "%s\n\n\n%s" % (changelog_entry, old_change_log)
    TextToFile(new_change_log, os.path.join(self.default_cwd, CHANGELOG_FILE))


class SetVersion(Step):
  MESSAGE = "Set correct version for candidates."

  def RunStep(self):
    self.SetVersion(os.path.join(self.default_cwd, VERSION_FILE), "new_")


class EnableMergeWatchlist(Step):
  MESSAGE = "Enable watchlist entry for merge notifications."

  def RunStep(self):
    old_watchlist_content = FileToText(os.path.join(self.default_cwd,
                                                    WATCHLISTS_FILE))
    new_watchlist_content = re.sub("(# 'v8-merges@googlegroups\.com',)",
                                   "'v8-merges@googlegroups.com',",
                                   old_watchlist_content)
    TextToFile(new_watchlist_content, os.path.join(self.default_cwd,
                                                   WATCHLISTS_FILE))


class CommitBranch(Step):
  MESSAGE = "Commit version and changelog to new branch."

  def RunStep(self):
    # Convert the ChangeLog entry to commit message format.
    text = FileToText(self.Config("CHANGELOG_ENTRY_FILE"))

    # Remove date and trailing white space.
    text = re.sub(r"^%s: " % self["date"], "", text.rstrip())

    # Remove indentation and merge paragraphs into single long lines, keeping
    # empty lines between them.
    def SplitMapJoin(split_text, fun, join_text):
      return lambda text: join_text.join(map(fun, text.split(split_text)))
    text = SplitMapJoin(
        "\n\n", SplitMapJoin("\n", str.strip, " "), "\n\n")(text)

    if not text:  # pragma: no cover
      self.Die("Commit message editing failed.")
    text += "\n\nTBR=%s" % self._options.reviewer
    self["commit_title"] = text.splitlines()[0]
    TextToFile(text, self.Config("COMMITMSG_FILE"))

    self.GitCommit(file_name = self.Config("COMMITMSG_FILE"))
    os.remove(self.Config("COMMITMSG_FILE"))
    os.remove(self.Config("CHANGELOG_ENTRY_FILE"))


class LandBranch(Step):
  MESSAGE = "Upload and land changes."

  def RunStep(self):
    if self._options.dry_run:
      print "Dry run - upload CL."
    else:
      self.GitUpload(author=self._options.author,
                     force=True,
                     bypass_hooks=True,
                     private=True)
    cmd = "cl land --bypass-hooks -f"
    if self._options.dry_run:
      print "Dry run. Command:\ngit %s" % cmd
    else:
      self.Git(cmd)


class TagRevision(Step):
  MESSAGE = "Tag the new revision."

  def RunStep(self):
    if self._options.dry_run:
      print ("Dry run. Tagging \"%s\" with %s" %
             (self["commit_title"], self["version"]))
    else:
      self.vc.Tag(self["version"],
                  "origin/%s" % self["version"],
                  self["commit_title"])


class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    print("Congratulations, you have successfully created version %s."
          % self["version"])

    self.GitCheckout("origin/master")
    self.DeleteBranch("work-branch")
    self.Git("gc")


class CreateRelease(ScriptsBase):
  def _PrepareOptions(self, parser):
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-f", "--force",
                      help="Don't prompt the user.",
                      default=True, action="store_true")
    group.add_argument("-m", "--manual",
                      help="Prompt the user at every important step.",
                      default=False, action="store_true")
    parser.add_argument("-R", "--revision",
                        help="The git commit ID to push (defaults to HEAD).")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.author or not options.reviewer:
      print "Reviewer (-r) and author (-a) are required."
      return False
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/create-releases-tempfile",
      "CHANGELOG_ENTRY_FILE":
          "/tmp/v8-create-releases-tempfile-changelog-entry",
      "COMMITMSG_FILE": "/tmp/v8-create-releases-tempfile-commitmsg",
    }

  def _Steps(self):
    return [
      Preparation,
      PrepareBranchRevision,
      IncrementVersion,
      DetectLastRelease,
      PrepareChangeLog,
      EditChangeLog,
      PushBranchRef,
      MakeBranch,
      AddChangeLog,
      SetVersion,
      EnableMergeWatchlist,
      CommitBranch,
      LandBranch,
      TagRevision,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(CreateRelease().Run())
