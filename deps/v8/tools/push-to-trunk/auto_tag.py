#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys

from common_includes import *


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    # TODO(machenbach): Remove after the git switch.
    if self.Config("PERSISTFILE_BASENAME") == "/tmp/v8-auto-tag-tempfile":
      print "This script is disabled until after the v8 git migration."
      return True

    self.CommonPrepare()
    self.PrepareBranch()
    self.GitCheckout("master")
    self.vc.Pull()


class GetTags(Step):
  MESSAGE = "Get all V8 tags."

  def RunStep(self):
    self.GitCreateBranch(self._config["BRANCHNAME"])
    self["tags"] = self.vc.GetTags()


class GetOldestUntaggedVersion(Step):
  MESSAGE = "Check if there's a version on bleeding edge without a tag."

  def RunStep(self):
    tags = set(self["tags"])
    self["candidate"] = None
    self["candidate_version"] = None
    self["next"] = None
    self["next_version"] = None

    # Iterate backwards through all automatic version updates.
    for git_hash in self.GitLog(
        format="%H", grep="\\[Auto\\-roll\\] Bump up version to").splitlines():

      # Get the version.
      if not self.GitCheckoutFileSafe(VERSION_FILE, git_hash):
        continue

      self.ReadAndPersistVersion()
      version = self.ArrayToVersion("")

      # Strip off trailing patch level (tags don't include tag level 0).
      if version.endswith(".0"):
        version = version[:-2]

      # Clean up checked-out version file.
      self.GitCheckoutFileSafe(VERSION_FILE, "HEAD")

      if version in tags:
        if self["candidate"]:
          # Revision "git_hash" is tagged already and "candidate" was the next
          # newer revision without a tag.
          break
        else:
          print("Stop as %s is the latest version and it has been tagged." %
                version)
          self.CommonCleanup()
          return True
      else:
        # This is the second oldest version without a tag.
        self["next"] = self["candidate"]
        self["next_version"] = self["candidate_version"]

        # This is the oldest version without a tag.
        self["candidate"] = git_hash
        self["candidate_version"] = version

    if not self["candidate"] or not self["candidate_version"]:
      print "Nothing found to tag."
      self.CommonCleanup()
      return True

    print("Candidate for tagging is %s with version %s" %
          (self["candidate"], self["candidate_version"]))


class GetLKGRs(Step):
  MESSAGE = "Get the last lkgrs."

  def RunStep(self):
    revision_url = "https://v8-status.appspot.com/revisions?format=json"
    status_json = self.ReadURL(revision_url, wait_plan=[5, 20])
    self["lkgrs"] = [entry["revision"]
                     for entry in json.loads(status_json) if entry["status"]]


class CalculateTagRevision(Step):
  MESSAGE = "Calculate the revision to tag."

  def LastLKGR(self, min_rev, max_rev):
    """Finds the newest lkgr between min_rev (inclusive) and max_rev
    (exclusive).
    """
    for lkgr in self["lkgrs"]:
      # LKGRs are reverse sorted.
      if int(min_rev) <= int(lkgr) and int(lkgr) < int(max_rev):
        return lkgr
    return None

  def RunStep(self):
    # Get the lkgr after the tag candidate and before the next tag candidate.
    candidate_svn = self.vc.GitSvn(self["candidate"])
    if self["next"]:
      next_svn = self.vc.GitSvn(self["next"])
    else:
      # Don't include the version change commit itself if there is no upper
      # limit yet.
      candidate_svn =  str(int(candidate_svn) + 1)
      next_svn = sys.maxint
    lkgr_svn = self.LastLKGR(candidate_svn, next_svn)

    if not lkgr_svn:
      print "There is no lkgr since the candidate version yet."
      self.CommonCleanup()
      return True

    # Let's check if the lkgr is at least three hours old.
    self["lkgr"] = self.vc.SvnGit(lkgr_svn)
    if not self["lkgr"]:
      print "Couldn't find git hash for lkgr %s" % lkgr_svn
      self.CommonCleanup()
      return True

    lkgr_utc_time = int(self.GitLog(n=1, format="%at", git_hash=self["lkgr"]))
    current_utc_time = self._side_effect_handler.GetUTCStamp()

    if current_utc_time < lkgr_utc_time + 10800:
      print "Candidate lkgr %s is too recent for tagging." % lkgr_svn
      self.CommonCleanup()
      return True

    print "Tagging revision %s with %s" % (lkgr_svn, self["candidate_version"])


class MakeTag(Step):
  MESSAGE = "Tag the version."

  def RunStep(self):
    if not self._options.dry_run:
      self.GitReset(self["lkgr"])
      self.vc.Tag(self["candidate_version"])


class CleanUp(Step):
  MESSAGE = "Clean up."

  def RunStep(self):
    self.CommonCleanup()


class AutoTag(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("--dry_run", help="Don't tag the new version.",
                        default=False, action="store_true")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.dry_run and not options.author:
      print "Specify your chromium.org email with -a"
      return False
    options.wait_for_lgtm = False
    options.force_readline_defaults = True
    options.force_upload = True
    return True

  def _Config(self):
    return {
      "BRANCHNAME": "auto-tag-v8",
      "PERSISTFILE_BASENAME": "/tmp/v8-auto-tag-tempfile",
    }

  def _Steps(self):
    return [
      Preparation,
      GetTags,
      GetOldestUntaggedVersion,
      GetLKGRs,
      CalculateTagRevision,
      MakeTag,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoTag().Run())
