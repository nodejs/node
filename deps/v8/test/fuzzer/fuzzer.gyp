# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['../../gypfiles/toolchain.gypi', '../../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'v8_simple_json_fuzzer',
      'type': 'executable',
      'dependencies': [
        'json_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'json_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'json.cc',
      ],
    },
    {
      'target_name': 'v8_simple_parser_fuzzer',
      'type': 'executable',
      'dependencies': [
        'parser_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'parser_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'parser.cc',
      ],
    },
    {
      'target_name': 'v8_simple_regexp_fuzzer',
      'type': 'executable',
      'dependencies': [
        'regexp_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'regexp_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'regexp.cc',
      ],
    },
    {
      'target_name': 'v8_simple_multi_return_fuzzer',
      'type': 'executable',
      'dependencies': [
        'multi_return_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'multi_return_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        '../compiler/c-signature.h',
        '../compiler/call-helper.h',
        '../compiler/raw-machine-assembler-tester.h',
        'multi-return.cc',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_async_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_async_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_async_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-async.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_call_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_call_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_call_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-call.cc',
        '../common/wasm/test-signatures.h',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_code_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_code_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_code_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-code.cc',
        '../common/wasm/test-signatures.h',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_compile_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_compile_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_compile_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-compile.cc',
        '../common/wasm/test-signatures.h',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_data_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_data_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_data_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-data-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_function_sigs_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_function_sigs_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_function_sigs_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-function-sigs-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_globals_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_globals_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_globals_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-globals-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_imports_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_imports_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_imports_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-imports-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_memory_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_memory_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_memory_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-memory-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_names_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_names_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_names_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-names-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_types_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_types_section_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_types_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-types-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-fuzzer-common.cc',
        'wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'fuzzer_support',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8',
        '../../src/v8.gyp:v8_libbase',
        '../../src/v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'fuzzer-support.cc',
        'fuzzer-support.h',
      ],
      'conditions': [
        ['v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'fuzzer_run',
          'type': 'none',
          'dependencies': [
            'v8_simple_json_fuzzer',
            'v8_simple_parser_fuzzer',
            'v8_simple_regexp_fuzzer',
            'v8_simple_wasm_fuzzer',
          ],
          'includes': [
            '../../gypfiles/isolate.gypi',
          ],
          'sources': [
            'fuzzer.isolate',
          ],
        },
      ],
    }],
  ],
}
