# Copyright 2008 the V8 project authors. All rights reserved.
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
import sys
import os
from os.path import join, dirname, abspath
from types import DictType, StringTypes
root_dir = dirname(File('SConstruct').rfile().abspath)
sys.path.append(join(root_dir, 'tools'))
import js2c, utils


# ANDROID_TOP is the top of the Android checkout, fetched from the environment
# variable 'TOP'.   You will also need to set the CXX, CC, AR and RANLIB
# environment variables to the cross-compiling tools.
ANDROID_TOP = os.environ.get('TOP')
if ANDROID_TOP is None:
  ANDROID_TOP=""

# TODO: Sort these issues out properly but as a temporary solution for gcc 4.4
# on linux we need these compiler flags to avoid crashes in the v8 test suite
# and avoid dtoa.c strict aliasing issues
if os.environ.get('GCC_VERSION') == '44':
    GCC_EXTRA_CCFLAGS = ['-fno-tree-vrp']
    GCC_DTOA_EXTRA_CCFLAGS = ['-fno-strict-aliasing']
else:
    GCC_EXTRA_CCFLAGS = []
    GCC_DTOA_EXTRA_CCFLAGS = []

ANDROID_FLAGS = ['-march=armv5te',
                 '-mtune=xscale',
                 '-msoft-float',
                 '-fpic',
                 '-mthumb-interwork',
                 '-funwind-tables',
                 '-fstack-protector',
                 '-fno-short-enums',
                 '-fmessage-length=0',
                 '-finline-functions',
                 '-fno-inline-functions-called-once',
                 '-fgcse-after-reload',
                 '-frerun-cse-after-loop',
                 '-frename-registers',
                 '-fomit-frame-pointer',
                 '-fno-strict-aliasing',
                 '-finline-limit=64',
                 '-MD']

ANDROID_INCLUDES = [ANDROID_TOP + '/bionic/libc/arch-arm/include',
                    ANDROID_TOP + '/bionic/libc/include',
                    ANDROID_TOP + '/bionic/libstdc++/include',
                    ANDROID_TOP + '/bionic/libc/kernel/common',
                    ANDROID_TOP + '/bionic/libc/kernel/arch-arm',
                    ANDROID_TOP + '/bionic/libm/include',
                    ANDROID_TOP + '/bionic/libm/include/arch/arm',
                    ANDROID_TOP + '/bionic/libthread_db/include',
                    ANDROID_TOP + '/frameworks/base/include',
                    ANDROID_TOP + '/system/core/include']

ANDROID_LINKFLAGS = ['-nostdlib',
                     '-Bdynamic',
                     '-Wl,-T,' + ANDROID_TOP + '/build/core/armelf.x',
                     '-Wl,-dynamic-linker,/system/bin/linker',
                     '-Wl,--gc-sections',
                     '-Wl,-z,nocopyreloc',
                     '-Wl,-rpath-link=' + ANDROID_TOP + '/out/target/product/generic/obj/lib',
                     ANDROID_TOP + '/out/target/product/generic/obj/lib/crtbegin_dynamic.o',
                     ANDROID_TOP + '/prebuilt/linux-x86/toolchain/arm-eabi-4.2.1/lib/gcc/arm-eabi/4.2.1/interwork/libgcc.a',
                     ANDROID_TOP + '/out/target/product/generic/obj/lib/crtend_android.o'];

LIBRARY_FLAGS = {
  'all': {
    'CPPPATH': [join(root_dir, 'src')],
    'regexp:native': {
        'CPPDEFINES': ['V8_NATIVE_REGEXP']
    },
    'mode:debug': {
      'CPPDEFINES': ['V8_ENABLE_CHECKS']
    },
    'profilingsupport:on': {
      'CPPDEFINES':   ['ENABLE_LOGGING_AND_PROFILING'],
    },
    'debuggersupport:on': {
      'CPPDEFINES':   ['ENABLE_DEBUGGER_SUPPORT'],
    }
  },
  'gcc': {
    'all': {
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['$CCFLAGS', '-fno-rtti', '-fno-exceptions'],
    },
    'visibility:hidden': {
      # Use visibility=default to disable this.
      'CXXFLAGS':     ['-fvisibility=hidden']
    },
    'mode:debug': {
      'CCFLAGS':      ['-g', '-O0'],
      'CPPDEFINES':   ['ENABLE_DISASSEMBLER', 'DEBUG'],
      'os:android': {
        'CCFLAGS':    ['-mthumb']
      }
    },
    'mode:release': {
      'CCFLAGS':      ['-O3', '-fomit-frame-pointer', '-fdata-sections',
                       '-ffunction-sections'],
      'os:android': {
        'CCFLAGS':    ['-mthumb', '-Os'],
        'CPPDEFINES': ['SK_RELEASE', 'NDEBUG']
      }
    },
    'os:linux': {
      'CCFLAGS':      ['-ansi'] + GCC_EXTRA_CCFLAGS,
      'library:shared': {
        'CPPDEFINES': ['V8_SHARED'],
        'LIBS': ['pthread']
      }
    },
    'os:macos': {
      'CCFLAGS':      ['-ansi', '-mmacosx-version-min=10.4'],
    },
    'os:freebsd': {
      'CPPPATH' : ['/usr/local/include'],
      'LIBPATH' : ['/usr/local/lib'],
      'CCFLAGS':      ['-ansi'],
    },
    'os:win32': {
      'CCFLAGS':      ['-DWIN32'],
      'CXXFLAGS':     ['-DWIN32'],
    },
    'os:android': {
      'CPPDEFINES':   ['ANDROID', '__ARM_ARCH_5__', '__ARM_ARCH_5T__',
                       '__ARM_ARCH_5E__', '__ARM_ARCH_5TE__'],
      'CCFLAGS':      ANDROID_FLAGS,
      'WARNINGFLAGS': ['-Wall', '-Wno-unused', '-Werror=return-type',
                       '-Wstrict-aliasing=2'],
      'CPPPATH':      ANDROID_INCLUDES,
    },
    'arch:ia32': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_IA32'],
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'arch:arm': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_ARM']
    },
    'simulator:arm': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'arch:x64': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_X64'],
      'CCFLAGS':      ['-m64'],
      'LINKFLAGS':    ['-m64'],
    },
    'prof:oprofile': {
      'CPPDEFINES':   ['ENABLE_OPROFILE_AGENT']
    }
  },
  'msvc': {
    'all': {
      'CCFLAGS':      ['$DIALECTFLAGS', '$WARNINGFLAGS'],
      'CXXFLAGS':     ['$CCFLAGS', '/GR-', '/Gy'],
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
        'LINKFLAGS':    ['/LTCG'],
        'ARFLAGS':      ['/LTCG'],
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
                       '-Wnon-virtual-dtor']
    },
    'os:win32': {
      'WARNINGFLAGS': ['-pedantic', '-Wno-long-long']
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
    'disassembler:on': {
      'CPPDEFINES':   ['ENABLE_DISASSEMBLER']
    }
  },
  'msvc': {
    'all': {
      'WARNINGFLAGS': ['/WX', '/wd4355', '/wd4800']
    },
    'library:shared': {
      'CPPDEFINES': ['BUILDING_V8_SHARED'],
      'LIBS': ['winmm', 'ws2_32']
    },
    'arch:ia32': {
      'WARNINGFLAGS': ['/W3']
    },
    'arch:x64': {
      'WARNINGFLAGS': ['/W2']
    },
    'arch:arm': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_ARM'],
      # /wd4996 is to silence the warning about sscanf
      # used by the arm simulator.
      'WARNINGFLAGS': ['/wd4996']
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
    'os:win32': {
      'LIBS': ['winmm', 'ws2_32'],
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
    'CPPPATH': [join(root_dir, 'src')],
    'LIBS': ['$LIBRARY']
  },
  'gcc': {
    'all': {
      'LIBPATH': [abspath('.')]
    },
    'os:linux': {
      'LIBS':         ['pthread'],
    },
    'os:macos': {
      'LIBS':         ['pthread'],
    },
    'os:freebsd': {
      'LIBS':         ['execinfo', 'pthread']
    },
    'os:win32': {
      'LIBS': ['winmm', 'ws2_32']
    },
    'os:android': {
      'CPPDEFINES':   ['ANDROID', '__ARM_ARCH_5__', '__ARM_ARCH_5T__',
                       '__ARM_ARCH_5E__', '__ARM_ARCH_5TE__'],
      'CCFLAGS':      ANDROID_FLAGS,
      'CPPPATH':      ANDROID_INCLUDES,
      'LIBPATH':     [ANDROID_TOP + '/out/target/product/generic/obj/lib'],
      'LINKFLAGS':    ANDROID_LINKFLAGS,
      'LIBS':         ['log', 'c', 'stdc++', 'm'],
      'mode:release': {
        'CPPDEFINES': ['SK_RELEASE', 'NDEBUG']
      }
    },
  },
  'msvc': {
    'all': {
      'CPPDEFINES': ['_HAS_EXCEPTIONS=0'],
      'LIBS': ['winmm', 'ws2_32']
    },
    'library:shared': {
      'CPPDEFINES': ['USING_V8_SHARED']
    },
    'arch:ia32': {
      'CPPDEFINES': ['V8_TARGET_ARCH_IA32']
    },
    'arch:x64': {
      'CPPDEFINES':   ['V8_TARGET_ARCH_X64']
    },
  }
}


SAMPLE_FLAGS = {
  'all': {
    'CPPPATH': [join(abspath('.'), 'include')],
    'LIBS': ['$LIBRARY'],
  },
  'gcc': {
    'all': {
      'LIBPATH': ['.'],
      'CCFLAGS': ['-fno-rtti', '-fno-exceptions']
    },
    'os:linux': {
      'LIBS':         ['pthread'],
    },
    'os:macos': {
      'LIBS':         ['pthread'],
    },
    'os:freebsd': {
      'LIBPATH' : ['/usr/local/lib'],
      'LIBS':         ['execinfo', 'pthread']
    },
    'os:win32': {
      'LIBS':         ['winmm', 'ws2_32']
    },
    'os:android': {
      'CPPDEFINES':   ['ANDROID', '__ARM_ARCH_5__', '__ARM_ARCH_5T__',
                       '__ARM_ARCH_5E__', '__ARM_ARCH_5TE__'],
      'CCFLAGS':      ANDROID_FLAGS,
      'CPPPATH':      ANDROID_INCLUDES,
      'LIBPATH':     [ANDROID_TOP + '/out/target/product/generic/obj/lib'],
      'LINKFLAGS':    ANDROID_LINKFLAGS,
      'LIBS':         ['log', 'c', 'stdc++', 'm'],
      'mode:release': {
        'CPPDEFINES': ['SK_RELEASE', 'NDEBUG']
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
    'simulator:arm': {
      'CCFLAGS':      ['-m32'],
      'LINKFLAGS':    ['-m32']
    },
    'mode:release': {
      'CCFLAGS':      ['-O2']
    },
    'mode:debug': {
      'CCFLAGS':      ['-g', '-O0']
    },
    'prof:oprofile': {
      'LIBPATH': ['/usr/lib32', '/usr/lib32/oprofile'],
      'LIBS': ['opagent']
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
    'library:shared': {
      'CPPDEFINES': ['USING_V8_SHARED']
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
        'LINKFLAGS':    ['/LTCG'],
      }
    },
    'arch:ia32': {
      'CPPDEFINES': ['V8_TARGET_ARCH_IA32'],
      'LINKFLAGS': ['/MACHINE:X86']
    },
    'arch:x64': {
      'CPPDEFINES': ['V8_TARGET_ARCH_X64'],
      'LINKFLAGS': ['/MACHINE:X64']
    },
    'mode:debug': {
      'CCFLAGS':   ['/Od'],
      'LINKFLAGS': ['/DEBUG'],
      'msvcrt:static': {
        'CCFLAGS': ['/MTd']
      },
      'msvcrt:shared': {
        'CCFLAGS': ['/MDd']
      }
    }
  }
}


D8_FLAGS = {
  'gcc': {
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
    'os:android': {
      'LIBPATH':     [ANDROID_TOP + '/out/target/product/generic/obj/lib'],
      'LINKFLAGS':    ANDROID_LINKFLAGS,
      'LIBS':         ['log', 'c', 'stdc++', 'm'],
    },
    'os:win32': {
      'LIBS': ['winmm', 'ws2_32'],
    },
  },
  'msvc': {
    'all': {
      'LIBS': ['winmm', 'ws2_32']
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


def GuessToolchain(os):
  tools = Environment()['TOOLS']
  if 'gcc' in tools:
    return 'gcc'
  elif 'msvc' in tools:
    return 'msvc'
  else:
    return None


OS_GUESS = utils.GuessOS()
TOOLCHAIN_GUESS = GuessToolchain(OS_GUESS)
ARCH_GUESS = utils.GuessArchitecture()


SIMPLE_OPTIONS = {
  'toolchain': {
    'values': ['gcc', 'msvc'],
    'default': TOOLCHAIN_GUESS,
    'help': 'the toolchain to use (' + TOOLCHAIN_GUESS + ')'
  },
  'os': {
    'values': ['freebsd', 'linux', 'macos', 'win32', 'android'],
    'default': OS_GUESS,
    'help': 'the os to build for (' + OS_GUESS + ')'
  },
  'arch': {
    'values':['arm', 'ia32', 'x64'],
    'default': ARCH_GUESS,
    'help': 'the architecture to build for (' + ARCH_GUESS + ')'
  },
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
    'values': ['on', 'off', 'oprofile'],
    'default': 'off',
    'help': 'enable profiling of build target'
  },
  'library': {
    'values': ['static', 'shared'],
    'default': 'static',
    'help': 'the type of library to produce'
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
    'values': ['arm', 'none'],
    'default': 'none',
    'help': 'build with simulator'
  },
  'disassembler': {
    'values': ['on', 'off'],
    'default': 'off',
    'help': 'enable the disassembler to inspect generated code'
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
    'default': 'hidden',
    'help': 'shared library symbol visibility'
  }
}


def GetOptions():
  result = Options()
  result.Add('mode', 'compilation mode (debug, release)', 'release')
  result.Add('sample', 'build sample (shell, process)', '')
  result.Add('env', 'override environment settings (NAME0:value0,NAME1:value1,...)', '')
  result.Add('importenv', 'import environment settings (NAME0,NAME1,...)', '')
  for (name, option) in SIMPLE_OPTIONS.iteritems():
    help = '%s (%s)' % (name, ", ".join(option['values']))
    result.Add(name, help, option.get('default'))
  return result


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


def VerifyOptions(env):
  if not IsLegal(env, 'mode', ['debug', 'release']):
    return False
  if not IsLegal(env, 'sample', ["shell", "process"]):
    return False
  if not IsLegal(env, 'regexp', ["native", "interpreted"]):
    return False
  if env['os'] == 'win32' and env['library'] == 'shared' and env['prof'] == 'on':
    Abort("Profiling on windows only supported for static library.")
  if env['prof'] == 'oprofile' and env['os'] != 'linux':
    Abort("OProfile is only supported on Linux.")
  if env['os'] == 'win32' and env['soname'] == 'on':
    Abort("Shared Object soname not applicable for Windows.")
  if env['soname'] == 'on' and env['library'] == 'static':
    Abort("Shared Object soname not applicable for static library.")
  for (name, option) in SIMPLE_OPTIONS.iteritems():
    if (not option.get('default')) and (name not in ARGUMENTS):
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


def PostprocessOptions(options):
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


def BuildSpecific(env, mode, env_overrides):
  options = {'mode': mode}
  for option in SIMPLE_OPTIONS:
    options[option] = env[option]
  PostprocessOptions(options)

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
  d8_flags = context.AddRelevantFlags(library_flags, D8_FLAGS)

  context.flags = {
    'v8': v8_flags,
    'mksnapshot': mksnapshot_flags,
    'dtoa': dtoa_flags,
    'cctest': cctest_flags,
    'sample': sample_flags,
    'd8': d8_flags
  }

  # Generate library base name.
  target_id = mode
  suffix = SUFFIXES[target_id]
  library_name = 'v8' + suffix
  version = GetVersion()
  if context.options['soname'] == 'on':
    # When building shared object with SONAME version the library name.
    library_name += '-' + version
  env['LIBRARY'] = library_name

  # Generate library SONAME if required by the build.
  if context.options['soname'] == 'on':
    soname = GetSpecificSONAME()
    if soname == '':
      soname = 'lib' + library_name + '.so'
    env['SONAME'] = soname

  # Build the object files by invoking SCons recursively.
  (object_files, shell_files, mksnapshot) = env.SConscript(
    join('src', 'SConscript'),
    build_dir=join('obj', target_id),
    exports='context',
    duplicate=False
  )

  context.mksnapshot_targets.append(mksnapshot)

  # Link the object files into a library.
  env.Replace(**context.flags['v8'])
  context.ApplyEnvOverrides(env)
  if context.options['library'] == 'static':
    library = env.StaticLibrary(library_name, object_files)
  else:
    # There seems to be a glitch in the way scons decides where to put
    # PDB files when compiling using MSVC so we specify it manually.
    # This should not affect any other platforms.
    pdb_name = library_name + '.dll.pdb'
    library = env.SharedLibrary(library_name, object_files, PDB=pdb_name)
  context.library_targets.append(library)

  d8_env = Environment()
  d8_env.Replace(**context.flags['d8'])
  shell = d8_env.Program('d8' + suffix, object_files + shell_files)
  context.d8_targets.append(shell)

  for sample in context.samples:
    sample_env = Environment(LIBRARY=library_name)
    sample_env.Replace(**context.flags['sample'])
    context.ApplyEnvOverrides(sample_env)
    sample_object = sample_env.SConscript(
      join('samples', 'SConscript'),
      build_dir=join('obj', 'sample', sample, target_id),
      exports='sample context',
      duplicate=False
    )
    sample_name = sample + suffix
    sample_program = sample_env.Program(sample_name, sample_object)
    sample_env.Depends(sample_program, library)
    context.sample_targets.append(sample_program)

  cctest_program = env.SConscript(
    join('test', 'cctest', 'SConscript'),
    build_dir=join('obj', 'test', target_id),
    exports='context object_files',
    duplicate=False
  )
  context.cctest_targets.append(cctest_program)

  return context


def Build():
  opts = GetOptions()
  env = Environment(options=opts)
  Help(opts.GenerateHelpText(env))
  VerifyOptions(env)
  env_overrides = ParseEnvOverrides(env['env'], env['importenv'])

  SourceSignatures(env['sourcesignatures'])

  libraries = []
  mksnapshots = []
  cctests = []
  samples = []
  d8s = []
  modes = SplitList(env['mode'])
  for mode in modes:
    context = BuildSpecific(env.Copy(), mode, env_overrides)
    libraries += context.library_targets
    mksnapshots += context.mksnapshot_targets
    cctests += context.cctest_targets
    samples += context.sample_targets
    d8s += context.d8_targets

  env.Alias('library', libraries)
  env.Alias('mksnapshot', mksnapshots)
  env.Alias('cctests', cctests)
  env.Alias('sample', samples)
  env.Alias('d8', d8s)

  if env['sample']:
    env.Default('sample')
  else:
    env.Default('library')


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
