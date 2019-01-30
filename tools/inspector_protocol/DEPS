# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# ! DO NOT ROLL THIS FILE INTO CHROMIUM (or other repositories). !
# ! It's only useful for the standalone configuration in         !
# ! https://chromium.googlesource.com/deps/inspector_protocol/   !
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#
# This file configures gclient, a tool that installs dependencies
# at particular versions into this source tree. See
# https://chromium.googlesource.com/chromium/tools/depot_tools.git
# To fetch these dependencies, run "gclient sync". The fetch
# command (from depot_tools) will also run gclient sync.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

# The keys in this dictionary define where the external dependencies (values)
# will be mapped into this gclient. The root of the gclient will
# be the parent directory of the directory in which this DEPS file is.
deps = {
    # gn (the build tool) and clang-format.
    'buildtools':
        Var('chromium_git') + '/chromium/buildtools.git@' +
        '6fe4a3251488f7af86d64fc25cf442e817cf6133',
    # The toolchain definitions (clang C++ compiler etc.)
    'src/third_party/mini_chromium/mini_chromium':
        Var('chromium_git') + '/chromium/mini_chromium@' +
        '737433ebade4d446643c6c07daae02a67e8decca',
    # For writing unittests.
    'src/third_party/gtest/gtest':
        Var('chromium_git') + '/external/github.com/google/googletest@' +
        'c091b0469ab4c04ee9411ef770f32360945f4c53',
}

hooks = [
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'gn_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-gn',
      '--sha1_file',
      'buildtools/linux64/gn.sha1',
    ],
  },
]
recursedeps = ['buildtools']
