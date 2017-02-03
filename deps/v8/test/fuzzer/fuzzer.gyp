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
        'fuzzer_support_nocomponent',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_asmjs_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_asmjs_fuzzer_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_asmjs_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-asmjs.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-data-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-function-sigs-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-globals-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-imports-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-memory-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-names-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
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
        'fuzzer_support',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'wasm-types-section.cc',
        '../common/wasm/wasm-module-runner.cc',
        '../common/wasm/wasm-module-runner.h',
        'wasm-section-fuzzers.cc',
        'wasm-section-fuzzers.h',
      ],
    },
    {
      'target_name': 'fuzzer_support',
      'type': 'static_library',
      'dependencies': [
        '../../src/v8.gyp:v8',
        '../../src/v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'fuzzer-support.cc',
        'fuzzer-support.h',
      ],
    },
    {
      'target_name': 'fuzzer_support_nocomponent',
      'type': 'static_library',
      'dependencies': [
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
        ['component=="shared_library"', {
          # fuzzers can't be built against a shared library, so we need to
          # depend on the underlying static target in that case.
          'dependencies': ['../../src/v8.gyp:v8_maybe_snapshot'],
        }, {
          'dependencies': ['../../src/v8.gyp:v8'],
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
            'v8_simple_wasm_asmjs_fuzzer',
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
