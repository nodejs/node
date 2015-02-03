# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'pgd_basename': 'test_pgo',
  },
  'targets': [
    # In the PGO (Profile-Guided Optimization) build flow, we need to build the
    # target binary multiple times. To implement this flow with gyp, here we
    # define multiple 'executable' targets, each of which represents one build
    # particular build/profile stage. On tricky part to do this is that these
    # 'executable' targets should share the code itself so that profile data
    # can be reused among these 'executable' files. In other words, the only
    # differences among below 'executable' targets are:
    #   1) PGO (Profile-Guided Optimization) database, and
    #   2) linker options.
    # The following static library contains all the logic including entry point.
    # Basically we don't need to rebuild this target once we enter profiling
    # phase of PGO.
    {
      'target_name': 'test_pgo_main',
      'type': 'static_library',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WholeProgramOptimization': 'true',  # /GL
        },
        'VCLibrarianTool': {
          'LinkTimeCodeGeneration': 'true',
        },
      },
      'link_settings': {
        'msvs_settings': {
          'VCLinkerTool': {
            'ProfileGuidedDatabase': '$(OutDir)\\<(pgd_basename).pgd',
            'TargetMachine': '1',  # x86 - 32
            'SubSystem': '1',      # /SUBSYSTEM:CONSOLE
            # Tell ninja generator not to pass /ManifestFile:<filename> option
            # to the linker, because it causes LNK1268 error in PGO biuld.
            'GenerateManifest': 'false',
            # We need to specify 'libcmt.lib' here so that the linker can pick
            # up a valid entry point.
            'AdditionalDependencies': [
              'libcmt.lib',
            ],
          },
        },
      },
      'sources': [
        'inline_test.h',
        'inline_test.cc',
        'inline_test_main.cc',
      ],
    },
    {
      'target_name': 'test_pgo_instrument',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkTimeCodeGeneration': '2',
        },
      },
      'dependencies': [
        'test_pgo_main',
      ],
    },
    {
      'target_name': 'gen_profile_guided_database',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'action_main',
          'inputs': [],
          'outputs': [
            '$(OutDir)\\<(pgd_basename).pgd',
          ],
          'action': [
            'python', 'update_pgd.py',
            '--vcbindir', '$(VCInstallDir)bin',
            '--exe', '$(OutDir)\\test_pgo_instrument.exe',
            '--pgd', '$(OutDir)\\<(pgd_basename).pgd',
          ],
        },
      ],
      'dependencies': [
        'test_pgo_instrument',
      ],
    },
    {
      'target_name': 'test_pgo_optimize',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkTimeCodeGeneration': '3',
        },
      },
      'sources': [
        '$(OutDir)\\<(pgd_basename).pgd',
      ],
      'dependencies': [
        'test_pgo_main',
        'gen_profile_guided_database',
      ],
    },
    {
      'target_name': 'test_pgo_update',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkTimeCodeGeneration': '4',
        },
      },
      'sources': [
        '$(OutDir)\\<(pgd_basename).pgd',
      ],
      'dependencies': [
        'test_pgo_main',
      ],
    },
    # A helper target to dump link.exe's command line options. We can use the
    # output to determine if PGO (Profile-Guided Optimization) is available on
    # the test environment.
    {
      'target_name': 'gen_linker_option',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'action_main',
          'inputs': [],
          'outputs': [
            '$(OutDir)\\linker_options.txt',
          ],
          'action': [
            'cmd.exe', '/c link.exe > $(OutDir)\\linker_options.txt & exit 0',
          ],
        },
      ],
    },
  ]
}
