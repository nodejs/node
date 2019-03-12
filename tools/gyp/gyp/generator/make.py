# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Notes:
#
# This is all roughly based on the Makefile system used by the Linux
# kernel, but is a non-recursive make -- we put the entire dependency
# graph in front of make and let it figure it out.
#
# The code below generates a separate .mk file for each target, but
# all are sourced by the top-level Makefile.  This means that all
# variables in .mk-files clobber one another.  Be careful to use :=
# where appropriate for immediate evaluation, and similarly to watch
# that you're not relying on a variable value to last beween different
# .mk files.
#

# TODO
# Global settings and utility functions are currently stuffed in the toplevel Makefile.
# It may make sense to generate some .mk files on the side to keep the the files readable.

from __future__ import print_function

import os
import re
import subprocess
import gyp
import gyp.common
import gyp.xcode_emulation
from gyp.common import GetEnvironFallback

generator_default_variables = {
  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  'STATIC_LIB_PREFIX': 'lib',
  'SHARED_LIB_PREFIX': 'lib',
  'STATIC_LIB_SUFFIX': '.a',
  'INTERMEDIATE_DIR': '$(obj).$(TOOLSET)/$(TARGET)/geni',
  'SHARED_INTERMEDIATE_DIR': '$(obj)/gen',
  'PRODUCT_DIR': '$(builddir)',
  'RULE_INPUT_ROOT': '%(INPUT_ROOT)s',  # This gets expanded by Python.
  'RULE_INPUT_DIRNAME': '%(INPUT_DIRNAME)s',  # This gets expanded by Python.
  'RULE_INPUT_PATH': '$(abspath $<)',
  'RULE_INPUT_EXT': '$(suffix $<)',
  'RULE_INPUT_NAME': '$(notdir $<)',
  'CONFIGURATION_NAME': '$(BUILDTYPE)',
}

# Make supports multiple toolsets
generator_supports_multiple_toolsets = True

# Request sorted dependencies in the order from dependents to dependencies.
generator_wants_sorted_dependencies = False

# Placates pylint.
generator_additional_non_configuration_keys = []
generator_additional_path_sections = []
generator_extra_sources_for_rules = []
generator_filelist_paths = None

LINK_COMMANDS_LINUX = """\
quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) crs $@ $(filter %.o,$^)

quiet_cmd_alink_thin = AR($(TOOLSET)) $@
cmd_alink_thin = rm -f $@ && $(AR.$(TOOLSET)) crsT $@ $(filter %.o,$^)

# Due to circular dependencies between libraries :(, we wrap the
# special "figure out circular dependencies" flags around the entire
# input list during linking.
quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ -Wl,--start-group $(LD_INPUTS) -Wl,--end-group $(LIBS)

# We support two kinds of shared objects (.so):
# 1) shared_library, which is just bundling together many dependent libraries
# into a link line.
# 2) loadable_module, which is generating a module intended for dlopen().
#
# They differ only slightly:
# In the former case, we want to package all dependent code into the .so.
# In the latter case, we want to package just the API exposed by the
# outermost module.
# This means shared_library uses --whole-archive, while loadable_module doesn't.
# (Note that --whole-archive is incompatible with the --start-group used in
# normal linking.)

# Other shared-object link notes:
# - Set SONAME to the library filename so our binaries don't reference
# the local, absolute paths used on the link command-line.
quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--whole-archive $(LD_INPUTS) -Wl,--no-whole-archive $(LIBS)

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--start-group $(filter-out FORCE_DO_CMD, $^) -Wl,--end-group $(LIBS)
"""

LINK_COMMANDS_MAC = """\
quiet_cmd_alink = LIBTOOL-STATIC $@
cmd_alink = rm -f $@ && ./gyp-mac-tool filter-libtool libtool $(GYP_LIBTOOLFLAGS) -static -o $@ $(filter %.o,$^)

quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o "$@" $(LD_INPUTS) $(LIBS)

quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o "$@" $(LD_INPUTS) $(LIBS)

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -bundle $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(filter-out FORCE_DO_CMD, $^) $(LIBS)
"""

LINK_COMMANDS_ANDROID = """\
quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) crs $@ $(filter %.o,$^)

quiet_cmd_alink_thin = AR($(TOOLSET)) $@
cmd_alink_thin = rm -f $@ && $(AR.$(TOOLSET)) crsT $@ $(filter %.o,$^)

# Due to circular dependencies between libraries :(, we wrap the
# special "figure out circular dependencies" flags around the entire
# input list during linking.
quiet_cmd_link = LINK($(TOOLSET)) $@
quiet_cmd_link_host = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ -Wl,--start-group $(LD_INPUTS) -Wl,--end-group $(LIBS)
cmd_link_host = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(LD_INPUTS) $(LIBS)

# Other shared-object link notes:
# - Set SONAME to the library filename so our binaries don't reference
# the local, absolute paths used on the link command-line.
quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--whole-archive $(LD_INPUTS) -Wl,--no-whole-archive $(LIBS)

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--start-group $(filter-out FORCE_DO_CMD, $^) -Wl,--end-group $(LIBS)
quiet_cmd_solink_module_host = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module_host = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ $(filter-out FORCE_DO_CMD, $^) $(LIBS)
"""

LINK_COMMANDS_AIX = """\
quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) -X32_64 crs $@ $(filter %.o,$^)

quiet_cmd_alink_thin = AR($(TOOLSET)) $@
cmd_alink_thin = rm -f $@ && $(AR.$(TOOLSET)) -X32_64 crs $@ $(filter %.o,$^)

quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(LD_INPUTS) $(LIBS)

quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(LD_INPUTS) $(LIBS)

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(filter-out FORCE_DO_CMD, $^) $(LIBS)
"""

LINK_COMMANDS_OS390 = """\
quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) crs $@ $(filter %.o,$^)

quiet_cmd_alink_thin = AR($(TOOLSET)) $@
cmd_alink_thin = rm -f $@ && $(AR.$(TOOLSET)) crsT $@ $(filter %.o,$^)

quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(LD_INPUTS) $(LIBS)

quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(LD_INPUTS) $(LIBS) -Wl,DLL

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(filter-out FORCE_DO_CMD, $^) $(LIBS) -Wl,DLL
"""

SHARED_HEADER_MAC_COMMANDS = """
quiet_cmd_objc = CXX($(TOOLSET)) $@
cmd_objc = $(CC.$(TOOLSET)) $(GYP_OBJCFLAGS) $(DEPFLAGS) -c -o $@ $<

quiet_cmd_objcxx = CXX($(TOOLSET)) $@
cmd_objcxx = $(CXX.$(TOOLSET)) $(GYP_OBJCXXFLAGS) $(DEPFLAGS) -c -o $@ $<

# Commands for precompiled header files.
quiet_cmd_pch_c = CXX($(TOOLSET)) $@
cmd_pch_c = $(CC.$(TOOLSET)) $(GYP_PCH_CFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<
quiet_cmd_pch_cc = CXX($(TOOLSET)) $@
cmd_pch_cc = $(CC.$(TOOLSET)) $(GYP_PCH_CXXFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<
quiet_cmd_pch_m = CXX($(TOOLSET)) $@
cmd_pch_m = $(CC.$(TOOLSET)) $(GYP_PCH_OBJCFLAGS) $(DEPFLAGS) -c -o $@ $<
quiet_cmd_pch_mm = CXX($(TOOLSET)) $@
cmd_pch_mm = $(CC.$(TOOLSET)) $(GYP_PCH_OBJCXXFLAGS) $(DEPFLAGS) -c -o $@ $<

# gyp-mac-tool is written next to the root Makefile by gyp.
# Use $(4) for the command, since $(2) and $(3) are used as flag by do_cmd
# already.
quiet_cmd_mac_tool = MACTOOL $(4) $<
cmd_mac_tool = ./gyp-mac-tool $(4) $< "$@"

quiet_cmd_mac_package_framework = PACKAGE FRAMEWORK $@
cmd_mac_package_framework = ./gyp-mac-tool package-framework "$@" $(4)

quiet_cmd_infoplist = INFOPLIST $@
cmd_infoplist = $(CC.$(TOOLSET)) -E -P -Wno-trigraphs -x c $(INFOPLIST_DEFINES) "$<" -o "$@"
"""

SHARED_FOOTER = """\
# "all" is a concatenation of the "all" targets from all the included
# sub-makefiles. This is just here to clarify.
all:

# Add in dependency-tracking rules.  $(all_deps) is the list of every single
# target in our tree. Only consider the ones with .d (dependency) info:
d_files := $(wildcard $(foreach f,$(all_deps),$(depsdir)/$(f).d))
ifneq ($(d_files),)
  include $(d_files)
endif
"""


def WriteAutoRegenerationRule(params, root_makefile, makefile_name, build_files, Sourceify):
  """Write the target to regenerate the Makefile."""
  options = params['options']
  build_files_args = [gyp.common.RelativePath(filename, options.toplevel_dir) for filename in params['build_files_arg']]

  gyp_binary = gyp.common.FixIfRelativePath(params['gyp_binary'], options.toplevel_dir)
  if not gyp_binary.startswith(os.sep):
    gyp_binary = os.path.join('.', gyp_binary)

  root_makefile.write(
    "quiet_cmd_regen_makefile = ACTION Regenerating $@\n"
    "cmd_regen_makefile = cd $(srcdir); %(cmd)s\n"
    "%(makefile_name)s: %(deps)s\n"
    "\t$(call do_cmd,regen_makefile)\n\n" % {
      'makefile_name': makefile_name,
      'deps': ' '.join(map(Sourceify, build_files)),
      'cmd': gyp.common.EncodePOSIXShellList([gyp_binary, '-fmake'] + gyp.RegenerateFlags(options) + build_files_args)
    }
  )


def PerformBuild(_, configurations, params):
  options = params['options']
  for config in configurations:
    arguments = ['make']
    if options.toplevel_dir and options.toplevel_dir != '.':
      arguments += '-C', options.toplevel_dir
    arguments.append('BUILDTYPE=' + config)
    print('Building [%s]: %s' % (config, arguments))
    subprocess.check_call(arguments)


def GenerateOutput(target_list, target_dicts, data, params):
  from gyp.MakefileWriter import MakefileWriter, Sourceify, WriteRootHeaderSuffixRules, SHARED_HEADER
  options = params['options']
  flavor = gyp.common.GetFlavor(params)
  generator_flags = params.get('generator_flags', {})
  builddir_name = generator_flags.get('output_dir', 'out')
  android_ndk_version = generator_flags.get('android_ndk_version', None)
  default_target = generator_flags.get('default_target', 'all')

  def CalculateMakefilePath(build_file_arg, base_name):
    """Determine where to write a Makefile for a given gyp file."""
    # Paths in gyp files are relative to the .gyp file, but we want
    # paths relative to the source root for the master makefile.  Grab
    # the path of the .gyp file as the base to relativize against.
    # E.g. "foo/bar" when we're constructing targets for "foo/bar/baz.gyp".
    base_makefile_path = gyp.common.RelativePath(os.path.dirname(build_file_arg), options.depth)
    # We write the file in the base_makefile_path directory.
    output_makefile = os.path.join(options.depth, base_makefile_path, base_name)
    if options.generator_output:
      output_makefile = os.path.join(options.depth, options.generator_output, base_makefile_path, base_name)
    base_makefile_path = gyp.common.RelativePath(os.path.dirname(build_file_arg), options.toplevel_dir)
    return base_makefile_path, output_makefile

  # TODO:  search for the first non-'Default' target.  This can go
  # away when we add verification that all targets have the
  # necessary configurations.
  default_configuration = None
  toolsets = set([target_dicts[target]['toolset'] for target in target_list])
  for target in target_list:
    spec = target_dicts[target]
    if spec['default_configuration'] != 'Default':
      default_configuration = spec['default_configuration']
      break
  if not default_configuration:
    default_configuration = 'Default'

  srcdir = '.'
  makefile_name = 'Makefile' + options.suffix
  makefile_path = os.path.join(options.toplevel_dir, makefile_name)
  if options.generator_output:
    makefile_path = os.path.join(options.toplevel_dir, options.generator_output, makefile_name)
    srcdir = gyp.common.RelativePath(srcdir, options.generator_output)
    Sourceify.srcdir_prefix = '$(srcdir)/'

  flock_command = 'flock'
  copy_archive_arguments = '-af'
  makedep_arguments = '-MMD'
  header_params = {
    'default_target': default_target,
    'builddir': builddir_name,
    'default_configuration': default_configuration,
    'flock': flock_command,
    'flock_index': 1,
    'link_commands': LINK_COMMANDS_LINUX,
    'extra_commands': '',
    'srcdir': srcdir,
    'copy_archive_args': copy_archive_arguments,
    'makedep_args': makedep_arguments,
  }
  if flavor == 'mac':
    flock_command = './gyp-mac-tool flock'
    header_params.update({
      'flock': flock_command,
      'flock_index': 2,
      'link_commands': LINK_COMMANDS_MAC,
      'extra_commands': SHARED_HEADER_MAC_COMMANDS,
    })
  elif flavor == 'android':
    header_params.update({
      'link_commands': LINK_COMMANDS_ANDROID,
    })
  elif flavor == 'zos':
    copy_archive_arguments = '-fPR'
    makedep_arguments = '-qmakedep=gcc'
    header_params.update({
      'copy_archive_args': copy_archive_arguments,
      'makedep_args': makedep_arguments,
      'link_commands': LINK_COMMANDS_OS390,
    })
  elif flavor == 'solaris':
    header_params.update({
      'flock': './gyp-flock-tool flock',
      'flock_index': 2,
    })
  elif flavor == 'freebsd':
    # Note: OpenBSD has sysutils/flock. lockf seems to be FreeBSD specific.
    header_params.update({
      'flock': 'lockf',
    })
  elif flavor == 'openbsd':
    copy_archive_arguments = '-pPRf'
    header_params.update({
      'copy_archive_args': copy_archive_arguments,
    })
  elif flavor == 'aix':
    copy_archive_arguments = '-pPRf'
    header_params.update({
      'copy_archive_args': copy_archive_arguments,
      'link_commands': LINK_COMMANDS_AIX,
      'flock': './gyp-flock-tool flock',
      'flock_index': 2,
    })

  header_params.update({
    'CC.target': GetEnvironFallback(('CC_target', 'CC'), '$(CC)'),
    'AR.target': GetEnvironFallback(('AR_target', 'AR'), '$(AR)'),
    'CXX.target': GetEnvironFallback(('CXX_target', 'CXX'), '$(CXX)'),
    'LINK.target': GetEnvironFallback(('LINK_target', 'LINK'), '$(LINK)'),
    'CC.host':     GetEnvironFallback(('CC_host',), 'cc'),
    'AR.host':     GetEnvironFallback(('AR_host',), 'ar'),
    'CXX.host':    GetEnvironFallback(('CXX_host',), 'c++'),
    'LINK.host':   GetEnvironFallback(('LINK_host',), '$(CXX.host)'),
  })

  build_file, _, _ = gyp.common.ParseQualifiedTarget(target_list[0])
  make_global_settings_array = data[build_file].get('make_global_settings', [])
  wrappers = {}
  for key, value in make_global_settings_array:
    if key.endswith('_wrapper'):
      wrappers[key[:-len('_wrapper')]] = '$(abspath %s)' % value
  make_global_settings = ''
  for key, value in make_global_settings_array:
    if re.match('.*_wrapper', key):
      continue
    if value[0] != '$':
      value = '$(abspath %s)' % value
    wrapper = wrappers.get(key)
    if wrapper:
      value = '%s %s' % (wrapper, value)
      del wrappers[key]
    if key in ('CC', 'CC.host', 'CXX', 'CXX.host'):
      make_global_settings += 'ifneq (,$(filter $(origin %s), undefined default))\n' % key
      # Let gyp-time envvars win over global settings.
      env_key = key.replace('.', '_')  # CC.host -> CC_host
      if env_key in os.environ:
        value = os.environ[env_key]
      make_global_settings += '  %s = %s\n' % (key, value)
      make_global_settings += 'endif\n'
    else:
      make_global_settings += '%s ?= %s\n' % (key, value)
  # TODO(ukai): define cmd when only wrapper is specified in
  # make_global_settings.

  header_params['make_global_settings'] = make_global_settings

  gyp.common.EnsureDirExists(makefile_path)
  root_makefile = open(makefile_path, 'w')
  root_makefile.write(SHARED_HEADER % header_params)
  # Currently any versions have the same effect, but in future the behavior
  # could be different.
  if android_ndk_version:
    root_makefile.write(
      '# Define LOCAL_PATH for build of Android applications.\n'
      'LOCAL_PATH := $(call my-dir)\n'
      '\n'
    )
  for toolset in toolsets:
    root_makefile.write('TOOLSET := %s\n' % toolset)
    WriteRootHeaderSuffixRules(root_makefile)

  # Put build-time support tools next to the root Makefile.
  dest_path = os.path.dirname(makefile_path)
  gyp.common.CopyTool(flavor, dest_path)

  # Find the list of targets that derive from the gyp file(s) being built.
  needed_targets = set()
  for build_file in params['build_files']:
    for target in gyp.common.AllTargets(target_list, target_dicts, build_file):
      needed_targets.add(target)

  build_files = set()
  include_list = set()
  writer = None
  for qualified_target in target_list:
    build_file, target, toolset = gyp.common.ParseQualifiedTarget(qualified_target)

    this_make_global_settings = data[build_file].get('make_global_settings', [])
    assert make_global_settings_array == this_make_global_settings, (
        "make_global_settings needs to be the same for all targets. %s vs. %s" %
        (this_make_global_settings, make_global_settings))

    build_files.add(gyp.common.RelativePath(build_file, options.toplevel_dir))
    included_files = data[build_file]['included_files']
    for included_file in included_files:
      # The included_files entries are relative to the dir of the build file
      # that included them, so we have to undo that and then make them relative
      # to the root dir.
      relative_include_file = gyp.common.RelativePath(
        gyp.common.UnrelativePath(included_file, build_file),
        options.toplevel_dir
      )
      abs_include_file = os.path.abspath(relative_include_file)
      # If the include file is from the ~/.gyp dir, we should use absolute path
      # so that relocating the src dir doesn't break the path.
      if params['home_dot_gyp'] and abs_include_file.startswith(params['home_dot_gyp']):
        build_files.add(abs_include_file)
      else:
        build_files.add(relative_include_file)

    base_path, output_file = CalculateMakefilePath(build_file, target + '.' + toolset + options.suffix + '.mk')

    spec = target_dicts[qualified_target]
    configs = spec['configurations']

    if flavor == 'mac':
      gyp.xcode_emulation.MergeGlobalXcodeSettingsToSpec(data[build_file], spec)

    writer = MakefileWriter(generator_flags, flavor)
    writer.Write(qualified_target, base_path, output_file, spec, configs, part_of_all=qualified_target in needed_targets)

    # Our root_makefile lives at the source root.  Compute the relative path
    # from there to the output_file for including.
    mkfile_rel_path = gyp.common.RelativePath(output_file, os.path.dirname(makefile_path))
    include_list.add(mkfile_rel_path)

  assert writer
  # Write out per-gyp (sub-project) Makefiles.
  depth_rel_path = gyp.common.RelativePath(options.depth, os.getcwd())
  for build_file in build_files:
    # The paths in build_files were relativized above, so undo that before
    # testing against the non-relativized items in target_list and before
    # calculating the Makefile path.
    build_file_path = os.path.join(depth_rel_path, build_file)
    related_gyp_targets = [t for t in target_list if t.startswith(build_file) and t in needed_targets]
    # Only generate Makefiles for gyp files with targets.
    if not related_gyp_targets:
      continue
    build_file_name = "%s.Makefile" % os.path.splitext(os.path.basename(build_file))[0]
    _, submake_output_file = CalculateMakefilePath(build_file_path, build_file_name)
    makefile_rel_path = gyp.common.RelativePath(os.path.dirname(makefile_path), os.path.dirname(submake_output_file))
    gyp_targets_names = [target_dicts[t]['target_name'] for t in related_gyp_targets]
    writer.WriteSubMake(submake_output_file, makefile_rel_path, gyp_targets_names, builddir_name)

  # Write out the sorted list of includes.
  root_makefile.write('\n')
  for include_file in sorted(include_list):
    # We wrap each .mk include in an if statement so users can tell make to
    # not load a file by setting NO_LOAD.  The below make code says, only
    # load the .mk file if the .mk filename doesn't start with a token in
    # NO_LOAD.
    include_conditional_tmpl="""\
ifeq ($(strip $(foreach prefix,$(NO_LOAD), $(findstring $(join ^,$(prefix)), $(join ^,%(include_file)s)))),)
  include %(include_file)s
endif
"""
    root_makefile.write(include_conditional_tmpl % { 'include_file': include_file })
  root_makefile.write('\n')

  if (not generator_flags.get('standalone')
      and generator_flags.get('auto_regeneration', True)):
    WriteAutoRegenerationRule(params, root_makefile, makefile_name, build_files, Sourceify)

  root_makefile.write(SHARED_FOOTER)

  root_makefile.close()


def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  flavor = gyp.common.GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.dylib')
    default_variables.setdefault('SHARED_LIB_DIR', generator_default_variables['PRODUCT_DIR'])
    default_variables.setdefault('LIB_DIR', generator_default_variables['PRODUCT_DIR'])

    # Copy additional generator configuration data from Xcode, which is shared
    # by the Mac Make generator.
    import gyp.generator.xcode as xcode_generator
    global generator_additional_non_configuration_keys
    generator_additional_non_configuration_keys = getattr(xcode_generator, 'generator_additional_non_configuration_keys', [])
    global generator_additional_path_sections
    generator_additional_path_sections = getattr(xcode_generator, 'generator_additional_path_sections', [])
    global generator_extra_sources_for_rules
    generator_extra_sources_for_rules = getattr(xcode_generator, 'generator_extra_sources_for_rules', [])
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)
    if flavor == 'aix':
      default_variables.setdefault('SHARED_LIB_SUFFIX', '.a')
    else:
      default_variables.setdefault('SHARED_LIB_SUFFIX', '.so')
    default_variables.setdefault('SHARED_LIB_DIR', '$(builddir)/lib.$(TOOLSET)')
    default_variables.setdefault('LIB_DIR', '$(obj).$(TOOLSET)')


def CalculateGeneratorInputInfo(params):
  """Calculate the generator specific info that gets fed to input (called by
  gyp)."""
  generator_flags = params.get('generator_flags', {})
  android_ndk_version = generator_flags.get('android_ndk_version', None)
  # Android NDK requires a strict link order.
  if android_ndk_version:
    global generator_wants_sorted_dependencies
    generator_wants_sorted_dependencies = True

  output_dir = params['options'].generator_output or \
               params['options'].toplevel_dir
  builddir_name = generator_flags.get('output_dir', 'out')
  qualified_out_dir = os.path.normpath(os.path.join(
    output_dir, builddir_name, 'gypfiles'))

  global generator_filelist_paths
  generator_filelist_paths = {
    'toplevel': params['options'].toplevel_dir,
    'qualified_out_dir': qualified_out_dir,
  }
