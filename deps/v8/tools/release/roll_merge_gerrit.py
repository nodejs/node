#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import re
import sys
import time
import urllib.parse

# Add depot tools to the sys path, for gerrit_util
sys.path.append(
    os.path.abspath(
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            '../../third_party/depot_tools')))

import gerrit_util

from common_includes import VERSION_FILE

GERRIT_HOST = 'chromium-review.googlesource.com'


def ExtractVersion(include_file_text):
  version = {}
  for line in include_file_text.split('\n'):

    def ReadAndPersist(var_name, def_name):
      match = re.match(r"^#define %s\s+(\d*)" % def_name, line)
      if match:
        value = match.group(1)
        version[var_name] = int(value)

    for (var_name, def_name) in [("major", "V8_MAJOR_VERSION"),
                                 ("minor", "V8_MINOR_VERSION"),
                                 ("build", "V8_BUILD_NUMBER"),
                                 ("patch", "V8_PATCH_LEVEL")]:
      ReadAndPersist(var_name, def_name)
  return version


def main():
  parser = argparse.ArgumentParser(
      description="Use the gerrit API to cherry-pick a revision")
  parser.add_argument(
      "-a",
      "--author",
      default="",
      help="The author email used for code review.")

  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument("--branch", help="The branch to merge to (e.g. 10.3.171)")
  # TODO(leszeks): Add support for more than one revision. This will need to
  # cherry pick one of them using the gerrit API, then somehow applying the rest
  # onto it as additional patches.
  parser.add_argument("revision", nargs=1, help="The revision to merge.")

  options = parser.parse_args()

  # Get the original commit.
  revision = options.revision[0]
  if not re.match(r"[0-9]+\.[0-9]+\.[0-9]+", options.branch):
    print("Branch is not of the form 1.2.3")
    exit(1)
  print("Cherry-picking %s onto %s" % (revision, options.branch))

  # Create a cherry pick commit from the original commit.
  cherry_pick = gerrit_util.CherryPick(GERRIT_HOST, revision, options.branch)
  # Use the cherry pick number to refer to it, rather than the 'id', because
  # cherry picks end up having the same Change-Id as the original CL.
  cherry_pick_id = cherry_pick['_number']
  print("Created cherry-pick: https://%s/c/%s" %
        (GERRIT_HOST, cherry_pick['_number']))

  print("Extracting target version...")
  # Get the version out of the cherry-picked commit's v8-version.h
  include_file_text = gerrit_util.GetFileContents(GERRIT_HOST, cherry_pick_id,
                                                  VERSION_FILE).decode('utf-8')
  version = ExtractVersion(include_file_text)
  print("... version is %s" % str(version))

  new_patch = version['patch'] + 1
  version_string = "%d.%d.%d.%d" % (version["major"], version["minor"],
                                    version["build"], new_patch)

  # Add the 'roll_merge' hashtag so that Rubberstamper knows that this is a
  # benign cherry pick.
  print("Setting 'roll-merge' hashtag...")
  gerrit_util.CallGerritApi(
      GERRIT_HOST,
      'changes/%s/hashtags' % cherry_pick_id,
      reqtype='POST',
      body={'add': ["roll-merge"]})

  # Increment the patch number in v8-version.h
  print("Updating %s to patch level %d..." % (VERSION_FILE, new_patch))
  include_file_text = re.sub(
      r"(?<=#define V8_PATCH_LEVEL)(?P<space>\s+)\d*$",
      r"\g<space>%d" % new_patch,
      include_file_text,
      flags=re.MULTILINE)
  gerrit_util.ChangeEdit(GERRIT_HOST, cherry_pick_id, VERSION_FILE,
                         include_file_text)

  # Create the commit message, using the new version and information about the
  # original commit.
  print("Updating commit message...")
  original_commit = gerrit_util.GetChangeCommit(GERRIT_HOST, revision)
  commit_msg = "\n".join([
      "Version %s (cherry-pick)" % version_string,  #
      "",  #
      "Merged %s" % original_commit['commit'],  #
      "",  #
      "%s" % original_commit['subject'],  #
      "",  #
      "Change-Id: %s" % cherry_pick['change_id'],  #
  ])
  gerrit_util.SetChangeEditMessage(GERRIT_HOST, cherry_pick_id, commit_msg)

  # Publish the change edit with the v8-version.h and commit message changes.
  print("Publishing changes...")
  gerrit_util.PublishChangeEdit(GERRIT_HOST, cherry_pick_id)

  # Set Owners-Override +1
  print("Setting 'Owners-Override +1'...")
  try:
    gerrit_util.SetReview(
        GERRIT_HOST, cherry_pick_id, labels={"Owners-Override": 1})
  except:
    logging.WARNING("Could not set Owners-Override +1")

  print("Adding Rubber Stamper as a reviewer...")
  gerrit_util.AddReviewers(
      GERRIT_HOST,
      cherry_pick_id,
      reviewers=['rubber-stamper@appspot.gserviceaccount.com'])

  print("Cherry-pick %s created successfully: https://%s/c/%s" %
        (version_string, GERRIT_HOST, cherry_pick['_number']))

  print("Waiting for Code-Review +1 or Bot-Commit +1...")
  while True:
    cherry_pick_review = gerrit_util.CallGerritApi(
        GERRIT_HOST,
        'changes/%s/revisions/current/review' % cherry_pick_id,
        reqtype='GET')
    if any(
        cr_label.get('value', None) == 1
        for cr_label in cherry_pick_review['labels']['Code-Review']['all']):
      break
    if any(
        cr_label.get('value', None) == 1
        for cr_label in cherry_pick_review['labels']['Bot-Commit']['all']):
      break
    time.sleep(5)

  print("Submitting...")
  cherry_pick = gerrit_util.SubmitChange(GERRIT_HOST, cherry_pick_id)
  assert cherry_pick['status'] == 'MERGED'

  cherry_pick_commit = gerrit_util.GetChangeCommit(GERRIT_HOST, cherry_pick_id,
                                                   'current')
  print("Found committed as %s..." % cherry_pick_commit['commit'])

  print("Setting %s tag..." % version_string)
  gerrit_util.CreateGerritTag(GERRIT_HOST,
                              urllib.parse.quote_plus(cherry_pick["project"]),
                              version_string, cherry_pick_commit['commit'])

  print("Done.")


if __name__ == "__main__":  # pragma: no cover
  sys.exit(main())
