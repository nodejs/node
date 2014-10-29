#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Script for auto-increasing the version on bleeding_edge.

The script can be run regularly by a cron job. It will increase the build
level of the version on bleeding_edge if:
- the lkgr version is smaller than the version of the latest revision,
- the lkgr version is not a version change itself,
- the tree is not closed for maintenance.

The new version will be the maximum of the bleeding_edge and trunk versions +1.
E.g. latest bleeding_edge version: 3.22.11.0 and latest trunk 3.23.0.0 gives
the new version 3.23.1.0.

This script requires a depot tools git checkout. I.e. 'fetch v8'.
"""

import argparse
import os
import sys

from common_includes import *

VERSION_BRANCH = "auto-bump-up-version"


# TODO(machenbach): Add vc interface that works on git mirror.
class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    # TODO(machenbach): Remove after the git switch.
    if(self.Config("PERSISTFILE_BASENAME") ==
       "/tmp/v8-bump-up-version-tempfile"):
      print "This script is disabled until after the v8 git migration."
      return True

    # Check for a clean workdir.
    if not self.GitIsWorkdirClean():  # pragma: no cover
      # This is in case a developer runs this script on a dirty tree.
      self.GitStash()

    self.GitCheckout("master")

    self.GitPull()

    # Ensure a clean version branch.
    self.DeleteBranch(VERSION_BRANCH)


class GetCurrentBleedingEdgeVersion(Step):
  MESSAGE = "Get latest bleeding edge version."

  def RunStep(self):
    self.GitCheckout("master")

    # Store latest version and revision.
    self.ReadAndPersistVersion()
    self["latest_version"] = self.ArrayToVersion("")
    self["latest"] = self.GitLog(n=1, format="%H")
    print "Bleeding edge version: %s" % self["latest_version"]


# This step is pure paranoia. It forbids the script to continue if the last
# commit changed version.cc. Just in case the other bailout has a bug, this
# prevents the script from continuously commiting version changes.
class LastChangeBailout(Step):
  MESSAGE = "Stop script if the last change modified the version."

  def RunStep(self):
    if VERSION_FILE in self.GitChangedFiles(self["latest"]):
      print "Stop due to recent version change."
      return True


# TODO(machenbach): Implement this for git.
class FetchLKGR(Step):
  MESSAGE = "Fetching V8 LKGR."

  def RunStep(self):
    lkgr_url = "https://v8-status.appspot.com/lkgr"
    self["lkgr_svn"] = self.ReadURL(lkgr_url, wait_plan=[5])


# TODO(machenbach): Implement this for git. With a git lkgr we could simply
# checkout that revision. With svn, we have to search backwards until that
# revision is found.
class GetLKGRVersion(Step):
  MESSAGE = "Get bleeding edge lkgr version."

  def RunStep(self):
    self.GitCheckout("master")
    # If the commit was made from svn, there is a mapping entry in the commit
    # message.
    self["lkgr"] = self.GitLog(
        grep="^git-svn-id: [^@]*@%s [A-Za-z0-9-]*$" % self["lkgr_svn"],
        format="%H")

    # FIXME(machenbach): http://crbug.com/391712 can lead to svn lkgrs on the
    # trunk branch (rarely).
    if not self["lkgr"]:  # pragma: no cover
      self.Die("No git hash found for svn lkgr.")

    self.GitCreateBranch(VERSION_BRANCH, self["lkgr"])
    self.ReadAndPersistVersion("lkgr_")
    self["lkgr_version"] = self.ArrayToVersion("lkgr_")
    print "LKGR version: %s" % self["lkgr_version"]

    # Ensure a clean version branch.
    self.GitCheckout("master")
    self.DeleteBranch(VERSION_BRANCH)


class LKGRVersionUpToDateBailout(Step):
  MESSAGE = "Stop script if the lkgr has a renewed version."

  def RunStep(self):
    # If a version-change commit becomes the lkgr, don't bump up the version
    # again.
    if VERSION_FILE in self.GitChangedFiles(self["lkgr"]):
      print "Stop because the lkgr is a version change itself."
      return True

    # Don't bump up the version if it got updated already after the lkgr.
    if SortingKey(self["lkgr_version"]) < SortingKey(self["latest_version"]):
      print("Stop because the latest version already changed since the lkgr "
            "version.")
      return True


class GetTrunkVersion(Step):
  MESSAGE = "Get latest trunk version."

  def RunStep(self):
    self.GitCheckout("candidates")
    self.GitPull()
    self.ReadAndPersistVersion("trunk_")
    self["trunk_version"] = self.ArrayToVersion("trunk_")
    print "Trunk version: %s" % self["trunk_version"]


class CalculateVersion(Step):
  MESSAGE = "Calculate the new version."

  def RunStep(self):
    if self["lkgr_build"] == "9999":  # pragma: no cover
      # If version control on bleeding edge was switched off, just use the last
      # trunk version.
      self["lkgr_version"] = self["trunk_version"]

    # The new version needs to be greater than the max on bleeding edge and
    # trunk.
    max_version = max(self["trunk_version"],
                      self["lkgr_version"],
                      key=SortingKey)

    # Strip off possible leading zeros.
    self["new_major"], self["new_minor"], self["new_build"], _ = (
        map(str, map(int, max_version.split("."))))

    self["new_build"] = str(int(self["new_build"]) + 1)
    self["new_patch"] = "0"

    self["new_version"] = ("%s.%s.%s.0" %
        (self["new_major"], self["new_minor"], self["new_build"]))
    print "New version is %s" % self["new_version"]

    if self._options.dry_run:  # pragma: no cover
      print "Dry run, skipping version change."
      return True


class CheckTreeStatus(Step):
  MESSAGE = "Checking v8 tree status message."

  def RunStep(self):
    status_url = "https://v8-status.appspot.com/current?format=json"
    status_json = self.ReadURL(status_url, wait_plan=[5, 20, 300, 300])
    message = json.loads(status_json)["message"]
    if re.search(r"maintenance|no commits", message, flags=re.I):
      print "Skip version change by tree status: \"%s\"" % message
      return True


class ChangeVersion(Step):
  MESSAGE = "Bump up the version."

  def RunStep(self):
    self.GitCreateBranch(VERSION_BRANCH, "master")

    self.SetVersion(os.path.join(self.default_cwd, VERSION_FILE), "new_")

    try:
      msg = "[Auto-roll] Bump up version to %s" % self["new_version"]
      self.GitCommit("%s\n\nTBR=%s" % (msg, self._options.author),
                     author=self._options.author)
      if self._options.svn:
        self.SVNCommit("branches/bleeding_edge", msg)
      else:
        self.GitUpload(author=self._options.author,
                       force=self._options.force_upload,
                       bypass_hooks=True)
        self.GitDCommit()
      print "Successfully changed the version."
    finally:
      # Clean up.
      self.GitCheckout("master")
      self.DeleteBranch(VERSION_BRANCH)


class BumpUpVersion(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("--dry_run", help="Don't commit the new version.",
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
      "PERSISTFILE_BASENAME": "/tmp/v8-bump-up-version-tempfile",
      "PATCH_FILE": "/tmp/v8-bump-up-version-tempfile-patch-file",
    }

  def _Steps(self):
    return [
      Preparation,
      GetCurrentBleedingEdgeVersion,
      LastChangeBailout,
      FetchLKGR,
      GetLKGRVersion,
      LKGRVersionUpToDateBailout,
      GetTrunkVersion,
      CalculateVersion,
      CheckTreeStatus,
      ChangeVersion,
    ]

if __name__ == "__main__":  # pragma: no cover
  sys.exit(BumpUpVersion().Run())
