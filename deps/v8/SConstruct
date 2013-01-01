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

import platform
import re
import subprocess
import sys
import os
from os.path import join, dirname, abspath
from types import DictType, StringTypes
root_dir = dirname(File('SConstruct').rfile().abspath)
src_dir = join(root_dir, 'src')
sys.path.insert(0, join(root_dir, 'tools'))
import js2c, utils

# ARM_TARGET_LIB is the path to the dynamic library to use on the target
# machine if cross-compiling to an arm machine. You will also need to set
# the additional cross-compiling environment variables to the cross compiler.
ARM_TARGET_LIB = os.environ.get('ARM_TARGET_LIB')
if ARM_TARGET_LIB:
  ARM_LINK_FLAGS = ['-Wl,-rpath=' + ARM_TARGET_LIB + '/lib:' +
                     ARM_TARGET_LIB + '/usr/lib',
                    '-Wl,--dynamic-linker=' + ARM_TARGET_LIB +
                    '/lib/ld-linux.so.3']
else:
  ARM_LINK_FLAGS = []

GCC_EXTRA_CCFLAGS = []
GCC_DTOA_EXTRA_CCFLAGS = []

LIBRARY_FLAGS = {
  'all': {
    'CPPPATH': [src_dir],
    'regexp:interpreted': {
      'CPPDEFINES': ['V8_INTERPRETED_REGEXP']
    },
    'mode:debug': {
      'CPPDEFINES': ['V8_ENABLE_CHECKS', 'OBJECT_PRINT', 'VERIFY_HEAP']
    },
    'objectprint:on': {
      'CPPDEFINES':   ['OBJECT_PRINT'],
    },
    'debuggersupport:on': {
      'CPPDEFINES':   ['ENABLE_DEBUGGER_SUPPORT'],
    },
    'inspector:on': {
      'CPPDEFINES':   ['INSPECTOR'],
    },
    'fasttls:off': {
      'CPPDEFINES':   ['V8_NO_FAST_TLS'],
    },
    'liveobjectlist:on': {
      'CPPDEFINES':   ['ENABLE_DEBUGGER_SUPPORT', 'INSPECTOR',
                       'LIVE_OBJECT_LIST', 'OBJECT_PRINT'],
    }
  },
  'gcc': {
    'all': {
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['-fno-rtti', '-fno-exceptions'],
    },
    'visibility:hidden': {
      # Use visibility=default to disable this.
      'CXXFLAGS':     ['-fvisibility=hidden']
    },
    'strictaliasing:off': {
      'CCFLAGS':      ['-fno-strict-aliasing']
    },
    'mode:debug': {
      'CCFLAGS':      ['-g', '-O0'],
      'CPPDEFINES':   ['ENABLE_DISASSEMBLER', 'DEBUG'],
    },
    'mode:release': {
      'CCFLAGS':      ['-O3', '-fomit-frame-pointer', '-fdata-sections',
                       '-ffunction-sections'],
    },
    'os:linux': {
      'CCFLAGS':      ['-ansi'] + GCC_EXTRA_CCFLAGS,
      'library:shared': {
        'CPPDEFINES': ['V8_SHARED', 'BUILDING_V8_SHARED'],
        'LIBS': ['pthread']
      }
    },
    'os:macos': {
      'CCFLAGS':      ['-ansi', '-mmacosx-version-min=10.4'],
      'library:shared': {
        'CPPDEFINES': ['V8_SHARED', 'BUILDING_V8_SHARED'],
      }
    },
    'os:freebsd': {
      'CPPPATH' : [src_dir, '/usr/local/include'],
      'LIBPATH' : ['/usr/local/lib'],
      'CCFLAGS':      ['-ansi'],
      'LIBS': ['execinfo']
    },
    'os:openbsd': {
      'CPPPATH' : [src_dir, '/usr/local/include'],
      'LIBPATH' : ['/usr/local/lib'],
      'CCFLAGS':      ['-ansi'],
    },
    'os:solaris': {
      # On Solaris, to get isinf, INFINITY, fpclassify and other macros one
      # needs to define __C99FEATURES__.
      'CPPDEFINES': ['__C99FEATURES__'],
      'CPPPATH' : [src_dir, '/usr/local/include'],
      'LIBPATH' : ['/usr/local/lib'],
      'CCFLAGS':      ['-ansi'],
    },
    'os:netbsd': {
      'CPPPATH' : [src_dir, '/usr/pkg/include'],
      'LIBPATH' : ['/usr/pkg/lib'],
    },
    'os:win32': {
      'CCFLAGS':      ['-DWIN32'],
      'CXXFLAGS':     ['-DWIN32'],
    },
    'arch:ia32': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_IA32'],
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'arch:arm': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_ARM'],
      'unalignedaccesses:on' : {
        'CPPDEFINES' : ['CAN_USE_UNALIGNED_ACCESSES=1']
      },
      'unalignedaccesses:off' : {
        'CPPDEFINES' : ['CAN_USE_UNALIGNED_ACCESSES=0']
      },
      'armeabi:soft' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=0'],
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=soft'],
        }
      },
      'armeabi:softfp' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=0'],
        'vfp3:on': {
          'CPPDEFINES' : ['CAN_USE_VFP_INSTRUCTIONS']
        },
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=softfp'],
        }
      },
      'armeabi:hard' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=1'],
        'vfp3:on': {
          'CPPDEFINES' : ['CAN_USE_VFP_INSTRUCTIONS']
        },
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=hard'],
        }
      }
    },
    'simulator:arm': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32'],
    },
    'arch:mips': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_MIPS'],
      'mips_arch_variant:mips32r2': {
        'CPPDEFINES':    ['_MIPS_ARCH_MIPS32R2']
      },
      'mips_arch_variant:loongson': {
        'CPPDEFINES':    ['_MIPS_ARCH_LOONGSON']
      },
      'simulator:none': {
        'CCFLAGS':      ['-EL'],
        'LINKFLAGS':    ['-EL'],
        'mips_arch_variant:mips32r2': {
          'CCFLAGS':      ['-mips32r2', '-Wa,-mips32r2']
        },
        'mips_arch_variant:mips32r1': {
          'CCFLAGS':      ['-mips32', '-Wa,-mips32']
        },
        'mips_arch_variant:loongson': {
          'CCFLAGS':      ['-march=mips3', '-Wa,-march=mips3']
        },
        'library:static': {
          'LINKFLAGS':    ['-static', '-static-libgcc']
        },
        'mipsabi:softfloat': {
          'CCFLAGS':      ['-msoft-float'],
          'LINKFLAGS':    ['-msoft-float']
        },
        'mipsabi:hardfloat': {
          'CCFLAGS':      ['-mhard-float'],
          'LINKFLAGS':    ['-mhard-float']
        }
      }
    },
    'simulator:mips': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32'],
      'mipsabi:softfloat': {
        'CPPDEFINES':    ['__mips_soft_float=1'],
        'fpu:on': {
          'CPPDEFINES' : ['CAN_USE_FPU_INSTRUCTIONS']
        }
      },
      'mipsabi:hardfloat': {
        'CPPDEFINES':    ['__mips_hard_float=1', 'CAN_USE_FPU_INSTRUCTIONS'],
      }
    },
    'arch:x64': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_X64'],
      'CCFLAGS':      ['-m64'],
      'LINKFLAGS':    ['-m64'],
    },
    'gdbjit:on': {
      'CPPDEFINES':   ['ENABLE_GDB_JIT_INTERFACE']
    },
    'compress_startup_data:bz2': {
      'CPPDEFINES':   ['COMPRESS_STARTUP_DATA_BZ2']
    }
  },
  'msvc': {
    'all': {
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['/GR-', '/Gy'],
      'CPPDEFINES':   ['WIN32'],
      'LINKFLAGS':    ['/INCREMENTAL:NO', '/NXCOMPAT', '/IGNORE:4221'],
      'CCPDBFLAGS':   ['/Zi']
    },
    'verbose:off': {
      'DIALECTFLAGS': ['/nologo'],
      'ARFLAGS':      ['/NOLOGO']
    },
    'arch:ia32': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_IA32', '_USE_32BIT_TIME_T'],
      'LINKFLAGS':    ['/MACHINE:X86'],
      'ARFLAGS':      ['/MACHINE:X86']
    },
    'arch:x64': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_X64'],
      'LINKFLAGS':    ['/MACHINE:X64'],
      'ARFLAGS':      ['/MACHINE:X64']
    },
    'mode:debug': {
      'CCFLAGS':      ['/Od', '/Gm'],
      'CPPDEFINES':   ['_DEBUG', 'ENABLE_DISASSEMBLER', 'DEBUG'],
      'LINKFLAGS':    ['/DEBUG'],
      'msvcrt:static': {
        'CCFLAGS': ['/MTd']
      },
      'msvcrt:shared': {
        'CCFLAGS': ['/MDd']
      }
    },
    'mode:release': {
      'CCFLAGS':      ['/O2'],
      'LINKFLAGS':    ['/OPT:REF', '/OPT:ICF'],
      'msvcrt:static': {
        'CCFLAGS': ['/MT']
      },
      'msvcrt:shared': {
        'CCFLAGS': ['/MD']
      },
      'msvcltcg:on': {
        'CCFLAGS':      ['/GL'],
        'ARFLAGS':      ['/LTCG'],
        'pgo:off': {
          'LINKFLAGS':    ['/LTCG'],
        },
        'pgo:instrument': {
          'LINKFLAGS':    ['/LTCG:PGI']
        },
        'pgo:optimize': {
          'LINKFLAGS':    ['/LTCG:PGO']
        }
      }
    }
  }
}


V8_EXTRA_FLAGS = {
  'gcc': {
    'all': {
      'WARNINGFLAGS': ['-Wall',
                       '-Werror',
                       '-W',
                       '-Wno-unused-parameter',
                       '-Woverloaded-virtual',
                       '-Wnon-virtual-dtor']
    },
    'os:win32': {
      'WARNINGFLAGS': ['-pedantic',
                       '-Wno-long-long',
                       '-Wno-pedantic-ms-format'],
      'library:shared': {
        'LIBS': ['winmm', 'ws2_32']
      }
    },
    'os:linux': {
      'WARNINGFLAGS': ['-pedantic'],
      'library:shared': {
        'soname:on': {
          'LINKFLAGS': ['-Wl,-soname,${SONAME}']
        }
      }
    },
    'os:macos': {
      'WARNINGFLAGS': ['-pedantic']
    },
    'arch:arm': {
      # This is to silence warnings about ABI changes that some versions of the
      # CodeSourcery G++ tool chain produce for each occurrence of varargs.
      'WARNINGFLAGS': ['-Wno-abi']
    },
    'disassembler:on': {
      'CPPDEFINES':   ['ENABLE_DISASSEMBLER']
    }
  },
  'msvc': {
    'all': {
      'WARNINGFLAGS': ['/W3', '/WX', '/wd4351', '/wd4355', '/wd4800']
    },
    'library:shared': {
      'CPPDEFINES': ['BUILDING_V8_SHARED'],
      'LIBS': ['winmm', 'ws2_32']
    },
    'arch:arm': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_ARM'],
      # /wd4996 is to silence the warning about sscanf
      # used by the arm simulator.
      'WARNINGFLAGS': ['/wd4996']
    },
    'arch:mips': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_MIPS'],
      'mips_arch_variant:mips32r2': {
        'CPPDEFINES':    ['_MIPS_ARCH_MIPS32R2']
      },
    },
    'disassembler:on': {
      'CPPDEFINES':   ['ENABLE_DISASSEMBLER']
    }
  }
}


MKSNAPSHOT_EXTRA_FLAGS = {
  'gcc': {
    'os:linux': {
      'LIBS': ['pthread'],
    },
    'os:macos': {
      'LIBS': ['pthread'],
    },
    'os:freebsd': {
      'LIBS': ['execinfo', 'pthread']
    },
    'os:solaris': {
      'LIBS': ['m', 'pthread', 'socket', 'nsl', 'rt'],
      'LINKFLAGS': ['-mt']
    },
    'os:openbsd': {
      'LIBS': ['execinfo', 'pthread']
    },
    'os:win32': {
      'LIBS': ['winmm', 'ws2_32'],
    },
    'os:netbsd': {
      'LIBS': ['execinfo', 'pthread']
    },
    'compress_startup_data:bz2': {
      'os:linux': {
        'LIBS': ['bz2']
      }
    },
  },
  'msvc': {
    'all': {
      'CPPDEFINES': ['_HAS_EXCEPTIONS=0'],
      'LIBS': ['winmm', 'ws2_32']
    }
  }
}


DTOA_EXTRA_FLAGS = {
  'gcc': {
    'all': {
      'WARNINGFLAGS': ['-Werror', '-Wno-uninitialized'],
      'CCFLAGS': GCC_DTOA_EXTRA_CCFLAGS
    }
  },
  'msvc': {
    'all': {
      'WARNINGFLAGS': ['/WX', '/wd4018', '/wd4244']
    }
  }
}


CCTEST_EXTRA_FLAGS = {
  'all': {
    'CPPPATH': [src_dir],
    'library:shared': {
      'CPPDEFINES': ['USING_V8_SHARED']
    },
  },
  'gcc': {
    'all': {
      'LIBPATH':      [abspath('.')],
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['-fno-rtti', '-fno-exceptions'],
      'LINKFLAGS':    ['$CCFLAGS'],
    },
    'os:linux': {
      'LIBS':         ['pthread'],
      'CCFLAGS':      ['-Wno-unused-but-set-variable'],
    },
    'os:macos': {
      'LIBS':         ['pthread'],
    },
    'os:freebsd': {
      'LIBS':         ['execinfo', 'pthread']
    },
    'os:solaris': {
      'LIBS':         ['m', 'pthread', 'socket', 'nsl', 'rt'],
      'LINKFLAGS':    ['-mt']
    },
    'os:openbsd': {
      'LIBS':         ['execinfo', 'pthread']
    },
    'os:win32': {
      'LIBS': ['winmm', 'ws2_32']
    },
    'os:netbsd': {
      'LIBS':         ['execinfo', 'pthread']
    },
    'arch:arm': {
      'LINKFLAGS':   ARM_LINK_FLAGS
    },
  },
  'msvc': {
    'all': {
      'CPPDEFINES': ['_HAS_EXCEPTIONS=0'],
      'LIBS': ['winmm', 'ws2_32']
    },
    'arch:ia32': {
      'CPPDEFINES': ['V8_TARGET_ARCH_IA32']
    },
    'arch:x64': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_X64'],
      'LINKFLAGS': ['/STACK:2097152']
    },
  }
}


SAMPLE_FLAGS = {
  'all': {
    'CPPPATH': [join(root_dir, 'include')],
    'library:shared': {
      'CPPDEFINES': ['USING_V8_SHARED']
    },
  },
  'gcc': {
    'all': {
      'LIBPATH':      ['.'],
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['-fno-rtti', '-fno-exceptions'],
      'LINKFLAGS':    ['$CCFLAGS'],
    },
    'os:linux': {
      'LIBS':         ['pthread'],
    },
    'os:macos': {
      'LIBS':         ['pthread'],
    },
    'os:freebsd': {
      'LIBPATH' :     ['/usr/local/lib'],
      'LIBS':         ['execinfo', 'pthread']
    },
    'os:solaris': {
      # On Solaris, to get isinf, INFINITY, fpclassify and other macros one
      # needs to define __C99FEATURES__.
      'CPPDEFINES': ['__C99FEATURES__'],
      'LIBPATH' :     ['/usr/local/lib'],
      'LIBS':         ['m', 'pthread', 'socket', 'nsl', 'rt'],
      'LINKFLAGS':    ['-mt']
    },
    'os:openbsd': {
      'LIBPATH' :     ['/usr/local/lib'],
      'LIBS':         ['execinfo', 'pthread']
    },
    'os:win32': {
      'LIBS':         ['winmm', 'ws2_32']
    },
    'os:netbsd': {
      'LIBPATH' :     ['/usr/pkg/lib'],
      'LIBS':         ['execinfo', 'pthread']
    },
    'arch:arm': {
      'LINKFLAGS':   ARM_LINK_FLAGS,
      'armeabi:soft' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=0'],
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=soft'],
        }
      },
      'armeabi:softfp' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=0'],
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=softfp'],
        }
      },
      'armeabi:hard' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=1'],
        'vfp3:on': {
          'CPPDEFINES' : ['CAN_USE_VFP_INSTRUCTIONS']
        },
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=hard'],
        }
      }
    },
    'arch:ia32': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'arch:x64': {
      'CCFLAGS':      ['-m64'],
      'LINKFLAGS':    ['-m64']
    },
    'arch:mips': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_MIPS'],
      'mips_arch_variant:mips32r2': {
        'CPPDEFINES':    ['_MIPS_ARCH_MIPS32R2']
      },
      'mips_arch_variant:loongson': {
        'CPPDEFINES':    ['_MIPS_ARCH_LOONGSON']
      },
      'simulator:none': {
        'CCFLAGS':      ['-EL'],
        'LINKFLAGS':    ['-EL'],
        'mips_arch_variant:mips32r2': {
          'CCFLAGS':      ['-mips32r2', '-Wa,-mips32r2']
        },
        'mips_arch_variant:mips32r1': {
          'CCFLAGS':      ['-mips32', '-Wa,-mips32']
        },
        'mips_arch_variant:loongson': {
          'CCFLAGS':      ['-march=mips3', '-Wa,-march=mips3']
        },
        'library:static': {
          'LINKFLAGS':    ['-static', '-static-libgcc']
        },
        'mipsabi:softfloat': {
          'CCFLAGS':      ['-msoft-float'],
          'LINKFLAGS':    ['-msoft-float']
        },
        'mipsabi:hardfloat': {
          'CCFLAGS':      ['-mhard-float'],
          'LINKFLAGS':    ['-mhard-float'],
          'fpu:on': {
            'CPPDEFINES' : ['CAN_USE_FPU_INSTRUCTIONS']
          }
        }
      }
    },
    'simulator:arm': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'simulator:mips': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'mode:release': {
      'CCFLAGS':      ['-O2']
    },
    'mode:debug': {
      'CCFLAGS':      ['-g', '-O0'],
      'CPPDEFINES':   ['DEBUG']
    },
    'compress_startup_data:bz2': {
      'CPPDEFINES':   ['COMPRESS_STARTUP_DATA_BZ2'],
      'os:linux': {
        'LIBS':       ['bz2']
      }
    },
  },
  'msvc': {
    'all': {
      'LIBS': ['winmm', 'ws2_32']
    },
    'verbose:off': {
      'CCFLAGS': ['/nologo'],
      'LINKFLAGS': ['/NOLOGO']
    },
    'verbose:on': {
      'LINKFLAGS': ['/VERBOSE']
    },
    'prof:on': {
      'LINKFLAGS': ['/MAP']
    },
    'mode:release': {
      'CCFLAGS':   ['/O2'],
      'LINKFLAGS': ['/OPT:REF', '/OPT:ICF'],
      'msvcrt:static': {
        'CCFLAGS': ['/MT']
      },
      'msvcrt:shared': {
        'CCFLAGS': ['/MD']
      },
      'msvcltcg:on': {
        'CCFLAGS':      ['/GL'],
        'pgo:off': {
          'LINKFLAGS':    ['/LTCG'],
        },
      },
      'pgo:instrument': {
        'LINKFLAGS':    ['/LTCG:PGI']
      },
      'pgo:optimize': {
        'LINKFLAGS':    ['/LTCG:PGO']
      }
    },
    'arch:ia32': {
      'CPPDEFINES': ['V8_TARGET_ARCH_IA32', 'WIN32'],
      'LINKFLAGS': ['/MACHINE:X86']
    },
    'arch:x64': {
      'CPPDEFINES': ['V8_TARGET_ARCH_X64', 'WIN32'],
      'LINKFLAGS': ['/MACHINE:X64', '/STACK:2097152']
    },
    'mode:debug': {
      'CCFLAGS':    ['/Od'],
      'LINKFLAGS':  ['/DEBUG'],
      'CPPDEFINES': ['DEBUG'],
      'msvcrt:static': {
        'CCFLAGS':  ['/MTd']
      },
      'msvcrt:shared': {
        'CCFLAGS':  ['/MDd']
      }
    }
  }
}


PREPARSER_FLAGS = {
  'all': {
    'CPPPATH': [join(root_dir, 'include'), src_dir],
    'library:shared': {
      'CPPDEFINES': ['USING_V8_SHARED']
    },
  },
  'gcc': {
    'all': {
      'LIBPATH':      ['.'],
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['-fno-rtti', '-fno-exceptions'],
      'LINKFLAGS':    ['$CCFLAGS'],
    },
    'os:win32': {
      'LIBS':         ['winmm', 'ws2_32']
    },
    'arch:arm': {
      'LINKFLAGS':   ARM_LINK_FLAGS,
      'armeabi:soft' : {
        'CPPDEFINES' : ['USE_EABI_HARDFLOAT=0'],
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=soft'],
        }
      },
      'armeabi:softfp' : {
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=softfp'],
        }
      },
      'armeabi:hard' : {
        'simulator:none': {
          'CCFLAGS':     ['-mfloat-abi=hard'],
        }
      }
    },
    'arch:ia32': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'arch:x64': {
      'CCFLAGS':      ['-m64'],
      'LINKFLAGS':    ['-m64']
    },
    'arch:mips': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_MIPS'],
      'mips_arch_variant:mips32r2': {
        'CPPDEFINES':    ['_MIPS_ARCH_MIPS32R2']
      },
      'mips_arch_variant:loongson': {
        'CPPDEFINES':    ['_MIPS_ARCH_LOONGSON']
      },
      'simulator:none': {
        'CCFLAGS':      ['-EL'],
        'LINKFLAGS':    ['-EL'],
        'mips_arch_variant:mips32r2': {
          'CCFLAGS':      ['-mips32r2', '-Wa,-mips32r2']
        },
        'mips_arch_variant:mips32r1': {
          'CCFLAGS':      ['-mips32', '-Wa,-mips32']
        },
        'mips_arch_variant:loongson': {
          'CCFLAGS':      ['-march=mips3', '-Wa,-march=mips3']
        },
        'library:static': {
          'LINKFLAGS':    ['-static', '-static-libgcc']
        },
        'mipsabi:softfloat': {
          'CCFLAGS':      ['-msoft-float'],
          'LINKFLAGS':    ['-msoft-float']
        },
        'mipsabi:hardfloat': {
          'CCFLAGS':      ['-mhard-float'],
          'LINKFLAGS':    ['-mhard-float']
        }
      }
    },
    'simulator:arm': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'simulator:mips': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32'],
      'mipsabi:softfloat': {
        'CPPDEFINES':    ['__mips_soft_float=1'],
      },
      'mipsabi:hardfloat': {
        'CPPDEFINES':    ['__mips_hard_float=1'],
      }
    },
    'mode:release': {
      'CCFLAGS':      ['-O2']
    },
    'mode:debug': {
      'CCFLAGS':      ['-g', '-O0'],
      'CPPDEFINES':   ['DEBUG']
    },
    'os:freebsd': {
      'LIBPATH' : ['/usr/local/lib'],
    },
  },
  'msvc': {
    'all': {
      'LIBS': ['winmm', 'ws2_32']
    },
    'verbose:off': {
      'CCFLAGS': ['/nologo'],
      'LINKFLAGS': ['/NOLOGO']
    },
    'verbose:on': {
      'LINKFLAGS': ['/VERBOSE']
    },
    'prof:on': {
      'LINKFLAGS': ['/MAP']
    },
    'mode:release': {
      'CCFLAGS':   ['/O2'],
      'LINKFLAGS': ['/OPT:REF', '/OPT:ICF'],
      'msvcrt:static': {
        'CCFLAGS': ['/MT']
      },
      'msvcrt:shared': {
        'CCFLAGS': ['/MD']
      },
      'msvcltcg:on': {
        'CCFLAGS':      ['/GL'],
        'pgo:off': {
          'LINKFLAGS':    ['/LTCG'],
        },
      },
      'pgo:instrument': {
        'LINKFLAGS':    ['/LTCG:PGI']
      },
      'pgo:optimize': {
        'LINKFLAGS':    ['/LTCG:PGO']
      }
    },
    'arch:ia32': {
      'CPPDEFINES': ['V8_TARGET_ARCH_IA32', 'WIN32'],
      'LINKFLAGS': ['/MACHINE:X86']
    },
    'arch:x64': {
      'CPPDEFINES': ['V8_TARGET_ARCH_X64', 'WIN32'],
      'LINKFLAGS': ['/MACHINE:X64', '/STACK:2097152']
    },
    'mode:debug': {
      'CCFLAGS':    ['/Od'],
      'LINKFLAGS':  ['/DEBUG'],
      'CPPDEFINES': ['DEBUG'],
      'msvcrt:static': {
        'CCFLAGS':  ['/MTd']
      },
      'msvcrt:shared': {
        'CCFLAGS':  ['/MDd']
      }
    }
  }
}


D8_FLAGS = {
  'all': {
    'library:shared': {
      'CPPDEFINES': ['V8_SHARED'],
      'LIBS': ['v8'],
      'LIBPATH': ['.']
    },
  },
  'gcc': {
    'all': {
      'CCFLAGS': ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS': ['-fno-rtti', '-fno-exceptions'],
      'LINKFLAGS': ['$CCFLAGS'],
    },
    'console:readline': {
      'LIBS': ['readline']
    },
    'os:linux': {
      'LIBS': ['pthread'],
    },
    'os:macos': {
      'LIBS': ['pthread'],
    },
    'os:freebsd': {
      'LIBS': ['pthread'],
    },
    'os:solaris': {
      'LIBS': ['m', 'pthread', 'socket', 'nsl', 'rt'],
      'LINKFLAGS': ['-mt']
    },
    'os:openbsd': {
      'LIBS': ['pthread'],
    },
    'os:win32': {
      'LIBS': ['winmm', 'ws2_32'],
    },
    'os:netbsd': {
      'LIBS': ['pthread'],
    },
    'arch:arm': {
      'LINKFLAGS':   ARM_LINK_FLAGS
    },
    'compress_startup_data:bz2': {
      'CPPDEFINES':   ['COMPRESS_STARTUP_DATA_BZ2'],
      'os:linux': {
        'LIBS': ['bz2']
      }
    }
  },
  'msvc': {
    'all': {
      'LIBS': ['winmm', 'ws2_32']
    },
    'verbose:off': {
      'CCFLAGS': ['/nologo'],
      'LINKFLAGS': ['/NOLOGO']
    },
    'verbose:on': {
      'LINKFLAGS': ['/VERBOSE']
    },
    'prof:on': {
      'LINKFLAGS': ['/MAP']
    },
    'mode:release': {
      'CCFLAGS':   ['/O2'],
      'LINKFLAGS': ['/OPT:REF', '/OPT:ICF'],
      'msvcrt:static': {
        'CCFLAGS': ['/MT']
      },
      'msvcrt:shared': {
        'CCFLAGS': ['/MD']
      },
      'msvcltcg:on': {
        'CCFLAGS':      ['/GL'],
        'pgo:off': {
          'LINKFLAGS':    ['/LTCG'],
        },
      },
      'pgo:instrument': {
        'LINKFLAGS':    ['/LTCG:PGI']
      },
      'pgo:optimize': {
        'LINKFLAGS':    ['/LTCG:PGO']
      }
    },
    'arch:ia32': {
      'CPPDEFINES': ['V8_TARGET_ARCH_IA32', 'WIN32'],
      'LINKFLAGS': ['/MACHINE:X86']
    },
    'arch:x64': {
      'CPPDEFINES': ['V8_TARGET_ARCH_X64', 'WIN32'],
      'LINKFLAGS': ['/MACHINE:X64', '/STACK:2097152']
    },
    'mode:debug': {
      'CCFLAGS':    ['/Od'],
      'LINKFLAGS':  ['/DEBUG'],
      'CPPDEFINES': ['DEBUG'],
      'msvcrt:static': {
        'CCFLAGS':  ['/MTd']
      },
      'msvcrt:shared': {
        'CCFLAGS':  ['/MDd']
      }
    }
  }
}


SUFFIXES = {
  'release': '',
  'debug': '_g'
}


def Abort(message):
  print message
  sys.exit(1)


def GuessOS(env):
  return utils.GuessOS()


def GuessArch(env):
  return utils.GuessArchitecture()


def GuessToolchain(env):
  tools = env['TOOLS']
  if 'gcc' in tools:
    return 'gcc'
  elif 'msvc' in tools:
    return 'msvc'
  else:
    return None


def GuessVisibility(env):
  os = env['os']
  toolchain = env['toolchain'];
  if (os == 'win32' or os == 'cygwin') and toolchain == 'gcc':
    # MinGW / Cygwin can't do it.
    return 'default'
  elif os == 'solaris':
    return 'default'
  else:
    return 'hidden'


def GuessStrictAliasing(env):
  # There seems to be a problem with gcc 4.5.x.
  # See http://code.google.com/p/v8/issues/detail?id=884
  # It can be worked around by disabling strict aliasing.
  toolchain = env['toolchain'];
  if toolchain == 'gcc':
    env = Environment(tools=['gcc'])
    # The gcc version should be available in env['CCVERSION'],
    # but when scons detects msvc this value is not set.
    version = subprocess.Popen([env['CC'], '-dumpversion'],
        stdout=subprocess.PIPE).communicate()[0]
    if version.find('4.5') == 0:
      return 'off'
  return 'default'


PLATFORM_OPTIONS = {
  'arch': {
    'values': ['arm', 'ia32', 'x64', 'mips'],
    'guess': GuessArch,
    'help': 'the architecture to build for'
  },
  'os': {
    'values': ['freebsd', 'linux', 'macos', 'win32', 'openbsd', 'solaris', 'cygwin', 'netbsd'],
    'guess': GuessOS,
    'help': 'the os to build for'
  },
  'toolchain': {
    'values': ['gcc', 'msvc'],
    'guess': GuessToolchain,
    'help': 'the toolchain to use'
  }
}

SIMPLE_OPTIONS = {
  'regexp': {
    'values': ['native', 'interpreted'],
    'default': 'native',
    'help': 'Whether to use native or interpreted regexp implementation'
  },
  'snapshot': {
    'values': ['on', 'off', 'nobuild'],
    'default': 'off',
    'help': 'build using snapshots for faster start-up'
  },
  'prof': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable profiling of build target'
  },
  'gdbjit': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable GDB JIT interface'
  },
  'library': {
    'values': ['static', 'shared'],
    'default': 'static',
    'help': 'the type of library to produce'
  },
  'objectprint': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable object printing'
  },
  'profilingsupport': {
    'values': ['on', 'off'],
    'default': 'on',
    'help': 'enable profiling of JavaScript code'
  },
  'debuggersupport': {
    'values': ['on', 'off'],
    'default': 'on',
    'help': 'enable debugging of JavaScript code'
  },
  'inspector': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable inspector features'
  },
  'liveobjectlist': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable live object list features in the debugger'
  },
  'soname': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'turn on setting soname for Linux shared library'
  },
  'msvcrt': {
    'values': ['static', 'shared'],
    'default': 'static',
    'help': 'the type of Microsoft Visual C++ runtime library to use'
  },
  'msvcltcg': {
    'values': ['on', 'off'],
    'default': 'on',
    'help': 'use Microsoft Visual C++ link-time code generation'
  },
  'simulator': {
    'values': ['arm', 'mips', 'none'],
    'default': 'none',
    'help': 'build with simulator'
  },
  'unalignedaccesses': {
    'values': ['default', 'on', 'off'],
    'default': 'default',
    'help': 'set whether the ARM target supports unaligned accesses'
  },
  'disassembler': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable the disassembler to inspect generated code'
  },
  'fasttls': {
    'values': ['on', 'off'],
    'default': 'on',
    'help': 'enable fast thread local storage support '
            '(if available on the current architecture/platform)'
  },
  'sourcesignatures': {
    'values': ['MD5', 'timestamp'],
    'default': 'MD5',
    'help': 'set how the build system detects file changes'
  },
  'console': {
    'values': ['dumb', 'readline'],
    'default': 'dumb',
    'help': 'the console to use for the d8 shell'
  },
  'verbose': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'more output from compiler and linker'
  },
  'visibility': {
    'values': ['default', 'hidden'],
    'guess': GuessVisibility,
    'help': 'shared library symbol visibility'
  },
  'strictaliasing': {
    'values': ['default', 'off'],
    'guess': GuessStrictAliasing,
    'help': 'assume strict aliasing while optimizing'
  },
  'pgo': {
    'values': ['off', 'instrument', 'optimize'],
    'default': 'off',
    'help': 'select profile guided optimization variant',
  },
  'armeabi': {
    'values': ['hard', 'softfp', 'soft'],
    'default': 'softfp',
    'help': 'generate calling conventiont according to selected ARM EABI variant'
  },
  'mipsabi': {
    'values': ['hardfloat', 'softfloat', 'none'],
    'default': 'hardfloat',
    'help': 'generate calling conventiont according to selected mips ABI'
  },
  'mips_arch_variant': {
    'values': ['mips32r2', 'mips32r1', 'loongson'],
    'default': 'mips32r2',
    'help': 'mips variant'
  },
  'compress_startup_data': {
    'values': ['off', 'bz2'],
    'default': 'off',
    'help': 'compress startup data (snapshot) [Linux only]'
  },
  'vfp3': {
    'values': ['on', 'off'],
    'default': 'on',
    'help': 'use vfp3 instructions when building the snapshot [Arm only]'
  },
  'fpu': {
    'values': ['on', 'off'],
    'default': 'on',
    'help': 'use fpu instructions when building the snapshot [MIPS only]'
  },
  'I_know_I_should_build_with_GYP': {
    'values': ['yes', 'no'],
    'default': 'no',
    'help': 'grace period: temporarily override SCons deprecation'
  }

}

ALL_OPTIONS = dict(PLATFORM_OPTIONS, **SIMPLE_OPTIONS)


def AddOptions(options, result):
  guess_env = Environment(options=result)
  for (name, option) in options.iteritems():
    if 'guess' in option:
      # Option has a guess function
      guess = option.get('guess')
      default = guess(guess_env)
    else:
      # Option has a fixed default
      default = option.get('default')
    help = '%s (%s)' % (option.get('help'), ", ".join(option['values']))
    result.Add(name, help, default)


def GetOptions():
  result = Options()
  result.Add('mode', 'compilation mode (debug, release)', 'release')
  result.Add('sample', 'build sample (shell, process, lineprocessor)', '')
  result.Add('cache', 'directory to use for scons build cache', '')
  result.Add('env', 'override environment settings (NAME0:value0,NAME1:value1,...)', '')
  result.Add('importenv', 'import environment settings (NAME0,NAME1,...)', '')
  AddOptions(PLATFORM_OPTIONS, result)
  AddOptions(SIMPLE_OPTIONS, result)
  return result


def GetTools(opts):
  env = Environment(options=opts)
  os = env['os']
  toolchain = env['toolchain']
  if os == 'win32' and toolchain == 'gcc':
    return ['mingw']
  elif os == 'win32' and toolchain == 'msvc':
    return ['msvc', 'mslink', 'mslib', 'msvs']
  else:
    return ['default']


def GetVersionComponents():
  MAJOR_VERSION_PATTERN = re.compile(r"#define\s+MAJOR_VERSION\s+(.*)")
  MINOR_VERSION_PATTERN = re.compile(r"#define\s+MINOR_VERSION\s+(.*)")
  BUILD_NUMBER_PATTERN = re.compile(r"#define\s+BUILD_NUMBER\s+(.*)")
  PATCH_LEVEL_PATTERN = re.compile(r"#define\s+PATCH_LEVEL\s+(.*)")

  patterns = [MAJOR_VERSION_PATTERN,
              MINOR_VERSION_PATTERN,
              BUILD_NUMBER_PATTERN,
              PATCH_LEVEL_PATTERN]

  source = open(join(root_dir, 'src', 'version.cc')).read()
  version_components = []
  for pattern in patterns:
    match = pattern.search(source)
    if match:
      version_components.append(match.group(1).strip())
    else:
      version_components.append('0')

  return version_components


def GetVersion():
  version_components = GetVersionComponents()

  if version_components[len(version_components) - 1] == '0':
    version_components.pop()
  return '.'.join(version_components)


def GetSpecificSONAME():
  SONAME_PATTERN = re.compile(r"#define\s+SONAME\s+\"(.*)\"")

  source = open(join(root_dir, 'src', 'version.cc')).read()
  match = SONAME_PATTERN.search(source)

  if match:
    return match.group(1).strip()
  else:
    return ''


def SplitList(str):
  return [ s for s in str.split(",") if len(s) > 0 ]


def IsLegal(env, option, values):
  str = env[option]
  for s in SplitList(str):
    if not s in values:
      Abort("Illegal value for option %s '%s'." % (option, s))
      return False
  return True


def WarnAboutDeprecation():
  print """
    #####################################################################
    #                                                                   #
    #  LAST WARNING: Building V8 with SCons is deprecated.              #
    #                                                                   #
    #  This only works because you have overridden the kill switch.     #
    #                                                                   #
    #              MIGRATE TO THE GYP-BASED BUILD NOW!                  #
    #                                                                   #
    #  Instructions: http://code.google.com/p/v8/wiki/BuildingWithGYP.  #
    #                                                                   #
    #####################################################################
  """


def VerifyOptions(env):
  if env['I_know_I_should_build_with_GYP'] != 'yes':
    Abort("Building V8 with SCons is no longer supported. Please use GYP "
          "instead; you can find instructions are at "
          "http://code.google.com/p/v8/wiki/BuildingWithGYP.\n\n"
          "Quitting.\n\n"
          "For a limited grace period, you can specify "
          "\"I_know_I_should_build_with_GYP=yes\" to override.")
  else:
    WarnAboutDeprecation()
    import atexit
    atexit.register(WarnAboutDeprecation)

  if not IsLegal(env, 'mode', ['debug', 'release']):
    return False
  if not IsLegal(env, 'sample', ["shell", "process", "lineprocessor"]):
    return False
  if not IsLegal(env, 'regexp', ["native", "interpreted"]):
    return False
  if env['os'] == 'win32' and env['library'] == 'shared' and env['prof'] == 'on':
    Abort("Profiling on windows only supported for static library.")
  if env['gdbjit'] == 'on' and ((env['os'] != 'linux' and env['os'] != 'macos') or (env['arch'] != 'ia32' and env['arch'] != 'x64' and env['arch'] != 'arm')):
    Abort("GDBJIT interface is supported only for Intel-compatible (ia32 or x64) Linux/OSX target.")
  if env['os'] == 'win32' and env['soname'] == 'on':
    Abort("Shared Object soname not applicable for Windows.")
  if env['soname'] == 'on' and env['library'] == 'static':
    Abort("Shared Object soname not applicable for static library.")
  if env['os'] != 'win32' and env['pgo'] != 'off':
    Abort("Profile guided optimization only supported on Windows.")
  if env['cache'] and not os.path.isdir(env['cache']):
    Abort("The specified cache directory does not exist.")
  if not (env['arch'] == 'arm' or env['simulator'] == 'arm') and ('unalignedaccesses' in ARGUMENTS):
    print env['arch']
    print env['simulator']
    Abort("Option unalignedaccesses only supported for the ARM architecture.")
  if env['os'] != 'linux' and env['compress_startup_data'] != 'off':
    Abort("Startup data compression is only available on Linux")
  for (name, option) in ALL_OPTIONS.iteritems():
    if (not name in env):
      message = ("A value for option %s must be specified (%s)." %
          (name, ", ".join(option['values'])))
      Abort(message)
    if not env[name] in option['values']:
      message = ("Unknown %s value '%s'.  Possible values are (%s)." %
          (name, env[name], ", ".join(option['values'])))
      Abort(message)


class BuildContext(object):

  def __init__(self, options, env_overrides, samples):
    self.library_targets = []
    self.mksnapshot_targets = []
    self.cctest_targets = []
    self.sample_targets = []
    self.d8_targets = []
    self.options = options
    self.env_overrides = env_overrides
    self.samples = samples
    self.preparser_targets = []
    self.use_snapshot = (options['snapshot'] != 'off')
    self.build_snapshot = (options['snapshot'] == 'on')
    self.flags = None

  def AddRelevantFlags(self, initial, flags):
    result = initial.copy()
    toolchain = self.options['toolchain']
    if toolchain in flags:
      self.AppendFlags(result, flags[toolchain].get('all'))
      for option in sorted(self.options.keys()):
        value = self.options[option]
        self.AppendFlags(result, flags[toolchain].get(option + ':' + value))
    self.AppendFlags(result, flags.get('all'))
    return result

  def AddRelevantSubFlags(self, options, flags):
    self.AppendFlags(options, flags.get('all'))
    for option in sorted(self.options.keys()):
      value = self.options[option]
      self.AppendFlags(options, flags.get(option + ':' + value))

  def GetRelevantSources(self, source):
    result = []
    result += source.get('all', [])
    for (name, value) in self.options.iteritems():
      source_value = source.get(name + ':' + value, [])
      if type(source_value) == dict:
        result += self.GetRelevantSources(source_value)
      else:
        result += source_value
    return sorted(result)

  def AppendFlags(self, options, added):
    if not added:
      return
    for (key, value) in added.iteritems():
      if key.find(':') != -1:
        self.AddRelevantSubFlags(options, { key: value })
      else:
        if not key in options:
          options[key] = value
        else:
          prefix = options[key]
          if isinstance(prefix, StringTypes): prefix = prefix.split()
          options[key] = prefix + value

  def ConfigureObject(self, env, input, **kw):
    if (kw.has_key('CPPPATH') and env.has_key('CPPPATH')):
      kw['CPPPATH'] += env['CPPPATH']
    if self.options['library'] == 'static':
      return env.StaticObject(input, **kw)
    else:
      return env.SharedObject(input, **kw)

  def ApplyEnvOverrides(self, env):
    if not self.env_overrides:
      return
    if type(env['ENV']) == DictType:
      env['ENV'].update(**self.env_overrides)
    else:
      env['ENV'] = self.env_overrides


def PostprocessOptions(options, os):
  # Adjust architecture if the simulator option has been set
  if (options['simulator'] != 'none') and (options['arch'] != options['simulator']):
    if 'arch' in ARGUMENTS:
      # Print a warning if arch has explicitly been set
      print "Warning: forcing architecture to match simulator (%s)" % options['simulator']
    options['arch'] = options['simulator']
  if (options['prof'] != 'off') and (options['profilingsupport'] == 'off'):
    # Print a warning if profiling is enabled without profiling support
    print "Warning: forcing profilingsupport on when prof is on"
    options['profilingsupport'] = 'on'
  if os == 'win32' and options['pgo'] != 'off' and options['msvcltcg'] == 'off':
    if 'msvcltcg' in ARGUMENTS:
      print "Warning: forcing msvcltcg on as it is required for pgo (%s)" % options['pgo']
    options['msvcltcg'] = 'on'
  if (options['mipsabi'] != 'none') and (options['arch'] != 'mips') and (options['simulator'] != 'mips'):
    options['mipsabi'] = 'none'
  if options['liveobjectlist'] == 'on':
    if (options['debuggersupport'] != 'on') or (options['mode'] == 'release'):
      # Print a warning that liveobjectlist will implicitly enable the debugger
      print "Warning: forcing debuggersupport on for liveobjectlist"
    options['debuggersupport'] = 'on'
    options['inspector'] = 'on'
    options['objectprint'] = 'on'


def ParseEnvOverrides(arg, imports):
  # The environment overrides are in the format NAME0:value0,NAME1:value1,...
  # The environment imports are in the format NAME0,NAME1,...
  overrides = {}
  for var in imports.split(','):
    if var in os.environ:
      overrides[var] = os.environ[var]
  for override in arg.split(','):
    pos = override.find(':')
    if pos == -1:
      continue
    overrides[override[:pos].strip()] = override[pos+1:].strip()
  return overrides


def BuildSpecific(env, mode, env_overrides, tools):
  options = {'mode': mode}
  for option in ALL_OPTIONS:
    options[option] = env[option]
  PostprocessOptions(options, env['os'])

  context = BuildContext(options, env_overrides, samples=SplitList(env['sample']))

  # Remove variables which can't be imported from the user's external
  # environment into a construction environment.
  user_environ = os.environ.copy()
  try:
    del user_environ['ENV']
  except KeyError:
    pass

  library_flags = context.AddRelevantFlags(user_environ, LIBRARY_FLAGS)
  v8_flags = context.AddRelevantFlags(library_flags, V8_EXTRA_FLAGS)
  mksnapshot_flags = context.AddRelevantFlags(library_flags, MKSNAPSHOT_EXTRA_FLAGS)
  dtoa_flags = context.AddRelevantFlags(library_flags, DTOA_EXTRA_FLAGS)
  cctest_flags = context.AddRelevantFlags(v8_flags, CCTEST_EXTRA_FLAGS)
  sample_flags = context.AddRelevantFlags(user_environ, SAMPLE_FLAGS)
  preparser_flags = context.AddRelevantFlags(user_environ, PREPARSER_FLAGS)
  d8_flags = context.AddRelevantFlags(library_flags, D8_FLAGS)

  context.flags = {
    'v8': v8_flags,
    'mksnapshot': mksnapshot_flags,
    'dtoa': dtoa_flags,
    'cctest': cctest_flags,
    'sample': sample_flags,
    'd8': d8_flags,
    'preparser': preparser_flags
  }

  # Generate library base name.
  target_id = mode
  suffix = SUFFIXES[target_id]
  library_name = 'v8' + suffix
  preparser_library_name = 'v8preparser' + suffix
  version = GetVersion()
  if context.options['soname'] == 'on':
    # When building shared object with SONAME version the library name.
    library_name += '-' + version

  # Generate library SONAME if required by the build.
  if context.options['soname'] == 'on':
    soname = GetSpecificSONAME()
    if soname == '':
      soname = 'lib' + library_name + '.so'
    env['SONAME'] = soname

  # Build the object files by invoking SCons recursively.
  d8_env = Environment(tools=tools)
  d8_env.Replace(**context.flags['d8'])
  (object_files, shell_files, mksnapshot, preparser_files) = env.SConscript(
    join('src', 'SConscript'),
    build_dir=join('obj', target_id),
    exports='context tools d8_env',
    duplicate=False
  )

  context.mksnapshot_targets.append(mksnapshot)

  # Link the object files into a library.
  env.Replace(**context.flags['v8'])

  context.ApplyEnvOverrides(env)
  if context.options['library'] == 'static':
    library = env.StaticLibrary(library_name, object_files)
    preparser_library = env.StaticLibrary(preparser_library_name,
                                          preparser_files)
  else:
    # There seems to be a glitch in the way scons decides where to put
    # PDB files when compiling using MSVC so we specify it manually.
    # This should not affect any other platforms.
    pdb_name = library_name + '.dll.pdb'
    library = env.SharedLibrary(library_name, object_files, PDB=pdb_name)
    preparser_pdb_name = preparser_library_name + '.dll.pdb';
    preparser_soname = 'lib' + preparser_library_name + '.so';
    preparser_library = env.SharedLibrary(preparser_library_name,
                                          preparser_files,
                                          PDB=preparser_pdb_name,
                                          SONAME=preparser_soname)
  context.library_targets.append(library)
  context.library_targets.append(preparser_library)

  context.ApplyEnvOverrides(d8_env)
  if context.options['library'] == 'static':
    shell = d8_env.Program('d8' + suffix, object_files + shell_files)
  else:
    shell = d8_env.Program('d8' + suffix, shell_files)
    d8_env.Depends(shell, library)
  context.d8_targets.append(shell)

  for sample in context.samples:
    sample_env = Environment(tools=tools)
    sample_env.Replace(**context.flags['sample'])
    sample_env.Prepend(LIBS=[library_name])
    context.ApplyEnvOverrides(sample_env)
    sample_object = sample_env.SConscript(
      join('samples', 'SConscript'),
      build_dir=join('obj', 'sample', sample, target_id),
      exports='sample context tools',
      duplicate=False
    )
    sample_name = sample + suffix
    sample_program = sample_env.Program(sample_name, sample_object)
    sample_env.Depends(sample_program, library)
    context.sample_targets.append(sample_program)

  cctest_env = env.Copy()
  cctest_env.Prepend(LIBS=[library_name])
  cctest_program = cctest_env.SConscript(
    join('test', 'cctest', 'SConscript'),
    build_dir=join('obj', 'test', target_id),
    exports='context object_files tools',
    duplicate=False
  )
  context.cctest_targets.append(cctest_program)

  preparser_env = env.Copy()
  preparser_env.Replace(**context.flags['preparser'])
  preparser_env.Prepend(LIBS=[preparser_library_name])
  context.ApplyEnvOverrides(preparser_env)
  preparser_object = preparser_env.SConscript(
    join('preparser', 'SConscript'),
    build_dir=join('obj', 'preparser', target_id),
    exports='context tools',
    duplicate=False
  )
  preparser_name = join('obj', 'preparser', target_id, 'preparser')
  preparser_program = preparser_env.Program(preparser_name, preparser_object);
  preparser_env.Depends(preparser_program, preparser_library)
  context.preparser_targets.append(preparser_program)

  return context


def Build():
  opts = GetOptions()
  tools = GetTools(opts)
  env = Environment(options=opts, tools=tools)

  Help(opts.GenerateHelpText(env))
  VerifyOptions(env)
  env_overrides = ParseEnvOverrides(env['env'], env['importenv'])

  SourceSignatures(env['sourcesignatures'])

  libraries = []
  mksnapshots = []
  cctests = []
  samples = []
  preparsers = []
  d8s = []
  modes = SplitList(env['mode'])
  for mode in modes:
    context = BuildSpecific(env.Copy(), mode, env_overrides, tools)
    libraries += context.library_targets
    mksnapshots += context.mksnapshot_targets
    cctests += context.cctest_targets
    samples += context.sample_targets
    preparsers += context.preparser_targets
    d8s += context.d8_targets

  env.Alias('library', libraries)
  env.Alias('mksnapshot', mksnapshots)
  env.Alias('cctests', cctests)
  env.Alias('sample', samples)
  env.Alias('d8', d8s)
  env.Alias('preparser', preparsers)

  if env['sample']:
    env.Default('sample')
  else:
    env.Default('library')

  if env['cache']:
    CacheDir(env['cache'])

# We disable deprecation warnings because we need to be able to use
# env.Copy without getting warnings for compatibility with older
# version of scons.  Also, there's a bug in some revisions that
# doesn't allow this flag to be set, so we swallow any exceptions.
# Lovely.
try:
  SetOption('warn', 'no-deprecated')
except:
  pass

Build()
