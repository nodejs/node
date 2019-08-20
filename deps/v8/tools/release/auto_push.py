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

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import json
import os
import re
import sys
import urllib

from common_includes import *
import create_release


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    # Fetch unfetched revisions.
    self.vc.Fetch()


class FetchCandidate(Step):
  MESSAGE = "Fetching V8 lkgr ref."

  def RunStep(self):
    # The roll ref points to the candidate to be rolled.
    self.Git("fetch origin +refs/heads/lkgr:refs/heads/lkgr")
    self["candidate"] = self.Git("show-ref -s refs/heads/lkgr").strip()


class LastReleaseBailout(Step):
  MESSAGE = "Checking last V8 release base."

  def RunStep(self):
    last_release = self.GetLatestReleaseBase()
    commits = self.GitLog(
        format="%H", git_hash="%s..%s" % (last_release, self["candidate"]))

    if not commits:
      print("Already pushed current candidate %s" % self["candidate"])
      return True


class CreateRelease(Step):
  MESSAGE = "Creating release if specified."

  def RunStep(self):
    print("Creating release for %s." % self["candidate"])

    args = [
      "--author", self._options.author,
      "--reviewer", self._options.reviewer,
      "--revision", self["candidate"],
      "--force",
    ]

    if self._options.work_dir:
      args.extend(["--work-dir", self._options.work_dir])

    if self._options.push:
      self._side_effect_handler.Call(
          create_release.CreateRelease().Run, args)


class AutoPush(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-p", "--push",
                        help="Create release. Dry run if unspecified.",
                        default=False, action="store_true")

  def _ProcessOptions(self, options):
    if not options.author or not options.reviewer:  # pragma: no cover
      print("You need to specify author and reviewer.")
      return False
    options.requires_editor = False
    return True

  def _Config(self):
    return {
      "PERSISTFILE_BASENAME": "/tmp/v8-auto-push-tempfile",
    }

  def _Steps(self):
    return [
      Preparation,
      FetchCandidate,
      LastReleaseBailout,
      CreateRelease,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoPush().Run())
