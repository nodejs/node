#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

from common_includes import *

ROLL_SUMMARY = ("Summary of changes available at:\n"
                "https://chromium.googlesource.com/v8/v8/+log/%s..%s")

ISSUE_MSG = (
"""Please follow these instructions for assigning/CC'ing issues:
https://code.google.com/p/v8-wiki/wiki/TriagingIssues

Please close rolling in case of a roll revert:
https://v8-roll.appspot.com/""")

class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    # Update v8 remote tracking branches.
    self.GitFetchOrigin()
    self.Git("fetch origin +refs/tags/*:refs/tags/*")


class PrepareRollCandidate(Step):
  MESSAGE = "Robustness checks of the roll candidate."

  def RunStep(self):
    self["roll_title"] = self.GitLog(n=1, format="%s",
                                     git_hash=self._options.roll)

    # Make sure the last roll and the roll candidate are releases.
    version = self.GetVersionTag(self._options.roll)
    assert version, "The revision to roll is not tagged."
    version = self.GetVersionTag(self._options.last_roll)
    assert version, "The revision used as last roll is not tagged."


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."

  def RunStep(self):
    cwd = self._options.chromium
    self.InitialEnvironmentChecks(cwd)
    # Check for a clean workdir.
    if not self.GitIsWorkdirClean(cwd=cwd):  # pragma: no cover
      self.Die("Workspace is not clean. Please commit or undo your changes.")
    # Assert that the DEPS file is there.
    if not os.path.exists(os.path.join(cwd, "DEPS")):  # pragma: no cover
      self.Die("DEPS file not present.")


class UpdateChromiumCheckout(Step):
  MESSAGE = "Update the checkout and create a new branch."

  def RunStep(self):
    cwd = self._options.chromium
    self.GitCheckout("master", cwd=cwd)
    self.DeleteBranch("work-branch", cwd=cwd)
    self.Command("gclient", "sync --nohooks", cwd=cwd)
    self.GitPull(cwd=cwd)

    # Update v8 remotes.
    self.GitFetchOrigin()

    self.GitCreateBranch("work-branch", cwd=cwd)


class UploadCL(Step):
  MESSAGE = "Create and upload CL."

  def RunStep(self):
    cwd = self._options.chromium
    # Patch DEPS file.
    if self.Command("roll-dep-svn", "v8 %s" %
                    self._options.roll, cwd=cwd) is None:
      self.Die("Failed to create deps for %s" % self._options.roll)

    message = []
    message.append("Update V8 to %s." % self["roll_title"].lower())

    message.append(
        ROLL_SUMMARY % (self._options.last_roll[:8], self._options.roll[:8]))

    message.append(ISSUE_MSG)

    message.append("TBR=%s" % self._options.reviewer)
    self.GitCommit("\n\n".join(message),  author=self._options.author, cwd=cwd)
    if not self._options.dry_run:
      self.GitUpload(author=self._options.author,
                     force=True,
                     cq=self._options.use_commit_queue,
                     cwd=cwd)
      print "CL uploaded."
    else:
      print "Dry run - don't upload."

    self.GitCheckout("master", cwd=cwd)
    self.GitDeleteBranch("work-branch", cwd=cwd)

class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    print("Congratulations, you have successfully rolled %s into "
          "Chromium."
          % self._options.roll)

    # Clean up all temporary files.
    Command("rm", "-f %s*" % self._config["PERSISTFILE_BASENAME"])


class ChromiumRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--last-roll", required=True,
                        help="The git commit ID of the last rolled version.")
    parser.add_argument("roll", nargs=1, help="Revision to roll."),
    parser.add_argument("--use-commit-queue",
                        help="Check the CQ bit on upload.",
                        default=False, action="store_true")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.author or not options.reviewer:
      print "A reviewer (-r) and an author (-a) are required."
      return False

    options.requires_editor = False
    options.force = True
    options.manual = False
    options.roll = options.roll[0]
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/v8-chromium-roll-tempfile",
    }

  def _Steps(self):
    return [
      Preparation,
      PrepareRollCandidate,
      DetermineV8Sheriff,
      SwitchChromium,
      UpdateChromiumCheckout,
      UploadCL,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(ChromiumRoll().Run())
