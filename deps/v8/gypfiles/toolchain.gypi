# Copyright 2013 the V8 project authors. All rights reserved.
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

# Shared definitions for all V8-related targets.

{
  'variables': {
    'msvs_use_common_release': 0,
    'clang%': 0,
    'asan%': 0,
    'lsan%': 0,
    'msan%': 0,
    'tsan%': 0,
    'ubsan%': 0,
    'ubsan_vptr%': 0,
    'has_valgrind%': 0,
    'coverage%': 0,
    'v8_target_arch%': '<(target_arch)',
    'v8_host_byteorder%': '<!(python -c "import sys; print sys.byteorder")',
    'force_dynamic_crt%': 0,

    # Setting 'v8_can_use_vfp32dregs' to 'true' will cause V8 to use the VFP
    # registers d16-d31 in the generated code, both in the snapshot and for the
    # ARM target. Leaving the default value of 'false' will avoid the use of
    # these registers in the snapshot and use CPU feature probing when running
    # on the target.
    'v8_can_use_vfp32dregs%': 'false',
    'arm_test_noprobe%': 'off',

    # Similar to vfp but on MIPS.
    'v8_can_use_fpu_instructions%': 'true',

    # Similar to the ARM hard float ABI but on MIPS.
    'v8_use_mips_abi_hardfloat%': 'true',

    # Print to stdout on Android.
    'v8_android_log_stdout%': 0,

    # Force disable libstdc++ debug mode.
    'disable_glibcxx_debug%': 0,

    'v8_enable_backtrace%': 0,

    # Enable profiling support. Only required on Windows.
    'v8_enable_prof%': 0,

    # Some versions of GCC 4.5 seem to need -fno-strict-aliasing.
    'v8_no_strict_aliasing%': 0,

    # Chrome needs this definition unconditionally. For standalone V8 builds,
    # it's handled in gypfiles/standalone.gypi.
    'want_separate_host_toolset%': 1,

    # Toolset the shell binary should be compiled for. Possible values are
    # 'host' and 'target'.
    # The setting is ignored if want_separate_host_toolset is 0.
    'v8_toolset_for_shell%': 'target',

    'host_os%': '<(OS)',
    'werror%': '-Werror',
    # For a shared library build, results in "libv8-<(soname_version).so".
    'soname_version%': '',

    # Allow to suppress the array bounds warning (default is no suppression).
    'wno_array_bounds%': '',

    # Override where to find binutils
    'binutils_dir%': '',

    'conditions': [
      ['OS=="linux" and host_arch=="x64"', {
        'binutils_dir%': 'third_party/binutils/Linux_x64/Release/bin',
      }],
      ['OS=="linux" and host_arch=="ia32"', {
        'binutils_dir%': 'third_party/binutils/Linux_ia32/Release/bin',
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
      # linux_use_bundled_binutils: whether to use the binary binutils
      # checked into third_party/binutils.  These are not multi-arch so cannot
      # be used except on x86 and x86-64 (the only two architectures which
      # are currently checke in).  Force this off via GYP_DEFINES when you
      # are using a custom toolchain and need to control -B in cflags.
      ['OS=="linux" and (target_arch=="ia32" or target_arch=="x64")', {
        'linux_use_bundled_binutils%': 1,
      }, {
        'linux_use_bundled_binutils%': 0,
      }],
      # linux_use_gold_flags: whether to use build flags that rely on gold.
      # On by default for x64 Linux.
      ['OS=="linux" and target_arch=="x64"', {
        'linux_use_gold_flags%': 1,
      }, {
        'linux_use_gold_flags%': 0,
      }],
    ],

    # Link-Time Optimizations
    'use_lto%': 0,

    # Indicates if gcmole tools are downloaded by a hook.
    'gcmole%': 0,
  },
  'conditions': [
    ['host_arch=="ia32" or host_arch=="x64" or \
      host_arch=="ppc" or host_arch=="ppc64" or \
      host_arch=="s390" or host_arch=="s390x" or \
      clang==1', {
      'variables': {
        'host_cxx_is_biarch%': 1,
       },
     }, {
      'variables': {
        'host_cxx_is_biarch%': 0,
      },
    }],
    ['target_arch=="ia32" or target_arch=="x64" or target_arch=="x87" or \
      target_arch=="ppc" or target_arch=="ppc64" or target_arch=="s390" or \
      target_arch=="s390x" or clang==1', {
      'variables': {
        'target_cxx_is_biarch%': 1,
       },
     }, {
      'variables': {
        'target_cxx_is_biarch%': 0,
      },
    }],
  ],
  'target_defaults': {
    'conditions': [
      ['v8_target_arch=="arm"', {
        'defines': [
          'V8_TARGET_ARCH_ARM',
        ],
        'conditions': [
          [ 'arm_version==7 or arm_version=="default"', {
            'defines': [
              'CAN_USE_ARMV7_INSTRUCTIONS',
            ],
          }],
          [ 'arm_fpu=="vfpv3-d16" or arm_fpu=="default"', {
            'defines': [
              'CAN_USE_VFP3_INSTRUCTIONS',
            ],
          }],
          [ 'arm_fpu=="vfpv3"', {
            'defines': [
              'CAN_USE_VFP3_INSTRUCTIONS',
              'CAN_USE_VFP32DREGS',
            ],
          }],
          [ 'arm_fpu=="neon"', {
            'defines': [
              'CAN_USE_VFP3_INSTRUCTIONS',
              'CAN_USE_VFP32DREGS',
              'CAN_USE_NEON',
            ],
          }],
          [ 'arm_test_noprobe=="on"', {
            'defines': [
              'ARM_TEST_NO_FEATURE_PROBE',
            ],
          }],
        ],
        'target_conditions': [
          ['_toolset=="host"', {
            'conditions': [
              ['v8_target_arch==host_arch', {
                # Host built with an Arm CXX compiler.
                'conditions': [
                  [ 'arm_version==7', {
                    'cflags': ['-march=armv7-a',],
                  }],
                  [ 'arm_version==7 or arm_version=="default"', {
                    'conditions': [
                      [ 'arm_fpu!="default"', {
                        'cflags': ['-mfpu=<(arm_fpu)',],
                      }],
                    ],
                  }],
                  [ 'arm_float_abi!="default"', {
                    'cflags': ['-mfloat-abi=<(arm_float_abi)',],
                  }],
                  [ 'arm_thumb==1', {
                    'cflags': ['-mthumb',],
                  }],
                  [ 'arm_thumb==0', {
                    'cflags': ['-marm',],
                  }],
                ],
              }, {
                # 'v8_target_arch!=host_arch'
                # Host not built with an Arm CXX compiler (simulator build).
                'conditions': [
                  [ 'arm_float_abi=="hard"', {
                    'defines': [
                      'USE_EABI_HARDFLOAT=1',
                    ],
                  }],
                  [ 'arm_float_abi=="softfp" or arm_float_abi=="default"', {
                    'defines': [
                      'USE_EABI_HARDFLOAT=0',
                    ],
                  }],
                ],
              }],
            ],
          }],  # _toolset=="host"
          ['_toolset=="target"', {
            'conditions': [
              ['v8_target_arch==target_arch', {
                # Target built with an Arm CXX compiler.
                'conditions': [
                  [ 'arm_version==7', {
                    'cflags': ['-march=armv7-a',],
                  }],
                  [ 'arm_version==7 or arm_version=="default"', {
                    'conditions': [
                      [ 'arm_fpu!="default"', {
                        'cflags': ['-mfpu=<(arm_fpu)',],
                      }],
                    ],
                  }],
                  [ 'arm_float_abi!="default"', {
                    'cflags': ['-mfloat-abi=<(arm_float_abi)',],
                  }],
                  [ 'arm_thumb==1', {
                    'cflags': ['-mthumb',],
                  }],
                  [ 'arm_thumb==0', {
                    'cflags': ['-marm',],
                  }],
                ],
              }, {
                # 'v8_target_arch!=target_arch'
                # Target not built with an Arm CXX compiler (simulator build).
                'conditions': [
                  [ 'arm_float_abi=="hard"', {
                    'defines': [
                      'USE_EABI_HARDFLOAT=1',
                    ],
                  }],
                  [ 'arm_float_abi=="softfp" or arm_float_abi=="default"', {
                    'defines': [
                      'USE_EABI_HARDFLOAT=0',
                    ],
                  }],
                ],
              }],
              # Disable GCC LTO for v8
              # v8 is optimized for speed. Because GCC LTO merges flags at link
              # time, we disable LTO to prevent any -O2 flags from taking
              # precedence over v8's -Os flag. However, LLVM LTO does not work
              # this way so we keep LTO enabled under LLVM.
              ['clang==0 and use_lto==1', {
                'cflags!': [
                  '-flto',
                  '-ffat-lto-objects',
                ],
              }],
            ],
          }],  # _toolset=="target"
        ],
      }],  # v8_target_arch=="arm"
      ['v8_target_arch=="arm64"', {
        'defines': [
          'V8_TARGET_ARCH_ARM64',
        ],
      }],
      ['v8_target_arch=="s390" or v8_target_arch=="s390x"', {
        'defines': [
          'V8_TARGET_ARCH_S390',
        ],
        'conditions': [
          ['v8_target_arch=="s390x"', {
            'defines': [
              'V8_TARGET_ARCH_S390X',
            ],
          }],
          ['v8_host_byteorder=="little"', {
            'defines': [
              'V8_TARGET_ARCH_S390_LE_SIM',
            ],
          }, {
            'cflags': [ '-march=z196' ],
          }],
          ],
      }],  # s390
      ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
        'defines': [
          'V8_TARGET_ARCH_PPC',
        ],
        'conditions': [
          ['v8_target_arch=="ppc64"', {
            'defines': [
              'V8_TARGET_ARCH_PPC64',
            ],
          }],
          ['v8_host_byteorder=="little"', {
            'defines': [
              'V8_TARGET_ARCH_PPC_LE',
            ],
          }],
          ['v8_host_byteorder=="big"', {
            'defines': [
              'V8_TARGET_ARCH_PPC_BE',
            ],
            'conditions': [
              ['OS=="aix"', {
                # Work around AIX ceil, trunc and round oddities.
                'cflags': [ '-mcpu=power5+ -mfprnd' ],
              }],
              ['OS=="aix"', {
                # Work around AIX assembler popcntb bug.
                'cflags': [ '-mno-popcntb' ],
              }],
            ],
          }],
        ],
      }],  # ppc
      ['v8_target_arch=="ia32"', {
        'defines': [
          'V8_TARGET_ARCH_IA32',
        ],
      }],  # v8_target_arch=="ia32"
      ['v8_target_arch=="x87"', {
        'defines': [
          'V8_TARGET_ARCH_X87',
        ],
        'cflags': ['-march=i586'],
      }],  # v8_target_arch=="x87"
      ['v8_target_arch=="mips" or v8_target_arch=="mipsel" \
        or v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
        'target_conditions': [
          ['_toolset=="target"', {
            'conditions': [
              ['v8_target_arch==target_arch', {
                # Target built with a Mips CXX compiler.
                'variables': {
                  'ldso_path%': '<!(/bin/echo -n $LDSO_PATH)',
                  'ld_r_path%': '<!(/bin/echo -n $LD_R_PATH)',
                },
                'conditions': [
                  ['ldso_path!=""', {
                    'ldflags': ['-Wl,--dynamic-linker=<(ldso_path)'],
                  }],
                  ['ld_r_path!=""', {
                    'ldflags': ['-Wl,--rpath=<(ld_r_path)'],
                  }],
                  [ 'clang==1', {
                    'cflags': ['-integrated-as'],
                  }],
                  ['OS!="mac"', {
                    'defines': ['_MIPS_TARGET_HW',],
                  }, {
                    'defines': ['_MIPS_TARGET_SIMULATOR',],
                  }],
                ],
              }, {
                'defines': ['_MIPS_TARGET_SIMULATOR',],
              }],
            ],
          }],  #'_toolset=="target"
          ['_toolset=="host"', {
            'conditions': [
              ['v8_target_arch==target_arch and OS!="mac"', {
                'defines': ['_MIPS_TARGET_HW',],
              }, {
                'defines': ['_MIPS_TARGET_SIMULATOR',],
              }],
            ],
          }],  #'_toolset=="host"
        ],
      }],
      ['v8_target_arch=="mips"', {
        'defines': [
          'V8_TARGET_ARCH_MIPS',
        ],
        'conditions': [
          [ 'v8_can_use_fpu_instructions=="true"', {
            'defines': [
              'CAN_USE_FPU_INSTRUCTIONS',
            ],
          }],
          [ 'v8_use_mips_abi_hardfloat=="true"', {
            'defines': [
              '__mips_hard_float=1',
              'CAN_USE_FPU_INSTRUCTIONS',
            ],
          }, {
            'defines': [
              '__mips_soft_float=1'
            ]
          }],
        ],
        'target_conditions': [
          ['_toolset=="target"', {
            'conditions': [
              ['v8_target_arch==target_arch', {
                # Target built with a Mips CXX compiler.
                'cflags': [
                  '-EB',
                  '-Wno-error=array-bounds',  # Workaround https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273
                ],
                'ldflags': ['-EB'],
                'conditions': [
                  [ 'v8_use_mips_abi_hardfloat=="true"', {
                    'cflags': ['-mhard-float'],
                    'ldflags': ['-mhard-float'],
                  }, {
                    'cflags': ['-msoft-float'],
                    'ldflags': ['-msoft-float'],
                  }],
                  ['mips_arch_variant=="r6"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R6',
                      'FPU_MODE_FP64',
                    ],
                    'cflags!': ['-mfp32', '-mfpxx'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32r6'],
                      }],
                    ],
                    'cflags': ['-mips32r6'],
                    'ldflags': ['-mips32r6'],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'conditions': [
                      [ 'mips_fpu_mode=="fp64"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP64',
                        ],
                        'cflags': ['-mfp64'],
                      }],
                      ['mips_fpu_mode=="fpxx"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FPXX',
                        ],
                        'cflags': ['-mfpxx'],
                      }],
                      ['mips_fpu_mode=="fp32"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP32',
                        ],
                        'cflags': ['-mfp32'],
                      }],
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32r2'],
                      }],
                    ],
                    'cflags': ['-mips32r2'],
                    'ldflags': ['-mips32r2'],
                  }],
                  ['mips_arch_variant=="r1"', {
                    'defines': [
                      'FPU_MODE_FP32',
                    ],
                    'cflags!': ['-mfp64', '-mfpxx'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32'],
                      }],
                    ],
                    'cflags': ['-mips32'],
                    'ldflags': ['-mips32'],
                  }],
                  ['mips_arch_variant=="rx"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32RX',
                      'FPU_MODE_FPXX',
                    ],
                    'cflags!': ['-mfp64', '-mfp32'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32'],
                      }],
                    ],
                    'cflags': ['-mips32', '-mfpxx'],
                    'ldflags': ['-mips32'],
                  }],
                ],
              }, {
                # 'v8_target_arch!=target_arch'
                # Target not built with an MIPS CXX compiler (simulator build).
                'conditions': [
                  ['mips_arch_variant=="r6"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R6',
                      'FPU_MODE_FP64',
                    ],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'conditions': [
                      [ 'mips_fpu_mode=="fp64"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP64',
                        ],
                      }],
                      ['mips_fpu_mode=="fpxx"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FPXX',
                        ],
                      }],
                      ['mips_fpu_mode=="fp32"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP32',
                        ],
                      }],
                    ],
                  }],
                  ['mips_arch_variant=="r1"', {
                    'defines': [
                      'FPU_MODE_FP32',
                    ],
                  }],
                  ['mips_arch_variant=="rx"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32RX',
                      'FPU_MODE_FPXX',
                    ],
                  }],
                ],
              }],
            ],
          }],  #_toolset=="target"
          ['_toolset=="host"', {
            'conditions': [
              ['mips_arch_variant=="rx"', {
                'defines': [
                  '_MIPS_ARCH_MIPS32RX',
                  'FPU_MODE_FPXX',
                ],
              }],
              ['mips_arch_variant=="r6"', {
                'defines': [
                  '_MIPS_ARCH_MIPS32R6',
                  'FPU_MODE_FP64',
                ],
              }],
              ['mips_arch_variant=="r2"', {
                'conditions': [
                  ['mips_fpu_mode=="fp64"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R2',
                      'FPU_MODE_FP64',
                    ],
                  }],
                  ['mips_fpu_mode=="fpxx"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R2',
                      'FPU_MODE_FPXX',
                    ],
                  }],
                  ['mips_fpu_mode=="fp32"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R2',
                      'FPU_MODE_FP32'
                    ],
                  }],
                ],
              }],
              ['mips_arch_variant=="r1"', {
                'defines': ['FPU_MODE_FP32',],
              }],
            ]
          }],  #_toolset=="host"
        ],
      }],  # v8_target_arch=="mips"
      ['v8_target_arch=="mipsel"', {
        'defines': [
          'V8_TARGET_ARCH_MIPS',
        ],
        'conditions': [
          [ 'v8_can_use_fpu_instructions=="true"', {
            'defines': [
              'CAN_USE_FPU_INSTRUCTIONS',
            ],
          }],
          [ 'v8_use_mips_abi_hardfloat=="true"', {
            'defines': [
              '__mips_hard_float=1',
              'CAN_USE_FPU_INSTRUCTIONS',
            ],
          }, {
            'defines': [
              '__mips_soft_float=1'
            ],
          }],
        ],
        'target_conditions': [
          ['_toolset=="target"', {
            'conditions': [
              ['v8_target_arch==target_arch', {
                # Target built with a Mips CXX compiler.
                'cflags': [
                  '-EL',
                  '-Wno-error=array-bounds',  # Workaround https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273
                ],
                'ldflags': ['-EL'],
                'conditions': [
                  [ 'v8_use_mips_abi_hardfloat=="true"', {
                    'cflags': ['-mhard-float'],
                    'ldflags': ['-mhard-float'],
                  }, {
                    'cflags': ['-msoft-float'],
                    'ldflags': ['-msoft-float'],
                  }],
                  ['mips_arch_variant=="r6"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R6',
                      'FPU_MODE_FP64',
                    ],
                    'cflags!': ['-mfp32', '-mfpxx'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32r6'],
                      }],
                    ],
                    'cflags': ['-mips32r6'],
                    'ldflags': ['-mips32r6'],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'conditions': [
                      [ 'mips_fpu_mode=="fp64"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP64',
                        ],
                        'cflags': ['-mfp64'],
                      }],
                      ['mips_fpu_mode=="fpxx"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FPXX',
                        ],
                        'cflags': ['-mfpxx'],
                      }],
                      ['mips_fpu_mode=="fp32"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP32',
                        ],
                        'cflags': ['-mfp32'],
                      }],
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32r2'],
                      }],
                    ],
                    'cflags': ['-mips32r2'],
                    'ldflags': ['-mips32r2'],
                  }],
                  ['mips_arch_variant=="r1"', {
                    'defines': [
                      'FPU_MODE_FP32',
                    ],
                    'cflags!': ['-mfp64', '-mfpxx'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32'],
                      }],
                    ],
                    'cflags': ['-mips32'],
                    'ldflags': ['-mips32'],
                  }],
                  ['mips_arch_variant=="rx"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32RX',
                      'FPU_MODE_FPXX',
                    ],
                    'cflags!': ['-mfp64', '-mfp32'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips32'],
                      }],
                    ],
                    'cflags': ['-mips32', '-mfpxx'],
                    'ldflags': ['-mips32'],
                  }],
                  ['mips_arch_variant=="loongson"', {
                    'defines': [
                      '_MIPS_ARCH_LOONGSON',
                      'FPU_MODE_FP32',
                    ],
                    'cflags!': ['-mfp64', '-mfpxx'],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips3'],
                      }],
                    ],
                    'cflags': ['-mips3', '-mfp32'],
                  }],
                ],
              }, {
                # 'v8_target_arch!=target_arch'
                # Target not built with an MIPS CXX compiler (simulator build).
                'conditions': [
                  ['mips_arch_variant=="r6"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R6',
                      'FPU_MODE_FP64',
                    ],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'conditions': [
                      [ 'mips_fpu_mode=="fp64"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP64',
                        ],
                      }],
                      ['mips_fpu_mode=="fpxx"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FPXX',
                        ],
                      }],
                      ['mips_fpu_mode=="fp32"', {
                        'defines': [
                          '_MIPS_ARCH_MIPS32R2',
                          'FPU_MODE_FP32',
                        ],
                      }],
                    ],
                  }],
                  ['mips_arch_variant=="r1"', {
                    'defines': [
                      'FPU_MODE_FP32',
                    ],
                  }],
                  ['mips_arch_variant=="rx"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32RX',
                      'FPU_MODE_FPXX',
                    ],
                  }],
                  ['mips_arch_variant=="loongson"', {
                    'defines': [
                      '_MIPS_ARCH_LOONGSON',
                      'FPU_MODE_FP32',
                    ],
                  }],
                ],
              }],
            ],
          }], #_toolset=="target
          ['_toolset=="host"', {
            'conditions': [
              ['mips_arch_variant=="rx"', {
                'defines': [
                  '_MIPS_ARCH_MIPS32RX',
                  'FPU_MODE_FPXX',
                ],
              }],
              ['mips_arch_variant=="r6"', {
                'defines': [
                  '_MIPS_ARCH_MIPS32R6',
                  'FPU_MODE_FP64',
                ],
              }],
              ['mips_arch_variant=="r2"', {
                'conditions': [
                  ['mips_fpu_mode=="fp64"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R2',
                      'FPU_MODE_FP64',
                    ],
                  }],
                  ['mips_fpu_mode=="fpxx"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R2',
                      'FPU_MODE_FPXX',
                    ],
                  }],
                  ['mips_fpu_mode=="fp32"', {
                    'defines': [
                      '_MIPS_ARCH_MIPS32R2',
                      'FPU_MODE_FP32'
                    ],
                  }],
                ],
              }],
              ['mips_arch_variant=="r1"', {
                'defines': ['FPU_MODE_FP32',],
              }],
              ['mips_arch_variant=="loongson"', {
                'defines': [
                  '_MIPS_ARCH_LOONGSON',
                  'FPU_MODE_FP32',
                ],
              }],
            ]
          }],
        ],
      }],  # v8_target_arch=="mipsel"
      ['v8_target_arch=="mips64el" or v8_target_arch=="mips64"', {
        'defines': [
          'V8_TARGET_ARCH_MIPS64',
        ],
        'conditions': [
          [ 'v8_can_use_fpu_instructions=="true"', {
            'defines': [
              'CAN_USE_FPU_INSTRUCTIONS',
            ],
          }],
          [ 'v8_host_byteorder=="little"', {
            'defines': [
              'V8_TARGET_ARCH_MIPS64_LE',
            ],
          }],
          [ 'v8_host_byteorder=="big"', {
            'defines': [
              'V8_TARGET_ARCH_MIPS64_BE',
            ],
          }],
          [ 'v8_use_mips_abi_hardfloat=="true"', {
            'defines': [
              '__mips_hard_float=1',
              'CAN_USE_FPU_INSTRUCTIONS',
            ],
          }, {
            'defines': [
              '__mips_soft_float=1'
            ],
          }],
         ],
        'target_conditions': [
          ['_toolset=="target"', {
            'conditions': [
              ['v8_target_arch==target_arch', {
                'cflags': [
                  '-Wno-error=array-bounds',  # Workaround https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273
                ],
                'conditions': [
                  ['v8_target_arch=="mips64el"', {
                    'cflags': ['-EL'],
                    'ldflags': ['-EL'],
                  }],
                  ['v8_target_arch=="mips64"', {
                    'cflags': ['-EB'],
                    'ldflags': ['-EB'],
                  }],
                  [ 'v8_use_mips_abi_hardfloat=="true"', {
                    'cflags': ['-mhard-float'],
                    'ldflags': ['-mhard-float'],
                  }, {
                    'cflags': ['-msoft-float'],
                    'ldflags': ['-msoft-float'],
                  }],
                  ['mips_arch_variant=="r6"', {
                    'defines': ['_MIPS_ARCH_MIPS64R6',],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips64r6'],
                      }],
                    ],
                    'cflags': ['-mips64r6', '-mabi=64'],
                    'ldflags': ['-mips64r6', '-mabi=64'],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'defines': ['_MIPS_ARCH_MIPS64R2',],
                    'conditions': [
                      [ 'clang==0', {
                        'cflags': ['-Wa,-mips64r2'],
                      }],
                    ],
                    'cflags': ['-mips64r2', '-mabi=64'],
                    'ldflags': ['-mips64r2', '-mabi=64'],
                  }],
                ],
              }, {
                # 'v8_target_arch!=target_arch'
                # Target not built with an MIPS CXX compiler (simulator build).
                'conditions': [
                  ['mips_arch_variant=="r6"', {
                    'defines': ['_MIPS_ARCH_MIPS64R6',],
                  }],
                  ['mips_arch_variant=="r2"', {
                    'defines': ['_MIPS_ARCH_MIPS64R2',],
                  }],
                ],
              }],
            ],
          }],  #'_toolset=="target"
          ['_toolset=="host"', {
            'conditions': [
              ['mips_arch_variant=="r6"', {
                'defines': ['_MIPS_ARCH_MIPS64R6',],
              }],
              ['mips_arch_variant=="r2"', {
                'defines': ['_MIPS_ARCH_MIPS64R2',],
              }],
            ],
          }],  #'_toolset=="host"
        ],
      }],  # v8_target_arch=="mips64el"
      ['v8_target_arch=="x64"', {
        'defines': [
          'V8_TARGET_ARCH_X64',
        ],
        'xcode_settings': {
          'ARCHS': [ 'x86_64' ],
        },
        'msvs_settings': {
          'VCLinkerTool': {
            'StackReserveSize': '2097152',
          },
        },
        'msvs_configuration_platform': 'x64',
      }],  # v8_target_arch=="x64"
      ['v8_target_arch=="x32"', {
        'defines': [
          # x32 port shares the source code with x64 port.
          'V8_TARGET_ARCH_X64',
          'V8_TARGET_ARCH_32_BIT',
        ],
        'cflags': [
          '-mx32',
          # Inhibit warning if long long type is used.
          '-Wno-long-long',
        ],
        'ldflags': [
          '-mx32',
        ],
      }],  # v8_target_arch=="x32"
      ['linux_use_gold_flags==1', {
        # Newer gccs and clangs support -fuse-ld, use the flag to force gold
        # selection.
        # gcc -- http://gcc.gnu.org/onlinedocs/gcc-4.8.0/gcc/Optimize-Options.html
        'ldflags': [ '-fuse-ld=gold', ],
      }],
      ['linux_use_bundled_binutils==1', {
        'cflags': [
          '-B<!(cd <(DEPTH) && pwd -P)/<(binutils_dir)',
        ],
      }],
      ['linux_use_bundled_gold==1', {
        # Put our binutils, which contains gold in the search path. We pass
        # the path to gold to the compiler. gyp leaves unspecified what the
        # cwd is when running the compiler, so the normal gyp path-munging
        # fails us. This hack gets the right path.
        'ldflags': [
          '-B<!(cd <(DEPTH) && pwd -P)/<(binutils_dir)',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WIN32',
        ],
        # 4351: VS 2005 and later are warning us that they've fixed a bug
        #       present in VS 2003 and earlier.
        'msvs_disabled_warnings': [4351],
        'msvs_configuration_attributes': {
          'CharacterSet': '1',
        },
      }],
      ['OS=="win" and v8_target_arch=="ia32"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            # Ensure no surprising artifacts from 80bit double math with x86.
            'AdditionalOptions': ['/arch:SSE2'],
          },
        },
      }],
      ['OS=="win" and v8_enable_prof==1', {
        'msvs_settings': {
          'VCLinkerTool': {
            'GenerateMapFile': 'true',
          },
        },
      }],
      ['(OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
         or OS=="netbsd" or OS=="mac" or OS=="android" or OS=="qnx") and \
        v8_target_arch=="ia32"', {
        'cflags': [
          '-msse2',
          '-mfpmath=sse',
          '-mmmx',  # Allows mmintrin.h for MMX intrinsics.
        ],
      }],
      ['(OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
         or OS=="netbsd" or OS=="mac" or OS=="android" or OS=="qnx") and \
        (v8_target_arch=="arm" or v8_target_arch=="ia32" or \
         v8_target_arch=="x87" or v8_target_arch=="mips" or \
         v8_target_arch=="mipsel" or v8_target_arch=="ppc" or \
         v8_target_arch=="s390")', {
        'target_conditions': [
          ['_toolset=="host"', {
            'conditions': [
              ['host_cxx_is_biarch==1', {
                'conditions': [
                  ['host_arch=="s390" or host_arch=="s390x"', {
                    'cflags': [ '-m31' ],
                    'ldflags': [ '-m31' ]
                  },{
                   'cflags': [ '-m32' ],
                   'ldflags': [ '-m32' ]
                  }],
                ],
              }],
            ],
            'xcode_settings': {
              'ARCHS': [ 'i386' ],
            },
          }],
          ['_toolset=="target"', {
            'conditions': [
              ['target_cxx_is_biarch==1', {
                'conditions': [
                  ['host_arch=="s390" or host_arch=="s390x"', {
                    'cflags': [ '-m31' ],
                    'ldflags': [ '-m31' ]
                  },{
                   'cflags': [ '-m32' ],
                   'ldflags': [ '-m32' ],
                  }],
                ],
              }],
            ],
            'xcode_settings': {
              'ARCHS': [ 'i386' ],
            },
          }],
        ],
      }],
      ['(OS=="linux" or OS=="android") and \
        (v8_target_arch=="x64" or v8_target_arch=="arm64" or \
         v8_target_arch=="ppc64" or v8_target_arch=="s390x")', {
        'target_conditions': [
          ['_toolset=="host"', {
            'conditions': [
              ['host_cxx_is_biarch==1', {
                'cflags': [ '-m64' ],
                'ldflags': [ '-m64' ]
              }],
             ],
           }],
           ['_toolset=="target"', {
             'conditions': [
               ['target_cxx_is_biarch==1', {
                 'cflags': [ '-m64' ],
                 'ldflags': [ '-m64' ],
               }],
             ]
           }],
         ],
      }],
      ['OS=="android" and v8_android_log_stdout==1', {
        'defines': [
          'V8_ANDROID_LOG_STDOUT',
        ],
      }],
      ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
         or OS=="netbsd" or OS=="qnx" or OS=="aix"', {
        'conditions': [
          [ 'v8_no_strict_aliasing==1', {
            'cflags': [ '-fno-strict-aliasing' ],
          }],
        ],  # conditions
      }],
      ['OS=="solaris"', {
        'defines': [ '__C99FEATURES__=1' ],  # isinf() etc.
      }],
      ['OS=="freebsd" or OS=="openbsd"', {
        'cflags': [ '-I/usr/local/include' ],
      }],
      ['OS=="netbsd"', {
        'cflags': [ '-I/usr/pkg/include' ],
      }],
      ['OS=="aix"', {
        'defines': [
          # Support for malloc(0)
          '_LINUX_SOURCE_COMPAT=1',
          '__STDC_FORMAT_MACROS',
          '_ALL_SOURCE=1'],
        'conditions': [
          [ 'v8_target_arch=="ppc"', {
            'ldflags': [ '-Wl,-bmaxdata:0x60000000/dsa' ],
          }],
          [ 'v8_target_arch=="ppc64"', {
            'cflags': [ '-maix64' ],
            'ldflags': [ '-maix64 -Wl,-bbigtoc' ],
          }],
        ],
      }],
    ],  # conditions
    'configurations': {
      # Abstract configuration for v8_optimized_debug == 0.
      'DebugBase0': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'conditions': [
              ['component=="shared_library" or force_dynamic_crt==1', {
                'RuntimeLibrary': '3',  # /MDd
              }, {
                'RuntimeLibrary': '1',  # /MTd
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
          },
        },
        'variables': {
          'v8_enable_slow_dchecks%': 1,
        },
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx" or OS=="aix"', {
            'cflags!': [
              '-O3',
              '-O2',
              '-O1',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
               'GCC_OPTIMIZATION_LEVEL': '0',  # -O0
            },
          }],
          ['v8_enable_slow_dchecks==1', {
            'defines': [
              'ENABLE_SLOW_DCHECKS',
            ],
          }],
        ],
      },  # DebugBase0
      # Abstract configuration for v8_optimized_debug == 1.
      'DebugBase1': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'InlineFunctionExpansion': '2',
            'EnableIntrinsicFunctions': 'true',
            'FavorSizeOrSpeed': '0',
            'StringPooling': 'true',
            'BasicRuntimeChecks': '0',
            'conditions': [
              ['component=="shared_library" or force_dynamic_crt==1', {
                'RuntimeLibrary': '3',  #/MDd
              }, {
                'RuntimeLibrary': '1',  #/MTd
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
            'OptimizeReferences': '2',
            'EnableCOMDATFolding': '2',
          },
        },
        'variables': {
          'v8_enable_slow_dchecks%': 0,
        },
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx" or OS=="aix"', {
            'cflags!': [
              '-O0',
              '-O1',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
            ],
            'conditions': [
              # Don't use -O3 with sanitizers.
              ['asan==0 and msan==0 and lsan==0 \
                and tsan==0 and ubsan==0 and ubsan_vptr==0', {
                'cflags': ['-O3'],
                'cflags!': ['-O2'],
                }, {
                'cflags': ['-O2'],
                'cflags!': ['-O3'],
              }],
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_OPTIMIZATION_LEVEL': '3',  # -O3
              'GCC_STRICT_ALIASING': 'YES',
            },
          }],
          ['v8_enable_slow_dchecks==1', {
            'defines': [
              'ENABLE_SLOW_DCHECKS',
            ],
          }],
        ],
      },  # DebugBase1
      # Common settings for the Debug configuration.
      'DebugBaseCommon': {
        'abstract': 1,
        'defines': [
          'ENABLE_DISASSEMBLER',
          'V8_ENABLE_CHECKS',
          'OBJECT_PRINT',
          'VERIFY_HEAP',
          'DEBUG',
          'V8_TRACE_MAPS'
        ],
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx" or OS=="aix"', {
            'cflags': [ '-Woverloaded-virtual', '<(wno_array_bounds)', ],
          }],
          ['OS=="linux" and v8_enable_backtrace==1', {
            # Support for backtrace_symbols.
            'ldflags': [ '-rdynamic' ],
          }],
          ['OS=="linux" and disable_glibcxx_debug==0', {
            # Enable libstdc++ debugging facilities to help catch problems
            # early, see http://crbug.com/65151 .
            'defines': ['_GLIBCXX_DEBUG=1',],
          }],
          ['OS=="aix"', {
            'ldflags': [ '-Wl,-bbigtoc' ],
            'conditions': [
              ['v8_target_arch=="ppc64"', {
                'cflags': [ '-maix64 -mcmodel=large' ],
              }],
            ],
          }],
          ['OS=="android"', {
            'variables': {
              'android_full_debug%': 1,
            },
            'conditions': [
              ['android_full_debug==0', {
                # Disable full debug if we want a faster v8 in a debug build.
                # TODO(2304): pass DISABLE_DEBUG_ASSERT instead of hiding DEBUG.
                'defines!': [
                  'DEBUG',
                  'ENABLE_SLOW_DCHECKS',
                ],
              }],
            ],
          }],
          # TODO(pcc): Re-enable in LTO builds once we've fixed the intermittent
          # link failures (crbug.com/513074).
          ['linux_use_gold_flags==1 and use_lto==0', {
            'target_conditions': [
              ['_toolset=="target"', {
                'ldflags': [
                  # Experimentation found that using four linking threads
                  # saved ~20% of link time.
                  # https://groups.google.com/a/chromium.org/group/chromium-dev/browse_thread/thread/281527606915bb36
                  # Only apply this to the target linker, since the host
                  # linker might not be gold, but isn't used much anyway.
                  '-Wl,--threads',
                  '-Wl,--thread-count=4',
                ],
              }],
            ],
          }],
        ],
      },  # DebugBaseCommon
      'Debug': {
        'inherit_from': ['DebugBaseCommon'],
        'conditions': [
          ['v8_optimized_debug==0', {
            'inherit_from': ['DebugBase0'],
          }, {
            'inherit_from': ['DebugBase1'],
          }],
        ],
      },  # Debug
      'ReleaseBase': {
        'abstract': 1,
        'variables': {
          'v8_enable_slow_dchecks%': 0,
        },
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" \
            or OS=="aix"', {
            'cflags!': [
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '<(wno_array_bounds)',
            ],
            'conditions': [
              # Don't use -O3 with sanitizers.
              ['asan==0 and msan==0 and lsan==0 \
                and tsan==0 and ubsan==0 and ubsan_vptr==0', {
                'cflags': ['-O3'],
                'cflags!': ['-O2'],
              }, {
                'cflags': ['-O2'],
                'cflags!': ['-O3'],
              }],
            ],
          }],
          ['OS=="android"', {
            'cflags!': [
              '-O3',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '-O2',
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_OPTIMIZATION_LEVEL': '3',  # -O3

              # -fstrict-aliasing.  Mainline gcc
              # enables this at -O2 and above,
              # but Apple gcc does not unless it
              # is specified explicitly.
              'GCC_STRICT_ALIASING': 'YES',
            },
          }],  # OS=="mac"
          ['OS=="win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'Optimization': '2',
                'InlineFunctionExpansion': '2',
                'EnableIntrinsicFunctions': 'true',
                'FavorSizeOrSpeed': '0',
                'StringPooling': 'true',
                'conditions': [
                  ['component=="shared_library" or force_dynamic_crt==1', {
                    'RuntimeLibrary': '2',  #/MD
                  }, {
                    'RuntimeLibrary': '0',  #/MT
                  }],
                ],
              },
              'VCLinkerTool': {
                'LinkIncremental': '1',
                'OptimizeReferences': '2',
                'EnableCOMDATFolding': '2',
              },
            },
          }],  # OS=="win"
          ['v8_enable_slow_dchecks==1', {
            'defines': [
              'ENABLE_SLOW_DCHECKS',
            ],
          }],
        ],  # conditions
      },  # Release
      'Release': {
        'inherit_from': ['ReleaseBase'],
      },  # Debug
      'conditions': [
        [ 'OS=="win"', {
          # TODO(bradnelson): add a gyp mechanism to make this more graceful.
          'Debug_x64': {
            'inherit_from': ['DebugBaseCommon'],
            'conditions': [
              ['v8_optimized_debug==0', {
                'inherit_from': ['DebugBase0'],
              }, {
                'inherit_from': ['DebugBase1'],
              }],
            ],
          },
          'Release_x64': {
            'inherit_from': ['ReleaseBase'],
          },
        }],
      ],
    },  # configurations
  },  # target_defaults
}
