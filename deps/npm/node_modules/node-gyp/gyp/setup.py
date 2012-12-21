#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from distutils.core import setup
from distutils.command.install import install
from distutils.command.install_lib import install_lib
from distutils.command.install_scripts import install_scripts

setup(
  name='gyp',
  version='0.1',
  description='Generate Your Projects',
  author='Chromium Authors',
  author_email='chromium-dev@googlegroups.com',
  url='http://code.google.com/p/gyp',
  package_dir = {'': 'pylib'},
  packages=['gyp', 'gyp.generator'],

  scripts = ['gyp'],
  cmdclass = {'install': install,
              'install_lib': install_lib,
              'install_scripts': install_scripts},
)
