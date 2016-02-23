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


class DetectLastRoll(Step):
  MESSAGE = "Detect commit ID of the last Chromium roll."

  def RunStep(self):
    # The revision that should be rolled. Check for the latest of the most
    # recent releases based on commit timestamp.
    revisions = self.GetRecentReleases(
        max_age=self._options.max_age * DAY_IN_SECONDS)
    assert revisions, "Didn't find any recent release."

    # Interpret the DEPS file to retrieve the v8 revision.
    # TODO(machenbach): This should be part or the roll-deps api of
    # depot_tools.
    Var = lambda var: '%s'
    exec(FileToText(os.path.join(self._options.chromium, "DEPS")))

    # The revision rolled last.
    self["last_roll"] = vars['v8_revision']
    last_version = self.GetVersionTag(self["last_roll"])
    assert last_version, "The last rolled v8 revision is not tagged."

    # There must be some progress between the last roll and the new candidate
    # revision (i.e. we don't go backwards). The revisions are ordered newest
    # to oldest. It is possible that the newest timestamp has no progress
    # compared to the last roll, i.e. if the newest release is a cherry-pick
    # on a release branch. Then we look further.
    for revision in revisions:
      version = self.GetVersionTag(revision)
      assert version, "Internal error. All recent releases should have a tag"

      if SortingKey(last_version) < SortingKey(version):
        self["roll"] = revision
        break
    else:
      print("There is no newer v8 revision than the one in Chromium (%s)."
            % self["last_roll"])
      return True


class RollChromium(Step):
  MESSAGE = "Roll V8 into Chromium."

  def RunStep(self):
    if self._options.roll:
      args = [
        "--author", self._options.author,
        "--reviewer", self._options.reviewer,
        "--chromium", self._options.chromium,
        "--last-roll", self["last_roll"],
        "--use-commit-queue",
        self["roll"],
      ]
      if self._options.sheriff:
        args.append("--sheriff")
      if self._options.dry_run:
        args.append("--dry-run")
      if self._options.work_dir:
        args.extend(["--work-dir", self._options.work_dir])
      self._side_effect_handler.Call(chromium_roll.ChromiumRoll().Run, args)


class AutoRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--max-age", default=3, type=int,
                        help="Maximum age in days of the latest release.")
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
    }

  def _Steps(self):
    return [
      CheckActiveRoll,
      DetectLastRoll,
      RollChromium,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoRoll().Run())
