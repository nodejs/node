#!/usr/bin/env python3
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.util
import os
import re
import sys
import tempfile
import unittest
from unittest import mock

# Configuring the path for the v8_presubmit module
TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
V8_ROOT = os.path.dirname(TOOLS_ROOT)
sys.path.append(TOOLS_ROOT)

from v8_presubmit import FileContentsCache, CacheableSourceFileProcessor


def _LoadTopLevelPresubmit():
  presubmit_path = os.path.join(V8_ROOT, 'PRESUBMIT.py')
  spec = importlib.util.spec_from_file_location('v8_top_presubmit',
                                                presubmit_path)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


PRESUBMIT = _LoadTopLevelPresubmit()


class FakeCachedProcessor(CacheableSourceFileProcessor):
  def __init__(self, cache_file_path):
    super(FakeCachedProcessor, self).__init__(
      use_cache=True, cache_file_path=cache_file_path, file_type='.test')
  def GetProcessorWorker(self):
    return object
  def GetProcessorScript(self):
    return "echo", []
  def DetectUnformattedFiles(_, cmd, worker, files):
    raise NotImplementedError

class FileContentsCacheTest(unittest.TestCase):
  def setUp(self):
    _, self.cache_file_path = tempfile.mkstemp()
    cache = FileContentsCache(self.cache_file_path)
    cache.Load()

    def generate_file():
      _, file_name = tempfile.mkstemp()
      with open(file_name, "w") as f:
        f.write(file_name)

      return file_name

    self.target_files = [generate_file() for _ in range(2)]
    unchanged_files = cache.FilterUnchangedFiles(self.target_files)
    self.assertEqual(len(unchanged_files), 2)
    cache.Save()

  def tearDown(self):
    for file in [self.cache_file_path] + self.target_files:
      os.remove(file)

  def testCachesFiles(self):
    cache = FileContentsCache(self.cache_file_path)
    cache.Load()

    changed_files = cache.FilterUnchangedFiles(self.target_files)
    self.assertListEqual(changed_files, [])

    modified_file = self.target_files[0]
    with open(modified_file, "w") as f:
      f.write("modification")

    changed_files = cache.FilterUnchangedFiles(self.target_files)
    self.assertListEqual(changed_files, [modified_file])

  def testCacheableSourceFileProcessor(self):
    class CachedProcessor(FakeCachedProcessor):
      def DetectFilesToChange(_, files):
        self.assertListEqual(files, [])
        return []

    cached_processor = CachedProcessor(cache_file_path=self.cache_file_path)
    cached_processor.ProcessFiles(self.target_files)

  def testCacheableSourceFileProcessorWithModifications(self):
    modified_file = self.target_files[0]
    with open(modified_file, "w") as f:
      f.write("modification")

    class CachedProcessor(FakeCachedProcessor):
      def DetectFilesToChange(_, files):
        self.assertListEqual(files, [modified_file])
        return []

    cached_processor = CachedProcessor(
      cache_file_path=self.cache_file_path,
    )
    cached_processor.ProcessFiles(self.target_files)


class _FakeAffectedFile:

  def __init__(self, local_path, changed_lines=2):
    self._local_path = local_path
    self._changed_lines = changed_lines

  def LocalPath(self):
    return self._local_path

  def UnixLocalPath(self):
    return self._local_path.replace('\\', '/')

  def ChangedContents(self):
    return [(i, 'line') for i in range(1, self._changed_lines + 1)]


class _FakeChange:

  def __init__(self, description='', issue=1, author_email='', bug=None):
    self._description = description
    self.issue = issue
    self.author_email = author_email
    self.BUG = bug

  def FullDescriptionText(self):
    return self._description


class _FakeInputApi:

  def __init__(
      self,
      description='',
      files=None,
      environ=None,
      issue=1,
      author_email='',
      is_committing=False,
      dry_run=False,
      bug=None,
  ):
    self.change = _FakeChange(
        description=description,
        issue=issue,
        author_email=author_email,
        bug=bug)
    self._files = [_FakeAffectedFile(f) for f in (files or [])]
    self.environ = dict(environ or {})
    self.is_committing = is_committing
    self.dry_run = dry_run
    self.re = re
    self.os_path = os.path
    self.python3_executable = sys.executable

  def AffectedFiles(self, include_deletes=True, file_filter=None):
    del include_deletes
    if file_filter:
      return [f for f in self._files if file_filter(f)]
    return list(self._files)

  def UnixLocalPaths(self):
    return [f.UnixLocalPath() for f in self._files]

  def PresubmitLocalPath(self):
    return V8_ROOT

  def Command(self, name, cmd, kwargs, message):
    return {'name': name, 'cmd': cmd, 'kwargs': kwargs, 'message': message}

  def RunTests(self, tests):
    return list(tests)


class _FakeOutputApi:

  class PresubmitError:

    def __init__(self, message, items=None):
      self.message = message
      self.items = items
      self.fatal = True

    def __repr__(self):
      return f'PresubmitError({self.message!r})'


class AgentPresubmitChecksTest(unittest.TestCase):

  def setUp(self):
    self.output_api = _FakeOutputApi()

  def testNoLocalPathsInDescriptionAllowsRepoPathsAndUrls(self):
    desc = ('[compiler] Fix issue in src/compiler/turboshaft/assembler.h\n\n'
            'See test/mjsunit/wasm/foo.js and https://crrev.com/c/1234567.\n'
            '# /tmp/commented_out_path.txt\n')
    input_api = _FakeInputApi(description=desc)
    errors = PRESUBMIT._CheckNoLocalPathsInDescription(input_api,
                                                       self.output_api)
    self.assertEqual(errors, [])

  def testNoLocalPathsInDescriptionRejectsWorkstationPaths(self):
    bad_paths = [
        'file:///path/to/file.cc',
        '/usr/local/google/home/user/v8/src/foo.cc',
        '/usr/local/btrfs_mount/task-123/v8/src/foo.cc',
        '/home/developer/v8/src/foo.cc',
        '/Users/developer/v8/src/foo.cc',
        '/google/src/cloud/user/ws/google3/foo.cc',
        '/google/cog/cloud/user/ws/foo.cc',
        '/tmp/repro.js',
        '~/v8/repro.js',
        '.gemini/jetski/brain/123/plan.md',
        'worktrees/task-fix-bug/src/foo.cc',
    ]
    for path in bad_paths:
      with self.subTest(path=path):
        desc = f'[compiler] Fix bug\n\nTested with {path}\n'
        input_api = _FakeInputApi(description=desc)
        errors = PRESUBMIT._CheckNoLocalPathsInDescription(
            input_api, self.output_api)
        self.assertEqual(len(errors), 1)
        self.assertIn(path, errors[0].message)

  def testIsAiAgentDetection(self):
    self.assertFalse(PRESUBMIT._IsAiAgent(_FakeInputApi(description='[a] B')))
    self.assertTrue(
        PRESUBMIT._IsAiAgent(
            _FakeInputApi(description='[a] B', environ={'GEMINI_CLI': '1'})))
    self.assertFalse(
        PRESUBMIT._IsAiAgent(
            _FakeInputApi(description='[a] B', environ={'GEMINI_CLI': '0'})))

  def testAgentDescriptionValid(self):
    desc = (
        '[maglev][turboshaft] Track double-field HeapNumber boxes\n\n'
        'Pass PropertyKey::DoubleField(name) when loading or storing\n'
        'double-field HeapNumber boxes in BuildLoadField.\n'
        'https://chromium-review.googlesource.com/c/v8/v8/+/12345678901234567890\n\n'
        'TAG=agy\n'
        'CONV=030aee38-e9f1-408a-9fca-9c36359b29d1\n'
        'Fixed: 509339491, b:349680002\n'
        'Change-Id: I1019c9d7d6b59d20ac241b73d31cb1f4a70a0c24\n')
    input_api = _FakeInputApi(description=desc)
    errors = PRESUBMIT._CheckAgentDescription(input_api, self.output_api)
    self.assertEqual(errors, [])

  def testAgentDescriptionRejectsMissingComponentOrLongTitle(self):
    desc_no_comp = ('Fix something without component prefix\n\n'
                    'TAG=agy\n'
                    'CONV=030aee38-e9f1-408a-9fca-9c36359b29d1\n')
    errors = PRESUBMIT._CheckAgentDescription(
        _FakeInputApi(description=desc_no_comp), self.output_api)
    self.assertTrue(any('[component] Title' in e.message for e in errors))

    long_title = '[compiler] ' + ('a' * 65)
    desc_long_title = (f'{long_title}\n\n'
                       'TAG=agy\n'
                       'CONV=030aee38-e9f1-408a-9fca-9c36359b29d1\n')
    errors = PRESUBMIT._CheckAgentDescription(
        _FakeInputApi(description=desc_long_title), self.output_api)
    self.assertTrue(any('Commit title is' in e.message for e in errors))

  def testAgentDescriptionRejectsNonBlankSecondLineAndLongBodyLine(self):
    desc = ('[compiler] Fix bug\n'
            'Non-blank second line.\n' + ('word ' * 16).strip() + '\n\n'
            'TAG=agy\n'
            'CONV=030aee38-e9f1-408a-9fca-9c36359b29d1\n')
    errors = PRESUBMIT._CheckAgentDescription(
        _FakeInputApi(description=desc), self.output_api)
    messages = '\n'.join(e.message for e in errors)
    self.assertIn('Second line of commit description must be blank', messages)
    self.assertIn('lines exceeding 72 chars', messages)

  def testDescriptionFooters(self):
    desc_dup_change_id = (
        '[compiler] Fix bug\n\n'
        'Body text.\n\n'
        'Change-Id: I1111111111111111111111111111111111111111\n'
        'Change-Id: I2222222222222222222222222222222222222222\n')
    # TAG=agy and CONV= are optional in _CheckAgentDescription.
    self.assertEqual(
        PRESUBMIT._CheckAgentDescription(
            _FakeInputApi(description=desc_dup_change_id), self.output_api), [])

    footer_errors = PRESUBMIT._CheckDescriptionFooters(
        _FakeInputApi(description=desc_dup_change_id), self.output_api)
    self.assertTrue(
        any('Multiple Change-Id lines' in e.message for e in footer_errors))

    desc_tags_not_at_bottom = ('[compiler] Fix bug\n\n'
                               'TAG=agy\n'
                               'CONV=030aee38-e9f1-408a-9fca-9c36359b29d1\n\n'
                               'Note: Body paragraph placed after tags.\n')
    errors = PRESUBMIT._CheckDescriptionFooters(
        _FakeInputApi(description=desc_tags_not_at_bottom), self.output_api)
    self.assertTrue(any('at the bottom' in e.message for e in errors))

    # Standard uppercase KEY=value footers after TAG=agy are allowed.
    desc_valid_footers = ('[compiler] Fix bug\n\n'
                          'TAG=agy\n'
                          'CQ_INCLUDE_TRYBOTS=luci.v8.try:v8_linux64_dbg\n'
                          'NOTRY=true\n'
                          'NOPRESUBMIT=true\n')
    self.assertEqual(
        PRESUBMIT._CheckDescriptionFooters(
            _FakeInputApi(description=desc_valid_footers), self.output_api), [])

    # Prose lines starting with TAG=agy followed by words are not footers.
    desc_prose_mention = (
        '[presubmit] Fix check\n\n'
        'TAG=agy should not trigger AI agent environment checks in\n'
        'any commit message.\n\n'
        'Change-Id: I1111111111111111111111111111111111111111\n')
    self.assertEqual(
        PRESUBMIT._CheckDescriptionFooters(
            _FakeInputApi(description=desc_prose_mention), self.output_api), [])

  def testHumanInitialUploadSkipsDescriptionChecksBeforeInteractiveEditor(self):
    bad_desc = ('[compiler] Fix bug\n\n'
                'Tested with /tmp/repro.js\n'
                'TAG=agy\n\n'
                'Trailing paragraph.\n'
                'Fixed: 509339491\n'
                'Change-Id: I1111111111111111111111111111111111111111\n'
                'Change-Id: I2222222222222222222222222222222222222222\n')
    human_initial = _FakeInputApi(
        description=bad_desc,
        files=['src/foo.cc'],
        issue=None,
        is_committing=False,
        bug='12345',
        environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '1'})
    self.assertFalse(PRESUBMIT._HasUpstreamDescription(human_initial))
    self.assertEqual(
        PRESUBMIT._CheckCommitMessageBugEntry(human_initial, self.output_api),
        [])
    self.assertEqual(
        PRESUBMIT._CheckNoLocalPathsInDescription(human_initial,
                                                  self.output_api), [])
    self.assertEqual(
        PRESUBMIT._CheckDescriptionFooters(human_initial, self.output_api), [])
    self.assertEqual(
        PRESUBMIT._CheckRegressionTest(human_initial, self.output_api), [])
    self.assertEqual(
        PRESUBMIT._CheckPriorityPrefix(human_initial, self.output_api), [])

    # Once an issue exists, on commit, or in an AI agent environment, the
    # checks run against the upstream description.
    for kwargs in (
        {
            'issue': 12345
        },
        {
            'issue': None,
            'is_committing': True
        },
        {
            'issue': None,
            'environ': {
                'GEMINI_CLI': '1'
            }
        },
    ):
      api = _FakeInputApi(
          description=bad_desc, files=['src/foo.cc'], bug='12345', **kwargs)
      self.assertTrue(PRESUBMIT._HasUpstreamDescription(api))
      self.assertEqual(
          len(PRESUBMIT._CheckCommitMessageBugEntry(api, self.output_api)), 1)
      self.assertEqual(
          len(PRESUBMIT._CheckNoLocalPathsInDescription(api, self.output_api)),
          1)
      self.assertEqual(
          len(PRESUBMIT._CheckDescriptionFooters(api, self.output_api)), 2)
      self.assertEqual(
          len(PRESUBMIT._CheckRegressionTest(api, self.output_api)), 1)

  def testAgentDescriptionValidatesBugAndFixedFormat(self):
    desc_bad_bugs = ('[compiler] Fix bug\n\n'
                     'BUG=12345\n'
                     'Bug: v8:12345\n'
                     'Fixed: 1, 2, 3, 4, 5, 6\n'
                     'TAG=agy\n')
    errors = PRESUBMIT._CheckAgentDescription(
        _FakeInputApi(description=desc_bad_bugs), self.output_api)
    messages = '\n'.join(e.message for e in errors)
    self.assertIn('Use "Bug:" or "Fixed:" instead of "BUG="', messages)
    self.assertIn('Invalid bug ID(s)', messages)
    self.assertIn('Too many bug IDs', messages)

  def testRegressionTestRequirement(self):
    desc_with_fixed = ('[compiler] Fix bug\n\n'
                       'Fixed: 509339491\n'
                       'TAG=agy\n')
    # Fixed: + src/ without test/ -> error
    errors = PRESUBMIT._CheckRegressionTest(
        _FakeInputApi(description=desc_with_fixed, files=['src/foo.cc']),
        self.output_api)
    self.assertEqual(len(errors), 1)
    self.assertIn('regression test', errors[0].message)

    # Fixed: + src/ + test/ -> passes
    errors = PRESUBMIT._CheckRegressionTest(
        _FakeInputApi(
            description=desc_with_fixed,
            files=['src/foo.cc', 'test/mjsunit/regress/regress-1.js']),
        self.output_api)
    self.assertEqual(errors, [])

    # No-Regression-Test: <reason> footer -> passes
    desc_with_no_test_reason = ('[compiler] Fix bug\n\n'
                                'Fixed: 509339491\n'
                                'No-Regression-Test: covered by existing unit\n'
                                'TAG=agy\n')
    errors = PRESUBMIT._CheckRegressionTest(
        _FakeInputApi(
            description=desc_with_no_test_reason, files=['src/foo.cc']),
        self.output_api)
    self.assertEqual(errors, [])

    # Empty No-Regression-Test: without reason -> still fails
    desc_empty_no_test = ('[compiler] Fix bug\n\n'
                          'Fixed: 509339491\n'
                          'No-Regression-Test:   \n'
                          'TAG=agy\n')
    errors = PRESUBMIT._CheckRegressionTest(
        _FakeInputApi(description=desc_empty_no_test, files=['src/foo.cc']),
        self.output_api)
    self.assertEqual(len(errors), 1)

    # No Fixed: line -> passes even when modifying src/ only
    desc_without_fixed = ('[compiler] Refactor helper\n\n'
                          'Bug: 509339491\n'
                          'TAG=agy\n')
    errors = PRESUBMIT._CheckRegressionTest(
        _FakeInputApi(description=desc_without_fixed, files=['src/foo.cc']),
        self.output_api)
    self.assertEqual(errors, [])

  def testReleaseMjsunit(self):
    # Skipped when UPLOAD_CL_NOCHECK=1
    self.assertEqual(
        PRESUBMIT._CheckReleaseMjsunit(
            _FakeInputApi(
                files=['src/foo.cc'],
                environ={
                    'V8_PRESUBMIT_RELEASE_MJSUNIT': '1',
                    'UPLOAD_CL_NOCHECK': '1',
                }), self.output_api), [])

    # Skipped when only docs/agents/markdown/OWNERS files are modified
    self.assertEqual(
        PRESUBMIT._CheckReleaseMjsunit(
            _FakeInputApi(
                files=['docs/README.md', 'agents/SKILL.md', 'OWNERS'],
                environ={'V8_PRESUBMIT_RELEASE_MJSUNIT': '1'}),
            self.output_api), [])

    # Runs gm.py when opted in via env var and code files are modified
    cmds = PRESUBMIT._CheckReleaseMjsunit(
        _FakeInputApi(
            files=['src/foo.cc'],
            environ={'V8_PRESUBMIT_RELEASE_MJSUNIT': '1'}), self.output_api)
    self.assertEqual(len(cmds), 1)
    self.assertIn('mjsunit', cmds[0]['cmd'])

    # Exercises git config opt-out and opt-in paths when env var is not set
    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=1, stdout='')):
      self.assertEqual(
          PRESUBMIT._CheckReleaseMjsunit(
              _FakeInputApi(files=['src/foo.cc']), self.output_api), [])

    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=0, stdout='true\n')):
      cmds = PRESUBMIT._CheckReleaseMjsunit(
          _FakeInputApi(files=['src/foo.cc']), self.output_api)
      self.assertEqual(len(cmds), 1)
      self.assertIn('mjsunit', cmds[0]['cmd'])

  def testAgentNoDuplicateCL(self):
    desc = ('[compiler] Fix bug\n\n'
            'Change-Id: I1111111111111111111111111111111111111111\n')
    fake_changes = [{
        '_number': 12345,
        'subject': '[compiler] Fix bug',
        'change_id': 'I1111111111111111111111111111111111111111',
        'current_revision': 'rev1',
        'revisions': {
            'rev1': {
                'files': {
                    'src/foo.cc': {
                        'lines_inserted': 2,
                        'lines_deleted': 1,
                    }
                }
            }
        },
    }]
    fake_gerrit = mock.MagicMock(QueryChanges=lambda *a, **kw: fake_changes)
    with mock.patch.dict(sys.modules, {'gerrit_util': fake_gerrit}):
      self.assertEqual(
          PRESUBMIT._CheckAgentNoDuplicateCL(
              _FakeInputApi(description=desc, issue=12345), self.output_api),
          [])
      self.assertEqual(
          PRESUBMIT._CheckAgentNoDuplicateCL(
              _FakeInputApi(
                  description=desc,
                  issue=None,
                  environ={'PRESUBMIT_ALLOW_DUPLICATE_CL': '1'}),
              self.output_api), [])
      errors = PRESUBMIT._CheckAgentNoDuplicateCL(
          _FakeInputApi(description=desc, files=['src/foo.cc'], issue=None),
          self.output_api)
      self.assertEqual(len(errors), 1)
      self.assertIn('same Change-Id', errors[0].message)
      self.assertIn('identical title', errors[0].message)
      self.assertIn('identical diff footprint', errors[0].message)

  def testIsPriorityPrefixEnabled(self):
    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=1, stdout='')):
      self.assertFalse(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(description='[a] B')))
      self.assertFalse(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(description='[a] B\n\nTAG=agy\n')))
      self.assertTrue(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(
                  description='[a] B',
                  environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '1'})))
      self.assertFalse(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(
                  description='[a] B',
                  environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '0'})))

    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=0, stdout='true\n')):
      self.assertTrue(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(description='[a] B')))
      self.assertFalse(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(
                  description='[a] B',
                  environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '0'})))

    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=0, stdout='false\n')):
      self.assertFalse(
          PRESUBMIT._IsPriorityPrefixEnabled(
              _FakeInputApi(description='[a] B')))

  def testPriorityPrefixUploadAndCommit(self):
    # Upload: skipped when not opted in
    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=1, stdout='')):
      self.assertEqual(
          PRESUBMIT._CheckPriorityPrefix(
              _FakeInputApi(description='[compiler] Fix bug\n\nBody.\n'),
              self.output_api), [])

    # Upload: accepts both "[P1][compiler] Title" and "[P1] [compiler] Title"
    for valid_title in ('[P1][compiler] Fix bug', '[P1] [compiler] Fix bug'):
      input_api = _FakeInputApi(
          description=f'{valid_title}\n\nBody.\n',
          environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '1'})
      self.assertEqual(
          PRESUBMIT._CheckPriorityPrefix(input_api, self.output_api), [])
      self.assertEqual(
          PRESUBMIT._CheckAgentDescription(input_api, self.output_api), [])

    # Upload: rejects missing [P0]-[P4] prefix when enabled
    missing_prio = _FakeInputApi(
        description='[compiler] Fix bug\n\nBody.\n',
        environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '1'})
    errors = PRESUBMIT._CheckPriorityPrefix(missing_prio, self.output_api)
    self.assertEqual(len(errors), 1)
    self.assertIn('priority prefix "[P0]"-"[P4]"', errors[0].message)

    # Upload: [P1] without [component] is allowed by _CheckPriorityPrefix for
    # human CLs, but rejected by _CheckAgentDescription for AI agent CLs.
    missing_comp = _FakeInputApi(
        description='[P1] Fix bug without component\n\nBody.\n',
        environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '1'})
    self.assertEqual(
        PRESUBMIT._CheckPriorityPrefix(missing_comp, self.output_api), [])
    agent_errors = PRESUBMIT._CheckAgentDescription(missing_comp,
                                                    self.output_api)
    self.assertTrue(any('[component] Title' in e.message for e in agent_errors))

    # Upload: rejects empty subject after [P1] or duplicate priority prefix
    for bad_title in ('[P1]', '[P1][P2] Fix bug'):
      bad_input = _FakeInputApi(
          description=f'{bad_title}\n\nBody.\n',
          environ={'V8_PRESUBMIT_PRIORITY_PREFIX': '1'})
      errors = PRESUBMIT._CheckPriorityPrefix(bad_input, self.output_api)
      self.assertEqual(len(errors), 1)
      self.assertIn('must include a subject after', errors[0].message)

    # Commit: allows [P1] on CQ dry-runs (dry_run=True), rejects on full commit
    commit_dry_run = _FakeInputApi(
        description='[P1] [compiler] Fix bug\n\nBody.\n',
        is_committing=True,
        dry_run=True)
    self.assertEqual(
        PRESUBMIT._CheckPriorityPrefix(commit_dry_run, self.output_api), [])

    with mock.patch.object(
        PRESUBMIT.subprocess,
        'run',
        return_value=mock.MagicMock(returncode=1, stdout='')):
      commit_full_with_prio = _FakeInputApi(
          description='[P1] [compiler] Fix bug\n\nBody.\n',
          is_committing=True,
          dry_run=False)
      commit_errors = PRESUBMIT._CheckPriorityPrefix(commit_full_with_prio,
                                                     self.output_api)
      self.assertEqual(len(commit_errors), 1)
      self.assertIn('strip the priority prefix before merging',
                    commit_errors[0].message)

    commit_full_stripped = _FakeInputApi(
        description='[compiler] Fix bug\n\nBody.\n',
        is_committing=True,
        dry_run=False)
    self.assertEqual(
        PRESUBMIT._CheckPriorityPrefix(commit_full_stripped, self.output_api),
        [])


if __name__ == '__main__':
  unittest.main()
