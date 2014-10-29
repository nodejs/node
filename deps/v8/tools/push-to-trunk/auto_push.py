#!/usr/bin/env python
# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import json
import os
import re
import sys
import urllib

from common_includes import *
import push_to_trunk

PUSH_MESSAGE_RE = re.compile(r".* \(based on bleeding_edge revision r(\d+)\)$")

class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.InitialEnvironmentChecks(self.default_cwd)
    self.CommonPrepare()


class CheckAutoPushSettings(Step):
  MESSAGE = "Checking settings file."

  def RunStep(self):
    settings_file = os.path.realpath(self.Config("SETTINGS_LOCATION"))
    if os.path.exists(settings_file):
      settings_dict = json.loads(FileToText(settings_file))
      if settings_dict.get("enable_auto_roll") is False:
        self.Die("Push to trunk disabled by auto-roll settings file: %s"
                 % settings_file)


class CheckTreeStatus(Step):
  MESSAGE = "Checking v8 tree status message."

  def RunStep(self):
    status_url = "https://v8-status.appspot.com/current?format=json"
    status_json = self.ReadURL(status_url, wait_plan=[5, 20, 300, 300])
    self["tree_message"] = json.loads(status_json)["message"]
    if re.search(r"nopush|no push", self["tree_message"], flags=re.I):
      self.Die("Push to trunk disabled by tree state: %s"
               % self["tree_message"])


class FetchLKGR(Step):
  MESSAGE = "Fetching V8 LKGR."

  def RunStep(self):
    lkgr_url = "https://v8-status.appspot.com/lkgr"
    # Retry several times since app engine might have issues.
    self["lkgr"] = self.ReadURL(lkgr_url, wait_plan=[5, 20, 300, 300])


class CheckLastPush(Step):
  MESSAGE = "Checking last V8 push to trunk."

  def RunStep(self):
    last_push = self.FindLastTrunkPush()

    # Retrieve the bleeding edge revision of the last push from the text in
    # the push commit message.
    last_push_title = self.GitLog(n=1, format="%s", git_hash=last_push)
    last_push_be = PUSH_MESSAGE_RE.match(last_push_title).group(1)

    if not last_push_be:  # pragma: no cover
      self.Die("Could not retrieve bleeding edge revision for trunk push %s"
               % last_push)

    # TODO(machenbach): This metric counts all revisions. It could be
    # improved by counting only the revisions on bleeding_edge.
    if int(self["lkgr"]) - int(last_push_be) < 10:  # pragma: no cover
      # This makes sure the script doesn't push twice in a row when the cron
      # job retries several times.
      self.Die("Last push too recently: %s" % last_push_be)


class PushToTrunk(Step):
  MESSAGE = "Pushing to trunk if specified."

  def RunStep(self):
    print "Pushing lkgr %s to trunk." % self["lkgr"]

    # TODO(machenbach): Update the script before calling it.
    if self._options.push:
      self._side_effect_handler.Call(
          push_to_trunk.PushToTrunk().Run,
          ["--author", self._options.author,
           "--reviewer", self._options.reviewer,
           "--revision", self["lkgr"],
           "--force"])


class AutoPush(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-p", "--push",
                        help="Push to trunk. Dry run if unspecified.",
                        default=False, action="store_true")

  def _ProcessOptions(self, options):
    if not options.author or not options.reviewer:  # pragma: no cover
      print "You need to specify author and reviewer."
      return False
    options.requires_editor = False
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/v8-auto-push-tempfile",
      "SETTINGS_LOCATION": "~/.auto-roll",
    }

  def _Steps(self):
    return [
      Preparation,
      CheckAutoPushSettings,
      CheckTreeStatus,
      FetchLKGR,
      CheckLastPush,
      PushToTrunk,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoPush().Run())
