#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import operator
import os
import re
from sets import Set
from subprocess import Popen, PIPE
import sys

def search_all_related_commits(
    git_working_dir, start_hash, until, separator, verbose=False):

  all_commits_raw = _find_commits_inbetween(
      start_hash, until, git_working_dir, verbose)
  if verbose:
    print("All commits between <of> and <until>: " + all_commits_raw)

  # Adding start hash too
  all_commits = [start_hash]
  all_commits.extend(all_commits_raw.splitlines())
  all_related_commits = {}
  already_treated_commits = Set([])
  for commit in all_commits:
    if commit in already_treated_commits:
      continue

    related_commits = _search_related_commits(
        git_working_dir, commit, until, separator, verbose)
    if len(related_commits) > 0:
      all_related_commits[commit] = related_commits
      already_treated_commits.update(related_commits)

    already_treated_commits.update(commit)

  return all_related_commits

def _search_related_commits(
    git_working_dir, start_hash, until, separator, verbose=False):

  if separator:
    commits_between = _find_commits_inbetween(
        start_hash, separator, git_working_dir, verbose)
    if commits_between == "":
      return []

  # Extract commit position
  original_message = git_execute(
      git_working_dir,
      ["show", "-s", "--format=%B", start_hash],
      verbose)
  title = original_message.splitlines()[0]

  matches = re.search("(\{#)([0-9]*)(\})", original_message)

  if not matches:
    return []

  commit_position = matches.group(2)
  if verbose:
    print("1.) Commit position to look for: " + commit_position)

  search_range = start_hash + ".." + until

  def git_args(grep_pattern):
    return [
      "log",
      "--reverse",
      "--grep=" + grep_pattern,
      "--format=%H",
      search_range,
    ]

  found_by_hash = git_execute(
      git_working_dir, git_args(start_hash), verbose).strip()

  if verbose:
    print("2.) Found by hash: " + found_by_hash)

  found_by_commit_pos = git_execute(
      git_working_dir, git_args(commit_position), verbose).strip()

  if verbose:
    print("3.) Found by commit position: " + found_by_commit_pos)

  # Replace brackets or else they are wrongly interpreted by --grep
  title = title.replace("[", "\\[")
  title = title.replace("]", "\\]")

  found_by_title = git_execute(
      git_working_dir, git_args(title), verbose).strip()

  if verbose:
    print("4.) Found by title: " + found_by_title)

  hits = (
      _convert_to_array(found_by_hash) +
      _convert_to_array(found_by_commit_pos) +
      _convert_to_array(found_by_title))
  hits = _remove_duplicates(hits)

  if separator:
    for current_hit in hits:
      commits_between = _find_commits_inbetween(
          separator, current_hit, git_working_dir, verbose)
      if commits_between != "":
        return hits
    return []

  return hits

def _find_commits_inbetween(start_hash, end_hash, git_working_dir, verbose):
  commits_between = git_execute(
        git_working_dir,
        ["rev-list", "--reverse", start_hash + ".." + end_hash],
        verbose)
  return commits_between.strip()

def _convert_to_array(string_of_hashes):
  return string_of_hashes.splitlines()

def _remove_duplicates(array):
   no_duplicates = []
   for current in array:
    if not current in no_duplicates:
      no_duplicates.append(current)
   return no_duplicates

def git_execute(working_dir, args, verbose=False):
  command = ["git", "-C", working_dir] + args
  if verbose:
    print("Git working dir: " + working_dir)
    print("Executing git command:" + str(command))
  p = Popen(args=command, stdin=PIPE,
            stdout=PIPE, stderr=PIPE)
  output, err = p.communicate()
  rc = p.returncode
  if rc != 0:
    raise Exception(err)
  if verbose:
    print("Git return value: " + output)
  return output

def _pretty_print_entry(hash, git_dir, pre_text, verbose):
  text_to_print = git_execute(
      git_dir,
      ["show",
       "--quiet",
       "--date=iso",
       hash,
       "--format=%ad # %H # %s"],
      verbose)
  return pre_text + text_to_print.strip()

def main(options):
    all_related_commits = search_all_related_commits(
        options.git_dir,
        options.of[0],
        options.until[0],
        options.separator,
        options.verbose)

    sort_key = lambda x: (
        git_execute(
            options.git_dir,
            ["show", "--quiet", "--date=iso", x, "--format=%ad"],
            options.verbose)).strip()

    high_level_commits = sorted(all_related_commits.keys(), key=sort_key)

    for current_key in high_level_commits:
      if options.prettyprint:
        yield _pretty_print_entry(
            current_key,
            options.git_dir,
            "+",
            options.verbose)
      else:
        yield "+" + current_key

      found_commits = all_related_commits[current_key]
      for current_commit in found_commits:
        if options.prettyprint:
          yield _pretty_print_entry(
              current_commit,
              options.git_dir,
              "| ",
              options.verbose)
        else:
          yield "| " + current_commit

if __name__ == "__main__":  # pragma: no cover
  parser = argparse.ArgumentParser(
      "This tool analyzes the commit range between <of> and <until>. "
      "It finds commits which belong together e.g. Implement/Revert pairs and "
      "Implement/Port/Revert triples. All supplied hashes need to be "
      "from the same branch e.g. master.")
  parser.add_argument("-g", "--git-dir", required=False, default=".",
                        help="The path to your git working directory.")
  parser.add_argument("--verbose", action="store_true",
      help="Enables a very verbose output")
  parser.add_argument("of", nargs=1,
      help="Hash of the commit to be searched.")
  parser.add_argument("until", nargs=1,
      help="Commit when searching should stop")
  parser.add_argument("--separator", required=False,
      help="The script will only list related commits "
            "which are separated by hash <--separator>.")
  parser.add_argument("--prettyprint", action="store_true",
      help="Pretty prints the output")

  args = sys.argv[1:]
  options = parser.parse_args(args)
  for current_line in main(options):
    print(current_line)
