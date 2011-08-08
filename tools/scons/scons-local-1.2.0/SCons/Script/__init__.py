"""SCons.Script

This file implements the main() function used by the scons script.

Architecturally, this *is* the scons script, and will likely only be
called from the external "scons" wrapper.  Consequently, anything here
should not be, or be considered, part of the build engine.  If it's
something that we expect other software to want to use, it should go in
some other module.  If it's specific to the "scons" script invocation,
it goes here.

"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Script/__init__.py 3842 2008/12/20 22:59:52 scons"

import time
start_time = time.time()

import os
import string
import sys
import UserList

# Special chicken-and-egg handling of the "--debug=memoizer" flag:
#
# SCons.Memoize contains a metaclass implementation that affects how
# the other classes are instantiated.  The Memoizer may add shim methods
# to classes that have methods that cache computed values in order to
# count and report the hits and misses.
#
# If we wait to enable the Memoization until after we've parsed the
# command line options normally, it will be too late, because the Memoizer
# will have already analyzed the classes that it's Memoizing and decided
# to not add the shims.  So we use a special-case, up-front check for
# the "--debug=memoizer" flag and enable Memoizer before we import any
# of the other modules that use it.

_args = sys.argv + string.split(os.environ.get('SCONSFLAGS', ''))
if "--debug=memoizer" in _args:
    import SCons.Memoize
    import SCons.Warnings
    try:
        SCons.Memoize.EnableMemoization()
    except SCons.Warnings.Warning:
        # Some warning was thrown (inability to --debug=memoizer on
        # Python 1.5.2 because it doesn't have metaclasses).  Arrange
        # for it to be displayed or not after warnings are configured.
        import Main
        exc_type, exc_value, tb = sys.exc_info()
        Main.delayed_warnings.append((exc_type, exc_value))
del _args

import SCons.Action
import SCons.Builder
import SCons.Environment
import SCons.Node.FS
import SCons.Options
import SCons.Platform
import SCons.Scanner
import SCons.SConf
import SCons.Subst
import SCons.Tool
import SCons.Util
import SCons.Variables
import SCons.Defaults

import Main

main                    = Main.main

# The following are global class definitions and variables that used to
# live directly in this module back before 0.96.90, when it contained
# a lot of code.  Some SConscript files in widely-distributed packages
# (Blender is the specific example) actually reached into SCons.Script
# directly to use some of these.  Rather than break those SConscript
# files, we're going to propagate these names into the SCons.Script
# namespace here.
#
# Some of these are commented out because it's *really* unlikely anyone
# used them, but we're going to leave the comment here to try to make
# it obvious what to do if the situation arises.
BuildTask               = Main.BuildTask
CleanTask               = Main.CleanTask
QuestionTask            = Main.QuestionTask
#PrintHelp               = Main.PrintHelp
#SConscriptSettableOptions = Main.SConscriptSettableOptions

AddOption               = Main.AddOption
GetOption               = Main.GetOption
SetOption               = Main.SetOption
Progress                = Main.Progress
GetBuildFailures        = Main.GetBuildFailures

#keep_going_on_error     = Main.keep_going_on_error
#print_dtree             = Main.print_dtree
#print_explanations      = Main.print_explanations
#print_includes          = Main.print_includes
#print_objects           = Main.print_objects
#print_time              = Main.print_time
#print_tree              = Main.print_tree
#memory_stats            = Main.memory_stats
#ignore_errors           = Main.ignore_errors
#sconscript_time         = Main.sconscript_time
#command_time            = Main.command_time
#exit_status             = Main.exit_status
#profiling               = Main.profiling
#repositories            = Main.repositories

#
import SConscript
_SConscript = SConscript

call_stack              = _SConscript.call_stack

#
Action                  = SCons.Action.Action
AddMethod               = SCons.Util.AddMethod
AllowSubstExceptions    = SCons.Subst.SetAllowableExceptions
Builder                 = SCons.Builder.Builder
Configure               = _SConscript.Configure
Environment             = SCons.Environment.Environment
#OptParser               = SCons.SConsOptions.OptParser
FindPathDirs            = SCons.Scanner.FindPathDirs
Platform                = SCons.Platform.Platform
Return                  = _SConscript.Return
Scanner                 = SCons.Scanner.Base
Tool                    = SCons.Tool.Tool
WhereIs                 = SCons.Util.WhereIs

#
BoolVariable            = SCons.Variables.BoolVariable
EnumVariable            = SCons.Variables.EnumVariable
ListVariable            = SCons.Variables.ListVariable
PackageVariable         = SCons.Variables.PackageVariable
PathVariable            = SCons.Variables.PathVariable

# Deprecated names that will go away some day.
BoolOption              = SCons.Options.BoolOption
EnumOption              = SCons.Options.EnumOption
ListOption              = SCons.Options.ListOption
PackageOption           = SCons.Options.PackageOption
PathOption              = SCons.Options.PathOption

# Action factories.
Chmod                   = SCons.Defaults.Chmod
Copy                    = SCons.Defaults.Copy
Delete                  = SCons.Defaults.Delete
Mkdir                   = SCons.Defaults.Mkdir
Move                    = SCons.Defaults.Move
Touch                   = SCons.Defaults.Touch

# Pre-made, public scanners.
CScanner                = SCons.Tool.CScanner
DScanner                = SCons.Tool.DScanner
DirScanner              = SCons.Defaults.DirScanner
ProgramScanner          = SCons.Tool.ProgramScanner
SourceFileScanner       = SCons.Tool.SourceFileScanner

# Functions we might still convert to Environment methods.
CScan                   = SCons.Defaults.CScan
DefaultEnvironment      = SCons.Defaults.DefaultEnvironment

# Other variables we provide.
class TargetList(UserList.UserList):
    def _do_nothing(self, *args, **kw):
        pass
    def _add_Default(self, list):
        self.extend(list)
    def _clear(self):
        del self[:]

ARGUMENTS               = {}
ARGLIST                 = []
BUILD_TARGETS           = TargetList()
COMMAND_LINE_TARGETS    = []
DEFAULT_TARGETS         = []

# BUILD_TARGETS can be modified in the SConscript files.  If so, we
# want to treat the modified BUILD_TARGETS list as if they specified
# targets on the command line.  To do that, though, we need to know if
# BUILD_TARGETS was modified through "official" APIs or by hand.  We do
# this by updating two lists in parallel, the documented BUILD_TARGETS
# list, above, and this internal _build_plus_default targets list which
# should only have "official" API changes.  Then Script/Main.py can
# compare these two afterwards to figure out if the user added their
# own targets to BUILD_TARGETS.
_build_plus_default = TargetList()

def _Add_Arguments(alist):
    for arg in alist:
        a, b = string.split(arg, '=', 1)
        ARGUMENTS[a] = b
        ARGLIST.append((a, b))

def _Add_Targets(tlist):
    if tlist:
        COMMAND_LINE_TARGETS.extend(tlist)
        BUILD_TARGETS.extend(tlist)
        BUILD_TARGETS._add_Default = BUILD_TARGETS._do_nothing
        BUILD_TARGETS._clear = BUILD_TARGETS._do_nothing
        _build_plus_default.extend(tlist)
        _build_plus_default._add_Default = _build_plus_default._do_nothing
        _build_plus_default._clear = _build_plus_default._do_nothing

def _Set_Default_Targets_Has_Been_Called(d, fs):
    return DEFAULT_TARGETS

def _Set_Default_Targets_Has_Not_Been_Called(d, fs):
    if d is None:
        d = [fs.Dir('.')]
    return d

_Get_Default_Targets = _Set_Default_Targets_Has_Not_Been_Called

def _Set_Default_Targets(env, tlist):
    global DEFAULT_TARGETS
    global _Get_Default_Targets
    _Get_Default_Targets = _Set_Default_Targets_Has_Been_Called
    for t in tlist:
        if t is None:
            # Delete the elements from the list in-place, don't
            # reassign an empty list to DEFAULT_TARGETS, so that the
            # variables will still point to the same object we point to.
            del DEFAULT_TARGETS[:]
            BUILD_TARGETS._clear()
            _build_plus_default._clear()
        elif isinstance(t, SCons.Node.Node):
            DEFAULT_TARGETS.append(t)
            BUILD_TARGETS._add_Default([t])
            _build_plus_default._add_Default([t])
        else:
            nodes = env.arg2nodes(t, env.fs.Entry)
            DEFAULT_TARGETS.extend(nodes)
            BUILD_TARGETS._add_Default(nodes)
            _build_plus_default._add_Default(nodes)

#
help_text = None

def HelpFunction(text):
    global help_text
    if SCons.Script.help_text is None:
        SCons.Script.help_text = text
    else:
        help_text = help_text + text

#
# Will be non-zero if we are reading an SConscript file.
sconscript_reading = 0

#
def Variables(files=[], args=ARGUMENTS):
    return SCons.Variables.Variables(files, args)

def Options(files=[], args=ARGUMENTS):
    return SCons.Options.Options(files, args)

# The list of global functions to add to the SConscript name space
# that end up calling corresponding methods or Builders in the
# DefaultEnvironment().
GlobalDefaultEnvironmentFunctions = [
    # Methods from the SConsEnvironment class, above.
    'Default',
    'EnsurePythonVersion',
    'EnsureSConsVersion',
    'Exit',
    'Export',
    'GetLaunchDir',
    'Help',
    'Import',
    #'SConscript', is handled separately, below.
    'SConscriptChdir',

    # Methods from the Environment.Base class.
    'AddPostAction',
    'AddPreAction',
    'Alias',
    'AlwaysBuild',
    'BuildDir',
    'CacheDir',
    'Clean',
    #The Command() method is handled separately, below.
    'Decider',
    'Depends',
    'Dir',
    'NoClean',
    'NoCache',
    'Entry',
    'Execute',
    'File',
    'FindFile',
    'FindInstalledFiles',
    'FindSourceFiles',
    'Flatten',
    'GetBuildPath',
    'Glob',
    'Ignore',
    'Install',
    'InstallAs',
    'Literal',
    'Local',
    'ParseDepends',
    'Precious',
    'Repository',
    'Requires',
    'SConsignFile',
    'SideEffect',
    'SourceCode',
    'SourceSignatures',
    'Split',
    'Tag',
    'TargetSignatures',
    'Value',
    'VariantDir',
]

GlobalDefaultBuilders = [
    # Supported builders.
    'CFile',
    'CXXFile',
    'DVI',
    'Jar',
    'Java',
    'JavaH',
    'Library',
    'M4',
    'MSVSProject',
    'Object',
    'PCH',
    'PDF',
    'PostScript',
    'Program',
    'RES',
    'RMIC',
    'SharedLibrary',
    'SharedObject',
    'StaticLibrary',
    'StaticObject',
    'Tar',
    'TypeLibrary',
    'Zip',
    'Package',
]

for name in GlobalDefaultEnvironmentFunctions + GlobalDefaultBuilders:
    exec "%s = _SConscript.DefaultEnvironmentCall(%s)" % (name, repr(name))
del name

# There are a handful of variables that used to live in the
# Script/SConscript.py module that some SConscript files out there were
# accessing directly as SCons.Script.SConscript.*.  The problem is that
# "SConscript" in this namespace is no longer a module, it's a global
# function call--or more precisely, an object that implements a global
# function call through the default Environment.  Nevertheless, we can
# maintain backwards compatibility for SConscripts that were reaching in
# this way by hanging some attributes off the "SConscript" object here.
SConscript = _SConscript.DefaultEnvironmentCall('SConscript')

# Make SConscript look enough like the module it used to be so
# that pychecker doesn't barf.
SConscript.__name__ = 'SConscript'

SConscript.Arguments = ARGUMENTS
SConscript.ArgList = ARGLIST
SConscript.BuildTargets = BUILD_TARGETS
SConscript.CommandLineTargets = COMMAND_LINE_TARGETS
SConscript.DefaultTargets = DEFAULT_TARGETS

# The global Command() function must be handled differently than the
# global functions for other construction environment methods because
# we want people to be able to use Actions that must expand $TARGET
# and $SOURCE later, when (and if) the Action is invoked to build
# the target(s).  We do this with the subst=1 argument, which creates
# a DefaultEnvironmentCall instance that wraps up a normal default
# construction environment that performs variable substitution, not a
# proxy that doesn't.
#
# There's a flaw here, though, because any other $-variables on a command
# line will *also* be expanded, each to a null string, but that should
# only be a problem in the unusual case where someone was passing a '$'
# on a command line and *expected* the $ to get through to the shell
# because they were calling Command() and not env.Command()...  This is
# unlikely enough that we're going to leave this as is and cross that
# bridge if someone actually comes to it.
Command = _SConscript.DefaultEnvironmentCall('Command', subst=1)
