#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

from common_includes import *

DEPS_FILE = "DEPS_FILE"
CHROMIUM = "CHROMIUM"

CONFIG = {
  PERSISTFILE_BASENAME: "/tmp/v8-chromium-roll-tempfile",
  DOT_GIT_LOCATION: ".git",
  DEPS_FILE: "DEPS",
}


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.CommonPrepare()


class DetectLastPush(Step):
  MESSAGE = "Detect commit ID of last push to trunk."

  def RunStep(self):
    self["last_push"] = self._options.last_push or self.FindLastTrunkPush()
    self["trunk_revision"] = self.GitSVNFindSVNRev(self["last_push"])
    self["push_title"] = self.GitLog(n=1, format="%s",
                                     git_hash=self["last_push"])


class CheckChromium(Step):
  MESSAGE = "Ask for chromium checkout."

  def Run(self):
    self["chrome_path"] = self._options.chromium
    while not self["chrome_path"]:
      self.DieNoManualMode("Please specify the path to a Chromium checkout in "
                           "forced mode.")
      print ("Please specify the path to the chromium \"src\" directory: "),
      self["chrome_path"] = self.ReadLine()


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."
  REQUIRES = "chrome_path"

  def RunStep(self):
    self["v8_path"] = os.getcwd()
    os.chdir(self["chrome_path"])
    self.InitialEnvironmentChecks()
    # Check for a clean workdir.
    if not self.GitIsWorkdirClean():  # pragma: no cover
      self.Die("Workspace is not clean. Please commit or undo your changes.")
    # Assert that the DEPS file is there.
    if not os.path.exists(self.Config(DEPS_FILE)):  # pragma: no cover
      self.Die("DEPS file not present.")


class UpdateChromiumCheckout(Step):
  MESSAGE = "Update the checkout and create a new branch."
  REQUIRES = "chrome_path"

  def RunStep(self):
    os.chdir(self["chrome_path"])
    self.GitCheckout("master")
    self.GitPull()
    self.GitCreateBranch("v8-roll-%s" % self["trunk_revision"])


class UploadCL(Step):
  MESSAGE = "Create and upload CL."
  REQUIRES = "chrome_path"

  def RunStep(self):
    os.chdir(self["chrome_path"])

    # Patch DEPS file.
    deps = FileToText(self.Config(DEPS_FILE))
    deps = re.sub("(?<=\"v8_revision\": \")([0-9]+)(?=\")",
                  self["trunk_revision"],
                  deps)
    TextToFile(deps, self.Config(DEPS_FILE))

    if self._options.reviewer:
      print "Using account %s for review." % self._options.reviewer
      rev = self._options.reviewer
    else:
      print "Please enter the email address of a reviewer for the roll CL: ",
      self.DieNoManualMode("A reviewer must be specified in forced mode.")
      rev = self.ReadLine()

    commit_title = "Update V8 to %s." % self["push_title"].lower()
    self.GitCommit("%s\n\nTBR=%s" % (commit_title, rev))
    self.GitUpload(author=self._options.author,
                   force=self._options.force_upload)
    print "CL uploaded."


class SwitchV8(Step):
  MESSAGE = "Returning to V8 checkout."
  REQUIRES = "chrome_path"

  def RunStep(self):
    os.chdir(self["v8_path"])


class CleanUp(Step):
  MESSAGE = "Done!"

  def RunStep(self):
    print("Congratulations, you have successfully rolled the push r%s it into "
          "Chromium. Please don't forget to update the v8rel spreadsheet."
          % self["trunk_revision"])

    # Clean up all temporary files.
    Command("rm", "-f %s*" % self._config[PERSISTFILE_BASENAME])


class ChromiumRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-f", "--force",
                      help="Don't prompt the user.",
                      default=False, action="store_true")
    group.add_argument("-m", "--manual",
                      help="Prompt the user at every important step.",
                      default=False, action="store_true")
    parser.add_argument("-c", "--chromium",
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("-l", "--last-push",
                        help="The git commit ID of the last push to trunk.")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.manual and not options.reviewer:
      print "A reviewer (-r) is required in (semi-)automatic mode."
      return False
    if not options.manual and not options.chromium:
      print "A chromium checkout (-c) is required in (semi-)automatic mode."
      return False
    if not options.manual and not options.author:
      print "Specify your chromium.org email with -a in (semi-)automatic mode."
      return False

    options.tbr_commit = not options.manual
    return True

  def _Steps(self):
    return [
      Preparation,
      DetectLastPush,
      CheckChromium,
      SwitchChromium,
      UpdateChromiumCheckout,
      UploadCL,
      SwitchV8,
      CleanUp,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(ChromiumRoll(CONFIG).Run())
