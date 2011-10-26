#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
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
# TODOs:
#
# Global settings and utility functions are currently stuffed in the
# toplevel Makefile.  It may make sense to generate some .mk files on
# the side to keep the the files readable.

import gyp
import gyp.common
import gyp.system_test
import os.path
import os
import sys

# Debugging-related imports -- remove me once we're solid.
import code
import pprint

generator_default_variables = {
  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  'STATIC_LIB_PREFIX': 'lib',
  'SHARED_LIB_PREFIX': 'lib',
  'STATIC_LIB_SUFFIX': '.a',
  'INTERMEDIATE_DIR': '$(obj).$(TOOLSET)/geni',
  'SHARED_INTERMEDIATE_DIR': '$(obj)/gen',
  'PRODUCT_DIR': '$(builddir)',
  'RULE_INPUT_ROOT': '%(INPUT_ROOT)s',  # This gets expanded by Python.
  'RULE_INPUT_DIRNAME': '%(INPUT_DIRNAME)s',  # This gets expanded by Python.
  'RULE_INPUT_PATH': '$(abspath $<)',
  'RULE_INPUT_EXT': '$(suffix $<)',
  'RULE_INPUT_NAME': '$(notdir $<)',

  # This appears unused --- ?
  'CONFIGURATION_NAME': '$(BUILDTYPE)',
}

# Make supports multiple toolsets
generator_supports_multiple_toolsets = True

# Request sorted dependencies in the order from dependents to dependencies.
generator_wants_sorted_dependencies = False


def GetFlavor(params):
  """Returns |params.flavor| if it's set, the system's default flavor else."""
  flavors = {
    'darwin': 'mac',
    'sunos5': 'solaris',
    'freebsd7': 'freebsd',
    'freebsd8': 'freebsd',
  }
  flavor = flavors.get(sys.platform, 'linux')
  return params.get('flavor', flavor)


def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  cc_target = os.environ.get('CC.target', os.environ.get('CC', 'cc'))
  default_variables['LINKER_SUPPORTS_ICF'] = \
      gyp.system_test.TestLinkerSupportsICF(cc_command=cc_target)

  flavor = GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.dylib')
    default_variables.setdefault('SHARED_LIB_DIR',
                                 generator_default_variables['PRODUCT_DIR'])
    default_variables.setdefault('LIB_DIR',
                                 generator_default_variables['PRODUCT_DIR'])

    # Copy additional generator configuration data from Xcode, which is shared
    # by the Mac Make generator.
    import gyp.generator.xcode as xcode_generator
    global generator_additional_non_configuration_keys
    generator_additional_non_configuration_keys = getattr(xcode_generator,
        'generator_additional_non_configuration_keys', [])
    global generator_additional_path_sections
    generator_additional_path_sections = getattr(xcode_generator,
        'generator_additional_path_sections', [])
    global generator_extra_sources_for_rules
    generator_extra_sources_for_rules = getattr(xcode_generator,
        'generator_extra_sources_for_rules', [])
    global COMPILABLE_EXTENSIONS
    COMPILABLE_EXTENSIONS.update({'.m': 'objc', '.mm' : 'objcxx'})
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)
    default_variables.setdefault('SHARED_LIB_SUFFIX', '.so')
    default_variables.setdefault('SHARED_LIB_DIR','$(builddir)/lib.$(TOOLSET)')
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


def ensure_directory_exists(path):
  dir = os.path.dirname(path)
  if dir and not os.path.exists(dir):
    os.makedirs(dir)


# The .d checking code below uses these functions:
# wildcard, sort, foreach, shell, wordlist
# wildcard can handle spaces, the rest can't.
# Since I could find no way to make foreach work with spaces in filenames
# correctly, the .d files have spaces replaced with another character. The .d
# file for
#     Chromium\ Framework.framework/foo
# is for example
#     out/Release/.deps/out/Release/Chromium?Framework.framework/foo
# This is the replacement character.
SPACE_REPLACEMENT = '?'


LINK_COMMANDS_LINUX = """\
quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) $(ARFLAGS.$(TOOLSET)) $@ $(filter %.o,$^)

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
cmd_alink = rm -f $@ && libtool -static -o $@ $(filter %.o,$^)

quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o "$@" $(LD_INPUTS) $(LIBS)

# TODO(thakis): Find out and document the difference between shared_library and
# loadable_module on mac.
quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o "$@" $(LD_INPUTS) $(LIBS)

# TODO(thakis): The solink_module rule is likely wrong. Xcode seems to pass
# -bundle -single_module here (for osmesa.so).
quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ $(filter-out FORCE_DO_CMD, $^) $(LIBS)
"""

LINK_COMMANDS_ANDROID = """\
quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) $(ARFLAGS.$(TOOLSET)) $@ $(filter %.o,$^)

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


# Header of toplevel Makefile.
# This should go into the build tree, but it's easier to keep it here for now.
SHARED_HEADER = ("""\
# We borrow heavily from the kernel build setup, though we are simpler since
# we don't have Kconfig tweaking settings on us.

# The implicit make rules have it looking for RCS files, among other things.
# We instead explicitly write all the rules we care about.
# It's even quicker (saves ~200ms) to pass -r on the command line.
MAKEFLAGS=-r

# The source directory tree.
srcdir := %(srcdir)s

# The name of the builddir.
builddir_name ?= %(builddir)s

# The V=1 flag on command line makes us verbosely print command lines.
ifdef V
  quiet=
else
  quiet=quiet_
endif

# Specify BUILDTYPE=Release on the command line for a release build.
BUILDTYPE ?= %(default_configuration)s

# Directory all our build output goes into.
# Note that this must be two directories beneath src/ for unit tests to pass,
# as they reach into the src/ directory for data with relative paths.
builddir ?= $(builddir_name)/$(BUILDTYPE)
abs_builddir := $(abspath $(builddir))
depsdir := $(builddir)/.deps

# Object output directory.
obj := $(builddir)/obj
abs_obj := $(abspath $(obj))

# We build up a list of every single one of the targets so we can slurp in the
# generated dependency rule Makefiles in one pass.
all_deps :=

%(make_global_settings)s

# C++ apps need to be linked with g++.
#
# Note: flock is used to seralize linking. Linking is a memory-intensive
# process so running parallel links can often lead to thrashing.  To disable
# the serialization, override LINK via an envrionment variable as follows:
#
#   export LINK=g++
#
# This will allow make to invoke N linker processes as specified in -jN.
LINK ?= %(flock)s $(builddir)/linker.lock $(CXX)

CC.target ?= $(CC)
CFLAGS.target ?= $(CFLAGS)
CXX.target ?= $(CXX)
CXXFLAGS.target ?= $(CXXFLAGS)
LINK.target ?= $(LINK)
LDFLAGS.target ?= $(LDFLAGS) %(LINK_flags)s
AR.target ?= $(AR)
ARFLAGS.target ?= %(ARFLAGS.target)s

# N.B.: the logic of which commands to run should match the computation done
# in gyp's make.py where ARFLAGS.host etc. is computed.
# TODO(evan): move all cross-compilation logic to gyp-time so we don't need
# to replicate this environment fallback in make as well.
CC.host ?= gcc
CFLAGS.host ?=
CXX.host ?= g++
CXXFLAGS.host ?=
LINK.host ?= g++
LDFLAGS.host ?=
AR.host ?= ar
ARFLAGS.host := %(ARFLAGS.host)s

# Define a dir function that can handle spaces.
# http://www.gnu.org/software/make/manual/make.html#Syntax-of-Functions
# "leading spaces cannot appear in the text of the first argument as written.
# These characters can be put into the argument value by variable substitution."
empty :=
space := $(empty) $(empty)

# http://stackoverflow.com/questions/1189781/using-make-dir-or-notdir-on-a-path-with-spaces
replace_spaces = $(subst $(space),""" + SPACE_REPLACEMENT + """,$1)
unreplace_spaces = $(subst """ + SPACE_REPLACEMENT + """,$(space),$1)
dirx = $(call unreplace_spaces,$(dir $(call replace_spaces,$1)))

# Flags to make gcc output dependency info.  Note that you need to be
# careful here to use the flags that ccache and distcc can understand.
# We write to a dep file on the side first and then rename at the end
# so we can't end up with a broken dep file.
depfile = $(depsdir)/$(call replace_spaces,$@).d
DEPFLAGS = -MMD -MF $(depfile).raw

# We have to fixup the deps output in a few ways.
# (1) the file output should mention the proper .o file.
# ccache or distcc lose the path to the target, so we convert a rule of
# the form:
#   foobar.o: DEP1 DEP2
# into
#   path/to/foobar.o: DEP1 DEP2
# (2) we want missing files not to cause us to fail to build.
# We want to rewrite
#   foobar.o: DEP1 DEP2 \\
#               DEP3
# to
#   DEP1:
#   DEP2:
#   DEP3:
# so if the files are missing, they're just considered phony rules.
# We have to do some pretty insane escaping to get those backslashes
# and dollar signs past make, the shell, and sed at the same time.
# Doesn't work with spaces, but that's fine: .d files have spaces in
# their names replaced with other characters."""
r"""
define fixup_dep
# The depfile may not exist if the input file didn't have any #includes.
touch $(depfile).raw
# Fixup path as in (1).
sed -e "s|^$(notdir $@)|$@|" $(depfile).raw >> $(depfile)
# Add extra rules as in (2).
# We remove slashes and replace spaces with new lines;
# remove blank lines;
# delete the first line and append a colon to the remaining lines.
sed -e 's|\\||' -e 'y| |\n|' $(depfile).raw |\
  grep -v '^$$'                             |\
  sed -e 1d -e 's|$$|:|'                     \
    >> $(depfile)
rm $(depfile).raw
endef
"""
"""
# Command definitions:
# - cmd_foo is the actual command to run;
# - quiet_cmd_foo is the brief-output summary of the command.

quiet_cmd_cc = CC($(TOOLSET)) $@
cmd_cc = $(CC.$(TOOLSET)) $(GYP_CFLAGS) $(DEPFLAGS) $(CFLAGS.$(TOOLSET)) -c -o $@ $<

quiet_cmd_cxx = CXX($(TOOLSET)) $@
cmd_cxx = $(CXX.$(TOOLSET)) $(GYP_CXXFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<
%(extra_commands)s
quiet_cmd_touch = TOUCH $@
cmd_touch = touch $@

quiet_cmd_copy = COPY $@
# send stderr to /dev/null to ignore messages when linking directories.
cmd_copy = ln -f "$<" "$@" 2>/dev/null || (rm -rf "$@" && cp -af "$<" "$@")

%(link_commands)s
"""

r"""
# Define an escape_quotes function to escape single quotes.
# This allows us to handle quotes properly as long as we always use
# use single quotes and escape_quotes.
escape_quotes = $(subst ','\'',$(1))
# This comment is here just to include a ' to unconfuse syntax highlighting.
# Define an escape_vars function to escape '$' variable syntax.
# This allows us to read/write command lines with shell variables (e.g.
# $LD_LIBRARY_PATH), without triggering make substitution.
escape_vars = $(subst $$,$$$$,$(1))
# Helper that expands to a shell command to echo a string exactly as it is in
# make. This uses printf instead of echo because printf's behaviour with respect
# to escape sequences is more portable than echo's across different shells
# (e.g., dash, bash).
exact_echo = printf '%%s\n' '$(call escape_quotes,$(1))'
"""
"""
# Helper to compare the command we're about to run against the command
# we logged the last time we ran the command.  Produces an empty
# string (false) when the commands match.
# Tricky point: Make has no string-equality test function.
# The kernel uses the following, but it seems like it would have false
# positives, where one string reordered its arguments.
#   arg_check = $(strip $(filter-out $(cmd_$(1)), $(cmd_$@)) \\
#                       $(filter-out $(cmd_$@), $(cmd_$(1))))
# We instead substitute each for the empty string into the other, and
# say they're equal if both substitutions produce the empty string.
# .d files contain """ + SPACE_REPLACEMENT + \
                   """ instead of spaces, take that into account.
command_changed = $(or $(subst $(cmd_$(1)),,$(cmd_$(call replace_spaces,$@))),\\
                       $(subst $(cmd_$(call replace_spaces,$@)),,$(cmd_$(1))))

# Helper that is non-empty when a prerequisite changes.
# Normally make does this implicitly, but we force rules to always run
# so we can check their command lines.
#   $? -- new prerequisites
#   $| -- order-only dependencies
prereq_changed = $(filter-out FORCE_DO_CMD,$(filter-out $|,$?))

# Helper that executes all postbuilds, and deletes the output file when done
# if any of the postbuilds failed.
define do_postbuilds
  @E=0;\\
  for p in $(POSTBUILDS); do\\
    eval $$p;\\
    F=$$?;\\
    if [ $$F -ne 0 ]; then\\
      E=$$F;\\
    fi;\\
  done;\\
  if [ $$E -ne 0 ]; then\\
    rm -rf "$@";\\
    exit $$E;\\
  fi
endef

# do_cmd: run a command via the above cmd_foo names, if necessary.
# Should always run for a given target to handle command-line changes.
# Second argument, if non-zero, makes it do asm/C/C++ dependency munging.
# Third argument, if non-zero, makes it do POSTBUILDS processing.
# Note: We intentionally do NOT call dirx for depfile, since it contains """ + \
                                                     SPACE_REPLACEMENT + """ for
# spaces already and dirx strips the """ + SPACE_REPLACEMENT + \
                                     """ characters.
define do_cmd
$(if $(or $(command_changed),$(prereq_changed)),
  @$(call exact_echo,  $($(quiet)cmd_$(1)))
  @mkdir -p "$(call dirx,$@)" "$(dir $(depfile))"
  $(if $(findstring flock,$(word %(flock_index)d,$(cmd_$1))),
    @$(cmd_$(1))
    @echo "  $(quiet_cmd_$(1)): Finished",
    @$(cmd_$(1))
  )
  @$(call exact_echo,$(call escape_vars,cmd_$(call replace_spaces,$@) := $(cmd_$(1)))) > $(depfile)
  @$(if $(2),$(fixup_dep))
  $(if $(and $(3), $(POSTBUILDS)),
    $(call do_postbuilds)
  )
)
endef

# Declare "all" target first so it is the default, even though we don't have the
# deps yet.
.PHONY: all
all:

# Use FORCE_DO_CMD to force a target to run.  Should be coupled with
# do_cmd.
.PHONY: FORCE_DO_CMD
FORCE_DO_CMD:

""")

SHARED_HEADER_MAC_COMMANDS = """
quiet_cmd_objc = CXX($(TOOLSET)) $@
cmd_objc = $(CC.$(TOOLSET)) $(GYP_OBJCFLAGS) $(DEPFLAGS) -c -o $@ $<

quiet_cmd_objcxx = CXX($(TOOLSET)) $@
cmd_objcxx = $(CXX.$(TOOLSET)) $(GYP_OBJCXXFLAGS) $(DEPFLAGS) -c -o $@ $<

# Commands for precompiled header files.
quiet_cmd_pch_c = CXX($(TOOLSET)) $@
cmd_pch_c = $(CC.$(TOOLSET)) $(GYP_PCH_CFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<
quiet_cmd_pch_cc = CXX($(TOOLSET)) $@
cmd_pch_cc = $(CC.$(TOOLSET)) $(GYP_PCH_CCFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<
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
"""

SHARED_HEADER_SUN_COMMANDS = """
# gyp-sun-tool is written next to the root Makefile by gyp.
# Use $(4) for the command, since $(2) and $(3) are used as flag by do_cmd
# already.
quiet_cmd_sun_tool = SUNTOOL $(4) $<
cmd_sun_tool = ./gyp-sun-tool $(4) $< "$@"
"""


def WriteRootHeaderSuffixRules(writer):
  extensions = sorted(COMPILABLE_EXTENSIONS.keys(), key=str.lower)

  writer.write('# Suffix rules, putting all outputs into $(obj).\n')
  for ext in extensions:
    writer.write('$(obj).$(TOOLSET)/%%.o: $(srcdir)/%%%s FORCE_DO_CMD\n' % ext)
    writer.write('\t@$(call do_cmd,%s,1)\n' % COMPILABLE_EXTENSIONS[ext])

  writer.write('\n# Try building from generated source, too.\n')
  for ext in extensions:
    writer.write(
        '$(obj).$(TOOLSET)/%%.o: $(obj).$(TOOLSET)/%%%s FORCE_DO_CMD\n' % ext)
    writer.write('\t@$(call do_cmd,%s,1)\n' % COMPILABLE_EXTENSIONS[ext])
  writer.write('\n')
  for ext in extensions:
    writer.write('$(obj).$(TOOLSET)/%%.o: $(obj)/%%%s FORCE_DO_CMD\n' % ext)
    writer.write('\t@$(call do_cmd,%s,1)\n' % COMPILABLE_EXTENSIONS[ext])
  writer.write('\n')


SHARED_HEADER_SUFFIX_RULES_COMMENT1 = ("""\
# Suffix rules, putting all outputs into $(obj).
""")


SHARED_HEADER_SUFFIX_RULES_COMMENT2 = ("""\
# Try building from generated source, too.
""")


SHARED_FOOTER = """\
# "all" is a concatenation of the "all" targets from all the included
# sub-makefiles. This is just here to clarify.
all:

# Add in dependency-tracking rules.  $(all_deps) is the list of every single
# target in our tree. Only consider the ones with .d (dependency) info:
d_files := $(wildcard $(foreach f,$(all_deps),$(depsdir)/$(f).d))
ifneq ($(d_files),)
  # Rather than include each individual .d file, concatenate them into a
  # single file which make is able to load faster.  We split this into
  # commands that take 1000 files at a time to avoid overflowing the
  # command line.
  $(shell cat $(wordlist 1,1000,$(d_files)) > $(depsdir)/all.deps)
%(generate_all_deps)s
  # make looks for ways to re-generate included makefiles, but in our case, we
  # don't have a direct way. Explicitly telling make that it has nothing to do
  # for them makes it go faster.
  $(depsdir)/all.deps: ;

  include $(depsdir)/all.deps
endif
"""

header = """\
# This file is generated by gyp; do not edit.

"""

# Maps every compilable file extension to the do_cmd that compiles it.
COMPILABLE_EXTENSIONS = {
  '.c': 'cc',
  '.cc': 'cxx',
  '.cpp': 'cxx',
  '.cxx': 'cxx',
  '.s': 'cc',
  '.S': 'cc',
}

def Compilable(filename):
  """Return true if the file is compilable (should be in OBJS)."""
  for res in (filename.endswith(e) for e in COMPILABLE_EXTENSIONS):
    if res:
      return True
  return False


def Linkable(filename):
  """Return true if the file is linkable (should be on the link line)."""
  return filename.endswith('.o')


def Target(filename):
  """Translate a compilable filename to its .o target."""
  return os.path.splitext(filename)[0] + '.o'


def EscapeShellArgument(s):
  """Quotes an argument so that it will be interpreted literally by a POSIX
     shell. Taken from
     http://stackoverflow.com/questions/35817/whats-the-best-way-to-escape-ossystem-calls-in-python
     """
  return "'" + s.replace("'", "'\\''") + "'"


def EscapeMakeVariableExpansion(s):
  """Make has its own variable expansion syntax using $. We must escape it for
     string to be interpreted literally."""
  return s.replace('$', '$$')


def EscapeCppDefine(s):
  """Escapes a CPP define so that it will reach the compiler unaltered."""
  s = EscapeShellArgument(s)
  s = EscapeMakeVariableExpansion(s)
  return s


def QuoteIfNecessary(string):
  """TODO: Should this ideally be replaced with one or more of the above
     functions?"""
  if '"' in string:
    string = '"' + string.replace('"', '\\"') + '"'
  return string


def StringToMakefileVariable(string):
  """Convert a string to a value that is acceptable as a make variable name."""
  # TODO: replace other metacharacters that we encounter.
  return string.replace(' ', '_')


srcdir_prefix = ''
def Sourceify(path):
  """Convert a path to its source directory form."""
  if '$(' in path:
    return path
  if os.path.isabs(path):
    return path
  return srcdir_prefix + path


def QuoteSpaces(s):
  return s.replace(' ', r'\ ')


def ReplaceQuotedSpaces(s):
  return s.replace(r'\ ', SPACE_REPLACEMENT)


# Map from qualified target to path to output.
target_outputs = {}
# Map from qualified target to any linkable output.  A subset
# of target_outputs.  E.g. when mybinary depends on liba, we want to
# include liba in the linker line; when otherbinary depends on
# mybinary, we just want to build mybinary first.
target_link_deps = {}


class XcodeSettings(object):
  """A class that understands the gyp 'xcode_settings' object."""

  def __init__(self, spec):
    self.spec = spec

    # Per-target 'xcode_settings' are pushed down into configs earlier by gyp.
    # This means self.xcode_settings[config] always contains all settings
    # for that config -- the per-target settings as well. Settings that are
    # the same for all configs are implicitly per-target settings.
    self.xcode_settings = {}
    configs = spec['configurations']
    for configname, config in configs.iteritems():
      self.xcode_settings[configname] = config.get('xcode_settings', {})

    # This is only non-None temporarily during the execution of some methods.
    self.configname = None

  def _Settings(self):
    assert self.configname
    return self.xcode_settings[self.configname]

  def _Test(self, test_key, cond_key, default):
    return self._Settings().get(test_key, default) == cond_key

  def _Appendf(self, lst, test_key, format_str, default=None):
    if test_key in self._Settings():
      lst.append(format_str % str(self._Settings()[test_key]))
    elif default:
      lst.append(format_str % str(default))

  def _WarnUnimplemented(self, test_key):
    if test_key in self._Settings():
      print 'Warning: Ignoring not yet implemented key "%s".' % test_key

  def _IsBundle(self):
    return int(self.spec.get('mac_bundle', 0)) != 0

  def GetFrameworkVersion(self):
    """Returns the framework version of the current target. Only valid for
    bundles."""
    assert self._IsBundle()
    return self.GetPerTargetSetting('FRAMEWORK_VERSION', default='A')

  def GetWrapperExtension(self):
    """Returns the bundle extension (.app, .framework, .plugin, etc).  Only
    valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] in ('loadable_module', 'shared_library'):
      default_wrapper_extension = {
        'loadable_module': 'bundle',
        'shared_library': 'framework',
      }[self.spec['type']]
      wrapper_extension = self.GetPerTargetSetting(
          'WRAPPER_EXTENSION', default=default_wrapper_extension)
      return '.' + self.spec.get('product_extension', wrapper_extension)
    elif self.spec['type'] == 'executable':
      return '.app'
    else:
      assert False, "Don't know extension for '%s', target '%s'" % (
          self.spec['type'], self.spec['target_name'])

  def GetProductName(self):
    """Returns PRODUCT_NAME."""
    return self.spec.get('product_name', self.spec['target_name'])

  def GetWrapperName(self):
    """Returns the directory name of the bundle represented by this target.
    Only valid for bundles."""
    assert self._IsBundle()
    return self.GetProductName() + self.GetWrapperExtension()

  def GetBundleContentsFolderPath(self):
    """Returns the qualified path to the bundle's contents folder. E.g.
    Chromium.app/Contents or Foo.bundle/Versions/A. Only valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] == 'shared_library':
      return os.path.join(
          self.GetWrapperName(), 'Versions', self.GetFrameworkVersion())
    else:
      # loadable_modules have a 'Contents' folder like executables.
      return os.path.join(self.GetWrapperName(), 'Contents')

  def GetBundleResourceFolder(self):
    """Returns the qualified path to the bundle's resource folder. E.g.
    Chromium.app/Contents/Resources. Only valid for bundles."""
    assert self._IsBundle()
    return os.path.join(self.GetBundleContentsFolderPath(), 'Resources')

  def GetBundlePlistPath(self):
    """Returns the qualified path to the bundle's plist file. E.g.
    Chromium.app/Contents/Info.plist. Only valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] in ('executable', 'loadable_module'):
      return os.path.join(self.GetBundleContentsFolderPath(), 'Info.plist')
    else:
      return os.path.join(self.GetBundleContentsFolderPath(),
                          'Resources', 'Info.plist')

  def GetProductType(self):
    """Returns the PRODUCT_TYPE of this target."""
    if self._IsBundle():
      return {
        'executable': 'com.apple.product-type.application',
        'loadable_module': 'com.apple.product-type.bundle',
        'shared_library': 'com.apple.product-type.framework',
      }[self.spec['type']]
    else:
      return {
        'executable': 'com.apple.product-type.tool',
        'loadable_module': 'com.apple.product-type.library.dynamic',
        'shared_library': 'com.apple.product-type.library.dynamic',
        'static_library': 'com.apple.product-type.library.static',
      }[self.spec['type']]

  def GetMachOType(self):
    """Returns the MACH_O_TYPE of this target."""
    # Weird, but matches Xcode.
    if not self._IsBundle() and self.spec['type'] == 'executable':
      return ''
    return {
      'executable': 'mh_execute',
      'static_library': 'staticlib',
      'shared_library': 'mh_dylib',
      'loadable_module': 'mh_bundle',
    }[self.spec['type']]

  def _GetBundleBinaryPath(self):
    """Returns the name of the bundle binary of by this target.
    E.g. Chromium.app/Contents/MacOS/Chromium. Only valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] in ('shared_library'):
      path = self.GetBundleContentsFolderPath()
    elif self.spec['type'] in ('executable', 'loadable_module'):
      path = os.path.join(self.GetBundleContentsFolderPath(), 'MacOS')
    return os.path.join(path, self.spec.get('product_name',
                                            self.spec['target_name']))

  def _GetStandaloneExecutableSuffix(self):
    if 'product_extension' in self.spec:
      return '.' + self.spec['product_extension']
    return {
      'executable': '',
      'static_library': '.a',
      'shared_library': '.dylib',
      'loadable_module': '.so',
    }[self.spec['type']]

  def _GetStandaloneExecutablePrefix(self):
    return self.spec.get('product_prefix', {
      'executable': '',
      'static_library': 'lib',
      'shared_library': 'lib',
      # Non-bundled loadable_modules are called foo.so for some reason
      # (that is, .so and no prefix) with the xcode build -- match that.
      'loadable_module': '',
    }[self.spec['type']])

  def _GetStandaloneBinaryPath(self):
    """Returns the name of the non-bundle binary represented by this target.
    E.g. hello_world. Only valid for non-bundles."""
    assert not self._IsBundle()
    assert self.spec['type'] in (
        'executable', 'shared_library', 'static_library', 'loadable_module')
    target = self.spec['target_name']
    if self.spec['type'] == 'static_library':
      if target[:3] == 'lib':
        target = target[3:]
    elif self.spec['type'] in ('loadable_module', 'shared_library'):
      if target[:3] == 'lib':
        target = target[3:]

    target_prefix = self._GetStandaloneExecutablePrefix()
    target = self.spec.get('product_name', target)
    target_ext = self._GetStandaloneExecutableSuffix()
    return target_prefix + target + target_ext

  def GetExecutablePath(self):
    """Returns the directory name of the bundle represented by this target. E.g.
    Chromium.app/Contents/MacOS/Chromium."""
    if self._IsBundle():
      return self._GetBundleBinaryPath()
    else:
      return self._GetStandaloneBinaryPath()

  def _SdkPath(self):
    sdk_root = 'macosx10.5'
    if 'SDKROOT' in self._Settings():
      sdk_root = self._Settings()['SDKROOT']
    if sdk_root.startswith('macosx'):
      sdk_root = 'MacOSX' + sdk_root[len('macosx'):]
    return '/Developer/SDKs/%s.sdk' % sdk_root

  def GetCflags(self, configname):
    """Returns flags that need to be added to .c, .cc, .m, and .mm
    compilations."""
    # This functions (and the similar ones below) do not offer complete
    # emulation of all xcode_settings keys. They're implemented on demand.

    self.configname = configname
    cflags = []

    sdk_root = self._SdkPath()
    if 'SDKROOT' in self._Settings():
      cflags.append('-isysroot %s' % sdk_root)

    if self._Test('GCC_CW_ASM_SYNTAX', 'YES', default='YES'):
      cflags.append('-fasm-blocks')

    if 'GCC_DYNAMIC_NO_PIC' in self._Settings():
      if self._Settings()['GCC_DYNAMIC_NO_PIC'] == 'YES':
        cflags.append('-mdynamic-no-pic')
    else:
      pass
      # TODO: In this case, it depends on the target. xcode passes
      # mdynamic-no-pic by default for executable and possibly static lib
      # according to mento

    if self._Test('GCC_ENABLE_PASCAL_STRINGS', 'YES', default='YES'):
      cflags.append('-mpascal-strings')

    self._Appendf(cflags, 'GCC_OPTIMIZATION_LEVEL', '-O%s', default='s')

    if self._Test('GCC_GENERATE_DEBUGGING_SYMBOLS', 'YES', default='YES'):
      dbg_format = self._Settings().get('DEBUG_INFORMATION_FORMAT', 'dwarf')
      if dbg_format == 'dwarf':
        cflags.append('-gdwarf-2')
      elif dbg_format == 'stabs':
        raise NotImplementedError('stabs debug format is not supported yet.')
      elif dbg_format == 'dwarf-with-dsym':
        cflags.append('-gdwarf-2')
      else:
        raise NotImplementedError('Unknown debug format %s' % dbg_format)

    if self._Test('GCC_SYMBOLS_PRIVATE_EXTERN', 'YES', default='NO'):
      cflags.append('-fvisibility=hidden')

    if self._Test('GCC_TREAT_WARNINGS_AS_ERRORS', 'YES', default='NO'):
      cflags.append('-Werror')

    if self._Test('GCC_WARN_ABOUT_MISSING_NEWLINE', 'YES', default='NO'):
      cflags.append('-Wnewline-eof')

    self._Appendf(cflags, 'MACOSX_DEPLOYMENT_TARGET', '-mmacosx-version-min=%s')

    # TODO:
    self._WarnUnimplemented('ARCHS')
    if self._Test('COPY_PHASE_STRIP', 'YES', default='NO'):
      self._WarnUnimplemented('COPY_PHASE_STRIP')
    self._WarnUnimplemented('GCC_DEBUGGING_SYMBOLS')
    self._WarnUnimplemented('GCC_ENABLE_OBJC_EXCEPTIONS')
    self._WarnUnimplemented('GCC_ENABLE_OBJC_GC')
    self._WarnUnimplemented('INFOPLIST_PREPROCESS')
    self._WarnUnimplemented('INFOPLIST_PREPROCESSOR_DEFINITIONS')

    # TODO: This is exported correctly, but assigning to it is not supported.
    self._WarnUnimplemented('MACH_O_TYPE')
    self._WarnUnimplemented('PRODUCT_TYPE')

    # TODO: Do not hardcode arch. Supporting fat binaries will be annoying.
    cflags.append('-arch i386')

    cflags += self._Settings().get('OTHER_CFLAGS', [])
    cflags += self._Settings().get('WARNING_CFLAGS', [])

    config = self.spec['configurations'][self.configname]
    framework_dirs = config.get('mac_framework_dirs', [])
    for directory in framework_dirs:
      cflags.append('-F ' + os.path.join(sdk_root, directory))

    self.configname = None
    return cflags

  def GetCflagsC(self, configname):
    """Returns flags that need to be added to .c, and .m compilations."""
    self.configname = configname
    cflags_c = []
    self._Appendf(cflags_c, 'GCC_C_LANGUAGE_STANDARD', '-std=%s')
    self.configname = None
    return cflags_c

  def GetCflagsCC(self, configname):
    """Returns flags that need to be added to .cc, and .mm compilations."""
    self.configname = configname
    cflags_cc = []
    if self._Test('GCC_ENABLE_CPP_RTTI', 'NO', default='YES'):
      cflags_cc.append('-fno-rtti')
    if self._Test('GCC_ENABLE_CPP_EXCEPTIONS', 'NO', default='YES'):
      cflags_cc.append('-fno-exceptions')
    if self._Test('GCC_INLINES_ARE_PRIVATE_EXTERN', 'YES', default='NO'):
      cflags_cc.append('-fvisibility-inlines-hidden')
    if self._Test('GCC_THREADSAFE_STATICS', 'NO', default='YES'):
      cflags_cc.append('-fno-threadsafe-statics')
    self.configname = None
    return cflags_cc

  def GetCflagsObjC(self, configname):
    """Returns flags that need to be added to .m compilations."""
    self.configname = configname
    self.configname = None
    return []

  def GetCflagsObjCC(self, configname):
    """Returns flags that need to be added to .mm compilations."""
    self.configname = configname
    cflags_objcc = []
    if self._Test('GCC_OBJC_CALL_CXX_CDTORS', 'YES', default='NO'):
      cflags_objcc.append('-fobjc-call-cxx-cdtors')
    self.configname = None
    return cflags_objcc

  def GetLdflags(self, target, configname):
    """Returns flags that need to be passed to the linker."""
    self.configname = configname
    ldflags = []

    # The xcode build is relative to a gyp file's directory, and OTHER_LDFLAGS
    # contains two entries that depend on this. Explicitly absolutify for these
    # two cases.
    def AbsolutifyPrefix(flag, prefix):
      if flag.startswith(prefix):
        flag = prefix + target.Absolutify(flag[len(prefix):])
      return flag
    for ldflag in self._Settings().get('OTHER_LDFLAGS', []):
      # Required for ffmpeg (no idea why they don't use LIBRARY_SEARCH_PATHS,
      # TODO(thakis): Update ffmpeg.gyp):
      ldflag = AbsolutifyPrefix(ldflag, '-L')
      # Required for the nacl plugin:
      ldflag = AbsolutifyPrefix(ldflag, '-Wl,-exported_symbols_list ')
      ldflags.append(ldflag)

    if self._Test('DEAD_CODE_STRIPPING', 'YES', default='NO'):
      ldflags.append('-Wl,-dead_strip')

    if self._Test('PREBINDING', 'YES', default='NO'):
      ldflags.append('-Wl,-prebind')

    self._Appendf(
        ldflags, 'DYLIB_COMPATIBILITY_VERSION', '-compatibility_version %s')
    self._Appendf(
        ldflags, 'DYLIB_CURRENT_VERSION', '-current_version %s')
    self._Appendf(
        ldflags, 'MACOSX_DEPLOYMENT_TARGET', '-mmacosx-version-min=%s')
    if 'SDKROOT' in self._Settings():
      ldflags.append('-isysroot ' + self._SdkPath())

    for library_path in self._Settings().get('LIBRARY_SEARCH_PATHS', []):
      ldflags.append('-L' + library_path)

    if 'ORDER_FILE' in self._Settings():
      ldflags.append('-Wl,-order_file ' +
                     '-Wl,' + target.Absolutify(self._Settings()['ORDER_FILE']))

    # TODO: Do not hardcode arch. Supporting fat binaries will be annoying.
    ldflags.append('-arch i386')

    # Xcode adds the product directory by default.
    ldflags.append('-L' + generator_default_variables['PRODUCT_DIR'])

    install_name = self.GetPerTargetSetting('LD_DYLIB_INSTALL_NAME')
    install_base = self.GetPerTargetSetting('DYLIB_INSTALL_NAME_BASE')
    default_install_name = \
          '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(EXECUTABLE_PATH)'
    if not install_name and install_base:
      install_name = default_install_name

    if install_name:
      # Hardcode support for the variables used in chromium for now, to unblock
      # people using the make build.
      if '$' in install_name:
        assert install_name in ('$(DYLIB_INSTALL_NAME_BASE:standardizepath)/'
            '$(WRAPPER_NAME)/$(PRODUCT_NAME)', default_install_name), (
            'Variables in LD_DYLIB_INSTALL_NAME are not generally supported yet'
            ' in target \'%s\' (got \'%s\')' %
                (self.spec['target_name'], install_name))
        # I'm not quite sure what :standardizepath does. Just call normpath(),
        # but don't let @executable_path/../foo collapse to foo.
        if '/' in install_base:
          prefix, rest = '', install_base
          if install_base.startswith('@'):
            prefix, rest = install_base.split('/', 1)
          rest = os.path.normpath(rest)  # :standardizepath
          install_base = os.path.join(prefix, rest)

        install_name = install_name.replace(
            '$(DYLIB_INSTALL_NAME_BASE:standardizepath)', install_base)
        if self._IsBundle():
          # These are only valid for bundles, hence the |if|.
          install_name = install_name.replace(
              '$(WRAPPER_NAME)', self.GetWrapperName())
          install_name = install_name.replace(
              '$(PRODUCT_NAME)', self.GetProductName())
        else:
          assert '$(WRAPPER_NAME)' not in install_name
          assert '$(PRODUCT_NAME)' not in install_name

        install_name = install_name.replace(
            '$(EXECUTABLE_PATH)', self.GetExecutablePath())

      install_name = QuoteSpaces(install_name)
      ldflags.append('-install_name ' + install_name)

    self.configname = None
    return ldflags

  def GetPerTargetSettings(self):
    """Gets a list of all the per-target settings. This will only fetch keys
    whose values are the same across all configurations."""
    first_pass = True
    result = {}
    for configname in sorted(self.xcode_settings.keys()):
      if first_pass:
        result = dict(self.xcode_settings[configname])
        first_pass = False
      else:
        for key, value in self.xcode_settings[configname].iteritems():
          if key not in result:
            continue
          elif result[key] != value:
            del result[key]
    return result

  def GetPerTargetSetting(self, setting, default=None):
    """Tries to get xcode_settings.setting from spec. Assumes that the setting
       has the same value in all configurations and throws otherwise."""
    first_pass = True
    result = None
    for configname in sorted(self.xcode_settings.keys()):
      if first_pass:
        result = self.xcode_settings[configname].get(setting, None)
        first_pass = False
      else:
        assert result == self.xcode_settings[configname].get(setting, None), (
            "Expected per-target setting for '%s', got per-config setting "
            "(target %s)" % (setting, spec['target_name']))
    if result is None:
      return default
    return result

  def _GetStripPostbuilds(self, configname, output_binary):
    """Returns a list of shell commands that contain the shell commands
    neccessary to strip this target's binary. These should be run as postbuilds
    before the actual postbuilds run."""
    self.configname = configname

    result = []
    if (self._Test('DEPLOYMENT_POSTPROCESSING', 'YES', default='NO') and
        self._Test('STRIP_INSTALLED_PRODUCT', 'YES', default='NO')):

      default_strip_style = 'debugging'
      if self._IsBundle():
        default_strip_style = 'non-global'
      elif self.spec['type'] == 'executable':
        default_strip_style = 'all'

      strip_style = self._Settings().get('STRIP_STYLE', default_strip_style)
      strip_flags = {
        'all': '',
        'non-global': '-x',
        'debugging': '-S',
      }[strip_style]

      explicit_strip_flags = self._Settings().get('STRIPFLAGS', '')
      if explicit_strip_flags:
        strip_flags += ' ' + explicit_strip_flags

      result.append('echo STRIP\\(%s\\)' % self.spec['target_name'])
      result.append('strip %s %s' % (strip_flags, output_binary))

    self.configname = None
    return result

  def _GetDebugPostbuilds(self, configname, output, output_binary):
    """Returns a list of shell commands that contain the shell commands
    neccessary to massage this target's debug information. These should be run
    as postbuilds before the actual postbuilds run."""
    self.configname = configname

    # For static libraries, no dSYMs are created.
    result = []
    if (self._Test('GCC_GENERATE_DEBUGGING_SYMBOLS', 'YES', default='YES') and
        self._Test(
            'DEBUG_INFORMATION_FORMAT', 'dwarf-with-dsym', default='dwarf') and
        self.spec['type'] != 'static_library'):
      result.append('echo DSYMUTIL\\(%s\\)' % self.spec['target_name'])
      result.append('dsymutil %s -o %s' % (output_binary, output + '.dSYM'))

    self.configname = None
    return result

  def GetTargetPostbuilds(self, configname, output, output_binary):
    """Returns a list of shell commands that contain the shell commands
    to run as postbuilds for this target, before the actual postbuilds."""
    # dSYMs need to build before stripping happens.
    return (self._GetDebugPostbuilds(configname, output, output_binary) +
            self._GetStripPostbuilds(configname, output_binary))


class MacPrefixHeader(object):
  """A class that helps with emulating Xcode's GCC_PREFIX_HEADER feature. If
  GCC_PREFIX_HEADER isn't present (in most gyp targets on mac, and always on
  non-mac systems), all methods of this class are no-ops."""

  def __init__(self, path_provider):
    # This doesn't support per-configuration prefix headers. Good enough
    # for now.
    self.header = None
    self.compile_headers = False
    if path_provider.flavor == 'mac':
      self.header = path_provider.xcode_settings.GetPerTargetSetting(
          'GCC_PREFIX_HEADER')
      self.compile_headers = path_provider.xcode_settings.GetPerTargetSetting(
          'GCC_PRECOMPILE_PREFIX_HEADER', default='NO') != 'NO'
    self.compiled_headers = {}
    if self.header:
      self.header = path_provider.Absolutify(self.header)
      if self.compile_headers:
        for lang in ['c', 'cc', 'm', 'mm']:
          self.compiled_headers[lang] = path_provider.Pchify(self.header, lang)

  def _Gch(self, lang):
    """Returns the actual file name of the prefix header for language |lang|."""
    assert self.compile_headers
    return self.compiled_headers[lang] + '.gch'

  def WriteObjDependencies(self, compilable, objs, writer):
    """Writes dependencies from the object files in |objs| to the corresponding
    precompiled header file. |compilable[i]| has to be the source file belonging
    to |objs[i]|."""
    if not self.header or not self.compile_headers:
      return

    writer.WriteLn('# Dependencies from obj files to their precompiled headers')
    for source, obj in zip(compilable, objs):
      ext = os.path.splitext(source)[1]
      lang = {
        '.c': 'c',
        '.cpp': 'cc', '.cc': 'cc', '.cxx': 'cc',
        '.m': 'm',
        '.mm': 'mm',
      }.get(ext, None)
      if lang:
        writer.WriteLn('%s: %s' % (obj, self._Gch(lang)))
    writer.WriteLn('# End precompiled header dependencies')

  def GetInclude(self, lang):
    """Gets the cflags to include the prefix header for language |lang|."""
    if self.compile_headers and lang in self.compiled_headers:
      return '-include %s ' % self.compiled_headers[lang]
    elif self.header:
      return '-include %s ' % self.header
    else:
      return ''

  def WritePchTargets(self, writer):
    """Writes make rules to compile the prefix headers."""
    if not self.header or not self.compile_headers:
      return

    writer.WriteLn(self._Gch('c') + ": GYP_PCH_CFLAGS := "
                 "-x c-header "
                 "$(DEFS_$(BUILDTYPE)) "
                 "$(INCS_$(BUILDTYPE)) "
                 "$(CFLAGS_$(BUILDTYPE)) "
                 "$(CFLAGS_C_$(BUILDTYPE))")

    writer.WriteLn(self._Gch('cc') + ": GYP_PCH_CCFLAGS := "
                 "-x c++-header "
                 "$(DEFS_$(BUILDTYPE)) "
                 "$(INCS_$(BUILDTYPE)) "
                 "$(CFLAGS_$(BUILDTYPE)) "
                 "$(CFLAGS_CC_$(BUILDTYPE))")

    writer.WriteLn(self._Gch('m') + ": GYP_PCH_OBJCFLAGS := "
                 "-x objective-c-header "
                 "$(DEFS_$(BUILDTYPE)) "
                 "$(INCS_$(BUILDTYPE)) "
                 "$(CFLAGS_$(BUILDTYPE)) "
                 "$(CFLAGS_C_$(BUILDTYPE)) "
                 "$(CFLAGS_OBJC_$(BUILDTYPE))")

    writer.WriteLn(self._Gch('mm') + ": GYP_PCH_OBJCXXFLAGS := "
                 "-x objective-c++-header "
                 "$(DEFS_$(BUILDTYPE)) "
                 "$(INCS_$(BUILDTYPE)) "
                 "$(CFLAGS_$(BUILDTYPE)) "
                 "$(CFLAGS_CC_$(BUILDTYPE)) "
                 "$(CFLAGS_OBJCC_$(BUILDTYPE))")

    for lang in self.compiled_headers:
      writer.WriteLn('%s: %s FORCE_DO_CMD' % (self._Gch(lang), self.header))
      writer.WriteLn('\t@$(call do_cmd,pch_%s,1)' % lang)
      writer.WriteLn('')
      assert ' ' not in self._Gch(lang), (
          "Spaces in gch filenames not supported (%s)"  % self._Gch(lang))
      writer.WriteLn('all_deps += %s' % self._Gch(lang))
      writer.WriteLn('')


class MakefileWriter:
  """MakefileWriter packages up the writing of one target-specific foobar.mk.

  Its only real entry point is Write(), and is mostly used for namespacing.
  """

  def __init__(self, generator_flags, flavor):
    self.generator_flags = generator_flags
    self.flavor = flavor
    # Keep track of the total number of outputs for this makefile.
    self._num_outputs = 0

    self.suffix_rules_srcdir = {}
    self.suffix_rules_objdir1 = {}
    self.suffix_rules_objdir2 = {}

    # Generate suffix rules for all compilable extensions.
    for ext in COMPILABLE_EXTENSIONS.keys():
      # Suffix rules for source folder.
      self.suffix_rules_srcdir.update({ext: ("""\
$(obj).$(TOOLSET)/$(TARGET)/%%.o: $(srcdir)/%%%s FORCE_DO_CMD
	@$(call do_cmd,%s,1)
""" % (ext, COMPILABLE_EXTENSIONS[ext]))})

      # Suffix rules for generated source files.
      self.suffix_rules_objdir1.update({ext: ("""\
$(obj).$(TOOLSET)/$(TARGET)/%%.o: $(obj).$(TOOLSET)/%%%s FORCE_DO_CMD
	@$(call do_cmd,%s,1)
""" % (ext, COMPILABLE_EXTENSIONS[ext]))})
      self.suffix_rules_objdir2.update({ext: ("""\
$(obj).$(TOOLSET)/$(TARGET)/%%.o: $(obj)/%%%s FORCE_DO_CMD
	@$(call do_cmd,%s,1)
""" % (ext, COMPILABLE_EXTENSIONS[ext]))})


  def NumOutputs(self):
    return self._num_outputs


  def Write(self, qualified_target, base_path, output_filename, spec, configs,
            part_of_all):
    """The main entry point: writes a .mk file for a single target.

    Arguments:
      qualified_target: target we're generating
      base_path: path relative to source root we're building in, used to resolve
                 target-relative paths
      output_filename: output .mk file name to write
      spec, configs: gyp info
      part_of_all: flag indicating this target is part of 'all'
    """
    ensure_directory_exists(output_filename)

    self.fp = open(output_filename, 'w')

    self.fp.write(header)

    self.path = base_path
    self.target = spec['target_name']
    self.type = spec['type']
    self.toolset = spec['toolset']

    if self.type == 'settings':
      # TODO: 'settings' is not actually part of gyp; it was
      # accidentally introduced somehow into just the Linux build files.
      # Remove this (or make it an error) once all the users are fixed.
      print ("WARNING: %s uses invalid type 'settings'.  " % self.target +
             "Please fix the source gyp file to use type 'none'.")
      print "See http://code.google.com/p/chromium/issues/detail?id=96629 ."
      self.type = 'none'

    # Bundles are directories with a certain subdirectory structure, instead of
    # just a single file. Bundle rules do not produce a binary but also package
    # resources into that directory.
    self.is_mac_bundle = (int(spec.get('mac_bundle', 0)) != 0 and
        self.flavor == 'mac')
    if self.is_mac_bundle:
      assert self.type != 'none', (
          'mac_bundle targets cannot have type none (target "%s")' %
          self.target)

    if self.flavor == 'mac':
      self.xcode_settings = XcodeSettings(spec)

    deps, link_deps = self.ComputeDeps(spec)

    # Some of the generation below can add extra output, sources, or
    # link dependencies.  All of the out params of the functions that
    # follow use names like extra_foo.
    extra_outputs = []
    extra_sources = []
    extra_link_deps = []
    extra_mac_bundle_resources = []
    mac_bundle_deps = []

    if self.is_mac_bundle:
      self.output = self.ComputeMacBundleOutput(spec)
      self.output_binary = self.ComputeMacBundleBinaryOutput(spec)
    else:
      self.output = self.output_binary = self.ComputeOutput(spec)

    self.output = QuoteSpaces(self.output)
    self.output_binary = QuoteSpaces(self.output_binary)

    self._INSTALLABLE_TARGETS = ('executable', 'loadable_module',
                                 'shared_library')
    if self.type in self._INSTALLABLE_TARGETS:
      self.alias = os.path.basename(self.output)
      install_path = self._InstallableTargetInstallPath()
    else:
      self.alias = self.output
      install_path = self.output

    self.WriteLn("TOOLSET := " + self.toolset)
    self.WriteLn("TARGET := " + self.target)

    # Actions must come first, since they can generate more OBJs for use below.
    if 'actions' in spec:
      self.WriteActions(spec['actions'], extra_sources, extra_outputs,
                        extra_mac_bundle_resources, part_of_all, spec)

    # Rules must be early like actions.
    if 'rules' in spec:
      self.WriteRules(spec['rules'], extra_sources, extra_outputs,
                      extra_mac_bundle_resources, part_of_all)

    if 'copies' in spec:
      self.WriteCopies(spec['copies'], extra_outputs, part_of_all, spec)

    # Bundle resources.
    if self.is_mac_bundle:
      all_mac_bundle_resources = (
          spec.get('mac_bundle_resources', []) + extra_mac_bundle_resources)
      if all_mac_bundle_resources:
        self.WriteMacBundleResources(
            all_mac_bundle_resources, mac_bundle_deps, spec)
      info_plist = self.xcode_settings.GetPerTargetSetting('INFOPLIST_FILE')
      if info_plist:
        self.WriteMacInfoPlist(info_plist, mac_bundle_deps, spec)

    # Sources.
    all_sources = spec.get('sources', []) + extra_sources
    if all_sources:
      self.WriteSources(
          configs, deps, all_sources, extra_outputs,
          extra_link_deps, part_of_all, MacPrefixHeader(self))
      sources = filter(Compilable, all_sources)
      if sources:
        self.WriteLn(SHARED_HEADER_SUFFIX_RULES_COMMENT1)
        extensions = set([os.path.splitext(s)[1] for s in sources])
        for ext in extensions:
          if ext in self.suffix_rules_srcdir:
            self.WriteLn(self.suffix_rules_srcdir[ext])
        self.WriteLn(SHARED_HEADER_SUFFIX_RULES_COMMENT2)
        for ext in extensions:
          if ext in self.suffix_rules_objdir1:
            self.WriteLn(self.suffix_rules_objdir1[ext])
        for ext in extensions:
          if ext in self.suffix_rules_objdir2:
            self.WriteLn(self.suffix_rules_objdir2[ext])
        self.WriteLn('# End of this set of suffix rules')

        # Add dependency from bundle to bundle binary.
        if self.is_mac_bundle:
          mac_bundle_deps.append(self.output_binary)

    self.WriteTarget(spec, configs, deps, extra_link_deps + link_deps,
                     mac_bundle_deps, extra_outputs, part_of_all)

    # Update global list of target outputs, used in dependency tracking.
    target_outputs[qualified_target] = install_path

    # Update global list of link dependencies.
    if self.type in ('static_library', 'shared_library'):
      target_link_deps[qualified_target] = self.output_binary

    # Currently any versions have the same effect, but in future the behavior
    # could be different.
    if self.generator_flags.get('android_ndk_version', None):
      self.WriteAndroidNdkModuleRule(self.target, all_sources, link_deps)

    self.fp.close()


  def WriteSubMake(self, output_filename, makefile_path, targets, build_dir):
    """Write a "sub-project" Makefile.

    This is a small, wrapper Makefile that calls the top-level Makefile to build
    the targets from a single gyp file (i.e. a sub-project).

    Arguments:
      output_filename: sub-project Makefile name to write
      makefile_path: path to the top-level Makefile
      targets: list of "all" targets for this sub-project
      build_dir: build output directory, relative to the sub-project
    """
    ensure_directory_exists(output_filename)
    self.fp = open(output_filename, 'w')
    self.fp.write(header)
    # For consistency with other builders, put sub-project build output in the
    # sub-project dir (see test/subdirectory/gyptest-subdir-all.py).
    self.WriteLn('export builddir_name ?= %s' %
                 os.path.join(os.path.dirname(output_filename), build_dir))
    self.WriteLn('.PHONY: all')
    self.WriteLn('all:')
    if makefile_path:
      makefile_path = ' -C ' + makefile_path
    self.WriteLn('\t$(MAKE)%s %s' % (makefile_path, ' '.join(targets)))
    self.fp.close()


  def WriteActions(self, actions, extra_sources, extra_outputs,
                   extra_mac_bundle_resources, part_of_all, spec):
    """Write Makefile code for any 'actions' from the gyp input.

    extra_sources: a list that will be filled in with newly generated source
                   files, if any
    extra_outputs: a list that will be filled in with any outputs of these
                   actions (used to make other pieces dependent on these
                   actions)
    part_of_all: flag indicating this target is part of 'all'
    """
    for action in actions:
      name = self.target + '_' + StringToMakefileVariable(action['action_name'])
      self.WriteLn('### Rules for action "%s":' % action['action_name'])
      inputs = action['inputs']
      outputs = action['outputs']

      # Build up a list of outputs.
      # Collect the output dirs we'll need.
      dirs = set()
      for out in outputs:
        dir = os.path.split(out)[0]
        if dir:
          dirs.add(dir)
      if int(action.get('process_outputs_as_sources', False)):
        extra_sources += outputs
      if int(action.get('process_outputs_as_mac_bundle_resources', False)):
        extra_mac_bundle_resources += outputs

      # Write the actual command.
      command = gyp.common.EncodePOSIXShellList(action['action'])
      if 'message' in action:
        self.WriteLn('quiet_cmd_%s = ACTION %s $@' % (name, action['message']))
      else:
        self.WriteLn('quiet_cmd_%s = ACTION %s $@' % (name, name))
      if len(dirs) > 0:
        command = 'mkdir -p %s' % ' '.join(dirs) + '; ' + command

      cd_action = 'cd %s; ' % Sourceify(self.path or '.')

      # Set LD_LIBRARY_PATH in case the action runs an executable from this
      # build which links to shared libs from this build.
      # actions run on the host, so they should in theory only use host
      # libraries, but until everything is made cross-compile safe, also use
      # target libraries.
      # TODO(piman): when everything is cross-compile safe, remove lib.target
      self.WriteLn('cmd_%s = LD_LIBRARY_PATH=$(builddir)/lib.host:'
                   '$(builddir)/lib.target:$$LD_LIBRARY_PATH; '
                   'export LD_LIBRARY_PATH; '
                   '%s%s'
                   % (name, cd_action, command))
      self.WriteLn()
      outputs = map(self.Absolutify, outputs)
      # The makefile rules are all relative to the top dir, but the gyp actions
      # are defined relative to their containing dir.  This replaces the obj
      # variable for the action rule with an absolute version so that the output
      # goes in the right place.
      # Only write the 'obj' and 'builddir' rules for the "primary" output (:1);
      # it's superfluous for the "extra outputs", and this avoids accidentally
      # writing duplicate dummy rules for those outputs.
      # Same for environment.
      self.WriteMakeRule(outputs[:1], ['obj := $(abs_obj)'])
      # Needs to be before builddir is redefined in the next line!
      self.WriteXcodeEnv(outputs[0], spec, target_relative_path=True)
      self.WriteMakeRule(outputs[:1], ['builddir := $(abs_builddir)'])

      for input in inputs:
        assert ' ' not in input, (
            "Spaces in action input filenames not supported (%s)"  % input)
      for output in outputs:
        assert ' ' not in output, (
            "Spaces in action output filenames not supported (%s)"  % output)

      self.WriteDoCmd(outputs, map(Sourceify, map(self.Absolutify, inputs)),
                      part_of_all=part_of_all, command=name)

      # Stuff the outputs in a variable so we can refer to them later.
      outputs_variable = 'action_%s_outputs' % name
      self.WriteLn('%s := %s' % (outputs_variable, ' '.join(outputs)))
      extra_outputs.append('$(%s)' % outputs_variable)
      self.WriteLn()

    self.WriteLn()


  def WriteRules(self, rules, extra_sources, extra_outputs,
                 extra_mac_bundle_resources, part_of_all):
    """Write Makefile code for any 'rules' from the gyp input.

    extra_sources: a list that will be filled in with newly generated source
                   files, if any
    extra_outputs: a list that will be filled in with any outputs of these
                   rules (used to make other pieces dependent on these rules)
    part_of_all: flag indicating this target is part of 'all'
    """
    for rule in rules:
      name = self.target + '_' + StringToMakefileVariable(rule['rule_name'])
      count = 0
      self.WriteLn('### Generated for rule %s:' % name)

      all_outputs = []

      for rule_source in rule.get('rule_sources', []):
        dirs = set()
        (rule_source_dirname, rule_source_basename) = os.path.split(rule_source)
        (rule_source_root, rule_source_ext) = \
            os.path.splitext(rule_source_basename)

        outputs = [self.ExpandInputRoot(out, rule_source_root,
                                        rule_source_dirname)
                   for out in rule['outputs']]

        # If an output is just the file name, turn it into a path so
        # FixupArgPath() will know to Absolutify() it.
        outputs = map(
            lambda x : os.path.dirname(x) and x or os.path.join('.', x),
            outputs)

        for out in outputs:
          dir = os.path.dirname(out)
          if dir:
            dirs.add(dir)
        if int(rule.get('process_outputs_as_sources', False)):
          extra_sources += outputs
        if int(rule.get('process_outputs_as_mac_bundle_resources', False)):
          extra_mac_bundle_resources += outputs
        all_outputs += outputs
        inputs = map(Sourceify, map(self.Absolutify, [rule_source] +
                                    rule.get('inputs', [])))
        actions = ['$(call do_cmd,%s_%d)' % (name, count)]

        if name == 'resources_grit':
          # HACK: This is ugly.  Grit intentionally doesn't touch the
          # timestamp of its output file when the file doesn't change,
          # which is fine in hash-based dependency systems like scons
          # and forge, but not kosher in the make world.  After some
          # discussion, hacking around it here seems like the least
          # amount of pain.
          actions += ['@touch --no-create $@']

        # Only write the 'obj' and 'builddir' rules for the "primary" output
        # (:1); it's superfluous for the "extra outputs", and this avoids
        # accidentally writing duplicate dummy rules for those outputs.
        self.WriteMakeRule(outputs[:1], ['obj := $(abs_obj)'])
        self.WriteMakeRule(outputs[:1], ['builddir := $(abs_builddir)'])
        self.WriteMakeRule(outputs, inputs + ['FORCE_DO_CMD'], actions)
        for output in outputs:
          assert ' ' not in output, (
              "Spaces in rule filenames not yet supported (%s)"  % output)
        self.WriteLn('all_deps += %s' % ' '.join(outputs))
        self._num_outputs += len(outputs)

        action = [self.ExpandInputRoot(ac, rule_source_root,
                                       rule_source_dirname)
                  for ac in rule['action']]
        mkdirs = ''
        if len(dirs) > 0:
          mkdirs = 'mkdir -p %s; ' % ' '.join(dirs)
        cd_action = 'cd %s; ' % Sourceify(self.path or '.')
        # Set LD_LIBRARY_PATH in case the rule runs an executable from this
        # build which links to shared libs from this build.
        # rules run on the host, so they should in theory only use host
        # libraries, but until everything is made cross-compile safe, also use
        # target libraries.
        # TODO(piman): when everything is cross-compile safe, remove lib.target
        self.WriteLn(
            "cmd_%(name)s_%(count)d = LD_LIBRARY_PATH="
              "$(builddir)/lib.host:$(builddir)/lib.target:$$LD_LIBRARY_PATH; "
              "export LD_LIBRARY_PATH; "
              "%(cd_action)s%(mkdirs)s%(action)s" % {
          'action': gyp.common.EncodePOSIXShellList(action),
          'cd_action': cd_action,
          'count': count,
          'mkdirs': mkdirs,
          'name': name,
        })
        self.WriteLn(
            'quiet_cmd_%(name)s_%(count)d = RULE %(name)s_%(count)d $@' % {
          'count': count,
          'name': name,
        })
        self.WriteLn()
        count += 1

      outputs_variable = 'rule_%s_outputs' % name
      self.WriteList(all_outputs, outputs_variable)
      extra_outputs.append('$(%s)' % outputs_variable)

      self.WriteLn('### Finished generating for rule: %s' % name)
      self.WriteLn()
    self.WriteLn('### Finished generating for all rules')
    self.WriteLn('')


  def WriteCopies(self, copies, extra_outputs, part_of_all, spec):
    """Write Makefile code for any 'copies' from the gyp input.

    extra_outputs: a list that will be filled in with any outputs of this action
                   (used to make other pieces dependent on this action)
    part_of_all: flag indicating this target is part of 'all'
    """
    self.WriteLn('### Generated for copy rule.')

    variable = self.target + '_copies'
    outputs = []
    for copy in copies:
      for path in copy['files']:
        path = Sourceify(self.Absolutify(path))
        filename = os.path.split(path)[1]
        output = Sourceify(self.Absolutify(os.path.join(copy['destination'],
                                                        filename)))
        path = QuoteSpaces(path)
        output = QuoteSpaces(output)

        # If the output path has variables in it, which happens in practice for
        # 'copies', writing the environment as target-local doesn't work,
        # because the variables are already needed for the target name.
        # Copying the environment variables into global make variables doesn't
        # work either, because then the .d files will potentially contain spaces
        # after variable expansion, and .d file handling cannot handle spaces.
        # As a workaround, manually expand variables at gyp time. Since 'copies'
        # can't run scripts, there's no need to write the env then.
        # WriteDoCmd() will escape spaces for .d files.
        import gyp.generator.xcode as xcode_generator
        env = self.GetXcodeEnv(spec)
        output = xcode_generator.ExpandXcodeVariables(output, env)
        path = xcode_generator.ExpandXcodeVariables(path, env)
        self.WriteDoCmd([output], [path], 'copy', part_of_all)
        outputs.append(output)
    self.WriteLn('%s = %s' % (variable, ' '.join(outputs)))
    extra_outputs.append('$(%s)' % variable)
    self.WriteLn()


  def WriteMacBundleResources(self, resources, bundle_deps, spec):
    """Writes Makefile code for 'mac_bundle_resources'."""
    self.WriteLn('### Generated for mac_bundle_resources')
    variable = self.target + '_mac_bundle_resources'
    path = generator_default_variables['PRODUCT_DIR']
    dest = os.path.join(path, self.xcode_settings.GetBundleResourceFolder())
    dest = QuoteSpaces(dest)
    for res in resources:
      output = dest

      assert ' ' not in res, (
        "Spaces in resource filenames not supported (%s)"  % res)

      # Split into (path,file).
      path = Sourceify(self.Absolutify(res))
      path_parts = os.path.split(path)

      # Now split the path into (prefix,maybe.lproj).
      lproj_parts = os.path.split(path_parts[0])
      # If the resource lives in a .lproj bundle, add that to the destination.
      if lproj_parts[1].endswith('.lproj'):
        output = os.path.join(output, lproj_parts[1])

      output = Sourceify(self.Absolutify(os.path.join(output, path_parts[1])))
      # Compiled XIB files are referred to by .nib.
      if output.endswith('.xib'):
        output = output[0:-3] + 'nib'

      self.WriteDoCmd([output], [path], 'mac_tool,,,copy-bundle-resource',
                      part_of_all=True)
      bundle_deps.append(output)


  def WriteMacInfoPlist(self, info_plist, bundle_deps, spec):
    """Write Makefile code for bundle Info.plist files."""
    assert ' ' not in info_plist, (
      "Spaces in resource filenames not supported (%s)"  % info_plist)
    info_plist = self.Absolutify(info_plist)
    path = generator_default_variables['PRODUCT_DIR']
    dest_plist = os.path.join(path, self.xcode_settings.GetBundlePlistPath())
    dest_plist = QuoteSpaces(dest_plist)
    extra_settings = self.xcode_settings.GetPerTargetSettings()
    # plists can contain envvars and substitute them into the file..
    self.WriteXcodeEnv(dest_plist, spec, additional_settings=extra_settings)
    self.WriteDoCmd([dest_plist], [info_plist], 'mac_tool,,,copy-info-plist',
                    part_of_all=True)
    bundle_deps.append(dest_plist)


  def WriteSources(self, configs, deps, sources,
                   extra_outputs, extra_link_deps,
                   part_of_all, precompiled_header):
    """Write Makefile code for any 'sources' from the gyp input.
    These are source files necessary to build the current target.

    configs, deps, sources: input from gyp.
    extra_outputs: a list of extra outputs this action should be dependent on;
                   used to serialize action/rules before compilation
    extra_link_deps: a list that will be filled in with any outputs of
                     compilation (to be used in link lines)
    part_of_all: flag indicating this target is part of 'all'
    """

    # Write configuration-specific variables for CFLAGS, etc.
    for configname in sorted(configs.keys()):
      config = configs[configname]
      self.WriteList(config.get('defines'), 'DEFS_%s' % configname, prefix='-D',
          quoter=EscapeCppDefine)

      if self.flavor == 'mac':
        cflags = self.xcode_settings.GetCflags(configname)
        cflags_c = self.xcode_settings.GetCflagsC(configname)
        cflags_cc = self.xcode_settings.GetCflagsCC(configname)
        cflags_objc = self.xcode_settings.GetCflagsObjC(configname)
        cflags_objcc = self.xcode_settings.GetCflagsObjCC(configname)
      else:
        cflags = config.get('cflags')
        cflags_c = config.get('cflags_c')
        cflags_cc = config.get('cflags_cc')

      self.WriteLn("# Flags passed to all source files.");
      self.WriteList(cflags, 'CFLAGS_%s' % configname)
      self.WriteLn("# Flags passed to only C files.");
      self.WriteList(cflags_c, 'CFLAGS_C_%s' % configname)
      self.WriteLn("# Flags passed to only C++ files.");
      self.WriteList(cflags_cc, 'CFLAGS_CC_%s' % configname)
      if self.flavor == 'mac':
        self.WriteLn("# Flags passed to only ObjC files.");
        self.WriteList(cflags_objc, 'CFLAGS_OBJC_%s' % configname)
        self.WriteLn("# Flags passed to only ObjC++ files.");
        self.WriteList(cflags_objcc, 'CFLAGS_OBJCC_%s' % configname)
      includes = config.get('include_dirs')
      if includes:
        includes = map(Sourceify, map(self.Absolutify, includes))
      self.WriteList(includes, 'INCS_%s' % configname, prefix='-I')

    compilable = filter(Compilable, sources)
    objs = map(self.Objectify, map(self.Absolutify, map(Target, compilable)))
    self.WriteList(objs, 'OBJS')

    for obj in objs:
      assert ' ' not in obj, (
          "Spaces in object filenames not supported (%s)"  % obj)
    self.WriteLn('# Add to the list of files we specially track '
                 'dependencies for.')
    self.WriteLn('all_deps += $(OBJS)')
    self._num_outputs += len(objs)
    self.WriteLn()

    # Make sure our dependencies are built first.
    if deps:
      self.WriteMakeRule(['$(OBJS)'], deps,
                         comment = 'Make sure our dependencies are built '
                                   'before any of us.',
                         order_only = True)

    # Make sure the actions and rules run first.
    # If they generate any extra headers etc., the per-.o file dep tracking
    # will catch the proper rebuilds, so order only is still ok here.
    if extra_outputs:
      self.WriteMakeRule(['$(OBJS)'], extra_outputs,
                         comment = 'Make sure our actions/rules run '
                                   'before any of us.',
                         order_only = True)

    precompiled_header.WriteObjDependencies(compilable, objs, self)

    if objs:
      extra_link_deps.append('$(OBJS)')
      self.WriteLn("""\
# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.""")
      self.WriteLn("$(OBJS): TOOLSET := $(TOOLSET)")
      self.WriteLn("$(OBJS): GYP_CFLAGS := "
                   "$(DEFS_$(BUILDTYPE)) "
                   "$(INCS_$(BUILDTYPE)) "
                   "%s" % precompiled_header.GetInclude('c') +
                   "$(CFLAGS_$(BUILDTYPE)) "
                   "$(CFLAGS_C_$(BUILDTYPE))")
      self.WriteLn("$(OBJS): GYP_CXXFLAGS := "
                   "$(DEFS_$(BUILDTYPE)) "
                   "$(INCS_$(BUILDTYPE)) "
                   "%s" % precompiled_header.GetInclude('cc') +
                   "$(CFLAGS_$(BUILDTYPE)) "
                   "$(CFLAGS_CC_$(BUILDTYPE))")
      if self.flavor == 'mac':
        self.WriteLn("$(OBJS): GYP_OBJCFLAGS := "
                     "$(DEFS_$(BUILDTYPE)) "
                     "$(INCS_$(BUILDTYPE)) "
                     "%s" % precompiled_header.GetInclude('m') +
                     "$(CFLAGS_$(BUILDTYPE)) "
                     "$(CFLAGS_C_$(BUILDTYPE)) "
                     "$(CFLAGS_OBJC_$(BUILDTYPE))")
        self.WriteLn("$(OBJS): GYP_OBJCXXFLAGS := "
                     "$(DEFS_$(BUILDTYPE)) "
                     "$(INCS_$(BUILDTYPE)) "
                     "%s" % precompiled_header.GetInclude('mm') +
                     "$(CFLAGS_$(BUILDTYPE)) "
                     "$(CFLAGS_CC_$(BUILDTYPE)) "
                     "$(CFLAGS_OBJCC_$(BUILDTYPE))")

    precompiled_header.WritePchTargets(self)

    # If there are any object files in our input file list, link them into our
    # output.
    extra_link_deps += filter(Linkable, sources)

    self.WriteLn()


  def ComputeOutputBasename(self, spec):
    """Return the 'output basename' of a gyp spec.

    E.g., the loadable module 'foobar' in directory 'baz' will produce
      'libfoobar.so'
    """
    assert not self.is_mac_bundle

    if self.flavor == 'mac' and self.type in (
        'static_library', 'executable', 'shared_library', 'loadable_module'):
      return self.xcode_settings.GetExecutablePath()

    target = spec['target_name']
    target_prefix = ''
    target_ext = ''
    if self.type == 'static_library':
      if target[:3] == 'lib':
        target = target[3:]
      target_prefix = 'lib'
      target_ext = '.a'
    elif self.type in ('loadable_module', 'shared_library'):
      if target[:3] == 'lib':
        target = target[3:]
      target_prefix = 'lib'
      target_ext = '.so'
    elif self.type == 'none':
      target = '%s.stamp' % target
    elif self.type != 'executable':
      print ("ERROR: What output file should be generated?",
             "type", self.type, "target", target)

    target_prefix = spec.get('product_prefix', target_prefix)
    target = spec.get('product_name', target)
    product_ext = spec.get('product_extension')
    if product_ext:
      target_ext = '.' + product_ext

    return target_prefix + target + target_ext


  def _InstallImmediately(self):
    return self.toolset == 'target' and self.flavor == 'mac' and self.type in (
          'static_library', 'executable', 'shared_library', 'loadable_module')


  def ComputeOutput(self, spec):
    """Return the 'output' (full output path) of a gyp spec.

    E.g., the loadable module 'foobar' in directory 'baz' will produce
      '$(obj)/baz/libfoobar.so'
    """
    assert not self.is_mac_bundle

    path = os.path.join('$(obj).' + self.toolset, self.path)
    if self.type == 'executable' or self._InstallImmediately():
      path = '$(builddir)'
    path = spec.get('product_dir', path)
    return os.path.join(path, self.ComputeOutputBasename(spec))


  def ComputeMacBundleOutput(self, spec):
    """Return the 'output' (full output path) to a bundle output directory."""
    assert self.is_mac_bundle
    path = generator_default_variables['PRODUCT_DIR']
    return os.path.join(path, self.xcode_settings.GetWrapperName())


  def ComputeMacBundleBinaryOutput(self, spec):
    """Return the 'output' (full output path) to the binary in a bundle."""
    path = generator_default_variables['PRODUCT_DIR']
    return os.path.join(path, self.xcode_settings.GetExecutablePath())


  def ComputeDeps(self, spec):
    """Compute the dependencies of a gyp spec.

    Returns a tuple (deps, link_deps), where each is a list of
    filenames that will need to be put in front of make for either
    building (deps) or linking (link_deps).
    """
    deps = []
    link_deps = []
    if 'dependencies' in spec:
      deps.extend([target_outputs[dep] for dep in spec['dependencies']
                   if target_outputs[dep]])
      for dep in spec['dependencies']:
        if dep in target_link_deps:
          link_deps.append(target_link_deps[dep])
      deps.extend(link_deps)
      # TODO: It seems we need to transitively link in libraries (e.g. -lfoo)?
      # This hack makes it work:
      # link_deps.extend(spec.get('libraries', []))
    return (gyp.common.uniquer(deps), gyp.common.uniquer(link_deps))


  def WriteDependencyOnExtraOutputs(self, target, extra_outputs):
    self.WriteMakeRule([self.output_binary], extra_outputs,
                       comment = 'Build our special outputs first.',
                       order_only = True)


  def WriteTarget(self, spec, configs, deps, link_deps, bundle_deps,
                  extra_outputs, part_of_all):
    """Write Makefile code to produce the final target of the gyp spec.

    spec, configs: input from gyp.
    deps, link_deps: dependency lists; see ComputeDeps()
    extra_outputs: any extra outputs that our target should depend on
    part_of_all: flag indicating this target is part of 'all'
    """

    self.WriteLn('### Rules for final target.')

    if extra_outputs:
      self.WriteDependencyOnExtraOutputs(self.output_binary, extra_outputs)
      self.WriteMakeRule(extra_outputs, deps,
                         comment=('Preserve order dependency of '
                                  'special output on deps.'),
                         order_only = True,
                         multiple_output_trick = False)

    has_target_postbuilds = False
    if self.type != 'none':
      for configname in sorted(configs.keys()):
        config = configs[configname]
        if self.flavor == 'mac':
          ldflags = self.xcode_settings.GetLdflags(self, configname)

          # TARGET_POSTBUILDS_$(BUILDTYPE) is added to postbuilds later on.
          target_postbuilds = self.xcode_settings.GetTargetPostbuilds(
              configname, self.output, self.output_binary)
          if target_postbuilds:
            has_target_postbuilds = True
            self.WriteLn('%s: TARGET_POSTBUILDS_%s := %s' %
                (self.output,
                 configname,
                 gyp.common.EncodePOSIXShellList(target_postbuilds)))
        else:
          ldflags = config.get('ldflags', [])
          # Compute an rpath for this output if needed.
          if any(dep.endswith('.so') for dep in deps):
            # We want to get the literal string "$ORIGIN" into the link command,
            # so we need lots of escaping.
            ldflags.append(r'-Wl,-rpath=\$$ORIGIN/lib.%s/' % self.toolset)
        self.WriteList(ldflags, 'LDFLAGS_%s' % configname)
      libraries = spec.get('libraries')
      if libraries:
        # Remove duplicate entries
        libraries = gyp.common.uniquer(libraries)
        # On Mac, framework libraries need to be passed as '-framework Cocoa'.
        if self.flavor == 'mac':
          libraries = [
              '-framework ' + os.path.splitext(os.path.basename(library))[0]
              if library.endswith('.framework') else library
              for library in libraries]
      self.WriteList(libraries, 'LIBS')
      self.WriteLn(
          '%s: GYP_LDFLAGS := $(LDFLAGS_$(BUILDTYPE))' % self.output_binary)
      self.WriteLn('%s: LIBS := $(LIBS)' % self.output_binary)

    postbuilds = []
    if self.flavor == 'mac':
      if has_target_postbuilds:
        postbuilds.append('$(TARGET_POSTBUILDS_$(BUILDTYPE))')
      # Postbuild actions. Like actions, but implicitly depend on the target's
      # output.
      for postbuild in spec.get('postbuilds', []):
        postbuilds.append('echo POSTBUILD\\(%s\\) %s' % (
              self.target, postbuild['postbuild_name']))
        shell_list = postbuild['action']
        # The first element is the command. If it's a relative path, it's
        # a script in the source tree relative to the gyp file and needs to be
        # absolutified. Else, it's in the PATH (e.g. install_name_tool, ln).
        if os.path.sep in shell_list[0]:
          shell_list[0] = self.Absolutify(shell_list[0])

          # "script.sh" -> "./script.sh"
          if not os.path.sep in shell_list[0]:
            shell_list[0] = os.path.join('.', shell_list[0])
        postbuilds.append(gyp.common.EncodePOSIXShellList(shell_list))

    if postbuilds:
      # Write envvars for postbuilds.
      extra_settings = {}

      # CHROMIUM_STRIP_SAVE_FILE is a chromium-specific hack.
      # TODO(thakis): It would be nice to have some general mechanism instead.
      strip_save_file = self.xcode_settings.GetPerTargetSetting(
          'CHROMIUM_STRIP_SAVE_FILE')
      if strip_save_file:
        strip_save_file = self.Absolutify(strip_save_file)
      else:
        # Explicitly clear this out, else a postbuild might pick up an export
        # from an earlier target.
        strip_save_file = ''
      extra_settings['CHROMIUM_STRIP_SAVE_FILE'] = strip_save_file

      self.WriteXcodeEnv(self.output, spec, additional_settings=extra_settings)

      for i in xrange(len(postbuilds)):
        if not postbuilds[i].startswith('$'):
          postbuilds[i] = EscapeShellArgument(postbuilds[i])
      self.WriteLn('%s: builddir := $(abs_builddir)' % self.output)
      self.WriteLn('%s: POSTBUILDS := %s' % (self.output, ' '.join(postbuilds)))

    # A bundle directory depends on its dependencies such as bundle resources
    # and bundle binary. When all dependencies have been built, the bundle
    # needs to be packaged.
    if self.is_mac_bundle:
      # If the framework doesn't contain a binary, then nothing depends
      # on the actions -- make the framework depend on them directly too.
      self.WriteDependencyOnExtraOutputs(self.output, extra_outputs)

      # Bundle dependencies. Note that the code below adds actions to this
      # target, so if you move these two lines, move the lines below as well.
      self.WriteList(bundle_deps, 'BUNDLE_DEPS')
      self.WriteLn('%s: $(BUNDLE_DEPS)' % self.output)

      # After the framework is built, package it. Needs to happen before
      # postbuilds, since postbuilds depend on this.
      if self.type in ('shared_library', 'loadable_module'):
        self.WriteLn('\t@$(call do_cmd,mac_package_framework,,,%s)' %
            self.xcode_settings.GetFrameworkVersion())

      # Bundle postbuilds can depend on the whole bundle, so run them after
      # the bundle is packaged, not already after the bundle binary is done.
      if postbuilds:
        self.WriteLn('\t@$(call do_postbuilds)')
      postbuilds = []  # Don't write postbuilds for target's output.

      # Needed by test/mac/gyptest-rebuild.py.
      self.WriteLn('\t@true  # No-op, used by tests')

      # Since this target depends on binary and resources which are in
      # nested subfolders, the framework directory will be older than
      # its dependencies usually. To prevent this rule from executing
      # on every build (expensive, especially with postbuilds), expliclity
      # update the time on the framework directory.
      self.WriteLn('\t@touch -c %s' % self.output)

    if postbuilds:
      assert not self.is_mac_bundle, ('Postbuilds for bundles should be done '
          'on the bundle, not the binary (target \'%s\')' % self.target)
      assert 'product_dir' not in spec, ('Postbuilds do not work with '
          'custom product_dir')

    if self.type == 'executable':
      self.WriteLn(
          '%s: LD_INPUTS := %s' % (self.output_binary, ' '.join(link_deps)))
      if self.toolset == 'host' and self.flavor == 'android':
        self.WriteDoCmd([self.output_binary], link_deps, 'link_host',
                        part_of_all, postbuilds=postbuilds)
      else:
        self.WriteDoCmd([self.output_binary], link_deps, 'link', part_of_all,
                        postbuilds=postbuilds)

    elif self.type == 'static_library':
      for link_dep in link_deps:
        assert ' ' not in link_dep, (
            "Spaces in alink input filenames not supported (%s)"  % link_dep)
      self.WriteDoCmd([self.output_binary], link_deps, 'alink', part_of_all,
                      postbuilds=postbuilds)
    elif self.type == 'shared_library':
      self.WriteLn(
          '%s: LD_INPUTS := %s' % (self.output_binary, ' '.join(link_deps)))
      self.WriteDoCmd([self.output_binary], link_deps, 'solink', part_of_all,
                      postbuilds=postbuilds)
    elif self.type == 'loadable_module':
      for link_dep in link_deps:
        assert ' ' not in link_dep, (
            "Spaces in module input filenames not supported (%s)"  % link_dep)
      if self.toolset == 'host' and self.flavor == 'android':
        self.WriteDoCmd([self.output_binary], link_deps, 'solink_module_host',
                        part_of_all, postbuilds=postbuilds)
      else:
        self.WriteDoCmd(
            [self.output_binary], link_deps, 'solink_module', part_of_all,
            postbuilds=postbuilds)
    elif self.type == 'none':
      # Write a stamp line.
      self.WriteDoCmd([self.output_binary], deps, 'touch', part_of_all,
                      postbuilds=postbuilds)
    else:
      print "WARNING: no output for", self.type, target

    # Add an alias for each target (if there are any outputs).
    # Installable target aliases are created below.
    if ((self.output and self.output != self.target) and
        (self.type not in self._INSTALLABLE_TARGETS)):
      self.WriteMakeRule([self.target], [self.output],
                         comment='Add target alias', phony = True)
      if part_of_all:
        self.WriteMakeRule(['all'], [self.target],
                           comment = 'Add target alias to "all" target.',
                           phony = True)

    # Add special-case rules for our installable targets.
    # 1) They need to install to the build dir or "product" dir.
    # 2) They get shortcuts for building (e.g. "make chrome").
    # 3) They are part of "make all".
    if self.type in self._INSTALLABLE_TARGETS:
      if self.type == 'shared_library':
        file_desc = 'shared library'
      else:
        file_desc = 'executable'
      install_path = self._InstallableTargetInstallPath()
      installable_deps = [self.output]
      if self.flavor == 'mac' and not 'product_dir' in spec:
        # On mac, products are created in install_path immediately.
        assert install_path == self.output, '%s != %s' % (
            install_path, self.output)

      # Point the target alias to the final binary output.
      self.WriteMakeRule([self.target], [install_path],
                         comment='Add target alias', phony = True)
      if install_path != self.output:
        assert not self.is_mac_bundle  # See comment a few lines above.
        self.WriteDoCmd([install_path], [self.output], 'copy',
                        comment = 'Copy this to the %s output path.' %
                        file_desc, part_of_all=part_of_all)
        installable_deps.append(install_path)
      if self.output != self.alias and self.alias != self.target:
        self.WriteMakeRule([self.alias], installable_deps,
                           comment = 'Short alias for building this %s.' %
                           file_desc, phony = True)
      if part_of_all:
        self.WriteMakeRule(['all'], [install_path],
                           comment = 'Add %s to "all" target.' % file_desc,
                           phony = True)


  def WriteList(self, list, variable=None, prefix='', quoter=QuoteIfNecessary):
    """Write a variable definition that is a list of values.

    E.g. WriteList(['a','b'], 'foo', prefix='blah') writes out
         foo = blaha blahb
    but in a pretty-printed style.
    """
    self.fp.write(variable + " := ")
    if list:
      list = [quoter(prefix + l) for l in list]
      self.fp.write(" \\\n\t".join(list))
    self.fp.write("\n\n")


  def WriteDoCmd(self, outputs, inputs, command, part_of_all, comment=None,
                 postbuilds=False):
    """Write a Makefile rule that uses do_cmd.

    This makes the outputs dependent on the command line that was run,
    as well as support the V= make command line flag.
    """
    suffix = ''
    if postbuilds:
      assert ',' not in command
      suffix = ',,1'  # Tell do_cmd to honor $POSTBUILDS
    self.WriteMakeRule(outputs, inputs,
                       actions = ['$(call do_cmd,%s%s)' % (command, suffix)],
                       comment = comment,
                       force = True)
    # Add our outputs to the list of targets we read depfiles from.
    # all_deps is only used for deps file reading, and for deps files we replace
    # spaces with ? because escaping doesn't work with make's $(sort) and
    # other functions.
    outputs = [ReplaceQuotedSpaces(o) for o in outputs]
    self.WriteLn('all_deps += %s' % ' '.join(outputs))
    self._num_outputs += len(outputs)


  def WriteMakeRule(self, outputs, inputs, actions=None, comment=None,
                    order_only=False, force=False, phony=False,
                    multiple_output_trick=True):
    """Write a Makefile rule, with some extra tricks.

    outputs: a list of outputs for the rule (note: this is not directly
             supported by make; see comments below)
    inputs: a list of inputs for the rule
    actions: a list of shell commands to run for the rule
    comment: a comment to put in the Makefile above the rule (also useful
             for making this Python script's code self-documenting)
    order_only: if true, makes the dependency order-only
    force: if true, include FORCE_DO_CMD as an order-only dep
    phony: if true, the rule does not actually generate the named output, the
           output is just a name to run the rule
    multiple_output_trick: if true (the default), perform tricks such as dummy
           rules to avoid problems with multiple outputs.
    """
    if comment:
      self.WriteLn('# ' + comment)
    if phony:
      self.WriteLn('.PHONY: ' + ' '.join(outputs))
    # TODO(evanm): just make order_only a list of deps instead of these hacks.
    if order_only:
      order_insert = '| '
    else:
      order_insert = ''
    if force:
      force_append = ' FORCE_DO_CMD'
    else:
      force_append = ''
    if actions:
      self.WriteLn("%s: TOOLSET := $(TOOLSET)" % outputs[0])
    self.WriteLn('%s: %s%s%s' % (outputs[0], order_insert, ' '.join(inputs),
                                 force_append))
    if actions:
      for action in actions:
        self.WriteLn('\t%s' % action)
    if multiple_output_trick and len(outputs) > 1:
      # If we have more than one output, a rule like
      #   foo bar: baz
      # that for *each* output we must run the action, potentially
      # in parallel.  That is not what we're trying to write -- what
      # we want is that we run the action once and it generates all
      # the files.
      # http://www.gnu.org/software/hello/manual/automake/Multiple-Outputs.html
      # discusses this problem and has this solution:
      # 1) Write the naive rule that would produce parallel runs of
      # the action.
      # 2) Make the outputs seralized on each other, so we won't start
      # a parallel run until the first run finishes, at which point
      # we'll have generated all the outputs and we're done.
      self.WriteLn('%s: %s' % (' '.join(outputs[1:]), outputs[0]))
      # Add a dummy command to the "extra outputs" rule, otherwise make seems to
      # think these outputs haven't (couldn't have?) changed, and thus doesn't
      # flag them as changed (i.e. include in '$?') when evaluating dependent
      # rules, which in turn causes do_cmd() to skip running dependent commands.
      self.WriteLn('%s: ;' % (' '.join(outputs[1:])))
    self.WriteLn()


  def WriteAndroidNdkModuleRule(self, module_name, all_sources, link_deps):
    """Write a set of LOCAL_XXX definitions for Android NDK.

    These variable definitions will be used by Android NDK but do nothing for
    non-Android applications.

    Arguments:
      module_name: Android NDK module name, which must be unique among all
          module names.
      all_sources: A list of source files (will be filtered by Compilable).
      link_deps: A list of link dependencies, which must be sorted in
          the order from dependencies to dependents.
    """
    if self.type not in ('executable', 'shared_library', 'static_library'):
      return

    self.WriteLn('# Variable definitions for Android applications')
    self.WriteLn('include $(CLEAR_VARS)')
    self.WriteLn('LOCAL_MODULE := ' + module_name)
    self.WriteLn('LOCAL_CFLAGS := $(CFLAGS_$(BUILDTYPE)) '
                 '$(DEFS_$(BUILDTYPE)) '
                 # LOCAL_CFLAGS is applied to both of C and C++.  There is
                 # no way to specify $(CFLAGS_C_$(BUILDTYPE)) only for C
                 # sources.
                 '$(CFLAGS_C_$(BUILDTYPE)) '
                 # $(INCS_$(BUILDTYPE)) includes the prefix '-I' while
                 # LOCAL_C_INCLUDES does not expect it.  So put it in
                 # LOCAL_CFLAGS.
                 '$(INCS_$(BUILDTYPE))')
    # LOCAL_CXXFLAGS is obsolete and LOCAL_CPPFLAGS is preferred.
    self.WriteLn('LOCAL_CPPFLAGS := $(CFLAGS_CC_$(BUILDTYPE))')
    self.WriteLn('LOCAL_C_INCLUDES :=')
    self.WriteLn('LOCAL_LDLIBS := $(LDFLAGS_$(BUILDTYPE)) $(LIBS)')

    # Detect the C++ extension.
    cpp_ext = {'.cc': 0, '.cpp': 0, '.cxx': 0}
    default_cpp_ext = '.cpp'
    for filename in all_sources:
      ext = os.path.splitext(filename)[1]
      if ext in cpp_ext:
        cpp_ext[ext] += 1
        if cpp_ext[ext] > cpp_ext[default_cpp_ext]:
          default_cpp_ext = ext
    self.WriteLn('LOCAL_CPP_EXTENSION := ' + default_cpp_ext)

    self.WriteList(map(self.Absolutify, filter(Compilable, all_sources)),
                   'LOCAL_SRC_FILES')

    # Filter out those which do not match prefix and suffix and produce
    # the resulting list without prefix and suffix.
    def DepsToModules(deps, prefix, suffix):
      modules = []
      for filepath in deps:
        filename = os.path.basename(filepath)
        if filename.startswith(prefix) and filename.endswith(suffix):
          modules.append(filename[len(prefix):-len(suffix)])
      return modules

    # Retrieve the default value of 'SHARED_LIB_SUFFIX'
    params = {'flavor': 'linux'}
    default_variables = {}
    CalculateVariables(default_variables, params)

    self.WriteList(
        DepsToModules(link_deps,
                      generator_default_variables['SHARED_LIB_PREFIX'],
                      default_variables['SHARED_LIB_SUFFIX']),
        'LOCAL_SHARED_LIBRARIES')
    self.WriteList(
        DepsToModules(link_deps,
                      generator_default_variables['STATIC_LIB_PREFIX'],
                      generator_default_variables['STATIC_LIB_SUFFIX']),
        'LOCAL_STATIC_LIBRARIES')

    if self.type == 'executable':
      self.WriteLn('include $(BUILD_EXECUTABLE)')
    elif self.type == 'shared_library':
      self.WriteLn('include $(BUILD_SHARED_LIBRARY)')
    elif self.type == 'static_library':
      self.WriteLn('include $(BUILD_STATIC_LIBRARY)')
    self.WriteLn()


  def WriteLn(self, text=''):
    self.fp.write(text + '\n')


  def GetXcodeEnv(self, spec, target_relative_path=False):
    """Return the environment variables that Xcode would set. See
    http://developer.apple.com/library/mac/#documentation/DeveloperTools/Reference/XcodeBuildSettingRef/1-Build_Setting_Reference/build_setting_ref.html#//apple_ref/doc/uid/TP40003931-CH3-SW153
    for a full list."""
    if self.flavor != 'mac': return {}

    built_products_dir = generator_default_variables['PRODUCT_DIR']
    def StripProductDir(s):
      assert s.startswith(built_products_dir), s
      return s[len(built_products_dir) + 1:]

    product_name = spec.get('product_name', self.output)

    if self._InstallImmediately():
      if product_name.startswith(built_products_dir):
        product_name = StripProductDir(product_name)

    srcroot = self.path
    if target_relative_path:
      built_products_dir = os.path.relpath(built_products_dir, srcroot)
      srcroot = '.'
    # These are filled in on a as-needed basis.
    env = {
      'BUILT_PRODUCTS_DIR' : built_products_dir,
      'CONFIGURATION' : '$(BUILDTYPE)',
      'PRODUCT_NAME' : product_name,
      # See /Developer/Platforms/MacOSX.platform/Developer/Library/Xcode/Specifications/MacOSX\ Product\ Types.xcspec for FULL_PRODUCT_NAME
      'FULL_PRODUCT_NAME' : product_name,
      'SRCROOT' : srcroot,
      # This is not true for static libraries, but currently the env is only
      # written for bundles:
      'TARGET_BUILD_DIR' : built_products_dir,
      'TEMP_DIR' : '$(TMPDIR)',
    }
    if self.type in ('executable', 'shared_library', 'loadable_module'):
      env['EXECUTABLE_NAME'] = os.path.basename(self.output_binary)
    if self.type in (
        'executable', 'static_library', 'shared_library', 'loadable_module'):
      env['EXECUTABLE_PATH'] = self.xcode_settings.GetExecutablePath()
      mach_o_type = self.xcode_settings.GetMachOType()
      if mach_o_type:
        env['MACH_O_TYPE'] = mach_o_type
      env['PRODUCT_TYPE'] = self.xcode_settings.GetProductType()
    if self.is_mac_bundle:
      env['CONTENTS_FOLDER_PATH'] = \
        self.xcode_settings.GetBundleContentsFolderPath()
      env['UNLOCALIZED_RESOURCES_FOLDER_PATH'] = \
          self.xcode_settings.GetBundleResourceFolder()
      env['INFOPLIST_PATH'] = self.xcode_settings.GetBundlePlistPath()
      env['WRAPPER_NAME'] = self.xcode_settings.GetWrapperName()

      # TODO(thakis): Remove this.
      env['EXECUTABLE_PATH'] = QuoteSpaces(env['EXECUTABLE_PATH'])
      env['CONTENTS_FOLDER_PATH'] = QuoteSpaces(env['CONTENTS_FOLDER_PATH'])
      env['INFOPLIST_PATH'] = QuoteSpaces(env['INFOPLIST_PATH'])
      env['WRAPPER_NAME'] = QuoteSpaces(env['WRAPPER_NAME'])

    return env


  def WriteXcodeEnv(self,
                    target,
                    spec,
                    target_relative_path=False,
                    additional_settings={}):
    env = additional_settings
    env.update(self.GetXcodeEnv(spec, target_relative_path))

    # Keys whose values will not have $(builddir) replaced with $(abs_builddir).
    # These have special substitution rules in some cases; see above in
    # GetXcodeEnv() for the full rationale.
    keys_to_not_absolutify = ('PRODUCT_NAME', 'FULL_PRODUCT_NAME')

    # Perform some transformations that are required to mimic Xcode behavior.
    for k in env:
      # Values that are not strings but are, for example, lists or tuples such
      # as LDFLAGS or CFLAGS, should not be written out because they are
      # not needed and it's undefined how multi-valued keys should be written.
      if not isinstance(env[k], str):
        continue

      # For
      #  foo := a\ b
      # the escaped space does the right thing. For
      #  export foo := a\ b
      # it does not -- the backslash is written to the env as literal character.
      # Hence, unescape all spaces here.
      v = env[k].replace(r'\ ', ' ')

      # Xcode works purely with absolute paths. When writing env variables to
      # mimic its usage, replace $(builddir) with $(abs_builddir).
      if k not in keys_to_not_absolutify:
        v = v.replace('$(builddir)', '$(abs_builddir)')

      self.WriteLn('%s: export %s := %s' % (target, k, v))


  def Objectify(self, path):
    """Convert a path to its output directory form."""
    if '$(' in path:
      path = path.replace('$(obj)/', '$(obj).%s/$(TARGET)/' % self.toolset)
      return path
    return '$(obj).%s/$(TARGET)/%s' % (self.toolset, path)


  def Pchify(self, path, lang):
    """Convert a prefix header path to its output directory form."""
    if '$(' in path:
      path = path.replace('$(obj)/', '$(obj).%s/$(TARGET)/pch-%s' %
                          (self.toolset, lang))
      return path
    return '$(obj).%s/$(TARGET)/pch-%s/%s' % (self.toolset, lang, path)


  def Absolutify(self, path):
    """Convert a subdirectory-relative path into a base-relative path.
    Skips over paths that contain variables."""
    if '$(' in path:
      return path
    return os.path.normpath(os.path.join(self.path, path))


  def FixupArgPath(self, arg):
    if '/' in arg or '.h.' in arg:
      return self.Absolutify(arg)
    return arg


  def ExpandInputRoot(self, template, expansion, dirname):
    if '%(INPUT_ROOT)s' not in template and '%(INPUT_DIRNAME)s' not in template:
      return template
    path = template % {
        'INPUT_ROOT': expansion,
        'INPUT_DIRNAME': dirname,
        }
    return path


  def _InstallableTargetInstallPath(self):
    """Returns the location of the final output for an installable target."""
    # Xcode puts shared_library results into PRODUCT_DIR, and some gyp files
    # rely on this. Emulate this behavior for mac.
    if self.type == 'shared_library' and self.flavor != 'mac':
      # Install all shared libs into a common directory (per toolset) for
      # convenient access with LD_LIBRARY_PATH.
      return '$(builddir)/lib.%s/%s' % (self.toolset, self.alias)
    return '$(builddir)/' + self.alias


def WriteAutoRegenerationRule(params, root_makefile, makefile_name,
                              build_files):
  """Write the target to regenerate the Makefile."""
  options = params['options']
  build_files_args = [gyp.common.RelativePath(filename, options.toplevel_dir)
                      for filename in params['build_files_arg']]
  gyp_binary = gyp.common.FixIfRelativePath(params['gyp_binary'],
                                            options.toplevel_dir)
  if not gyp_binary.startswith(os.sep):
    gyp_binary = os.path.join('.', gyp_binary)
  root_makefile.write(
      "quiet_cmd_regen_makefile = ACTION Regenerating $@\n"
      "cmd_regen_makefile = %(cmd)s\n"
      "%(makefile_name)s: %(deps)s\n"
      "\t$(call do_cmd,regen_makefile)\n\n" % {
          'makefile_name': makefile_name,
          'deps': ' '.join(map(Sourceify, build_files)),
          'cmd': gyp.common.EncodePOSIXShellList(
                     [gyp_binary, '-fmake'] +
                     gyp.RegenerateFlags(options) +
                     build_files_args)})


def RunSystemTests(flavor):
  """Run tests against the system to compute default settings for commands.

  Returns:
    dictionary of settings matching the block of command-lines used in
    SHARED_HEADER.  E.g. the dictionary will contain a ARFLAGS.target
    key for the default ARFLAGS for the target ar command.
  """
  # Compute flags used for building static archives.
  # N.B.: this fallback logic should match the logic in SHARED_HEADER.
  # See comment there for more details.
  ar_target = os.environ.get('AR.target', os.environ.get('AR', 'ar'))
  cc_target = os.environ.get('CC.target', os.environ.get('CC', 'cc'))
  arflags_target = 'crs'
  # ar -T enables thin archives on Linux. OS X's ar supports a -T flag, but it
  # does something useless (it limits filenames in the archive to 15 chars).
  if flavor != 'mac' and gyp.system_test.TestArSupportsT(ar_command=ar_target,
                                                         cc_command=cc_target):
    arflags_target = 'crsT'

  ar_host = os.environ.get('AR.host', 'ar')
  cc_host = os.environ.get('CC.host', 'gcc')
  arflags_host = 'crs'
  # It feels redundant to compute this again given that most builds aren't
  # cross-compiles, but due to quirks of history CC.host defaults to 'gcc'
  # while CC.target defaults to 'cc', so the commands really are different
  # even though they're nearly guaranteed to run the same code underneath.
  if flavor != 'mac' and gyp.system_test.TestArSupportsT(ar_command=ar_host,
                                                         cc_command=cc_host):
    arflags_host = 'crsT'

  link_flags = ''
  if gyp.system_test.TestLinkerSupportsThreads(cc_command=cc_target):
    # N.B. we don't test for cross-compilation; as currently written, we
    # don't even use flock when linking in the cross-compile setup!
    # TODO(evan): refactor cross-compilation such that this code can
    # be reused.
    link_flags = '-Wl,--threads -Wl,--thread-count=4'

  # TODO(evan): cache this output.  (But then we'll need to add extra
  # flags to gyp to flush the cache, yuk!  It's fast enough for now to
  # just run it every time.)

  return { 'ARFLAGS.target': arflags_target,
           'ARFLAGS.host': arflags_host,
           'LINK_flags': link_flags }


def CopyTool(flavor, out_path):
  """Finds (mac|sun)_tool.gyp in the gyp directory and copies it
  to |out_path|."""
  prefix = { 'solaris': 'sun', 'mac': 'mac' }.get(flavor, None)
  if not prefix:
    return

  tool_path = os.path.join(out_path, 'gyp-%s-tool' % prefix)
  if os.path.exists(tool_path):
    os.remove(tool_path)

  # Slurp input file.
  source_path = os.path.join(
      os.path.dirname(os.path.abspath(__file__)), '..', '%s_tool.py' % prefix)
  source_file = open(source_path)
  source = source_file.readlines()
  source_file.close()

  # Add header and write it out.
  tool_file = open(tool_path, 'w')
  tool_file.write(
      ''.join([source[0], '# Generated by gyp. Do not edit.\n'] + source[1:]))
  tool_file.close()

  # Make file executable.
  os.chmod(tool_path, 0755)


def GenerateOutput(target_list, target_dicts, data, params):
  options = params['options']
  flavor = GetFlavor(params)
  generator_flags = params.get('generator_flags', {})
  builddir_name = generator_flags.get('output_dir', 'out')
  android_ndk_version = generator_flags.get('android_ndk_version', None)

  def CalculateMakefilePath(build_file, base_name):
    """Determine where to write a Makefile for a given gyp file."""
    # Paths in gyp files are relative to the .gyp file, but we want
    # paths relative to the source root for the master makefile.  Grab
    # the path of the .gyp file as the base to relativize against.
    # E.g. "foo/bar" when we're constructing targets for "foo/bar/baz.gyp".
    base_path = gyp.common.RelativePath(os.path.dirname(build_file),
                                        options.depth)
    # We write the file in the base_path directory.
    output_file = os.path.join(options.depth, base_path, base_name)
    if options.generator_output:
      output_file = os.path.join(options.generator_output, output_file)
    base_path = gyp.common.RelativePath(os.path.dirname(build_file),
                                        options.toplevel_dir)
    return base_path, output_file

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
    global srcdir_prefix
    makefile_path = os.path.join(options.generator_output, makefile_path)
    srcdir = gyp.common.RelativePath(srcdir, options.generator_output)
    srcdir_prefix = '$(srcdir)/'

  flock_command= 'flock'
  header_params = {
      'builddir': builddir_name,
      'default_configuration': default_configuration,
      'flock': flock_command,
      'flock_index': 1,
      'link_commands': LINK_COMMANDS_LINUX,
      'extra_commands': '',
      'srcdir': srcdir,
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
  elif flavor == 'solaris':
    header_params.update({
        'flock': './gyp-sun-tool flock',
        'flock_index': 2,
        'extra_commands': SHARED_HEADER_SUN_COMMANDS,
    })
  elif flavor == 'freebsd':
    header_params.update({
        'flock': 'lockf',
    })
  header_params.update(RunSystemTests(flavor))

  build_file, _, _ = gyp.common.ParseQualifiedTarget(target_list[0])
  make_global_settings_dict = data[build_file].get('make_global_settings', {})
  make_global_settings = ''
  for key, value in make_global_settings_dict:
    if value[0] != '$':
      value = '$(abspath %s)' % value
    if key == 'LINK':
      make_global_settings += '%s ?= %s %s\n' % (flock_command, key, value)
    elif key in ['CC', 'CXX']:
      make_global_settings += (
          'ifneq (,$(filter $(origin %s), undefined default))\n' % key)
      # Let gyp-time envvars win over global settings.
      if key in os.environ:
        value = os.environ[key]
      make_global_settings += '  %s = %s\n' % (key, value)
      make_global_settings += 'endif\n'
    else:
      make_global_settings += '%s ?= %s\n' % (key, value)
  header_params['make_global_settings'] = make_global_settings

  ensure_directory_exists(makefile_path)
  root_makefile = open(makefile_path, 'w')
  root_makefile.write(SHARED_HEADER % header_params)
  # Currently any versions have the same effect, but in future the behavior
  # could be different.
  if android_ndk_version:
    root_makefile.write(
        '# Define LOCAL_PATH for build of Android applications.\n'
        'LOCAL_PATH := $(call my-dir)\n'
        '\n')
  for toolset in toolsets:
    root_makefile.write('TOOLSET := %s\n' % toolset)
    WriteRootHeaderSuffixRules(root_makefile)

  # Put mac_tool next to the root Makefile.
  dest_path = os.path.dirname(makefile_path)
  CopyTool(flavor, dest_path)

  # Find the list of targets that derive from the gyp file(s) being built.
  needed_targets = set()
  for build_file in params['build_files']:
    for target in gyp.common.AllTargets(target_list, target_dicts, build_file):
      needed_targets.add(target)

  num_outputs = 0
  build_files = set()
  include_list = set()
  for qualified_target in target_list:
    build_file, target, toolset = gyp.common.ParseQualifiedTarget(
        qualified_target)

    this_make_global_settings = data[build_file].get('make_global_settings', {})
    assert make_global_settings_dict == this_make_global_settings, (
        "make_global_settings needs to be the same for all targets.")

    build_files.add(gyp.common.RelativePath(build_file, options.toplevel_dir))
    included_files = data[build_file]['included_files']
    for included_file in included_files:
      # The included_files entries are relative to the dir of the build file
      # that included them, so we have to undo that and then make them relative
      # to the root dir.
      relative_include_file = gyp.common.RelativePath(
          gyp.common.UnrelativePath(included_file, build_file),
          options.toplevel_dir)
      abs_include_file = os.path.abspath(relative_include_file)
      # If the include file is from the ~/.gyp dir, we should use absolute path
      # so that relocating the src dir doesn't break the path.
      if (params['home_dot_gyp'] and
          abs_include_file.startswith(params['home_dot_gyp'])):
        build_files.add(abs_include_file)
      else:
        build_files.add(relative_include_file)

    base_path, output_file = CalculateMakefilePath(build_file,
        target + '.' + toolset + options.suffix + '.mk')

    spec = target_dicts[qualified_target]
    configs = spec['configurations']

    # The xcode generator special-cases global xcode_settings and does something
    # that amounts to merging in the global xcode_settings into each local
    # xcode_settings dict.
    if flavor == 'mac':
      global_xcode_settings = data[build_file].get('xcode_settings', {})
      for configname in configs.keys():
        config = configs[configname]
        if 'xcode_settings' in config:
          new_settings = global_xcode_settings.copy()
          new_settings.update(config['xcode_settings'])
          config['xcode_settings'] = new_settings

    writer = MakefileWriter(generator_flags, flavor)
    writer.Write(qualified_target, base_path, output_file, spec, configs,
                 part_of_all=qualified_target in needed_targets)
    num_outputs += writer.NumOutputs()

    # Our root_makefile lives at the source root.  Compute the relative path
    # from there to the output_file for including.
    mkfile_rel_path = gyp.common.RelativePath(output_file,
                                              os.path.dirname(makefile_path))
    include_list.add(mkfile_rel_path)

  # Write out per-gyp (sub-project) Makefiles.
  depth_rel_path = gyp.common.RelativePath(options.depth, os.getcwd())
  for build_file in build_files:
    # The paths in build_files were relativized above, so undo that before
    # testing against the non-relativized items in target_list and before
    # calculating the Makefile path.
    build_file = os.path.join(depth_rel_path, build_file)
    gyp_targets = [target_dicts[target]['target_name'] for target in target_list
                   if target.startswith(build_file) and
                   target in needed_targets]
    # Only generate Makefiles for gyp files with targets.
    if not gyp_targets:
      continue
    base_path, output_file = CalculateMakefilePath(build_file,
        os.path.splitext(os.path.basename(build_file))[0] + '.Makefile')
    makefile_rel_path = gyp.common.RelativePath(os.path.dirname(makefile_path),
                                                os.path.dirname(output_file))
    writer.WriteSubMake(output_file, makefile_rel_path, gyp_targets,
                        builddir_name)


  # Write out the sorted list of includes.
  root_makefile.write('\n')
  for include_file in sorted(include_list):
    # We wrap each .mk include in an if statement so users can tell make to
    # not load a file by setting NO_LOAD.  The below make code says, only
    # load the .mk file if the .mk filename doesn't start with a token in
    # NO_LOAD.
    root_makefile.write(
        "ifeq ($(strip $(foreach prefix,$(NO_LOAD),\\\n"
        "    $(findstring $(join ^,$(prefix)),\\\n"
        "                 $(join ^," + include_file + ")))),)\n")
    root_makefile.write("  include " + include_file + "\n")
    root_makefile.write("endif\n")
  root_makefile.write('\n')

  if generator_flags.get('auto_regeneration', True):
    WriteAutoRegenerationRule(params, root_makefile, makefile_name, build_files)

  # Write the rule to load dependencies.  We batch 1000 files at a time to
  # avoid overflowing the command line.
  all_deps = ""
  for i in range(1001, num_outputs, 1000):
    all_deps += ("""
  ifneq ($(word %(start)d,$(d_files)),)
    $(shell cat $(wordlist %(start)d,%(end)d,$(d_files)) >> $(depsdir)/all.deps)
  endif""" % { 'start': i, 'end': i + 999 })

  # Add a check to make sure we tried to process all the .d files.
  all_deps += """
  ifneq ($(word %(last)d,$(d_files)),)
    $(error Found unprocessed dependency files (gyp didn't generate enough rules!))
  endif
""" % { 'last': ((num_outputs / 1000) + 1) * 1000 + 1 }

  root_makefile.write(SHARED_FOOTER % { 'generate_all_deps': all_deps })

  root_makefile.close()
