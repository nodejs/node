#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys
import urllib

from common_includes import *
import chromium_roll


class CheckActiveRoll(Step):
  MESSAGE = "Check active roll."

  @staticmethod
  def ContainsChromiumRoll(changes):
    for change in changes:
      if change["subject"].startswith("Update V8 to"):
        return True
    return False

  def RunStep(self):
    params = {
      "closed": 3,
      "owner": self._options.author,
      "limit": 30,
      "format": "json",
    }
    params = urllib.urlencode(params)
    search_url = "https://codereview.chromium.org/search"
    result = self.ReadURL(search_url, params, wait_plan=[5, 20])
    if self.ContainsChromiumRoll(json.loads(result)["results"]):
      print "Stop due to existing Chromium roll."
      return True


class DetectLastPush(Step):
  MESSAGE = "Detect commit ID of the last push to trunk."

  def RunStep(self):
    push_hash = self.FindLastTrunkPush(
        branch="origin/candidates", include_patches=True)
    self["last_push"] = self.GetCommitPositionNumber(push_hash)


class DetectLastRoll(Step):
  MESSAGE = "Detect commit ID of the last Chromium roll."

  def RunStep(self):
    # Interpret the DEPS file to retrieve the v8 revision.
    # TODO(machenbach): This should be part or the roll-deps api of
    # depot_tools.
    Var = lambda var: '%s'
    exec(FileToText(os.path.join(self._options.chromium, "DEPS")))
    last_roll = self.GetCommitPositionNumber(vars['v8_revision'])
    # FIXME(machenbach): When rolling from bleeding edge and from trunk there
    # be different commit numbers here. Better use version?
    if int(last_roll) >= int(self["last_push"]):
      print("There is no newer v8 revision than the one in Chromium (%s)."
            % last_roll)
      return True


class CheckClusterFuzz(Step):
  MESSAGE = "Check ClusterFuzz api for new problems."

  def RunStep(self):
    if not os.path.exists(self.Config("CLUSTERFUZZ_API_KEY_FILE")):
      print "Skipping ClusterFuzz check. No api key file found."
      return False
    api_key = FileToText(self.Config("CLUSTERFUZZ_API_KEY_FILE"))
    # Check for open, reproducible issues that have no associated bug.
    result = self._side_effect_handler.ReadClusterFuzzAPI(
        api_key, job_type="linux_asan_d8_dbg", reproducible="True",
        open="True", bug_information="",
        revision_greater_or_equal=str(self["last_push"]))
    if result:
      print "Stop due to pending ClusterFuzz issues."
      return True


class RollChromium(Step):
  MESSAGE = "Roll V8 into Chromium."

  def RunStep(self):
    if self._options.roll:
      args = [
        "--author", self._options.author,
        "--reviewer", self._options.reviewer,
        "--chromium", self._options.chromium,
        "--use-commit-queue",
      ]
      if self._options.sheriff:
        args.extend([
            "--sheriff", "--googlers-mapping", self._options.googlers_mapping])
      if self._options.dry_run:
        args.extend(["--dry-run"])
      self._side_effect_handler.Call(chromium_roll.ChromiumRoll().Run, args)


class AutoRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--roll", help="Call Chromium roll script.",
                        default=False, action="store_true")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.reviewer:
      print "A reviewer (-r) is required."
      return False
    if not options.author:
      print "An author (-a) is required."
      return False
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/v8-auto-roll-tempfile",
      "CLUSTERFUZZ_API_KEY_FILE": ".cf_api_key",
    }

  def _Steps(self):
    return [
      CheckActiveRoll,
      DetectLastPush,
      DetectLastRoll,
      CheckClusterFuzz,
      RollChromium,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoRoll().Run())
