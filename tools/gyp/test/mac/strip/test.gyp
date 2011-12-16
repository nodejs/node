# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These xcode_settings affect stripping:
# "Deployment postprocessing involves stripping the binary, and setting
# its file mode, owner, and group."
#'DEPLOYMENT_POSTPROCESSING': 'YES',

# "Specifies whether to strip symbol information from the binary.
# Prerequisite: $DEPLOYMENT_POSTPROCESSING = YES" "Default Value: 'NO'"
#'STRIP_INSTALLED_PRODUCT': 'YES',

# "Values:
# * all: Strips the binary completely, removing the symbol table and
#        relocation information
# * non-global: Strips nonglobal symbols but saves external symbols.
# * debugging: Strips debugging symbols but saves local and global
#              symbols."
# (maps to no flag, -x, -S in that order)
#'STRIP_STYLE': 'non-global',

# "Additional strip flags"
#'STRIPFLAGS': '-c',

# "YES: Copied binaries are stripped of debugging symbols. This does
# not cause the binary produced by the linker to be stripped. Use
# 'STRIP_INSTALLED_PRODUCT (Strip Linked Product)' to have the linker
# strip the binary."
#'COPY_PHASE_STRIP': 'NO',
{
  'targets': [
    {
      'target_name': 'no_postprocess',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'NO',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
    {
      'target_name': 'no_strip',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'NO',
      },
    },
    {
      'target_name': 'strip_all',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIP_STYLE': 'all',
      },
    },
    {
      'target_name': 'strip_nonglobal',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIP_STYLE': 'non-global',
      },
    },
    {
      'target_name': 'strip_debugging',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIP_STYLE': 'debugging',
      },
    },
    {
      'target_name': 'strip_all_custom_flags',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIP_STYLE': 'all',
        'STRIPFLAGS': '-c',
      },
    },
    {
      'target_name': 'strip_all_bundle',
      'type': 'shared_library',
      'mac_bundle': '1',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIP_STYLE': 'all',
      },
    },
    {
      'target_name': 'strip_save',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'STRIPFLAGS': '-s $(CHROMIUM_STRIP_SAVE_FILE)',
        'CHROMIUM_STRIP_SAVE_FILE': 'strip.saves',
      },
    },
  ],
}
