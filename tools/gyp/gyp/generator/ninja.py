# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import codecs
import hashlib
import os.path
import re
import subprocess
import sys
from collections import OrderedDict

import gyp
import gyp.common
import gyp.msvs_emulation
import gyp.xcode_emulation
import gyp.MSVS as MSVS
from gyp.common import GetEnvironFallback
from gyp.lib import ninja_syntax

try:
  # noinspection PyCompatibility
  from cStringIO import StringIO
except ImportError:
  from io import StringIO


generator_default_variables = {
  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  'STATIC_LIB_PREFIX': 'lib',
  'STATIC_LIB_SUFFIX': '.a',
  'SHARED_LIB_PREFIX': 'lib',

  # Gyp expects the following variables to be expandable by the build
  # system to the appropriate locations.  Ninja prefers paths to be
  # known at gyp time.  To resolve this, introduce special
  # variables starting with $! and $| (which begin with a $ so gyp knows it
  # should be treated specially, but is otherwise an invalid
  # ninja/shell variable) that are passed to gyp here but expanded
  # before writing out into the target .ninja files; see NinjaWriter._ExpandSpecial.
  # $! is used for variables that represent a path and that can only appear at
  # the start of a string, while $| is used for variables that can appear
  # anywhere in a string.
  'INTERMEDIATE_DIR': '$!INTERMEDIATE_DIR',
  'SHARED_INTERMEDIATE_DIR': '$!PRODUCT_DIR/gen',
  'PRODUCT_DIR': '$!PRODUCT_DIR',
  'CONFIGURATION_NAME': '$|CONFIGURATION_NAME',

  # Special variables that may be used by gyp 'rule' targets.
  # We generate definitions for these variables on the fly when processing a
  # rule.
  'RULE_INPUT_ROOT': '${root}',
  'RULE_INPUT_DIRNAME': '${dirname}',
  'RULE_INPUT_PATH': '${source}',
  'RULE_INPUT_EXT': '${ext}',
  'RULE_INPUT_NAME': '${name}',
}

# Placates pylint.
generator_additional_non_configuration_keys = []
generator_additional_path_sections = []
generator_extra_sources_for_rules = []
generator_filelist_paths = None

generator_supports_multiple_toolsets = gyp.common.CrossCompileRequested()


def ComputeOutputDir(params):
  """Returns the path from the toplevel_dir to the build output directory."""
  # generator_dir: relative path from pwd to where make puts build files.
  # Makes migrating from make to ninja easier, ninja doesn't put anything here.
  generator_dir = os.path.relpath(params['options'].generator_output or '.')

  # output_dir: relative path from generator_dir to the build directory.
  output_dir = params.get('generator_flags', {}).get('output_dir', 'out')

  # Relative path from source root to our output files.  e.g. "out"
  return os.path.normpath(os.path.join(generator_dir, output_dir))

# module level API
def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  global generator_additional_non_configuration_keys
  global generator_additional_path_sections
  flavor = gyp.common.GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.dylib')
    default_variables.setdefault('SHARED_LIB_DIR', generator_default_variables['PRODUCT_DIR'])
    default_variables.setdefault('LIB_DIR', generator_default_variables['PRODUCT_DIR'])

    # Copy additional generator configuration data from Xcode, which is shared
    # by the Mac Ninja generator.
    import gyp.generator.xcode as xcode_generator
    generator_additional_non_configuration_keys = getattr(xcode_generator, 'generator_additional_non_configuration_keys', [])
    generator_additional_path_sections = getattr(xcode_generator, 'generator_additional_path_sections', [])
    global generator_extra_sources_for_rules
    generator_extra_sources_for_rules = getattr(xcode_generator, 'generator_extra_sources_for_rules', [])
  elif flavor == 'win':
    exts = MSVS.TARGET_TYPE_EXT
    default_variables.setdefault('OS', 'win')
    default_variables['EXECUTABLE_SUFFIX'] = '.' + exts['executable']
    default_variables['STATIC_LIB_PREFIX'] = ''
    default_variables['STATIC_LIB_SUFFIX'] = '.' + exts['static_library']
    default_variables['SHARED_LIB_PREFIX'] = ''
    default_variables['SHARED_LIB_SUFFIX'] = '.' + exts['shared_library']

    # Copy additional generator configuration data from VS, which is shared
    # by the Windows Ninja generator.
    import gyp.generator.msvs as msvs_generator
    generator_additional_non_configuration_keys = getattr(msvs_generator, 'generator_additional_non_configuration_keys', [])
    generator_additional_path_sections = getattr(msvs_generator, 'generator_additional_path_sections', [])

    gyp.msvs_emulation.CalculateCommonVariables(default_variables, params)
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.so')
    default_variables.setdefault('SHARED_LIB_DIR', os.path.join('$!PRODUCT_DIR', 'lib'))
    default_variables.setdefault('LIB_DIR', os.path.join('$!PRODUCT_DIR', 'obj'))


# module level API
def CalculateGeneratorInputInfo(params):
  """Called by __init__ to initialize generator values based on params."""
  # E.g. "out/gypfiles"
  toplevel = params['options'].toplevel_dir
  qualified_out_dir = os.path.normpath(os.path.join(toplevel, ComputeOutputDir(params), 'gypfiles'))

  global generator_filelist_paths
  generator_filelist_paths = {
      'toplevel': toplevel,
      'qualified_out_dir': qualified_out_dir,
  }


def OpenOutput(path):
  """Open |path| for writing, creating directories if necessary."""
  gyp.common.EnsureDirExists(path)
  return codecs.open(path, mode='w', encoding='utf-8')


def CommandWithWrapper(cmd, wrappers, prog):
  wrapper = wrappers.get(cmd, '')
  if wrapper:
    return wrapper + ' ' + prog
  return prog


def GetDefaultConcurrentLinks():
  """Returns a best-guess for a number of concurrent links."""
  pool_size = int(os.environ.get('GYP_LINK_CONCURRENCY', 0))
  if pool_size:
    return pool_size

  if sys.platform in ('win32', 'cygwin'):
    import ctypes

    class MEMORYSTATUSEX(ctypes.Structure):
      _fields_ = [
        ("dwLength", ctypes.c_ulong),
        ("dwMemoryLoad", ctypes.c_ulong),
        ("ullTotalPhys", ctypes.c_ulonglong),
        ("ullAvailPhys", ctypes.c_ulonglong),
        ("ullTotalPageFile", ctypes.c_ulonglong),
        ("ullAvailPageFile", ctypes.c_ulonglong),
        ("ullTotalVirtual", ctypes.c_ulonglong),
        ("ullAvailVirtual", ctypes.c_ulonglong),
        ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
      ]

    stat = MEMORYSTATUSEX()
    stat.dwLength = ctypes.sizeof(stat)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))

    # VS 2015 uses 20% more working set than VS 2013 and can consume all RAM
    # on a 64 GB machine.
    mem_limit = max(1, stat.ullTotalPhys // (5 * (2 ** 30)))  # total / 5GB
    hard_cap = max(1, int(os.environ.get('GYP_LINK_CONCURRENCY_MAX', 2 ** 32)))
    return min(mem_limit, hard_cap)
  elif sys.platform.startswith('linux'):
    if os.path.exists("/proc/meminfo"):
      with open("/proc/meminfo") as meminfo:
        memtotal_re = re.compile(r'^MemTotal:\s*(\d*)\s*kB')
        for line in meminfo:
          match = memtotal_re.match(line)
          if not match:
            continue
          # Allow 8Gb per link on Linux because Gold is quite memory hungry
          return max(1, int(match.group(1)) // (8 * (2 ** 20)))
    return 1
  elif sys.platform == 'darwin':
    try:
      avail_bytes = int(subprocess.check_output(['sysctl', '-n', 'hw.memsize']))
      # A static library debug build of Chromium's unit_tests takes ~2.7GB, so
      # 4GB per ld process allows for some more bloat.
      return max(1, avail_bytes // (4 * (2 ** 30)))  # total / 4GB
    except ValueError:
      return 1
  else:
    # TODO(scottmg): Implement this for other platforms.
    return 1


def GetWinLinkRuleNameSuffix(embed_manifest):
  """
  Returns the suffix used to select an appropriate linking rule depending on  whether the manifest embedding is enabled.
  """
  return '_embed' if embed_manifest else ''


def _AddWinLinkRules(master_ninja, embed_manifest):
  """Adds link rules for Windows platform to |master_ninja|."""

  def FullLinkCommand(ldcmd, out, binary_type):
    resource_name = {
      'exe': '1',
      'dll': '2',
    }[binary_type]
    return '%(python)s gyp-win-tool link-with-manifests $arch %(embed)s %(out)s "%(ldcmd)s" %(resname)s $mt $rc "$intermediatemanifest" $manifests' % {
               'python': sys.executable,
               'out': out,
               'ldcmd': ldcmd,
               'resname': resource_name,
               'embed': embed_manifest }
  rule_name_suffix = GetWinLinkRuleNameSuffix(embed_manifest)
  use_separate_mspdbsrv = (int(os.environ.get('GYP_USE_SEPARATE_MSPDBSRV', '0')) != 0)
  dlldesc = 'LINK%s(DLL) $binary' % rule_name_suffix.upper()
  dllcmd = ('%s gyp-win-tool link-wrapper $arch %s $ld /nologo $implibflag /DLL /OUT:$binary @$binary.rsp' % (sys.executable, use_separate_mspdbsrv))
  dllcmd = FullLinkCommand(dllcmd, '$binary', 'dll')
  master_ninja.rule('solink' + rule_name_suffix,
                    description=dlldesc, command=dllcmd,
                    rspfile='$binary.rsp',
                    rspfile_content='$libs $in_newline $ldflags',
                    restat=True,
                    pool='link_pool')
  master_ninja.rule('solink_module' + rule_name_suffix,
                    description=dlldesc, command=dllcmd,
                    rspfile='$binary.rsp',
                    rspfile_content='$libs $in_newline $ldflags',
                    restat=True,
                    pool='link_pool')
  # Note that ldflags goes at the end so that it has the option of
  # overriding default settings earlier in the command line.
  exe_cmd = ('%s gyp-win-tool link-wrapper $arch %s $ld /nologo /OUT:$binary @$binary.rsp' % (sys.executable, use_separate_mspdbsrv))
  exe_cmd = FullLinkCommand(exe_cmd, '$binary', 'exe')
  master_ninja.rule('link' + rule_name_suffix, description='LINK%s $binary' % rule_name_suffix.upper(), command=exe_cmd, rspfile='$binary.rsp', rspfile_content='$in_newline $libs $ldflags', pool='link_pool')


def GenerateOutputForConfig(target_list, target_dicts, data, params, config_name):
  options = params['options']
  flavor = gyp.common.GetFlavor(params)
  generator_flags = params.get('generator_flags', OrderedDict())

  # build_dir: relative path from source root to our output files.
  # e.g. "out/Debug"
  build_dir = os.path.normpath(os.path.join(ComputeOutputDir(params), config_name))

  toplevel_build = os.path.join(options.toplevel_dir, build_dir)

  master_ninja_file = OpenOutput(os.path.join(toplevel_build, 'build.ninja'))
  master_ninja_writer = ninja_syntax.Writer(master_ninja_file, width=250)

  # Put build-time support tools in out/{config_name}.
  gyp.common.CopyTool(flavor, toplevel_build, generator_flags.get('mac_toolchain_dir'))

  # Grab make settings for CC/CXX.
  # The rules are
  # - The priority from low to high is gcc/g++, the 'make_global_settings' in
  #   gyp, the environment variable.
  # - If there is no 'make_global_settings' for CC.host/CXX.host or
  #   'CC_host'/'CXX_host' enviroment variable, cc_host/cxx_host should be set
  #   to cc/cxx.
  if flavor == 'win':
    ar = 'lib.exe'
    # cc and cxx must be set to the correct architecture by overriding with one
    # of cl_x86 or cl_x64 below.
    cc = 'cl.exe'
    cxx = 'cl.exe'
    ld = 'link.exe'
    ld_host = '$ld'
    ldxx = 'UNSET'
    ldxx_host = 'UNSET'
  else:
    ar = 'ar'
    cc = 'cc'
    cxx = 'c++'
    ld = '$cc'
    ldxx = '$cxx'
    ld_host = '$cc_host'
    ldxx_host = '$cxx_host'

  ar_host = ar
  cc_host = None
  cxx_host = None
  cc_host_global_setting = None
  cxx_host_global_setting = None
  nm = 'nm'
  nm_host = 'nm'
  readelf = 'readelf'
  readelf_host = 'readelf'

  build_file, _, _ = gyp.common.ParseQualifiedTarget(target_list[0])
  make_global_settings = data[build_file].get('make_global_settings', [])
  build_to_root = gyp.common.InvertRelativePath(build_dir, options.toplevel_dir)
  wrappers = {}
  for key, value in make_global_settings:
    if key == 'AR':
      ar = os.path.join(build_to_root, value)
    if key == 'AR.host':
      ar_host = os.path.join(build_to_root, value)
    if key == 'CC':
      cc = os.path.join(build_to_root, value)
    if key == 'CXX':
      cxx = os.path.join(build_to_root, value)
    if key == 'CC.host':
      cc_host = os.path.join(build_to_root, value)
      cc_host_global_setting = value
    if key == 'CXX.host':
      cxx_host = os.path.join(build_to_root, value)
      cxx_host_global_setting = value
    if key == 'LD':
      ld = os.path.join(build_to_root, value)
    if key == 'LD.host':
      ld_host = os.path.join(build_to_root, value)
    if key == 'NM':
      nm = os.path.join(build_to_root, value)
    if key == 'NM.host':
      nm_host = os.path.join(build_to_root, value)
    if key == 'READELF':
      readelf = os.path.join(build_to_root, value)
    if key == 'READELF.host':
      readelf_host = os.path.join(build_to_root, value)
    if key.endswith('_wrapper'):
      wrappers[key[:-len('_wrapper')]] = os.path.join(build_to_root, value)

  # Support wrappers from environment variables too.
  for key, value in os.environ.items():
    if key.lower().endswith('_wrapper'):
      key_prefix = key[:-len('_wrapper')]
      key_prefix = re.sub(r'\.HOST$', '.host', key_prefix)
      wrappers[key_prefix] = os.path.join(build_to_root, value)

  mac_toolchain_dir = generator_flags.get('mac_toolchain_dir', None)
  if mac_toolchain_dir:
    wrappers['LINK'] = "export DEVELOPER_DIR='%s' &&" % mac_toolchain_dir

  if flavor == 'win':
    gyp.msvs_emulation.GenerateEnvironmentFiles(toplevel_build, generator_flags)

  cc = GetEnvironFallback(['CC_target', 'CC'], cc)
  master_ninja_writer.variable('cc', CommandWithWrapper('CC', wrappers, cc))
  cxx = GetEnvironFallback(['CXX_target', 'CXX'], cxx)
  master_ninja_writer.variable('cxx', CommandWithWrapper('CXX', wrappers, cxx))

  if flavor == 'win':
    master_ninja_writer.variable('ld', ld)
    master_ninja_writer.variable('idl', 'midl.exe')
    master_ninja_writer.variable('ar', ar)
    master_ninja_writer.variable('rc', 'rc.exe')
    master_ninja_writer.variable('ml_x86', 'ml.exe')
    master_ninja_writer.variable('ml_x64', 'ml64.exe')
    master_ninja_writer.variable('mt', 'mt.exe')
  else:
    master_ninja_writer.variable('ld', CommandWithWrapper('LINK', wrappers, ld))
    master_ninja_writer.variable('ldxx', CommandWithWrapper('LINK', wrappers, ldxx))
    master_ninja_writer.variable('ar', GetEnvironFallback(['AR_target', 'AR'], ar))
    if flavor != 'mac':
      # Mac does not use readelf/nm for .TOC generation, so avoiding polluting
      # the master ninja with extra unused variables.
      master_ninja_writer.variable('nm', GetEnvironFallback(['NM_target', 'NM'], nm))
      master_ninja_writer.variable('readelf', GetEnvironFallback(['READELF_target', 'READELF'], readelf))

  if generator_supports_multiple_toolsets:
    if not cc_host:
      cc_host = cc
    if not cxx_host:
      cxx_host = cxx

    master_ninja_writer.variable('ar_host', GetEnvironFallback(['AR_host'], ar_host))
    master_ninja_writer.variable('nm_host', GetEnvironFallback(['NM_host'], nm_host))
    master_ninja_writer.variable('readelf_host', GetEnvironFallback(['READELF_host'], readelf_host))
    cc_host = GetEnvironFallback(['CC_host'], cc_host)
    cxx_host = GetEnvironFallback(['CXX_host'], cxx_host)

    # The environment variable could be used in 'make_global_settings', like
    # ['CC.host', '$(CC)'] or ['CXX.host', '$(CXX)'], transform them here.
    if '$(CC)' in cc_host and cc_host_global_setting:
      cc_host = cc_host_global_setting.replace('$(CC)', cc)
    if '$(CXX)' in cxx_host and cxx_host_global_setting:
      cxx_host = cxx_host_global_setting.replace('$(CXX)', cxx)
    master_ninja_writer.variable('cc_host', CommandWithWrapper('CC.host', wrappers, cc_host))
    master_ninja_writer.variable('cxx_host', CommandWithWrapper('CXX.host', wrappers, cxx_host))
    if flavor == 'win':
      master_ninja_writer.variable('ld_host', ld_host)
    else:
      master_ninja_writer.variable('ld_host', CommandWithWrapper('LINK', wrappers, ld_host))
      master_ninja_writer.variable('ldxx_host', CommandWithWrapper('LINK', wrappers, ldxx_host))

  master_ninja_writer.newline()

  master_ninja_writer.pool('link_pool', depth=GetDefaultConcurrentLinks())
  master_ninja_writer.newline()

  deps = 'msvc' if flavor == 'win' else 'gcc'

  if flavor != 'win':
    master_ninja_writer.rule(
      'cc',
      description='CC $out',
      command=('$cc -MMD -MF $out.d $defines $includes $cflags $cflags_c '
              '$cflags_pch_c -c $in -o $out'),
      depfile='$out.d',
      deps=deps)
    master_ninja_writer.rule(
      'cc_s',
      description='CC $out',
      command=('$cc $defines $includes $cflags $cflags_c '
              '$cflags_pch_c -c $in -o $out'))
    master_ninja_writer.rule(
      'cxx',
      description='CXX $out',
      command=('$cxx -MMD -MF $out.d $defines $includes $cflags $cflags_cc '
              '$cflags_pch_cc -c $in -o $out'),
      depfile='$out.d',
      deps=deps)
  else:
    # TODO(scottmg) Separate pdb names is a test to see if it works around
    # http://crbug.com/142362. It seems there's a race between the creation of
    # the .pdb by the precompiled header step for .cc and the compilation of
    # .c files. This should be handled by mspdbsrv, but rarely errors out with
    #   c1xx : fatal error C1033: cannot open program database
    # By making the rules target separate pdb files this might be avoided.
    cc_command = ('ninja -t msvc -e $arch ' +
                  '-- '
                  '$cc /nologo /showIncludes /FC '
                  '@$out.rsp /c $in /Fo$out /Fd$pdbname_c ')
    cxx_command = ('ninja -t msvc -e $arch ' +
                   '-- '
                   '$cxx /nologo /showIncludes /FC '
                   '@$out.rsp /c $in /Fo$out /Fd$pdbname_cc ')
    master_ninja_writer.rule(
      'cc',
      description='CC $out',
      command=cc_command,
      rspfile='$out.rsp',
      rspfile_content='$defines $includes $cflags $cflags_c',
      deps=deps)
    master_ninja_writer.rule(
      'cxx',
      description='CXX $out',
      command=cxx_command,
      rspfile='$out.rsp',
      rspfile_content='$defines $includes $cflags $cflags_cc',
      deps=deps)
    master_ninja_writer.rule(
      'idl',
      description='IDL $in',
      command=('%s gyp-win-tool midl-wrapper $arch $outdir '
               '$tlb $h $dlldata $iid $proxy $in '
               '$midl_includes $idlflags' % sys.executable))
    master_ninja_writer.rule(
      'rc',
      description='RC $in',
      # Note: $in must be last otherwise rc.exe complains.
      command=('%s gyp-win-tool rc-wrapper '
               '$arch $rc $defines $resource_includes $rcflags /fo$out $in' %
               sys.executable))
    master_ninja_writer.rule(
      'asm',
      description='ASM $out',
      command=('%s gyp-win-tool asm-wrapper '
               '$arch $asm $defines $includes $asmflags /c /Fo $out $in' %
               sys.executable))

  if flavor != 'mac' and flavor != 'win':
    master_ninja_writer.rule('alink', description='AR $out', command='rm -f $out && $ar rcs $arflags $out $in')
    master_ninja_writer.rule('alink_thin', description='AR $out', command='rm -f $out && $ar rcsT $arflags $out $in')

    # This allows targets that only need to depend on $lib's API to declare an
    # order-only dependency on $lib.TOC and avoid relinking such downstream
    # dependencies when $lib changes only in non-public ways.
    # The resulting string leaves an uninterpolated %{suffix} which
    # is used in the final substitution below.
    mtime_preserving_solink_base = (
        'if [ ! -e $lib -o ! -e $lib.TOC ]; then '
        '%(solink)s && %(extract_toc)s > $lib.TOC; else '
        '%(solink)s && %(extract_toc)s > $lib.tmp && '
        'if ! cmp -s $lib.tmp $lib.TOC; then mv $lib.tmp $lib.TOC ; '
        'fi; fi'
        % { 'solink':
              '$ld -shared $ldflags -o $lib -Wl,-soname=$soname %(suffix)s',
            'extract_toc':
              ('{ $readelf -d $lib | grep SONAME ; '
               '$nm -gD -f p $lib | cut -f1-2 -d\' \'; }')})

    master_ninja_writer.rule(
      'solink',
      description='SOLINK $lib',
      restat=True,
      command=mtime_preserving_solink_base % {'suffix': '@$link_file_list'},
      rspfile='$link_file_list',
      rspfile_content=
          '-Wl,--whole-archive $in $solibs -Wl,--no-whole-archive $libs',
      pool='link_pool')
    master_ninja_writer.rule(
      'solink_module',
      description='SOLINK(module) $lib',
      restat=True,
      command=mtime_preserving_solink_base % {'suffix': '@$link_file_list'},
      rspfile='$link_file_list',
      rspfile_content='-Wl,--start-group $in -Wl,--end-group $solibs $libs',
      pool='link_pool')
    master_ninja_writer.rule(
      'link',
      description='LINK $out',
      command=('$ld $ldflags -o $out '
               '-Wl,--start-group $in -Wl,--end-group $solibs $libs'),
      pool='link_pool')
  elif flavor == 'win':
    master_ninja_writer.rule(
        'alink',
        description='LIB $out',
        command=('%s gyp-win-tool link-wrapper $arch False '
                 '$ar /nologo /ignore:4221 /OUT:$out @$out.rsp' %
                 sys.executable),
        rspfile='$out.rsp',
        rspfile_content='$in_newline $libflags')
    _AddWinLinkRules(master_ninja_writer, embed_manifest=True)
    _AddWinLinkRules(master_ninja_writer, embed_manifest=False)
  else:
    master_ninja_writer.rule(
      'objc',
      description='OBJC $out',
      command=('$cc -MMD -MF $out.d $defines $includes $cflags $cflags_objc '
               '$cflags_pch_objc -c $in -o $out'),
      depfile='$out.d',
      deps=deps)
    master_ninja_writer.rule(
      'objcxx',
      description='OBJCXX $out',
      command=('$cxx -MMD -MF $out.d $defines $includes $cflags $cflags_objcc '
               '$cflags_pch_objcc -c $in -o $out'),
      depfile='$out.d',
      deps=deps)
    master_ninja_writer.rule(
      'alink',
      description='LIBTOOL-STATIC $out, POSTBUILDS',
      command='rm -f $out && '
              './gyp-mac-tool filter-libtool libtool $libtool_flags '
              '-static -o $out $in'
              '$postbuilds')
    master_ninja_writer.rule(
      'lipo',
      description='LIPO $out, POSTBUILDS',
      command='rm -f $out && lipo -create $in -output $out$postbuilds')
    master_ninja_writer.rule(
      'solipo',
      description='SOLIPO $out, POSTBUILDS',
      command=(
          'rm -f $lib $lib.TOC && lipo -create $in -output $lib$postbuilds &&'
          '%(extract_toc)s > $lib.TOC'
          % { 'extract_toc':
                '{ otool -l $lib | grep LC_ID_DYLIB -A 5; '
                'nm -gP $lib | cut -f1-2 -d\' \' | grep -v U$$; true; }'}))


    # Record the public interface of $lib in $lib.TOC. See the corresponding
    # comment in the posix section above for details.
    solink_base = '$ld %(type)s $ldflags -o $lib %(suffix)s'
    mtime_preserving_solink_base = (
        'if [ ! -e $lib -o ! -e $lib.TOC ] || '
             # Always force dependent targets to relink if this library
             # reexports something. Handling this correctly would require
             # recursive TOC dumping but this is rare in practice, so punt.
             'otool -l $lib | grep -q LC_REEXPORT_DYLIB ; then '
          '%(solink)s && %(extract_toc)s > $lib.TOC; '
        'else '
          '%(solink)s && %(extract_toc)s > $lib.tmp && '
          'if ! cmp -s $lib.tmp $lib.TOC; then '
            'mv $lib.tmp $lib.TOC ; '
          'fi; '
        'fi'
        % { 'solink': solink_base,
            'extract_toc':
              '{ otool -l $lib | grep LC_ID_DYLIB -A 5; '
              'nm -gP $lib | cut -f1-2 -d\' \' | grep -v U$$; true; }'})

    solink_suffix = '@$link_file_list$postbuilds'
    master_ninja_writer.rule(
      'solink',
      description='SOLINK $lib, POSTBUILDS',
      restat=True,
      command=mtime_preserving_solink_base % {'suffix': solink_suffix,
                                              'type': '-shared'},
      rspfile='$link_file_list',
      rspfile_content='$in $solibs $libs',
      pool='link_pool')
    master_ninja_writer.rule(
      'solink_notoc',
      description='SOLINK $lib, POSTBUILDS',
      restat=True,
      command=solink_base % {'suffix':solink_suffix, 'type': '-shared'},
      rspfile='$link_file_list',
      rspfile_content='$in $solibs $libs',
      pool='link_pool')

    master_ninja_writer.rule(
      'solink_module',
      description='SOLINK(module) $lib, POSTBUILDS',
      restat=True,
      command=mtime_preserving_solink_base % {'suffix': solink_suffix,
                                              'type': '-bundle'},
      rspfile='$link_file_list',
      rspfile_content='$in $solibs $libs',
      pool='link_pool')
    master_ninja_writer.rule(
      'solink_module_notoc',
      description='SOLINK(module) $lib, POSTBUILDS',
      restat=True,
      command=solink_base % {'suffix': solink_suffix, 'type': '-bundle'},
      rspfile='$link_file_list',
      rspfile_content='$in $solibs $libs',
      pool='link_pool')

    master_ninja_writer.rule(
      'link',
      description='LINK $out, POSTBUILDS',
      command=('$ld $ldflags -o $out '
               '$in $solibs $libs$postbuilds'),
      pool='link_pool')
    master_ninja_writer.rule(
      'preprocess_infoplist',
      description='PREPROCESS INFOPLIST $out',
      command=('$cc -E -P -Wno-trigraphs -x c $defines $in -o $out && '
               'plutil -convert xml1 $out $out'))
    master_ninja_writer.rule(
      'copy_infoplist',
      description='COPY INFOPLIST $in',
      command='$env ./gyp-mac-tool copy-info-plist $in $out $binary $keys')
    master_ninja_writer.rule(
      'merge_infoplist',
      description='MERGE INFOPLISTS $in',
      command='$env ./gyp-mac-tool merge-info-plist $out $in')
    master_ninja_writer.rule(
      'compile_xcassets',
      description='COMPILE XCASSETS $in',
      command='$env ./gyp-mac-tool compile-xcassets $keys $in')
    master_ninja_writer.rule(
      'compile_ios_framework_headers',
      description='COMPILE HEADER MAPS AND COPY FRAMEWORK HEADERS $in',
      command='$env ./gyp-mac-tool compile-ios-framework-header-map $out '
              '$framework $in && $env ./gyp-mac-tool '
              'copy-ios-framework-headers $framework $copy_headers')
    master_ninja_writer.rule(
      'mac_tool',
      description='MACTOOL $mactool_cmd $in',
      command='$env ./gyp-mac-tool $mactool_cmd $in $out $binary')
    master_ninja_writer.rule(
      'package_framework',
      description='PACKAGE FRAMEWORK $out, POSTBUILDS',
      command='./gyp-mac-tool package-framework $out $version$postbuilds '
              '&& touch $out')
    master_ninja_writer.rule(
      'package_ios_framework',
      description='PACKAGE IOS FRAMEWORK $out, POSTBUILDS',
      command='./gyp-mac-tool package-ios-framework $out $postbuilds '
              '&& touch $out')
  if flavor == 'win':
    master_ninja_writer.rule('stamp', description='STAMP $out', command='%s gyp-win-tool stamp $out' % sys.executable)
  else:
    master_ninja_writer.rule('stamp', description='STAMP $out', command='${postbuilds}touch $out')
  if flavor == 'win':
    master_ninja_writer.rule('copy', description='COPY $in $out', command='%s gyp-win-tool recursive-mirror $in $out' % sys.executable)
  elif flavor == 'zos':
    master_ninja_writer.rule('copy', description='COPY $in $out', command='rm -rf $out && cp -fRP $in $out')
  else:
    master_ninja_writer.rule('copy', description='COPY $in $out', command='ln -f $in $out 2>/dev/null || (rm -rf $out && cp -af $in $out)')
  master_ninja_writer.newline()

  all_targets = set()
  for build_file in params['build_files']:
    for target in gyp.common.AllTargets(target_list, target_dicts, os.path.normpath(build_file)):
      all_targets.add(target)
  all_outputs = set()

  # target_outputs is a map from qualified target name to a Target object.
  target_outputs = {}
  # target_short_names is a map from target short name to a list of Target
  # objects.
  target_short_names = {}

  # short name of targets that were skipped because they didn't contain anything
  # interesting.
  # NOTE: there may be overlap between this an non_empty_target_names.
  empty_target_names = set()

  # Set of non-empty short target names.
  # NOTE: there may be overlap between this an empty_target_names.
  non_empty_target_names = set()

  for qualified_target in target_list:
    # qualified_target is like: third_party/icu/icu.gyp:icui18n#target
    build_file, name, toolset = gyp.common.ParseQualifiedTarget(qualified_target)

    this_make_global_settings = data[build_file].get('make_global_settings', [])
    assert make_global_settings == this_make_global_settings, ("make_global_settings needs to be the same for all targets. %s vs. %s" % (this_make_global_settings, make_global_settings))

    spec = target_dicts[qualified_target]
    if flavor == 'mac':
      gyp.xcode_emulation.MergeGlobalXcodeSettingsToSpec(data[build_file], spec)

    # If build_file is a symlink, we must not follow it because there's a chance
    # it could point to a path above toplevel_dir, and we cannot correctly deal
    # with that case at the moment.
    build_file = gyp.common.RelativePath(build_file, options.toplevel_dir, False)

    qualified_target_for_hash = gyp.common.QualifiedTarget(build_file, name, toolset)
    qualified_target_for_hash = qualified_target_for_hash.encode('utf-8')
    hash_for_rules = hashlib.md5(qualified_target_for_hash).hexdigest()

    base_path = os.path.dirname(build_file)
    obj = 'obj'
    if toolset != 'target':
      obj += '.' + toolset
    output_file = os.path.join(obj, base_path, name + '.ninja')

    ninja_output = StringIO()
    from gyp.NinjaWriter import NinjaWriter
    writer = NinjaWriter(hash_for_rules, target_outputs, base_path, build_dir, ninja_output, toplevel_build, output_file, flavor, spec, generator_flags, config_name, options.toplevel_dir)

    target = writer.WriteSpec()

    if ninja_output.tell() > 0:
      # Only create files for ninja files that actually have contents.
      with OpenOutput(os.path.join(toplevel_build, output_file)) as ninja_file:
        ninja_file.write(ninja_output.getvalue())
      ninja_output.close()
      master_ninja_writer.subninja(output_file)

    if target:
      if name != target.FinalOutput() and spec['toolset'] == 'target':
        target_short_names.setdefault(name, []).append(target)
      target_outputs[qualified_target] = target
      if qualified_target in all_targets:
        all_outputs.add(target.FinalOutput())
      non_empty_target_names.add(name)
    else:
      empty_target_names.add(name)

  if target_short_names:
    # Write a short name to build this target.  This benefits both the
    # "build chrome" case as well as the gyp tests, which expect to be
    # able to run actions and build libraries by their short name.
    master_ninja_writer.newline()
    master_ninja_writer.comment('Short names for targets.')
    for short_name in sorted(target_short_names):
      master_ninja_writer.build(short_name, 'phony', [x.FinalOutput() for x in target_short_names[short_name]])

  # Write phony targets for any empty targets that weren't written yet. As
  # short names are  not necessarily unique only do this for short names that
  # haven't already been output for another target.
  empty_target_names = empty_target_names - non_empty_target_names
  if empty_target_names:
    master_ninja_writer.newline()
    master_ninja_writer.comment('Empty targets (output for completeness).')
    for name in sorted(empty_target_names):
      master_ninja_writer.build(name, 'phony')

  if all_outputs:
    master_ninja_writer.newline()
    master_ninja_writer.build('all', 'phony', sorted(all_outputs))
    master_ninja_writer.default(generator_flags.get('default_target', 'all'))

  master_ninja_file.close()


def PerformBuild(_, configurations, params):
  options = params['options']
  for config in configurations:
    builddir = os.path.join(options.toplevel_dir, 'out', config)
    arguments = ['ninja', '-C', builddir]
    print('Building [%s]: %s' % (config, arguments))
    subprocess.check_call(arguments)


def GenerateOutput(target_list, target_dicts, data, params):
  user_config = params['generator_flags'].get('config', None)
  if gyp.common.GetFlavor(params) == 'win':
    target_list, target_dicts = MSVS.ShardTargets(target_list, target_dicts)
    target_list, target_dicts = MSVS.InsertLargePdbShims(target_list, target_dicts, generator_default_variables)
  elif gyp.common.GetFlavor(params) == 'mac':
    # Update target_dicts for iOS device builds.
    target_dicts = gyp.xcode_emulation.CloneConfigurationForDeviceAndEmulator(target_dicts)

  if user_config:
    GenerateOutputForConfig(target_list, target_dicts, data, params, user_config)
  else:
    config_names = target_dicts[target_list[0]]['configurations']
    for config_name in config_names:
      GenerateOutputForConfig(target_list, target_dicts, data, params, config_name)
