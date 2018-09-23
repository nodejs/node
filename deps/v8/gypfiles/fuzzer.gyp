# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      'target_name': 'v8_simple_json_fuzzer',
      'type': 'executable',
      'dependencies': [
        'json_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'json_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/json.cc',
      ],
    },
    {
      'target_name': 'v8_simple_parser_fuzzer',
      'type': 'executable',
      'dependencies': [
        'parser_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'parser_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/parser.cc',
      ],
    },
    {
      'target_name': 'v8_simple_regexp_builtins_fuzzer',
      'type': 'executable',
      'dependencies': [
        'regexp_builtins_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'regexp_builtins_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/regexp-builtins.cc',
        '../test/fuzzer/regexp_builtins/mjsunit.js.h',
      ],
    },
    {
      'target_name': 'v8_simple_regexp_fuzzer',
      'type': 'executable',
      'dependencies': [
        'regexp_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'regexp_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/regexp.cc',
      ],
    },
    {
      'target_name': 'v8_simple_multi_return_fuzzer',
      'type': 'executable',
      'dependencies': [
        'multi_return_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'multi_return_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/cctest/compiler/c-signature.h',
        '../test/cctest/compiler/call-helper.h',
        '../test/cctest/compiler/raw-machine-assembler-tester.h',
        '../test/fuzzer/multi-return.cc',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_async_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_async_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_async_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-async.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_code_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_code_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_code_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-code.cc',
        '../test/common/wasm/test-signatures.h',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_compile_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_compile_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_compile_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-compile.cc',
        '../test/common/wasm/test-signatures.h',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_data_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_data_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_data_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-data-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_function_sigs_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_function_sigs_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_function_sigs_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-function-sigs-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_globals_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_globals_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_globals_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-globals-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_imports_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_imports_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_imports_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-imports-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_memory_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_memory_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_memory_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-memory-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_names_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_names_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_names_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-names-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'v8_simple_wasm_types_section_fuzzer',
      'type': 'executable',
      'dependencies': [
        'wasm_types_section_fuzzer_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer.cc',
      ],
    },
    {
      'target_name': 'wasm_types_section_fuzzer_lib',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'fuzzer_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/wasm-types-section.cc',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '../test/fuzzer/wasm-fuzzer-common.cc',
        '../test/fuzzer/wasm-fuzzer-common.h',
      ],
    },
    {
      'target_name': 'fuzzer_support',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8',
        'v8.gyp:v8_libbase',
        'v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/fuzzer/fuzzer-support.cc',
        '../test/fuzzer/fuzzer-support.h',
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
}
