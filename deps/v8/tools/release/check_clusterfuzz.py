#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Script to check for new clusterfuzz issues since the last rolled v8 revision.

Returns a json list with test case IDs if any.

Security considerations: The security key and request data must never be
written to public logs. Public automated callers of this script should
suppress stdout and stderr and only process contents of the results_file.
"""

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import httplib
import json
import os
import re
import sys
import urllib
import urllib2


# Constants to git repos.
BASE_URL = "https://chromium.googlesource.com"
DEPS_LOG = BASE_URL + "/chromium/src/+log/master/DEPS?format=JSON"

# Constants for retrieving v8 rolls.
CRREV = "https://cr-rev.appspot.com/_ah/api/crrev/v1/commit/%s"
V8_COMMIT_RE = re.compile(
    r"^Update V8 to version \d+\.\d+\.\d+ \(based on ([a-fA-F0-9]+)\)\..*")

# Constants for the clusterfuzz backend.
HOSTNAME = "backend-dot-cluster-fuzz.appspot.com"

# Crash patterns.
V8_INTERNAL_RE = re.compile(r"^v8::internal.*")
ANY_RE = re.compile(r".*")

# List of all api requests.
BUG_SPECS = [
  {
    "args": {
      "job_type": "linux_asan_chrome_v8",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": V8_INTERNAL_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8_dbg",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8_ignition_dbg",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8_v8_arm_dbg",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8_ignition_v8_arm_dbg",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8_v8_arm64_dbg",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
  {
    "args": {
      "job_type": "linux_asan_d8_v8_mipsel_dbg",
      "reproducible": "True",
      "open": "True",
      "bug_information": "",
    },
    "crash_state": ANY_RE,
  },
]


def GetRequest(url):
  url_fh = urllib2.urlopen(url, None, 60)
  try:
    return url_fh.read()
  finally:
    url_fh.close()


def GetLatestV8InChromium():
  """Returns the commit position number of the latest v8 roll in chromium."""

  # Check currently rolled v8 revision.
  result = GetRequest(DEPS_LOG)
  if not result:
    return None

  # Strip security header and load json.
  commits = json.loads(result[5:])

  git_revision = None
  for commit in commits["log"]:
    # Get latest commit that matches the v8 roll pattern. Ignore cherry-picks.
    match = re.match(V8_COMMIT_RE, commit["message"])
    if match:
      git_revision = match.group(1)
      break
  else:
    return None

  # Get commit position number for v8 revision.
  result = GetRequest(CRREV % git_revision)
  if not result:
    return None

  commit = json.loads(result)
  assert commit["repo"] == "v8/v8"
  return commit["number"]


def APIRequest(key, **params):
  """Send a request to the clusterfuzz api.

  Returns a json dict of the response.
  """

  params["api_key"] = key
  params = urllib.urlencode(params)

  headers = {"Content-type": "application/x-www-form-urlencoded"}

  try:
    conn = httplib.HTTPSConnection(HOSTNAME)
    conn.request("POST", "/_api/", params, headers)

    response = conn.getresponse()

    # Never leak "data" into public logs.
    data = response.read()
  except:
    raise Exception("ERROR: Connection problem.")

  try:
    return json.loads(data)
  except:
    raise Exception("ERROR: Could not read response. Is your key valid?")

  return None


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument("-k", "--key-file", required=True,
                      help="A file with the clusterfuzz api key.")
  parser.add_argument("-r", "--results-file",
                      help="A file to write the results to.")
  options = parser.parse_args()

  # Get api key. The key's content must never be logged.
  assert options.key_file
  with open(options.key_file) as f:
    key = f.read().strip()
  assert key

  revision_number = GetLatestV8InChromium()

  results = []
  for spec in BUG_SPECS:
    args = dict(spec["args"])
    # Use incremented revision as we're interested in all revision greater than
    # what's currently rolled into chromium.
    if revision_number:
      args["revision_greater_or_equal"] = str(int(revision_number) + 1)

    # Never print issue details in public logs.
    issues = APIRequest(key, **args)
    assert issues is not None
    for issue in issues:
      if (re.match(spec["crash_state"], issue["crash_state"]) and
          not issue.get('has_bug_flag')):
        results.append(issue["id"])

  if options.results_file:
    with open(options.results_file, "w") as f:
      f.write(json.dumps(results))
  else:
    print(results)


if __name__ == "__main__":
  sys.exit(Main())
