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


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    # Update v8 remote tracking branches.
    self.GitFetchOrigin()


class DetectLastPush(Step):
  MESSAGE = "Detect commit ID of last release."

  def RunStep(self):
    # The revision that should be rolled.
    self["last_push"] = self._options.last_push or self.GetLatestRelease()
    self["push_title"] = self.GitLog(n=1, format="%s",
                                     git_hash=self["last_push"])

    # The master revision this release is based on.
    self["push_base"] = self.GetLatestReleaseBase()

    # FIXME(machenbach): Manually specifying a revision doesn't work at the
    # moment. Needs more complicated logic to find the correct push_base above.
    # Maybe delete that parameter entirely?
    assert not self._options.last_push

    # Determine the master revision of the last roll.
    version = self.GetVersionTag(self._options.last_roll)
    assert version
    self["last_rolled_base"] = self.GetLatestReleaseBase(version=version)
    assert self["last_rolled_base"]


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."

  def RunStep(self):
    self["v8_path"] = os.getcwd()
    cwd = self._options.chromium
    os.chdir(cwd)
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
    self.GitCheckout("master", cwd=self._options.chromium)
    self.Command("gclient", "sync --nohooks", cwd=self._options.chromium)
    self.GitPull(cwd=self._options.chromium)

    # Update v8 remotes.
    self.GitFetchOrigin()

    self.GitCreateBranch("v8-roll-%s" % self["last_push"],
                         cwd=self._options.chromium)


class UploadCL(Step):
  MESSAGE = "Create and upload CL."

  def RunStep(self):
    # Patch DEPS file.
    if self.Command(
        "roll-dep", "v8 %s" % self["last_push"],
        cwd=self._options.chromium) is None:
      self.Die("Failed to create deps for %s" % self["last_push"])

    message = []
    message.append("Update V8 to %s." % self["push_title"].lower())

    message.append(
        ROLL_SUMMARY % (self["last_rolled_base"][:8], self["push_base"][:8]))

    if self["sheriff"]:
      message.append("Please reply to the V8 sheriff %s in case of problems."
          % self["sheriff"])
    message.append("TBR=%s" % self._options.reviewer)
    self.GitCommit("\n\n".join(message),
                   author=self._options.author,
                   cwd=self._options.chromium)
    if not self._options.dry_run:
      self.GitUpload(author=self._options.author,
                     force=True,
                     cq=self._options.use_commit_queue,
                     cwd=self._options.chromium)
      print "CL uploaded."
    else:
      self.GitCheckout("master", cwd=self._options.chromium)
      self.GitDeleteBranch("v8-roll-%s" % self["last_push"],
                           cwd=self._options.chromium)
      print "Dry run - don't upload."


# TODO(machenbach): Make this obsolete. We are only in the chromium chechout
# for the initial .git check.
class SwitchV8(Step):
  MESSAGE = "Returning to V8 checkout."

  def RunStep(self):
    os.chdir(self["v8_path"])


class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    print("Congratulations, you have successfully rolled %s into "
          "Chromium. Please don't forget to update the v8rel spreadsheet."
          % self["last_push"])

    # Clean up all temporary files.
    Command("rm", "-f %s*" % self._config["PERSISTFILE_BASENAME"])


class ChromiumRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("-l", "--last-push",
                        help="The git commit ID of the last candidates push.")
    parser.add_argument("--last-roll", required=True,
                        help="The git commit ID of the last rolled version.")
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
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/v8-chromium-roll-tempfile",
    }

  def _Steps(self):
    return [
      Preparation,
      DetectLastPush,
      DetermineV8Sheriff,
      SwitchChromium,
      UpdateChromiumCheckout,
      UploadCL,
      SwitchV8,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(ChromiumRoll().Run())
