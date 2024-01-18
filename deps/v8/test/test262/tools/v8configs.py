# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains all project-specific configuration and entities for V8/Tets262 host."""

import json

from blinkpy.w3c.local_wpt import LocalRepo
from blinkpy.w3c.wpt_github import GitHubRepo, PROVISIONAL_PR_LABEL
from blinkpy.w3c.chromium_finder import absolute_chromium_dir
from blinkpy.w3c.chromium_configs import ProjectConfig


def to_object(item):
  """
  Recursively convert a dictionary to an object.
  """
  if isinstance(item, dict):
    return type('_dict_obj', (), {k: to_object(v) for k, v in item.items()})
  if isinstance(item, list):
    return [to_object(element) for element in item]
  return item


class Test262GitHub(GitHubRepo):

  def __init__(self,
               config,
               host,
               user=None,
               token=None,
               pr_history_window=1000):
    super(Test262GitHub, self).__init__(
        gh_org=config.github.org,
        gh_repo_name=config.github.repo,
        export_pr_label=config.export.pr_label,
        provisional_pr_label=PROVISIONAL_PR_LABEL,
        host=host,
        user=user,
        token=token,
        pr_history_window=pr_history_window,
        main_branch=config.github.main_branch,
        min_expected_prs=0)

  @property
  def skipped_revisions(self):
    return []


class V8Test262Config(ProjectConfig):

  def __init__(self, filesystem, config_file):
    super().__init__(filesystem)
    with open(config_file, 'r') as f:
      self.config = to_object(json.loads(f.read()))
    self.relative_tests_path = self.config.gerrit.relative_tests_path
    self.revision_footer = self.config.gerrit.revision_footer
    self.gerrit_project = self.config.gerrit.project
    self.gerrit_branch = self.config.gerrit.branch
    self.github_factory = github_factory(self.config)
    self.local_repo_factory = local_repo_factory(self.config)

  @property
  def project_root(self):
    return absolute_chromium_dir(self.filesystem) + '/v8'

  @property
  def test_root(self):
    return self.project_root + '/' + self.relative_tests_path

  @property
  def pr_updated_comment_template(self):
    return self.config.export.pr_updated_comment

  def inflight_cl_comment_template(self):
    return self.config.export.inflight_cl_comment


def config_from_file(config_file):
  return lambda filesystem: V8Test262Config(filesystem, config_file)


def local_repo_factory(config):
  return lambda host, gh_token: LocalRepo(
      name=config.github.name,
      gh_org=config.github.org,
      gh_repo_name=config.github.repo,
      gh_ssh_url_template= 'https://{}@github.com/%s/%s.git' % (config.github.org, config.github.repo),
      mirror_url=None,
      default_committer_email=config.export.committer_email,
      default_committer_name=config.export.committer_name,
      source_relative_tests=config.github.source_relative_tests,
      destination_relative_tests=config.github.destination_relative_tests,
      host=host,
      gh_token=gh_token,
      main_branch=config.github.main_branch,
  )


def github_factory(config):
  return lambda host, user, token: Test262GitHub(config, host, user, token)
