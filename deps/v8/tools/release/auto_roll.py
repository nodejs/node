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
https://github.com/v8/v8/wiki/Triaging%20issues

Please close rolling in case of a roll revert:
https://v8-roll.appspot.com/
This only works with a Google account.

CQ_INCLUDE_TRYBOTS=master.tryserver.blink:linux_trusty_blink_rel;master.tryserver.chromium.linux:linux_optional_gpu_tests_rel;master.tryserver.chromium.mac:mac_optional_gpu_tests_rel;master.tryserver.chromium.win:win_optional_gpu_tests_rel;master.tryserver.chromium.android:android_optional_gpu_tests_rel""")

class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'preparation'
    # Update v8 remote tracking branches.
    self.GitFetchOrigin()
    self.Git("fetch origin +refs/tags/*:refs/tags/*")


class DetectLastRoll(Step):
  MESSAGE = "Detect commit ID of the last Chromium roll."

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'detect_last_roll'
    self["last_roll"] = self._options.last_roll
    if not self["last_roll"]:
      # Interpret the DEPS file to retrieve the v8 revision.
      # TODO(machenbach): This should be part or the roll-deps api of
      # depot_tools.
      Var = lambda var: '%s'
      exec(FileToText(os.path.join(self._options.chromium, "DEPS")))

      # The revision rolled last.
      self["last_roll"] = vars['v8_revision']
    self["last_version"] = self.GetVersionTag(self["last_roll"])
    assert self["last_version"], "The last rolled v8 revision is not tagged."


class DetectRevisionToRoll(Step):
  MESSAGE = "Detect commit ID of the V8 revision to roll."

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'detect_revision'
    self["roll"] = self._options.revision
    if self["roll"]:
      # If the revision was passed on the cmd line, continue script execution
      # in the next step.
      return False

    # The revision that should be rolled. Check for the latest of the most
    # recent releases based on commit timestamp.
    revisions = self.GetRecentReleases(
        max_age=self._options.max_age * DAY_IN_SECONDS)
    assert revisions, "Didn't find any recent release."

    # There must be some progress between the last roll and the new candidate
    # revision (i.e. we don't go backwards). The revisions are ordered newest
    # to oldest. It is possible that the newest timestamp has no progress
    # compared to the last roll, i.e. if the newest release is a cherry-pick
    # on a release branch. Then we look further.
    for revision in revisions:
      version = self.GetVersionTag(revision)
      assert version, "Internal error. All recent releases should have a tag"

      if SortingKey(self["last_version"]) < SortingKey(version):
        self["roll"] = revision
        break
    else:
      print("There is no newer v8 revision than the one in Chromium (%s)."
            % self["last_roll"])
      self['json_output']['monitoring_state'] = 'up_to_date'
      return True


class PrepareRollCandidate(Step):
  MESSAGE = "Robustness checks of the roll candidate."

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'prepare_candidate'
    self["roll_title"] = self.GitLog(n=1, format="%s",
                                     git_hash=self["roll"])

    # Make sure the last roll and the roll candidate are releases.
    version = self.GetVersionTag(self["roll"])
    assert version, "The revision to roll is not tagged."
    version = self.GetVersionTag(self["last_roll"])
    assert version, "The revision used as last roll is not tagged."


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'switch_chromium'
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
    self['json_output']['monitoring_state'] = 'update_chromium'
    cwd = self._options.chromium
    self.GitCheckout("master", cwd=cwd)
    self.DeleteBranch("work-branch", cwd=cwd)
    self.GitPull(cwd=cwd)

    # Update v8 remotes.
    self.GitFetchOrigin()

    self.GitCreateBranch("work-branch", cwd=cwd)


class UploadCL(Step):
  MESSAGE = "Create and upload CL."

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'upload'
    cwd = self._options.chromium
    # Patch DEPS file.
    if self.Command("roll-dep-svn", "v8 %s" %
                    self["roll"], cwd=cwd) is None:
      self.Die("Failed to create deps for %s" % self["roll"])

    message = []
    message.append("Update V8 to %s." % self["roll_title"].lower())

    message.append(
        ROLL_SUMMARY % (self["last_roll"][:8], self["roll"][:8]))

    message.append(ISSUE_MSG)

    message.append("TBR=%s" % self._options.reviewer)
    self.GitCommit("\n\n".join(message),  author=self._options.author, cwd=cwd)
    if not self._options.dry_run:
      self.GitUpload(author=self._options.author,
                     force=True,
                     bypass_hooks=True,
                     cq=self._options.use_commit_queue,
                     cq_dry_run=self._options.use_dry_run,
                     cwd=cwd)
      print "CL uploaded."
    else:
      print "Dry run - don't upload."

    self.GitCheckout("master", cwd=cwd)
    self.GitDeleteBranch("work-branch", cwd=cwd)

class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    self['json_output']['monitoring_state'] = 'success'
    print("Congratulations, you have successfully rolled %s into "
          "Chromium."
          % self["roll"])

    # Clean up all temporary files.
    Command("rm", "-f %s*" % self._config["PERSISTFILE_BASENAME"])


class AutoRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--last-roll",
                        help="The git commit ID of the last rolled version. "
                             "Auto-detected if not specified.")
    parser.add_argument("--max-age", default=7, type=int,
                        help="Maximum age in days of the latest release.")
    parser.add_argument("--revision",
                        help="Revision to roll. Auto-detected if not "
                             "specified."),
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
      print "A reviewer (-r) and an author (-a) are required."
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
      DetectLastRoll,
      DetectRevisionToRoll,
      PrepareRollCandidate,
      SwitchChromium,
      UpdateChromiumCheckout,
      UploadCL,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoRoll().Run())
