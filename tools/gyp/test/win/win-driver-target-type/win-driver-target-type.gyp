# Copyright (c) 2016 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'win_driver_target_type',
      'type': 'windows_driver',
      'msvs_target_version': 'Windows7',
      'sources': [
        'win-driver-target-type.c',
        'win-driver-target-type.h',
        'win-driver-target-type.rc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'wdmsec.lib',
            'ntoskrnl.lib',
            'hal.lib',
            'wmilib.lib',
            'bufferoverflowfastfailk.lib',
          ],
        },
        'VCCLCompilerTool': {
          'WarnAsError': 'false',
        },
      },
    },
  ]
}
