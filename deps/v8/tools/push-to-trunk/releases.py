#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script retrieves the history of all V8 branches and trunk revisions and
# their corresponding Chromium revisions.

# Requires a chromium checkout with branch heads:
# gclient sync --with_branch_heads
# gclient fetch

import argparse
import csv
import itertools
import json
import os
import re
import sys

from common_includes import *

CONFIG = {
  "BRANCHNAME": "retrieve-v8-releases",
  "PERSISTFILE_BASENAME": "/tmp/v8-releases-tempfile",
}

# Expression for retrieving the bleeding edge revision from a commit message.
PUSH_MSG_SVN_RE = re.compile(r".* \(based on bleeding_edge revision r(\d+)\)$")
PUSH_MSG_GIT_RE = re.compile(r".* \(based on ([a-fA-F0-9]+)\)$")

# Expression for retrieving the merged patches from a merge commit message
# (old and new format).
MERGE_MESSAGE_RE = re.compile(r"^.*[M|m]erged (.+)(\)| into).*$", re.M)

CHERRY_PICK_TITLE_GIT_RE = re.compile(r"^.* \(cherry\-pick\)\.?$")

# New git message for cherry-picked CLs. One message per line.
MERGE_MESSAGE_GIT_RE = re.compile(r"^Merged ([a-fA-F0-9]+)\.?$")

# Expression for retrieving reverted patches from a commit message (old and
# new format).
ROLLBACK_MESSAGE_RE = re.compile(r"^.*[R|r]ollback of (.+)(\)| in).*$", re.M)

# New git message for reverted CLs. One message per line.
ROLLBACK_MESSAGE_GIT_RE = re.compile(r"^Rollback of ([a-fA-F0-9]+)\.?$")

# Expression for retrieving the code review link.
REVIEW_LINK_RE = re.compile(r"^Review URL: (.+)$", re.M)

# Expression with three versions (historical) for extracting the v8 revision
# from the chromium DEPS file.
DEPS_RE = re.compile(r"""^\s*(?:["']v8_revision["']: ["']"""
                     """|\(Var\("googlecode_url"\) % "v8"\) \+ "\/trunk@"""
                     """|"http\:\/\/v8\.googlecode\.com\/svn\/trunk@)"""
                     """([^"']+)["'].*$""", re.M)

# Expression to pick tag and revision for bleeding edge tags. To be used with
# output of 'svn log'.
BLEEDING_EDGE_TAGS_RE = re.compile(
    r"A \/tags\/([^\s]+) \(from \/branches\/bleeding_edge\:(\d+)\)")


def SortBranches(branches):
  """Sort branches with version number names."""
  return sorted(branches, key=SortingKey, reverse=True)


def FilterDuplicatesAndReverse(cr_releases):
  """Returns the chromium releases in reverse order filtered by v8 revision
  duplicates.

  cr_releases is a list of [cr_rev, v8_rev] reverse-sorted by cr_rev.
  """
  last = ""
  result = []
  for release in reversed(cr_releases):
    if last == release[1]:
      continue
    last = release[1]
    result.append(release)
  return result


def BuildRevisionRanges(cr_releases):
  """Returns a mapping of v8 revision -> chromium ranges.
  The ranges are comma-separated, each range has the form R1:R2. The newest
  entry is the only one of the form R1, as there is no end range.

  cr_releases is a list of [cr_rev, v8_rev] reverse-sorted by cr_rev.
  cr_rev either refers to a chromium svn revision or a chromium branch number.
  """
  range_lists = {}
  cr_releases = FilterDuplicatesAndReverse(cr_releases)

  # Visit pairs of cr releases from oldest to newest.
  for cr_from, cr_to in itertools.izip(
      cr_releases, itertools.islice(cr_releases, 1, None)):

    # Assume the chromium revisions are all different.
    assert cr_from[0] != cr_to[0]

    # TODO(machenbach): Subtraction is not git friendly.
    ran = "%s:%d" % (cr_from[0], int(cr_to[0]) - 1)

    # Collect the ranges in lists per revision.
    range_lists.setdefault(cr_from[1], []).append(ran)

  # Add the newest revision.
  if cr_releases:
    range_lists.setdefault(cr_releases[-1][1], []).append(cr_releases[-1][0])

  # Stringify and comma-separate the range lists.
  return dict((rev, ", ".join(ran)) for rev, ran in range_lists.iteritems())


def MatchSafe(match):
  if match:
    return match.group(1)
  else:
    return ""


class Preparation(Step):
  MESSAGE = "Preparation."

  def RunStep(self):
    self.CommonPrepare()
    self.PrepareBranch()


class RetrieveV8Releases(Step):
  MESSAGE = "Retrieve all V8 releases."

  def ExceedsMax(self, releases):
    return (self._options.max_releases > 0
            and len(releases) > self._options.max_releases)

  def GetBleedingEdgeFromPush(self, title):
    return MatchSafe(PUSH_MSG_SVN_RE.match(title))

  def GetBleedingEdgeGitFromPush(self, title):
    return MatchSafe(PUSH_MSG_GIT_RE.match(title))

  def GetMergedPatches(self, body):
    patches = MatchSafe(MERGE_MESSAGE_RE.search(body))
    if not patches:
      patches = MatchSafe(ROLLBACK_MESSAGE_RE.search(body))
      if patches:
        # Indicate reverted patches with a "-".
        patches = "-%s" % patches
    return patches

  def GetMergedPatchesGit(self, body):
    patches = []
    for line in body.splitlines():
      patch = MatchSafe(MERGE_MESSAGE_GIT_RE.match(line))
      if patch:
        patches.append(patch)
      patch = MatchSafe(ROLLBACK_MESSAGE_GIT_RE.match(line))
      if patch:
        patches.append("-%s" % patch)
    return ", ".join(patches)


  def GetReleaseDict(
      self, git_hash, bleeding_edge_rev, bleeding_edge_git, branch, version,
      patches, cl_body):
    revision = self.vc.GitSvn(git_hash)
    return {
      # The SVN revision on the branch.
      "revision": revision,
      # The git revision on the branch.
      "revision_git": git_hash,
      # The SVN revision on bleeding edge (only for newer trunk pushes).
      "bleeding_edge": bleeding_edge_rev,
      # The same for git.
      "bleeding_edge_git": bleeding_edge_git,
      # The branch name.
      "branch": branch,
      # The version for displaying in the form 3.26.3 or 3.26.3.12.
      "version": version,
      # The date of the commit.
      "date": self.GitLog(n=1, format="%ci", git_hash=git_hash),
      # Merged patches if available in the form 'r1234, r2345'.
      "patches_merged": patches,
      # Default for easier output formatting.
      "chromium_revision": "",
      # Default for easier output formatting.
      "chromium_branch": "",
      # Link to the CL on code review. Trunk pushes are not uploaded, so this
      # field will be populated below with the recent roll CL link.
      "review_link": MatchSafe(REVIEW_LINK_RE.search(cl_body)),
      # Link to the commit message on google code.
      "revision_link": ("https://code.google.com/p/v8/source/detail?r=%s"
                        % revision),
    }

  def GetRelease(self, git_hash, branch):
    self.ReadAndPersistVersion()
    base_version = [self["major"], self["minor"], self["build"]]
    version = ".".join(base_version)
    body = self.GitLog(n=1, format="%B", git_hash=git_hash)

    patches = ""
    if self["patch"] != "0":
      version += ".%s" % self["patch"]
      if CHERRY_PICK_TITLE_GIT_RE.match(body.splitlines()[0]):
        patches = self.GetMergedPatchesGit(body)
      else:
        patches = self.GetMergedPatches(body)

    title = self.GitLog(n=1, format="%s", git_hash=git_hash)
    bleeding_edge_revision = self.GetBleedingEdgeFromPush(title)
    bleeding_edge_git = ""
    if bleeding_edge_revision:
      bleeding_edge_git = self.vc.SvnGit(bleeding_edge_revision,
                                         self.vc.RemoteMasterBranch())
    else:
      bleeding_edge_git = self.GetBleedingEdgeGitFromPush(title)
    return self.GetReleaseDict(
        git_hash, bleeding_edge_revision, bleeding_edge_git, branch, version,
        patches, body), self["patch"]

  def GetReleasesFromMaster(self):
    tag_text = self.SVN("log https://v8.googlecode.com/svn/tags -v --limit 20")
    releases = []
    for (tag, revision) in re.findall(BLEEDING_EDGE_TAGS_RE, tag_text):
      git_hash = self.vc.SvnGit(revision)

      # Add bleeding edge release. It does not contain patches or a code
      # review link, as tags are not uploaded.
      releases.append(self.GetReleaseDict(
        git_hash, revision, git_hash, self.vc.MasterBranch(), tag, "", ""))
    return releases

  def GetReleasesFromBranch(self, branch):
    self.GitReset(self.vc.RemoteBranch(branch))
    if branch == self.vc.MasterBranch():
      return self.GetReleasesFromMaster()

    releases = []
    try:
      for git_hash in self.GitLog(format="%H").splitlines():
        if VERSION_FILE not in self.GitChangedFiles(git_hash):
          continue
        if self.ExceedsMax(releases):
          break  # pragma: no cover
        if not self.GitCheckoutFileSafe(VERSION_FILE, git_hash):
          break  # pragma: no cover

        release, patch_level = self.GetRelease(git_hash, branch)
        releases.append(release)

        # Follow branches only until their creation point.
        # TODO(machenbach): This omits patches if the version file wasn't
        # manipulated correctly. Find a better way to detect the point where
        # the parent of the branch head leads to the trunk branch.
        if branch != self.vc.CandidateBranch() and patch_level == "0":
          break

    # Allow Ctrl-C interrupt.
    except (KeyboardInterrupt, SystemExit):  # pragma: no cover
      pass

    # Clean up checked-out version file.
    self.GitCheckoutFileSafe(VERSION_FILE, "HEAD")
    return releases

  def RunStep(self):
    self.GitCreateBranch(self._config["BRANCHNAME"])
    branches = self.vc.GetBranches()
    releases = []
    if self._options.branch == 'recent':
      # Get only recent development on trunk, beta and stable.
      if self._options.max_releases == 0:  # pragma: no cover
        self._options.max_releases = 10
      beta, stable = SortBranches(branches)[0:2]
      releases += self.GetReleasesFromBranch(stable)
      releases += self.GetReleasesFromBranch(beta)
      releases += self.GetReleasesFromBranch(self.vc.CandidateBranch())
      releases += self.GetReleasesFromBranch(self.vc.MasterBranch())
    elif self._options.branch == 'all':  # pragma: no cover
      # Retrieve the full release history.
      for branch in branches:
        releases += self.GetReleasesFromBranch(branch)
      releases += self.GetReleasesFromBranch(self.vc.CandidateBranch())
      releases += self.GetReleasesFromBranch(self.vc.MasterBranch())
    else:  # pragma: no cover
      # Retrieve history for a specified branch.
      assert self._options.branch in (branches +
          [self.vc.CandidateBranch(), self.vc.MasterBranch()])
      releases += self.GetReleasesFromBranch(self._options.branch)

    self["releases"] = sorted(releases,
                              key=lambda r: SortingKey(r["version"]),
                              reverse=True)


class SwitchChromium(Step):
  MESSAGE = "Switch to Chromium checkout."

  def RunStep(self):
    cwd = self._options.chromium
    # Check for a clean workdir.
    if not self.GitIsWorkdirClean(cwd=cwd):  # pragma: no cover
      self.Die("Workspace is not clean. Please commit or undo your changes.")
    # Assert that the DEPS file is there.
    if not os.path.exists(os.path.join(cwd, "DEPS")):  # pragma: no cover
      self.Die("DEPS file not present.")


class UpdateChromiumCheckout(Step):
  MESSAGE = "Update the checkout and create a new branch."

  def RunStep(self):
    cwd = self._options.chromium
    self.GitCheckout("master", cwd=cwd)
    self.GitPull(cwd=cwd)
    self.GitCreateBranch(self.Config("BRANCHNAME"), cwd=cwd)


def ConvertToCommitNumber(step, revision):
  # Simple check for git hashes.
  if revision.isdigit() and len(revision) < 8:
    return revision
  return step.GitConvertToSVNRevision(
      revision, cwd=os.path.join(step._options.chromium, "v8"))


class RetrieveChromiumV8Releases(Step):
  MESSAGE = "Retrieve V8 releases from Chromium DEPS."

  def RunStep(self):
    cwd = self._options.chromium
    releases = filter(
        lambda r: r["branch"] in [self.vc.CandidateBranch(),
                                  self.vc.MasterBranch()],
        self["releases"])
    if not releases:  # pragma: no cover
      print "No releases detected. Skipping chromium history."
      return True

    # Update v8 checkout in chromium.
    self.GitFetchOrigin(cwd=os.path.join(cwd, "v8"))

    oldest_v8_rev = int(releases[-1]["revision"])

    cr_releases = []
    try:
      for git_hash in self.GitLog(
          format="%H", grep="V8", cwd=cwd).splitlines():
        if "DEPS" not in self.GitChangedFiles(git_hash, cwd=cwd):
          continue
        if not self.GitCheckoutFileSafe("DEPS", git_hash, cwd=cwd):
          break  # pragma: no cover
        deps = FileToText(os.path.join(cwd, "DEPS"))
        match = DEPS_RE.search(deps)
        if match:
          cr_rev = self.GetCommitPositionNumber(git_hash, cwd=cwd)
          if cr_rev:
            v8_rev = ConvertToCommitNumber(self, match.group(1))
            cr_releases.append([cr_rev, v8_rev])

          # Stop after reaching beyond the last v8 revision we want to update.
          # We need a small buffer for possible revert/reland frenzies.
          # TODO(machenbach): Subtraction is not git friendly.
          if int(v8_rev) < oldest_v8_rev - 100:
            break  # pragma: no cover

    # Allow Ctrl-C interrupt.
    except (KeyboardInterrupt, SystemExit):  # pragma: no cover
      pass

    # Clean up.
    self.GitCheckoutFileSafe("DEPS", "HEAD", cwd=cwd)

    # Add the chromium ranges to the v8 trunk and bleeding_edge releases.
    all_ranges = BuildRevisionRanges(cr_releases)
    releases_dict = dict((r["revision"], r) for r in releases)
    for revision, ranges in all_ranges.iteritems():
      releases_dict.get(revision, {})["chromium_revision"] = ranges


# TODO(machenbach): Unify common code with method above.
class RietrieveChromiumBranches(Step):
  MESSAGE = "Retrieve Chromium branch information."

  def RunStep(self):
    cwd = self._options.chromium
    trunk_releases = filter(lambda r: r["branch"] == self.vc.CandidateBranch(),
                            self["releases"])
    if not trunk_releases:  # pragma: no cover
      print "No trunk releases detected. Skipping chromium history."
      return True

    oldest_v8_rev = int(trunk_releases[-1]["revision"])

    # Filter out irrelevant branches.
    branches = filter(lambda r: re.match(r"branch-heads/\d+", r),
                      self.GitRemotes(cwd=cwd))

    # Transform into pure branch numbers.
    branches = map(lambda r: int(re.match(r"branch-heads/(\d+)", r).group(1)),
                   branches)

    branches = sorted(branches, reverse=True)

    cr_branches = []
    try:
      for branch in branches:
        if not self.GitCheckoutFileSafe("DEPS",
                                        "branch-heads/%d" % branch,
                                        cwd=cwd):
          break  # pragma: no cover
        deps = FileToText(os.path.join(cwd, "DEPS"))
        match = DEPS_RE.search(deps)
        if match:
          v8_rev = ConvertToCommitNumber(self, match.group(1))
          cr_branches.append([str(branch), v8_rev])

          # Stop after reaching beyond the last v8 revision we want to update.
          # We need a small buffer for possible revert/reland frenzies.
          # TODO(machenbach): Subtraction is not git friendly.
          if int(v8_rev) < oldest_v8_rev - 100:
            break  # pragma: no cover

    # Allow Ctrl-C interrupt.
    except (KeyboardInterrupt, SystemExit):  # pragma: no cover
      pass

    # Clean up.
    self.GitCheckoutFileSafe("DEPS", "HEAD", cwd=cwd)

    # Add the chromium branches to the v8 trunk releases.
    all_ranges = BuildRevisionRanges(cr_branches)
    trunk_dict = dict((r["revision"], r) for r in trunk_releases)
    for revision, ranges in all_ranges.iteritems():
      trunk_dict.get(revision, {})["chromium_branch"] = ranges


class CleanUp(Step):
  MESSAGE = "Clean up."

  def RunStep(self):
    self.GitCheckout("master", cwd=self._options.chromium)
    self.GitDeleteBranch(self.Config("BRANCHNAME"), cwd=self._options.chromium)
    self.CommonCleanup()


class WriteOutput(Step):
  MESSAGE = "Print output."

  def Run(self):
    if self._options.csv:
      with open(self._options.csv, "w") as f:
        writer = csv.DictWriter(f,
                                ["version", "branch", "revision",
                                 "chromium_revision", "patches_merged"],
                                restval="",
                                extrasaction="ignore")
        for release in self["releases"]:
          writer.writerow(release)
    if self._options.json:
      with open(self._options.json, "w") as f:
        f.write(json.dumps(self["releases"]))
    if not self._options.csv and not self._options.json:
      print self["releases"]  # pragma: no cover


class Releases(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-b", "--branch", default="recent",
                        help=("The branch to analyze. If 'all' is specified, "
                              "analyze all branches. If 'recent' (default) "
                              "is specified, track beta, stable and trunk."))
    parser.add_argument("-c", "--chromium",
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--csv", help="Path to a CSV file for export.")
    parser.add_argument("-m", "--max-releases", type=int, default=0,
                        help="The maximum number of releases to track.")
    parser.add_argument("--json", help="Path to a JSON file for export.")

  def _ProcessOptions(self, options):  # pragma: no cover
    return True

  def _Config(self):
    return {
      "BRANCHNAME": "retrieve-v8-releases",
      "PERSISTFILE_BASENAME": "/tmp/v8-releases-tempfile",
    }

  def _Steps(self):
    return [
      Preparation,
      RetrieveV8Releases,
      SwitchChromium,
      UpdateChromiumCheckout,
      RetrieveChromiumV8Releases,
      RietrieveChromiumBranches,
      CleanUp,
      WriteOutput,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(Releases().Run())
