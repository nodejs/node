#!/usr/bin/env python3
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest

# Configuring the path for the v8_presubmit module
TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(TOOLS_ROOT)

from v8_presubmit import FileContentsCache, CacheableSourceFileProcessor


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


if __name__ == '__main__':
  unittest.main()
