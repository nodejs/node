#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file exists to be able to mock some of the imports in ClusterFuzz. Since
# we only need a specific functionality in ClusterFuzz (Stack Analysis) we do
# not need most of the imports that it uses, so we fake them here in order to
# not crash when we run.


def empty_fn(*args, **kwargs):
  raise 'empty function was used'


kernel_utils, \
storage, \
fetch_artifact, \
settings, \
symbols_downloader, \
= [empty_fn] * 5


class ProjectConfig:

  def get(self):
    """Return empty config properties when ClusterFuzz tries to find a project
    config.
    """
    return None
