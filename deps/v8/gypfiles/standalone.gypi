# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Definitions to be used when building stand-alone V8 binaries.

{
  # We need to include toolchain.gypi here for third-party sources that don't
  # directly include it themselves.
  'includes': ['toolchain.gypi'],
  'variables': {
    'component%': 'static_library',
    'clang_xcode%': 0,
    # Track where uninitialized memory originates from. From fastest to
    # slowest: 0 - no tracking, 1 - track only the initial allocation site, 2
    # - track the chain of stores leading from allocation site to use site.
    'msan_track_origins%': 2,
    'visibility%': 'hidden',
    'v8_enable_backtrace%': 0,
    'v8_enable_i18n_support%': 1,
    'v8_deprecation_warnings': 1,
    'v8_imminent_deprecation_warnings': 1,
    'v8_check_microtasks_scopes_consistency': 'true',
    'msvs_multi_core_compile%': '1',
    'mac_deployment_target%': '10.7',
    'release_extra_cflags%': '',
    'variables': {
      'variables': {
        'variables': {
          'variables': {
            'conditions': [
              ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or \
                 OS=="netbsd" or OS=="mac" or OS=="qnx" or OS=="aix"', {
                # This handles the Unix platforms we generally deal with.
                # Anything else gets passed through, which probably won't work
                # very well; such hosts should pass an explicit target_arch
                # to gyp.
                'host_arch%': '<!pymod_do_main(detect_v8_host_arch)',
              }, {
                # OS!="linux" and OS!="freebsd" and OS!="openbsd" and
                # OS!="netbsd" and OS!="mac" and OS!="aix"
                'host_arch%': 'ia32',
              }],
            ],
          },
          'host_arch%': '<(host_arch)',
          'target_arch%': '<(host_arch)',

          # By default we build against a stable sysroot image to avoid
          # depending on the packages installed on the local machine. Set this
          # to 0 to build against locally installed headers and libraries (e.g.
          # if packaging for a linux distro)
          'use_sysroot%': 1,
        },
        'host_arch%': '<(host_arch)',
        'target_arch%': '<(target_arch)',
        'use_sysroot%': '<(use_sysroot)',
        'base_dir%': '<!(cd <(DEPTH) && python -c "import os; print os.getcwd()")',

        # Instrument for code coverage and use coverage wrapper to exclude some
        # files. Uses gcov if clang=0 is set explicitly. Otherwise,
        # sanitizer_coverage must be set too.
        'coverage%': 0,

        # Default sysroot if no sysroot can be provided.
        'sysroot%': '',

        'conditions': [
          # The system root for linux builds.
          ['OS=="linux" and use_sysroot==1', {
            'conditions': [
              ['target_arch=="arm"', {
                'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_jessie_arm-sysroot',
              }],
              ['target_arch=="x64"', {
                'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_jessie_amd64-sysroot',
              }],
              ['target_arch=="ia32"', {
                'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_jessie_i386-sysroot',
              }],
              ['target_arch=="mipsel"', {
                'sysroot%': '<!(cd <(DEPTH) && pwd -P)/build/linux/debian_jessie_mips-sysroot',
              }],
            ],
          }], # OS=="linux" and use_sysroot==1
        ],
      },
      'base_dir%': '<(base_dir)',
      'host_arch%': '<(host_arch)',
      'target_arch%': '<(target_arch)',
      'v8_target_arch%': '<(target_arch)',
      'coverage%': '<(coverage)',
      'sysroot%': '<(sysroot)',
      'asan%': 0,
      'lsan%': 0,
      'msan%': 0,
      'tsan%': 0,
      # Enable coverage gathering instrumentation in sanitizer tools. This flag
      # also controls coverage granularity (1 for function-level, 2 for
      # block-level, 3 for edge-level).
      'sanitizer_coverage%': 0,

      # Use dynamic libraries instrumented by one of the sanitizers
      # instead of the standard system libraries. Set this flag to download
      # prebuilt binaries from GCS.
      'use_prebuilt_instrumented_libraries%': 0,

      # Use libc++ (buildtools/third_party/libc++ and
      # buildtools/third_party/libc++abi) instead of stdlibc++ as standard
      # library. This is intended to be used for instrumented builds.
      'use_custom_libcxx%': 0,

      'clang_dir%': '<(base_dir)/third_party/llvm-build/Release+Asserts',
      'make_clang_dir%': '<(base_dir)/third_party/llvm-build/Release+Asserts',

      # Control Flow Integrity for virtual calls and casts.
      # See http://clang.llvm.org/docs/ControlFlowIntegrity.html
      'cfi_vptr%': 0,
      'cfi_diag%': 0,

      'cfi_blacklist%': '<(base_dir)/tools/cfi/blacklist.txt',

      # Set to 1 to enable fast builds.
      # TODO(machenbach): Only configured for windows.
      'fastbuild%': 0,

      # goma settings.
      # 1 to use goma.
      # If no gomadir is set, it uses the default gomadir.
      'use_goma%': 0,
      'gomadir%': '',

      'test_isolation_mode%': 'noop',

      # By default, use ICU data file (icudtl.dat).
      'icu_use_data_file_flag%': 1,

      'conditions': [
        # Set default gomadir.
        ['OS=="win"', {
          'gomadir': 'c:\\goma\\goma-win',
        }, {
          'gomadir': '<!(/bin/echo -n ${HOME}/goma)',
        }],
        ['host_arch!="ppc" and host_arch!="ppc64" and host_arch!="ppc64le" and host_arch!="s390" and host_arch!="s390x"', {
          'host_clang%': 1,
        }, {
          'host_clang%': 0,
        }],
        # linux_use_bundled_gold: whether to use the gold linker binary checked
        # into third_party/binutils.  Force this off via GYP_DEFINES when you
        # are using a custom toolchain and need to control -B in ldflags.
        # Do not use 32-bit gold on 32-bit hosts as it runs out address space
        # for component=static_library builds.
        ['((OS=="linux" or OS=="android") and (target_arch=="x64" or target_arch=="arm" or (target_arch=="ia32" and host_arch=="x64"))) or (OS=="linux" and target_arch=="mipsel")', {
          'linux_use_bundled_gold%': 1,
        }, {
          'linux_use_bundled_gold%': 0,
        }],
      ],
    },
    'base_dir%': '<(base_dir)',
    'clang_dir%': '<(clang_dir)',
    'make_clang_dir%': '<(make_clang_dir)',
    'host_arch%': '<(host_arch)',
    'host_clang%': '<(host_clang)',
    'target_arch%': '<(target_arch)',
    'v8_target_arch%': '<(v8_target_arch)',
    'werror%': '-Werror',
    'use_goma%': '<(use_goma)',
    'gomadir%': '<(gomadir)',
    'asan%': '<(asan)',
    'lsan%': '<(lsan)',
    'msan%': '<(msan)',
    'tsan%': '<(tsan)',
    'sanitizer_coverage%': '<(sanitizer_coverage)',
    'use_prebuilt_instrumented_libraries%': '<(use_prebuilt_instrumented_libraries)',
    'use_custom_libcxx%': '<(use_custom_libcxx)',
    'linux_use_bundled_gold%': '<(linux_use_bundled_gold)',
    'cfi_vptr%': '<(cfi_vptr)',
    'cfi_diag%': '<(cfi_diag)',
    'cfi_blacklist%': '<(cfi_blacklist)',
    'test_isolation_mode%': '<(test_isolation_mode)',
    'fastbuild%': '<(fastbuild)',
    'coverage%': '<(coverage)',
    'sysroot%': '<(sysroot)',
    'icu_use_data_file_flag%': '<(icu_use_data_file_flag)',

    # Add a simple extras solely for the purpose of the cctests
    'v8_extra_library_files': ['../test/cctest/test-extra.js'],
    'v8_experimental_extra_library_files': ['../test/cctest/test-experimental-extra.js'],

    # .gyp files or targets should set v8_code to 1 if they build V8 specific
    # code, as opposed to external code.  This variable is used to control such
    # things as the set of warnings to enable, and whether warnings are treated
    # as errors.
    'v8_code%': 0,

    # Speeds up Debug builds:
    # 0 - Compiler optimizations off (debuggable) (default). This may
    #     be 5x slower than Release (or worse).
    # 1 - Turn on optimizations and disable slow DCHECKs, but leave
    #     V8_ENABLE_CHECKS and most other assertions enabled.  This may cause
    #     some v8 tests to fail in the Debug configuration.  This roughly
    #     matches the performance of a Release build and can be used by
    #     embedders that need to build their own code as debug but don't want
    #     or need a debug version of V8. This should produce near-release
    #     speeds.
    'v8_optimized_debug%': 0,

    # Use external files for startup data blobs:
    # the JS builtins sources and the start snapshot.
    # Embedders that don't use standalone.gypi will need to add
    # their own default value.
    'v8_use_external_startup_data%': 1,

    # Relative path to icu.gyp from this file.
    'icu_gyp_path': '../third_party/icu/icu.gyp',

    'conditions': [
      ['(v8_target_arch=="arm" and host_arch!="arm") or \
        (v8_target_arch=="arm64" and host_arch!="arm64") or \
        (v8_target_arch=="mipsel" and host_arch!="mipsel") or \
        (v8_target_arch=="mips64el" and host_arch!="mips64el") or \
        (v8_target_arch=="x64" and host_arch!="x64") or \
        (OS=="android" or OS=="qnx")', {
        'want_separate_host_toolset': 1,
      }, {
        'want_separate_host_toolset': 0,
      }],
      ['OS == "win"', {
        'os_posix%': 0,
      }, {
        'os_posix%': 1,
      }],
      ['OS=="win" and use_goma==1', {
        # goma doesn't support pch yet.
        'chromium_win_pch': 0,
        # goma doesn't support PDB yet.
        'fastbuild%': 1,
      }],
      ['((v8_target_arch=="ia32" or v8_target_arch=="x64") and \
        (OS=="linux" or OS=="mac")) or (v8_target_arch=="ppc64" and OS=="linux")', {
        'v8_enable_gdbjit%': 1,
      }, {
        'v8_enable_gdbjit%': 0,
      }],
      ['(OS=="linux" or OS=="mac") and (target_arch=="ia32" or target_arch=="x64") and \
        v8_target_arch!="x32"', {
        'clang%': 1,
      }, {
        'clang%': 0,
      }],
      ['asan==1 or lsan==1 or msan==1 or tsan==1', {
        'clang%': 1,
        'use_allocator%': 'none',
      }],
      ['asan==1 and OS=="linux"', {
        'use_custom_libcxx%': 1,
      }],
      ['tsan==1', {
        'use_custom_libcxx%': 1,
      }],
      ['msan==1', {
        # Use a just-built, MSan-instrumented libc++ instead of the system-wide
        # libstdc++. This is required to avoid false positive reports whenever
        # the C++ standard library is used.
        'use_custom_libcxx%': 1,
      }],
      ['OS=="android"', {
        # Location of Android NDK.
        'variables': {
          'variables': {
            # The Android toolchain needs to use the absolute path to the NDK
            # because it is used at different levels in the GYP files.
            'android_ndk_root%': '<(base_dir)/third_party/android_tools/ndk/',
            'android_host_arch%': "<!(uname -m | sed -e 's/i[3456]86/x86/')",
            # Version of the NDK. Used to ensure full rebuilds on NDK rolls.
            'android_ndk_version%': 'r12b',
            'host_os%': "<!(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')",
            'os_folder_name%': "<!(uname -s | sed -e 's/Linux/linux/;s/Darwin/darwin/')",
          },

          # Copy conditionally-set variables out one scope.
          'android_ndk_root%': '<(android_ndk_root)',
          'android_ndk_version%': '<(android_ndk_version)',
          'host_os%': '<(host_os)',
          'os_folder_name%': '<(os_folder_name)',

          'conditions': [
            ['target_arch == "ia32"', {
              'android_toolchain%': '<(android_ndk_root)/toolchains/x86-4.9/prebuilt/<(os_folder_name)-<(android_host_arch)/bin',
              'android_target_arch%': 'x86',
              'android_target_platform%': '16',
              'arm_version%': 'default',
            }],
            ['target_arch == "x64"', {
              'android_toolchain%': '<(android_ndk_root)/toolchains/x86_64-4.9/prebuilt/<(os_folder_name)-<(android_host_arch)/bin',
              'android_target_arch%': 'x86_64',
              'android_target_platform%': '21',
              'arm_version%': 'default',
            }],
            ['target_arch=="arm"', {
              'android_toolchain%': '<(android_ndk_root)/toolchains/arm-linux-androideabi-4.9/prebuilt/<(os_folder_name)-<(android_host_arch)/bin',
              'android_target_arch%': 'arm',
              'android_target_platform%': '16',
              'arm_version%': 7,
            }],
            ['target_arch == "arm64"', {
              'android_toolchain%': '<(android_ndk_root)/toolchains/aarch64-linux-android-4.9/prebuilt/<(os_folder_name)-<(android_host_arch)/bin',
              'android_target_arch%': 'arm64',
              'android_target_platform%': '21',
              'arm_version%': 'default',
            }],
            ['target_arch == "mipsel"', {
              'android_toolchain%': '<(android_ndk_root)/toolchains/mipsel-linux-android-4.9/prebuilt/<(os_folder_name)-<(android_host_arch)/bin',
              'android_target_arch%': 'mips',
              'android_target_platform%': '16',
              'arm_version%': 'default',
            }],
            ['target_arch == "mips64el"', {
              'android_toolchain%': '<(android_ndk_root)/toolchains/mips64el-linux-android-4.9/prebuilt/<(os_folder_name)-<(android_host_arch)/bin',
              'android_target_arch%': 'mips64',
              'android_target_platform%': '21',
              'arm_version%': 'default',
            }],
          ],
        },

        # Copy conditionally-set variables out one scope.
        'android_ndk_version%': '<(android_ndk_version)',
        'android_target_arch%': '<(android_target_arch)',
        'android_target_platform%': '<(android_target_platform)',
        'android_toolchain%': '<(android_toolchain)',
        'arm_version%': '<(arm_version)',
        'host_os%': '<(host_os)',

        # Print to stdout on Android.
        'v8_android_log_stdout%': 1,

        'conditions': [
          ['android_ndk_root==""', {
            'variables': {
              'android_sysroot': '<(android_toolchain)/sysroot/',
              'android_stl': '<(android_toolchain)/sources/cxx-stl/',
            },
            'conditions': [
              ['target_arch=="x64"', {
                'android_lib': '<(android_sysroot)/usr/lib64',
              }, {
                'android_lib': '<(android_sysroot)/usr/lib',
              }],
            ],
            'android_libcpp_include': '<(android_stl)/llvm-libc++/libcxx/include',
            'android_libcpp_abi_include': '<(android_stl)/llvm-libc++abi/libcxxabi/include',
            'android_libcpp_libs': '<(android_stl)/llvm-libc++/libs',
            'android_support_include': '<(android_toolchain)/sources/android/support/include',
            'android_sysroot': '<(android_sysroot)',
          }, {
            'variables': {
              'android_sysroot': '<(android_ndk_root)/platforms/android-<(android_target_platform)/arch-<(android_target_arch)',
              'android_stl': '<(android_ndk_root)/sources/cxx-stl/',
            },
            'conditions': [
              ['target_arch=="x64"', {
                'android_lib': '<(android_sysroot)/usr/lib64',
              }, {
                'android_lib': '<(android_sysroot)/usr/lib',
              }],
            ],
            'android_libcpp_include': '<(android_stl)/llvm-libc++/libcxx/include',
            'android_libcpp_abi_include': '<(android_stl)/llvm-libc++abi/libcxxabi/include',
            'android_libcpp_libs': '<(android_stl)/llvm-libc++/libs',
            'android_support_include': '<(android_ndk_root)/sources/android/support/include',
            'android_sysroot': '<(android_sysroot)',
          }],
        ],
        'android_libcpp_library': 'c++_static',
      }],  # OS=="android"
      ['host_clang==1', {
        'conditions':[
          ['OS=="android"', {
            'host_ld': '<!(which ld)',
            'host_ranlib': '<!(which ranlib)',
          }],
        ],
        'host_cc': '<(clang_dir)/bin/clang',
        'host_cxx': '<(clang_dir)/bin/clang++',
      }, {
        'host_cc': '<!(which gcc)',
        'host_cxx': '<!(which g++)',
      }],
    ],
    # Default ARM variable settings.
    'arm_version%': 'default',
    'arm_fpu%': 'vfpv3',
    'arm_float_abi%': 'default',
    'arm_thumb': 'default',

    # Default MIPS variable settings.
    'mips_arch_variant%': 'r2',
    # Possible values fp32, fp64, fpxx.
    # fp32 - 32 32-bit FPU registers are available, doubles are placed in
    #        register pairs.
    # fp64 - 32 64-bit FPU registers are available.
    # fpxx - compatibility mode, it chooses fp32 or fp64 depending on runtime
    #        detection
    'mips_fpu_mode%': 'fp32',
  },
  'target_defaults': {
    'variables': {
      'v8_code%': '<(v8_code)',
      'clang_warning_flags': [
        '-Wsign-compare',
        # TODO(thakis): https://crbug.com/604888
        '-Wno-undefined-var-template',
        # TODO(yangguo): issue 5258
        '-Wno-nonportable-include-path',
        '-Wno-tautological-constant-compare',
      ],
      'conditions':[
        ['OS=="android"', {
          'host_os%': '<(host_os)',
        }],
      ],
    },
    'includes': [ 'set_clang_warning_flags.gypi', ],
    'default_configuration': 'Debug',
    'configurations': {
      'DebugBaseCommon': {
        'conditions': [
          ['OS=="aix"', {
            'cflags': [ '-g', '-Og', '-gxcoff' ],
          }, {
            'cflags': [ '-g', '-O0' ],
          }],
        ],
      },
      'Optdebug': {
        'inherit_from': [ 'DebugBaseCommon', 'DebugBase1' ],
      },
      'Debug': {
        # Xcode insists on this empty entry.
      },
      'Release': {
        'cflags+': ['<@(release_extra_cflags)'],
      },
      'conditions': [
        ['OS=="win"', {
          'Optdebug_x64': {
            'inherit_from': ['Optdebug'],
          },
          'Debug_x64': {
            'inherit_from': ['Debug'],
          },
          'Release_x64': {
            'inherit_from': ['Release'],
          },
        }],
      ],
    },
    'conditions':[
      ['clang==0', {
        'cflags+': [
          '-Wno-uninitialized',
        ],
      }],
      ['clang==1 or host_clang==1', {
        # This is here so that all files get recompiled after a clang roll and
        # when turning clang on or off.
        # (defines are passed via the command line, and build systems rebuild
        # things when their commandline changes). Nothing should ever read this
        # define.
        'defines': ['CR_CLANG_REVISION=<!(python <(DEPTH)/tools/clang/scripts/update.py --print-revision)'],
      }],
      ['clang==1 and target_arch=="ia32"', {
        'cflags': ['-mstack-alignment=16', '-mstackrealign'],
      }],
      ['fastbuild!=0', {
        'conditions': [
          ['OS=="win" and fastbuild==1', {
            'msvs_settings': {
              'VCLinkerTool': {
                # This tells the linker to generate .pdbs, so that
                # we can get meaningful stack traces.
                'GenerateDebugInformation': 'true',
              },
              'VCCLCompilerTool': {
                # No debug info to be generated by compiler.
                'DebugInformationFormat': '0',
              },
            },
          }],
        ],
      }],  # fastbuild!=0
    ],
    'target_conditions': [
      ['v8_code == 0', {
        'defines!': [
          'DEBUG',
        ],
        'conditions': [
          ['os_posix == 1 and OS != "mac"', {
            # We don't want to get warnings from third-party code,
            # so remove any existing warning-enabling flags like -Wall.
            'cflags!': [
              '-pedantic',
              '-Wall',
              '-Werror',
              '-Wextra',
              '-Wshorten-64-to-32',
            ],
            'cflags+': [
              # Clang considers the `register` keyword as deprecated, but
              # ICU uses it all over the place.
              '-Wno-deprecated-register',
              # ICU uses its own deprecated functions.
              '-Wno-deprecated-declarations',
              # ICU prefers `a && b || c` over `(a && b) || c`.
              '-Wno-logical-op-parentheses',
              # ICU has some `unsigned < 0` checks.
              '-Wno-tautological-compare',
              # uresdata.c has switch(RES_GET_TYPE(x)) code. The
              # RES_GET_TYPE macro returns an UResType enum, but some switch
              # statement contains case values that aren't part of that
              # enum (e.g. URES_TABLE32 which is in UResInternalType). This
              # is on purpose.
              '-Wno-switch',
            ],
            'cflags_cc!': [
              '-Wnon-virtual-dtor',
            ],
          }],
          ['OS == "mac"', {
            'xcode_settings': {
              'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',    # -Werror
            },
          }],
          ['OS == "win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'WarnAsError': 'false',
              },
            },
          }],
        ],
      }],
    ],
  },
  'conditions': [
    ['os_posix==1 and OS!="mac"', {
      'target_defaults': {
        'conditions': [
          # Common options for AddressSanitizer, LeakSanitizer,
          # ThreadSanitizer, MemorySanitizer and CFI builds.
          ['asan==1 or lsan==1 or tsan==1 or msan==1 or cfi_vptr==1', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fno-omit-frame-pointer',
                  '-gline-tables-only',
                ],
                'cflags!': [
                  '-fomit-frame-pointer',
                ],
              }],
            ],
          }],
          ['asan==1', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fsanitize=address',
                ],
                'ldflags': [
                  '-fsanitize=address',
                ],
                'defines': [
                  'ADDRESS_SANITIZER',
                ],
              }],
            ],
          }],
          ['lsan==1', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fsanitize=leak',
                ],
                'ldflags': [
                  '-fsanitize=leak',
                ],
                'defines': [
                  'LEAK_SANITIZER',
                ],
              }],
            ],
          }],
          ['tsan==1', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fsanitize=thread',
                ],
                'ldflags': [
                  '-fsanitize=thread',
                ],
                'defines': [
                  'THREAD_SANITIZER',
                ],
              }],
            ],
          }],
          ['msan==1', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fsanitize=memory',
                  '-fsanitize-memory-track-origins=<(msan_track_origins)',
                  '-fPIC',
                ],
                'ldflags': [
                  '-fsanitize=memory',
                  '-pie',
                ],
                'defines': [
                  'MEMORY_SANITIZER',
                ],
              }],
            ],
          }],
          ['use_prebuilt_instrumented_libraries==1', {
            'dependencies': [
              '<(DEPTH)/third_party/instrumented_libraries/instrumented_libraries.gyp:prebuilt_instrumented_libraries',
            ],
          }],
          ['use_custom_libcxx==1', {
            'dependencies': [
              '<(DEPTH)/buildtools/third_party/libc++/libc++.gyp:libcxx_proxy',
            ],
          }],
          ['sanitizer_coverage!=0', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fsanitize-coverage=<(sanitizer_coverage)',
                ],
                'defines': [
                  'SANITIZER_COVERAGE',
                ],
              }],
            ],
          }],
          ['linux_use_bundled_gold==1', {
            # Put our binutils, which contains gold in the search path. We pass
            # the path to gold to the compiler. gyp leaves unspecified what the
            # cwd is when running the compiler, so the normal gyp path-munging
            # fails us. This hack gets the right path.
            'ldflags': [
              # Note, Chromium allows ia32 host arch as well, we limit this to
              # x64 in v8.
              '-B<(base_dir)/third_party/binutils/Linux_x64/Release/bin',
            ],
          }],
          ['sysroot!="" and clang==1', {
            'target_conditions': [
              ['_toolset=="target"', {
                'variables': {
                  'ld_paths': ['<!(<(DEPTH)/build/linux/sysroot_ld_path.sh <(sysroot))'],
                },
                'cflags': [
                  '--sysroot=<(sysroot)',
                ],
                'ldflags': [
                  '--sysroot=<(sysroot)',
                  '<!(<(base_dir)/gypfiles/sysroot_ld_flags.sh <@(ld_paths))',
                ],
              }]]
          }],
        ],
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
       'conditions': [
          ['asan==1', {
            'xcode_settings': {
              # FIXME(machenbach): This is outdated compared to common.gypi.
              'OTHER_CFLAGS+': [
                '-fno-omit-frame-pointer',
                '-gline-tables-only',
                '-fsanitize=address',
                '-w',  # http://crbug.com/162783
              ],
              'OTHER_CFLAGS!': [
                '-fomit-frame-pointer',
              ],
              'defines': [
                'ADDRESS_SANITIZER',
              ],
            },
            'dependencies': [
              '<(DEPTH)/gypfiles/mac/asan.gyp:asan_dynamic_runtime',
            ],
            'target_conditions': [
              ['_type!="static_library"', {
                'xcode_settings': {'OTHER_LDFLAGS': ['-fsanitize=address']},
              }],
            ],
          }],
          ['sanitizer_coverage!=0', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-fsanitize-coverage=<(sanitizer_coverage)',
                ],
                'defines': [
                  'SANITIZER_COVERAGE',
                ],
              }],
            ],
          }],
        ],
      },  # target_defaults
    }],  # OS=="mac"
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
       or OS=="netbsd" or OS=="aix"', {
      'target_defaults': {
        'cflags': [
          '-Wall',
          '<(werror)',
          '-Wno-unused-parameter',
          '-pthread',
          '-pedantic',
          '-Wno-missing-field-initializers',
          '-Wno-gnu-zero-variadic-macro-arguments',
        ],
        'cflags_cc': [
          '-Wnon-virtual-dtor',
          '-fno-exceptions',
          '-fno-rtti',
          '-std=gnu++11',
        ],
        'ldflags': [ '-pthread', ],
        'conditions': [
          # Don't warn about TRACE_EVENT_* macros with zero arguments passed to
          # ##__VA_ARGS__. C99 strict mode prohibits having zero variadic macro
          # arguments in gcc.
          [ 'clang==0', {
            'cflags!' : [
              '-pedantic' ,
              # Don't warn about unrecognized command line option.
              '-Wno-gnu-zero-variadic-macro-arguments',
            ],
            'cflags' : [
              # Disable gcc warnings for optimizations based on the assumption
              # that signed overflow does not occur. Generates false positives
              # (see http://crbug.com/v8/6341).
              "-Wno-strict-overflow",
              # Don't rely on strict aliasing; v8 does weird pointer casts all
              # over the place.
              '-fno-strict-aliasing',
            ],
          }, {
            'cflags' : [
              # TODO(hans): https://crbug.com/767059
              '-Wno-tautological-constant-compare',
            ],
          }],
          [ 'clang==1 and (v8_target_arch=="x64" or v8_target_arch=="arm64" \
            or v8_target_arch=="mips64el")', {
            'cflags': [ '-Wshorten-64-to-32' ],
          }],
          [ 'host_arch=="ppc64" and OS!="aix"', {
            'cflags': [ '-mminimal-toc' ],
          }],
          [ 'visibility=="hidden" and v8_enable_backtrace==0', {
            'cflags': [ '-fvisibility=hidden' ],
          }],
          [ 'component=="shared_library"', {
            'cflags': [ '-fPIC', ],
          }],
          [ 'clang==0 and coverage==1', {
            'cflags': [ '-fprofile-arcs', '-ftest-coverage'],
            'ldflags': [ '-fprofile-arcs'],
          }],
        ],
      },
    }],
    # 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"
    #  or OS=="netbsd"'
    ['OS=="qnx"', {
      'target_defaults': {
        'cflags': [
          '-Wall',
          '<(werror)',
          '-Wno-unused-parameter',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          '-Wno-gnu-zero-variadic-macro-arguments',
        ],
        'cflags_cc': [
          '-Wnon-virtual-dtor',
          '-fno-exceptions',
          '-fno-rtti',
          '-std=gnu++11',
        ],
        'conditions': [
          [ 'visibility=="hidden"', {
            'cflags': [ '-fvisibility=hidden' ],
          }],
          [ 'component=="shared_library"', {
            'cflags': [ '-fPIC' ],
          }],
        ],
        'target_conditions': [
          [ '_toolset=="host" and host_os=="linux"', {
            'cflags': [ '-pthread' ],
            'ldflags': [ '-pthread' ],
            'libraries': [ '-lrt' ],
          }],
          [ '_toolset=="target"', {
            'cflags': [ '-Wno-psabi' ],
            'libraries': [ '-lbacktrace', '-lsocket', '-lm' ],
          }],
        ],
      },
    }],  # OS=="qnx"
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_DEPRECATE',
          '_USING_V110_SDK71_',
        ],
        'conditions': [
          ['component=="static_library"', {
            'defines': [
              '_HAS_EXCEPTIONS=0',
            ],
          }],
        ],
        'msvs_cygwin_shell': 0,
        'msvs_disabled_warnings': [
          # C4091: 'typedef ': ignored on left of 'X' when no variable is
          #                    declared.
          # This happens in a number of Windows headers. Dumb.
          4091,

          # C4127: conditional expression is constant
          # This warning can in theory catch dead code and other problems, but
          # triggers in far too many desirable cases where the conditional
          # expression is either set by macros or corresponds some legitimate
          # compile-time constant expression (due to constant template args,
          # conditionals comparing the sizes of different types, etc.).  Some of
          # these can be worked around, but it's not worth it.
          4127,

          # C4351: new behavior: elements of array 'array' will be default
          #        initialized
          # This is a silly "warning" that basically just alerts you that the
          # compiler is going to actually follow the language spec like it's
          # supposed to, instead of not following it like old buggy versions
          # did.  There's absolutely no reason to turn this on.
          4351,

          # C4355: 'this': used in base member initializer list
          # It's commonly useful to pass |this| to objects in a class'
          # initializer list.  While this warning can catch real bugs, most of
          # the time the constructors in question don't attempt to call methods
          # on the passed-in pointer (until later), and annotating every legit
          # usage of this is simply more hassle than the warning is worth.
          4355,

          # C4503: 'identifier': decorated name length exceeded, name was
          #        truncated
          # This only means that some long error messages might have truncated
          # identifiers in the presence of lots of templates.  It has no effect
          # on program correctness and there's no real reason to waste time
          # trying to prevent it.
          4503,

          # Warning C4589 says: "Constructor of abstract class ignores
          # initializer for virtual base class." Disable this warning because it
          # is flaky in VS 2015 RTM. It triggers on compiler generated
          # copy-constructors in some cases.
          4589,

          # C4611: interaction between 'function' and C++ object destruction is
          #        non-portable
          # This warning is unavoidable when using e.g. setjmp/longjmp.  MSDN
          # suggests using exceptions instead of setjmp/longjmp for C++, but
          # Chromium code compiles without exception support.  We therefore have
          # to use setjmp/longjmp for e.g. JPEG decode error handling, which
          # means we have to turn off this warning (and be careful about how
          # object destruction happens in such cases).
          4611,

          # TODO(jochen): These warnings are level 4. They will be slowly
          # removed as code is fixed.
          4100, # Unreferenced formal parameter
          4121, # Alignment of a member was sensitive to packing
          4244, # Conversion from 'type1' to 'type2', possible loss of data
          4302, # Truncation from 'type 1' to 'type 2'
          4309, # Truncation of constant value
          4311, # Pointer truncation from 'type' to 'type'
          4312, # Conversion from 'type1' to 'type2' of greater size
          4505, # Unreferenced local function has been removed
          4510, # Default constructor could not be generated
          4512, # Assignment operator could not be generated
          4610, # Object can never be instantiated
          4800, # Forcing value to bool.
          4838, # Narrowing conversion. Doesn't seem to be very useful.
          4995, # 'X': name was marked as #pragma deprecated
          4996, # 'X': was declared deprecated (for GetVersionEx).

          # These are variable shadowing warnings that are new in VS2015. We
          # should work through these at some point -- they may be removed from
          # the RTM release in the /W4 set.
          4456, 4457, 4458, 4459,
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'Detect64BitPortabilityProblems': 'false',
            'conditions': [
              [ 'msvs_multi_core_compile', {
                'AdditionalOptions': ['/MP'],
              }],
              ['component=="shared_library"', {
                'ExceptionHandling': '1',  # /EHsc
              }, {
                'ExceptionHandling': '0',
              }],
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
            'conditions': [
              ['v8_target_arch=="x64"', {
                'TargetMachine': '17',  # x64
              }, {
                'TargetMachine': '1',  # ia32
              }],
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'ws2_32.lib',
            ],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'FixedBaseAddress': '1',
            # LinkIncremental values:
            #   0 == default
            #   1 == /INCREMENTAL:NO
            #   2 == /INCREMENTAL
            'LinkIncremental': '1',
            # SubSystem values:
            #   0 == not set
            #   1 == /SUBSYSTEM:CONSOLE
            #   2 == /SUBSYSTEM:WINDOWS
            'SubSystem': '1',

            'conditions': [
              ['v8_enable_i18n_support==1', {
                'AdditionalDependencies': [
                  'advapi32.lib',
                ],
              }],
              ['v8_target_arch=="x64"', {
                'MinimumRequiredVersion': '5.02',  # Server 2003.
                'TargetMachine': '17',  # x64
              }, {
                'MinimumRequiredVersion': '5.01',  # XP.
                'TargetMachine': '1',  # ia32
              }],
            ],
          },
          'conditions': [
            ['clang==1', {
              'VCCLCompilerTool': {
                'AdditionalOptions': [
                  # Don't warn about unused function parameters.
                  # (This is also used on other platforms.)
                  '-Wno-unused-parameter',
                  # Don't warn about the "struct foo f = {0};" initialization
                  # pattern.
                  '-Wno-missing-field-initializers',

                  # TODO(hans): Make this list shorter eventually, http://crbug.com/504657
                  '-Qunused-arguments',  # http://crbug.com/504658
                  '-Wno-microsoft-enum-value',  # http://crbug.com/505296
                  '-Wno-unknown-pragmas',  # http://crbug.com/505314
                  '-Wno-microsoft-cast',  # http://crbug.com/550065
                ],
              },
            }],
            ['clang==1 and MSVS_VERSION == "2013"', {
              'VCCLCompilerTool': {
                'AdditionalOptions': [
                  '-fmsc-version=1800',
                ],
              },
            }],
            ['clang==1 and MSVS_VERSION == "2015"', {
              'VCCLCompilerTool': {
                'AdditionalOptions': [
                  '-fmsc-version=1900',
                ],
              },
            }],
          ],
        },
      },
    }],  # OS=="win"
    ['OS=="mac"', {
      'xcode_settings': {
        'SDKROOT': 'macosx',
        'SYMROOT': '<(DEPTH)/xcodebuild',
      },
      'target_defaults': {
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'c99',         # -std=c99
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          # GCC_INLINES_ARE_PRIVATE_EXTERN maps to -fvisibility-inlines-hidden
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_WARN_NON_VIRTUAL_DESTRUCTOR': 'YES', # -Wnon-virtual-dtor
          # MACOSX_DEPLOYMENT_TARGET maps to -mmacosx-version-min
          'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'SYMROOT': '<(DEPTH)/xcodebuild',
          'USE_HEADERMAP': 'NO',
          'OTHER_CFLAGS': [
            '-fno-strict-aliasing',
          ],
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-Wno-unused-parameter',
            # Don't warn about the "struct foo f = {0};" initialization pattern.
            '-Wno-missing-field-initializers',
            '-Wno-gnu-zero-variadic-macro-arguments',
          ],
        },
        'conditions': [
          ['werror==""', {
            'xcode_settings': {'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO'},
          }, {
            'xcode_settings': {'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES'},
          }],
          ['clang==1', {
            'xcode_settings': {
              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'CLANG_CXX_LANGUAGE_STANDARD': 'c++11',  # -std=c++11
            },
            'conditions': [
              ['clang_xcode==0', {
                'xcode_settings': {
                  'CC': '<(clang_dir)/bin/clang',
                  'LDPLUSPLUS': '<(clang_dir)/bin/clang++',
                  'CLANG_CXX_LIBRARY': 'libc++'
                },
              }],
              ['v8_target_arch=="x64" or v8_target_arch=="arm64" \
                or v8_target_arch=="mips64el"', {
                'xcode_settings': {'WARNING_CFLAGS': ['-Wshorten-64-to-32']},
              }],
            ],
          }],
        ],
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
    ['OS=="android"', {
      'target_defaults': {
        'defines': [
          'ANDROID',
        ],
        'configurations': {
          'Release': {
            'cflags': [
              '-fomit-frame-pointer',
            ],
          },  # Release
        },  # configurations
        'cflags': [ '-Wno-abi', '-Wall', '-W', '-Wno-unused-parameter'],
        'cflags_cc': [ '-Wnon-virtual-dtor', '-fno-rtti', '-fno-exceptions',
                       '-std=gnu++11' ],
        'target_conditions': [
          ['_toolset=="target"', {
            'cflags!': [
              '-pthread',  # Not supported by Android toolchain.
            ],
            'cflags': [
              '-ffunction-sections',
              '-funwind-tables',
              '-fstack-protector',
              '-fno-short-enums',
              '-finline-limit=64',
              '-Wa,--noexecstack',
              '--sysroot=<(android_sysroot)',
            ],
            'cflags_cc': [
              '-isystem<(android_libcpp_include)',
              '-isystem<(android_libcpp_abi_include)',
              '-isystem<(android_support_include)',
            ],
            'defines': [
              'ANDROID',
              #'__GNU_SOURCE=1',  # Necessary for clone()
              'HAVE_OFF64_T',
              'HAVE_SYS_UIO_H',
              'ANDROID_BINSIZE_HACK', # Enable temporary hacks to reduce binsize.
              'ANDROID_NDK_VERSION=<(android_ndk_version)',
            ],
            'ldflags!': [
              '-pthread',  # Not supported by Android toolchain.
            ],
            'ldflags': [
              '-Wl,--no-undefined',
              '--sysroot=<(android_sysroot)',
              '-nostdlib',
            ],
            'libraries!': [
                '-lrt',  # librt is built into Bionic.
                # Not supported by Android toolchain.
                # Where do these come from?  Can't find references in
                # any Chromium gyp or gypi file.  Maybe they come from
                # gyp itself?
                '-lpthread', '-lnss3', '-lnssutil3', '-lsmime3', '-lplds4', '-lplc4', '-lnspr4',
              ],
              'libraries': [
                '-l<(android_libcpp_library)',
                '-latomic',
                # Manually link the libgcc.a that the cross compiler uses.
                '<!(<(android_toolchain)/*-gcc -print-libgcc-file-name)',
                '-lc',
                '-ldl',
                '-lm',
            ],
            'conditions': [
              ['target_arch == "arm"', {
                'ldflags': [
                  # Enable identical code folding to reduce size.
                  '-Wl,--icf=safe',
                ],
              }],
              ['target_arch=="arm" and arm_version==7', {
                'cflags': [
                  '-march=armv7-a',
                  '-mtune=cortex-a8',
                  '-mfpu=vfp3',
                ],
                'ldflags': [
                  '-L<(android_libcpp_libs)/armeabi-v7a',
                ],
              }],
              ['target_arch=="arm" and arm_version < 7', {
                'ldflags': [
                  '-L<(android_libcpp_libs)/armeabi',
                ],
              }],
              ['target_arch=="x64"', {
                'ldflags': [
                  '-L<(android_libcpp_libs)/x86_64',
                ],
              }],
              ['target_arch=="arm64"', {
                'ldflags': [
                  '-L<(android_libcpp_libs)/arm64-v8a',
                ],
              }],
              ['target_arch=="ia32"', {
                # The x86 toolchain currently has problems with stack-protector.
                'cflags!': [
                  '-fstack-protector',
                ],
                'cflags': [
                  '-fno-stack-protector',
                ],
                'ldflags': [
                  '-L<(android_libcpp_libs)/x86',
                ],
              }],
              ['target_arch=="mipsel"', {
                # The mips toolchain currently has problems with stack-protector.
                'cflags!': [
                  '-fstack-protector',
                  '-U__linux__'
                ],
                'cflags': [
                  '-fno-stack-protector',
                ],
                'ldflags': [
                  '-L<(android_libcpp_libs)/mips',
                ],
              }],
              ['(target_arch=="arm" or target_arch=="arm64" or target_arch=="x64" or target_arch=="ia32") and component!="shared_library"', {
                'cflags': [
                  '-fPIE',
                ],
                'ldflags': [
                  '-pie',
                ],
              }],
            ],
            'target_conditions': [
              ['_type=="executable"', {
                'conditions': [
                  ['target_arch=="arm64" or target_arch=="x64"', {
                    'ldflags': [
                      '-Wl,-dynamic-linker,/system/bin/linker64',
                    ],
                  }, {
                    'ldflags': [
                      '-Wl,-dynamic-linker,/system/bin/linker',
                    ],
                  }]
                ],
                'ldflags': [
                  '-Bdynamic',
                  '-Wl,-z,nocopyreloc',
                  # crtbegin_dynamic.o should be the last item in ldflags.
                  '<(android_lib)/crtbegin_dynamic.o',
                ],
                'libraries': [
                  # crtend_android.o needs to be the last item in libraries.
                  # Do not add any libraries after this!
                  '<(android_lib)/crtend_android.o',
                ],
              }],
              ['_type=="shared_library"', {
                'ldflags': [
                  '-Wl,-shared,-Bsymbolic',
                  '<(android_lib)/crtbegin_so.o',
                ],
              }],
              ['_type=="static_library"', {
                'ldflags': [
                  # Don't export symbols from statically linked libraries.
                  '-Wl,--exclude-libs=ALL',
                ],
              }],
            ],
          }],  # _toolset=="target"
          # Settings for building host targets using the system toolchain.
          ['_toolset=="host"', {
            'cflags': [ '-pthread' ],
            'ldflags': [ '-pthread' ],
            'ldflags!': [
              '-Wl,-z,noexecstack',
              '-Wl,--gc-sections',
              '-Wl,-O1',
              '-Wl,--as-needed',
            ],
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="android"
    ['OS=="android" and clang==0', {
      # Hardcode the compiler names in the Makefile so that
      # it won't depend on the environment at make time.
      'make_global_settings': [
        ['LD', '<!(/bin/echo -n <(android_toolchain)/../*/bin/ld)'],
        ['RANLIB', '<!(/bin/echo -n <(android_toolchain)/../*/bin/ranlib)'],
        ['CC', '<!(/bin/echo -n <(android_toolchain)/*-gcc)'],
        ['CXX', '<!(/bin/echo -n <(android_toolchain)/*-g++)'],
        ['LD.host', '<(host_ld)'],
        ['RANLIB.host', '<(host_ranlib)'],
        ['CC.host', '<(host_cc)'],
        ['CXX.host', '<(host_cxx)'],
      ],
    }],
    ['clang!=1 and host_clang==1 and target_arch!="ia32" and target_arch!="x64"', {
      'make_global_settings': [
        ['CC.host', '<(clang_dir)/bin/clang'],
        ['CXX.host', '<(clang_dir)/bin/clang++'],
      ],
    }],
    ['clang==0 and host_clang==1 and target_arch!="ia32" and target_arch!="x64"', {
      'target_conditions': [
        ['_toolset=="host"', {
          'cflags_cc': [ '-std=gnu++11', ],
        }],
      ],
      'target_defaults': {
        'target_conditions': [
          ['_toolset=="host"', { 'cflags!': [ '-Wno-unused-local-typedefs' ]}],
        ],
      },
    }],
    ['clang==1 and "<(GENERATOR)"=="ninja"', {
      # See http://crbug.com/110262
      'target_defaults': {
        'cflags': [ '-fcolor-diagnostics' ],
        'xcode_settings': { 'OTHER_CFLAGS': [ '-fcolor-diagnostics' ] },
      },
    }],
    ['clang==1 and ((OS!="mac" and OS!="ios") or clang_xcode==0) '
        'and OS!="win" and "<(GENERATOR)"=="make"', {
      'make_global_settings': [
        ['CC', '<(clang_dir)/bin/clang'],
        ['CXX', '<(clang_dir)/bin/clang++'],
        ['CC.host', '$(CC)'],
        ['CXX.host', '$(CXX)'],
      ],
    }],
    ['clang==1 and ((OS!="mac" and OS!="ios") or clang_xcode==0) '
        'and OS!="win" and "<(GENERATOR)"=="ninja"', {
      'make_global_settings': [
        ['CC', '<(clang_dir)/bin/clang'],
        ['CXX', '<(clang_dir)/bin/clang++'],
        ['CC.host', '$(CC)'],
        ['CXX.host', '$(CXX)'],
      ],
    }],
    ['clang==1 and OS=="win"', {
      'make_global_settings': [
        # On Windows, gyp's ninja generator only looks at CC.
        ['CC', '<(clang_dir)/bin/clang-cl'],
      ],
    }],
    ['OS=="linux" and target_arch=="arm" and host_arch!="arm" and clang==0 and "<(GENERATOR)"=="ninja"', {
      # Set default ARM cross tools on linux.  These can be overridden
      # using CC,CXX,CC.host and CXX.host environment variables.
      'make_global_settings': [
        ['CC', '<!(which arm-linux-gnueabihf-gcc)'],
        ['CXX', '<!(which arm-linux-gnueabihf-g++)'],
        ['CC.host', '<(host_cc)'],
        ['CXX.host', '<(host_cxx)'],
      ],
    }],
    # TODO(yyanagisawa): supports GENERATOR==make
    #  make generator doesn't support CC_wrapper without CC
    #  in make_global_settings yet.
    ['use_goma==1 and ("<(GENERATOR)"=="ninja" or clang==1)', {
      'conditions': [
        ['coverage==1', {
          # Wrap goma with coverage wrapper.
          'make_global_settings': [
            ['CC_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py <(gomadir)/gomacc'],
            ['CXX_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py <(gomadir)/gomacc'],
            ['CC.host_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py <(gomadir)/gomacc'],
            ['CXX.host_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py <(gomadir)/gomacc'],
          ],
        }, {
          # Use only goma wrapper.
          'make_global_settings': [
            ['CC_wrapper', '<(gomadir)/gomacc'],
            ['CXX_wrapper', '<(gomadir)/gomacc'],
            ['CC.host_wrapper', '<(gomadir)/gomacc'],
            ['CXX.host_wrapper', '<(gomadir)/gomacc'],
          ],
        }],
      ],
    }, {
      'conditions': [
        ['coverage==1', {
          # Use only coverage wrapper.
          'make_global_settings': [
            ['CC_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py'],
            ['CXX_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py'],
            ['CC.host_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py'],
            ['CXX.host_wrapper', '<(base_dir)/gypfiles/coverage_wrapper.py'],
          ],
        }],
      ],
    }],
  ],
}
