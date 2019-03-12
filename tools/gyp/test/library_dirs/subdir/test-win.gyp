# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # This creates a static library and puts it in a nonstandard location for
      # libraries-search-path-test.
      'target_name': 'mylib',
      'type': 'static_library',
      'standalone_static_library': 1,
      # This directory is NOT in the default library search locations. It also
      # MUST be passed in on the gyp command line:
      #
      #  -D abs_path_to_secret_library_location=/some_absolute_path
      #
      # The gyptest itself (../gyptest-library-dirs.py) provides this.
      'product_dir': '<(abs_path_to_secret_library_location)',
      'sources': [
        'mylib.cc',
      ],
    },
    {
      'target_name': 'libraries-search-path-test-lib-suffix',
      'type': 'executable',
      'dependencies': [
        # It is important to NOT list the mylib as a dependency here, because
        # some build systems will track it down based on its product_dir,
        # such that the link succeeds even without the library_dirs below.
        #
        # The point of this weird structuring is to ensure that 'library_dirs'
        # works as advertised, such that just '-lmylib' (or its equivalent)
        # works based on the directories that library_dirs puts in the library
        # link path.
        #
        # If 'mylib' was listed as a proper dependency here, the build system
        # would find it and link with its path on disk.
        #
        # Note that this implies 'mylib' must already be built when building
        # 'libraries-search-path-test' (see ../gyptest-library-dirs.py).
        #
        #'mylib',
      ],
      'sources': [
        'hello.cc',
      ],
      # Note that without this, the mylib library would not be found and
      # successfully linked.
      'library_dirs': [
        '<(abs_path_to_secret_library_location)',
      ],
      'link_settings': {
        'libraries': [
          '-lmylib.lib',
        ],
      },
    },
  ],
}
