#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from common_includes import *

ROLL_SUMMARY = ("Summary of changes available at:\n"
                "https://chromium.googlesource.com/v8/v8/+log/%s..%s")

ISSUE_MSG = ("""Please follow these instructions for assigning/CC'ing issues:
https://v8.dev/docs/triage-issues

Please close rolling in case of a roll revert:
https://v8-roll.appspot.com/
This only works with a Google account.

CQ_INCLUDE_TRYBOTS=luci.chromium.try:linux-blink-rel
CQ_INCLUDE_TRYBOTS=luci.chromium.try:linux_optional_gpu_tests_rel
CQ_INCLUDE_TRYBOTS=luci.chromium.try:mac_optional_gpu_tests_rel
CQ_INCLUDE_TRYBOTS=luci.chromium.try:win_optional_gpu_tests_rel
CQ_INCLUDE_TRYBOTS=luci.chromium.try:android_optional_gpu_tests_rel
CQ_INCLUDE_TRYBOTS=luci.chromium.try:dawn-linux-x64-deps-rel""")

REF_LINE_PATTERN = r"refs\/tags\/(\d+(?:\.\d+){2,3})-pgo\ ([0-9a-f]{40})"

class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    # Update v8 remote tracking branches.
    self.GitFetchOrigin()
    self.Git("fetch origin +refs/tags/*:refs/tags/*")


class PrepareRollCandidate(Step):
  MESSAGE = "Robustness checks of the roll candidate."

  def RunStep(self):
    self["roll_title"] = self.GitLog(
        n=1, format="%s", git_hash=self._options.revision)

    # Make sure the last roll and the roll candidate are releases.
    version = self.GetVersionTag(self._options.revision)
    assert version, "The revision to roll is not tagged."
    version = self.GetVersionTag(self._options.last_roll)
    assert version, "The revision used as last roll is not tagged."


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."

  def RunStep(self):
    cwd = self._options.chromium
    self.InitialEnvironmentChecks(cwd)
    # Assert that the DEPS file is there.
    if not os.path.exists(os.path.join(cwd, "DEPS")):  # pragma: no cover
      self.Die("DEPS file not present.")


class ChromiumCreateBranch(Step):
  MESSAGE = "Create a new branch."

  def RunStep(self):
    cwd = self._options.chromium
    self.GitCheckout("main", cwd=cwd)
    self.DeleteBranch("work-branch", cwd=cwd)
    self.GitCreateBranch("work-branch", cwd=cwd)


class UploadCL(Step):
  MESSAGE = "Create and upload CL."

  def RunStep(self):
    cwd = self._options.chromium
    # Patch DEPS file.
    if self.Command("gclient", "setdep -r src/v8@%s" %
                    self._options.revision, cwd=cwd) is None:
      self.Die("Failed to create deps for %s" % self._options.revision)
    self.GitAdd('DEPS', cwd=cwd)

    message = []
    message.append("Update V8 to %s." % self["roll_title"].lower())

    message.append(
        ROLL_SUMMARY % (self._options.last_roll[:8],
                        self._options.revision[:8]))

    message.append(ISSUE_MSG)

    message.append("R=%s" % self._options.reviewer)
    self.GitCommit(
        "\n\n".join(message),
        author=self._options.author,
        prefix=["-c", "diff.ignoreSubmodules=all"],
        cwd=cwd)
    if not self._options.dry_run:
      self.GitUpload(force=True,
                     bypass_hooks=True,
                     cq=self._options.use_commit_queue,
                     cq_dry_run=self._options.use_dry_run,
                     set_bot_commit=True,
                     cwd=cwd)
      print("CL uploaded.")
    else:
      print("Dry run - don't upload.")

    self.GitCheckout("main", cwd=cwd)
    self.GitDeleteBranch("work-branch", cwd=cwd)

class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    print("Congratulations, you have successfully rolled %s into "
          "Chromium." % self._options.revision)

    # Clean up all temporary files.
    Command("rm", "-f %s*" % self._config["PERSISTFILE_BASENAME"])


class AutoRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--last-roll",
                        help="The git commit ID of the last rolled version.")
    parser.add_argument("--revision", help="Revision to roll."),
    parser.add_argument("--roll", help="Deprecated.",
                        default=True, action="store_true")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--use-commit-queue",
                       help="Trigger the CQ full run on upload.",
                       default=False, action="store_true")
    group.add_argument("--use-dry-run",
                       help="Trigger the CQ dry run on upload.",
                       default=True, action="store_true")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.author or not options.reviewer:
      print("A reviewer (-r) and an author (-a) are required.")
      return False

    if not options.last_roll or not options.revision:
      print("Options --last-roll and --revision are required.")
      return False

    options.requires_editor = False
    options.force = True
    options.manual = False
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/v8-chromium-roll-tempfile",
    }

  def _Steps(self):
    return [
      Preparation,
      PrepareRollCandidate,
      SwitchChromium,
      ChromiumCreateBranch,
      UploadCL,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoRoll().Run())
