#!/usr/bin/env python3
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import tempfile

from common_includes import *

import urllib.request as urllib2


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.Git("fetch origin +refs/heads/*:refs/heads/*")
    self.GitCheckout("origin/main")
    self.DeleteBranch("work-branch")


class PrepareBranchRevision(Step):
  MESSAGE = "Check from which revision to branch off."

  def RunStep(self):
    self["push_hash"] = (self._options.revision or
                         self.GitLog(n=1, format="%H", branch="origin/main"))
    assert self["push_hash"]
    print("Release revision %s" % self["push_hash"])


class IncrementVersion(Step):
  MESSAGE = "Increment version number."

  def RunStep(self):
    latest_version = self.GetLatestVersion()

    # The version file on main can be used to bump up major/minor at
    # branch time.
    self.GitCheckoutFile(VERSION_FILE, self.vc.RemoteMainBranch())
    self.ReadAndPersistVersion("main_")
    main_version = self.ArrayToVersion("main_")

    # Use the highest version from main or from tags to determine the new
    # version.
    authoritative_version = sorted(
        [main_version, latest_version], key=LooseVersion)[1]
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
    self["last_push_main"] = self.GetLatestReleaseBase()


class DeleteBranchRef(Step):
  MESSAGE = "Delete branch ref."

  def RunStep(self):
    cmd = "push origin :refs/heads/%s" % self["version"]
    if self._options.dry_run:
      print("Dry run. Command:\ngit %s" % cmd)
    else:
      try:
        self.Git(cmd)
      except Exception:
        # Be forgiving if branch ref does not exist.
        pass


class PushBranchRef(Step):
  MESSAGE = "Create branch ref."

  def RunStep(self):
    cmd = "push origin %s:refs/heads/%s" % (self["push_hash"], self["version"])
    if self._options.dry_run:
      print("Dry run. Command:\ngit %s" % cmd)
    else:
      self.Git(cmd)


class MakeBranch(Step):
  MESSAGE = "Create the branch."

  def RunStep(self):
    self.Git("reset --hard origin/main")
    self.Git("new-branch work-branch --upstream origin/%s" % self["version"])
    self.GitCheckoutFile(VERSION_FILE, self["latest_version"])


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
  MESSAGE = "Commit version to new branch."

  def RunStep(self):
    self["commit_title"] = "Version %s" % self["version"]
    text = "%s" % (self["commit_title"])
    TextToFile(text, self.Config("COMMITMSG_FILE"))

    self.GitCommit(file_name=self.Config("COMMITMSG_FILE"))


class LandBranch(Step):
  MESSAGE = "Upload and land changes."

  def RunStep(self):
    if self._options.dry_run:
      print("Dry run - upload CL.")
    else:
      self.GitUpload(force=True,
                     bypass_hooks=True,
                     no_autocc=True,
                     set_bot_commit=True,
                     message_file=self.Config("COMMITMSG_FILE"))
    # TODO(crbug.com/1176141): This might need to go through CQ.
    # We'd need to wait for it to land and then tag it.
    cmd = "cl land --bypass-hooks -f"
    if self._options.dry_run:
      print("Dry run. Command:\ngit %s" % cmd)
    else:
      self.Git(cmd)

    os.remove(self.Config("COMMITMSG_FILE"))


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

    self.GitCheckout("origin/main")
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
      print("Reviewer (-r) and author (-a) are required.")
      return False
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/create-releases-tempfile",
      "COMMITMSG_FILE": "/tmp/v8-create-releases-tempfile-commitmsg",
    }

  def _Steps(self):
    return [
      Preparation,
      PrepareBranchRevision,
      IncrementVersion,
      DetectLastRelease,
      DeleteBranchRef,
      PushBranchRef,
      MakeBranch,
      SetVersion,
      EnableMergeWatchlist,
      CommitBranch,
      LandBranch,
      TagRevision,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(CreateRelease().Run())
