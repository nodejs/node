"""SCons.Defaults

Builders and other things for the local site.  Here's where we'll
duplicate the functionality of autoconf until we move it into the
installation procedure or use something like qmconf.

The code that reads the registry to find MSVC components was borrowed
from distutils.msvccompiler.

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

__revision__ = "src/engine/SCons/Defaults.py 3842 2008/12/20 22:59:52 scons"



import os
import os.path
import shutil
import stat
import string
import time
import types
import sys

import SCons.Action
import SCons.Builder
import SCons.CacheDir
import SCons.Environment
import SCons.PathList
import SCons.Subst
import SCons.Tool

# A placeholder for a default Environment (for fetching source files
# from source code management systems and the like).  This must be
# initialized later, after the top-level directory is set by the calling
# interface.
_default_env = None

# Lazily instantiate the default environment so the overhead of creating
# it doesn't apply when it's not needed.
def _fetch_DefaultEnvironment(*args, **kw):
    """
    Returns the already-created default construction environment.
    """
    global _default_env
    return _default_env

def DefaultEnvironment(*args, **kw):
    """
    Initial public entry point for creating the default construction
    Environment.

    After creating the environment, we overwrite our name
    (DefaultEnvironment) with the _fetch_DefaultEnvironment() function,
    which more efficiently returns the initialized default construction
    environment without checking for its existence.

    (This function still exists with its _default_check because someone
    else (*cough* Script/__init__.py *cough*) may keep a reference
    to this function.  So we can't use the fully functional idiom of
    having the name originally be a something that *only* creates the
    construction environment and then overwrites the name.)
    """
    global _default_env
    if not _default_env:
        import SCons.Util
        _default_env = apply(SCons.Environment.Environment, args, kw)
        if SCons.Util.md5:
            _default_env.Decider('MD5')
        else:
            _default_env.Decider('timestamp-match')
        global DefaultEnvironment
        DefaultEnvironment = _fetch_DefaultEnvironment
        _default_env._CacheDir_path = None
    return _default_env

# Emitters for setting the shared attribute on object files,
# and an action for checking that all of the source files
# going into a shared library are, in fact, shared.
def StaticObjectEmitter(target, source, env):
    for tgt in target:
        tgt.attributes.shared = None
    return (target, source)

def SharedObjectEmitter(target, source, env):
    for tgt in target:
        tgt.attributes.shared = 1
    return (target, source)

def SharedFlagChecker(source, target, env):
    same = env.subst('$STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME')
    if same == '0' or same == '' or same == 'False':
        for src in source:
            try:
                shared = src.attributes.shared
            except AttributeError:
                shared = None
            if not shared:
                raise SCons.Errors.UserError, "Source file: %s is static and is not compatible with shared target: %s" % (src, target[0])

SharedCheck = SCons.Action.Action(SharedFlagChecker, None)

# Some people were using these variable name before we made
# SourceFileScanner part of the public interface.  Don't break their
# SConscript files until we've given them some fair warning and a
# transition period.
CScan = SCons.Tool.CScanner
DScan = SCons.Tool.DScanner
LaTeXScan = SCons.Tool.LaTeXScanner
ObjSourceScan = SCons.Tool.SourceFileScanner
ProgScan = SCons.Tool.ProgramScanner

# These aren't really tool scanners, so they don't quite belong with
# the rest of those in Tool/__init__.py, but I'm not sure where else
# they should go.  Leave them here for now.
import SCons.Scanner.Dir
DirScanner = SCons.Scanner.Dir.DirScanner()
DirEntryScanner = SCons.Scanner.Dir.DirEntryScanner()

# Actions for common languages.
CAction = SCons.Action.Action("$CCCOM", "$CCCOMSTR")
ShCAction = SCons.Action.Action("$SHCCCOM", "$SHCCCOMSTR")
CXXAction = SCons.Action.Action("$CXXCOM", "$CXXCOMSTR")
ShCXXAction = SCons.Action.Action("$SHCXXCOM", "$SHCXXCOMSTR")

ASAction = SCons.Action.Action("$ASCOM", "$ASCOMSTR")
ASPPAction = SCons.Action.Action("$ASPPCOM", "$ASPPCOMSTR")

LinkAction = SCons.Action.Action("$LINKCOM", "$LINKCOMSTR")
ShLinkAction = SCons.Action.Action("$SHLINKCOM", "$SHLINKCOMSTR")

LdModuleLinkAction = SCons.Action.Action("$LDMODULECOM", "$LDMODULECOMSTR")

# Common tasks that we allow users to perform in platform-independent
# ways by creating ActionFactory instances.
ActionFactory = SCons.Action.ActionFactory

def get_paths_str(dest):
    # If dest is a list, we need to manually call str() on each element
    if SCons.Util.is_List(dest):
        elem_strs = []
        for element in dest:
            elem_strs.append('"' + str(element) + '"')
        return '[' + string.join(elem_strs, ', ') + ']'
    else:
        return '"' + str(dest) + '"'

def chmod_func(dest, mode):
    SCons.Node.FS.invalidate_node_memos(dest)
    if not SCons.Util.is_List(dest):
        dest = [dest]
    for element in dest:
        os.chmod(str(element), mode)

def chmod_strfunc(dest, mode):
    return 'Chmod(%s, 0%o)' % (get_paths_str(dest), mode)

Chmod = ActionFactory(chmod_func, chmod_strfunc)

def copy_func(dest, src):
    SCons.Node.FS.invalidate_node_memos(dest)
    if SCons.Util.is_List(src) and os.path.isdir(dest):
        for file in src:
            shutil.copy2(file, dest)
        return 0
    elif os.path.isfile(src):
        return shutil.copy2(src, dest)
    else:
        return shutil.copytree(src, dest, 1)

Copy = ActionFactory(copy_func,
                     lambda dest, src: 'Copy("%s", "%s")' % (dest, src),
                     convert=str)

def delete_func(dest, must_exist=0):
    SCons.Node.FS.invalidate_node_memos(dest)
    if not SCons.Util.is_List(dest):
        dest = [dest]
    for entry in dest:
        entry = str(entry)
        if not must_exist and not os.path.exists(entry):
            continue
        if not os.path.exists(entry) or os.path.isfile(entry):
            os.unlink(entry)
            continue
        else:
            shutil.rmtree(entry, 1)
            continue

def delete_strfunc(dest, must_exist=0):
    return 'Delete(%s)' % get_paths_str(dest)

Delete = ActionFactory(delete_func, delete_strfunc)

def mkdir_func(dest):
    SCons.Node.FS.invalidate_node_memos(dest)
    if not SCons.Util.is_List(dest):
        dest = [dest]
    for entry in dest:
        os.makedirs(str(entry))

Mkdir = ActionFactory(mkdir_func,
                      lambda dir: 'Mkdir(%s)' % get_paths_str(dir))

def move_func(dest, src):
    SCons.Node.FS.invalidate_node_memos(dest)
    SCons.Node.FS.invalidate_node_memos(src)
    os.rename(src, dest)

Move = ActionFactory(move_func,
                     lambda dest, src: 'Move("%s", "%s")' % (dest, src),
                     convert=str)

def touch_func(dest):
    SCons.Node.FS.invalidate_node_memos(dest)
    if not SCons.Util.is_List(dest):
        dest = [dest]
    for file in dest:
        file = str(file)
        mtime = int(time.time())
        if os.path.exists(file):
            atime = os.path.getatime(file)
        else:
            open(file, 'w')
            atime = mtime
        os.utime(file, (atime, mtime))

Touch = ActionFactory(touch_func,
                      lambda file: 'Touch(%s)' % get_paths_str(file))

# Internal utility functions

def _concat(prefix, list, suffix, env, f=lambda x: x, target=None, source=None):
    """
    Creates a new list from 'list' by first interpolating each element
    in the list using the 'env' dictionary and then calling f on the
    list, and finally calling _concat_ixes to concatenate 'prefix' and
    'suffix' onto each element of the list.
    """
    if not list:
        return list

    l = f(SCons.PathList.PathList(list).subst_path(env, target, source))
    if not l is None:
        list = l

    return _concat_ixes(prefix, list, suffix, env)

def _concat_ixes(prefix, list, suffix, env):
    """
    Creates a new list from 'list' by concatenating the 'prefix' and
    'suffix' arguments onto each element of the list.  A trailing space
    on 'prefix' or leading space on 'suffix' will cause them to be put
    into separate list elements rather than being concatenated.
    """

    result = []

    # ensure that prefix and suffix are strings
    prefix = str(env.subst(prefix, SCons.Subst.SUBST_RAW))
    suffix = str(env.subst(suffix, SCons.Subst.SUBST_RAW))

    for x in list:
        if isinstance(x, SCons.Node.FS.File):
            result.append(x)
            continue
        x = str(x)
        if x:

            if prefix:
                if prefix[-1] == ' ':
                    result.append(prefix[:-1])
                elif x[:len(prefix)] != prefix:
                    x = prefix + x

            result.append(x)

            if suffix:
                if suffix[0] == ' ':
                    result.append(suffix[1:])
                elif x[-len(suffix):] != suffix:
                    result[-1] = result[-1]+suffix

    return result

def _stripixes(prefix, list, suffix, stripprefixes, stripsuffixes, env, c=None):
    """
    This is a wrapper around _concat()/_concat_ixes() that checks for the
    existence of prefixes or suffixes on list elements and strips them
    where it finds them.  This is used by tools (like the GNU linker)
    that need to turn something like 'libfoo.a' into '-lfoo'.
    """
    
    if not list:
        return list

    if not callable(c):
        env_c = env['_concat']
        if env_c != _concat and callable(env_c):
            # There's a custom _concat() method in the construction
            # environment, and we've allowed people to set that in
            # the past (see test/custom-concat.py), so preserve the
            # backwards compatibility.
            c = env_c
        else:
            c = _concat_ixes
    
    stripprefixes = map(env.subst, SCons.Util.flatten(stripprefixes))
    stripsuffixes = map(env.subst, SCons.Util.flatten(stripsuffixes))

    stripped = []
    for l in SCons.PathList.PathList(list).subst_path(env, None, None):
        if isinstance(l, SCons.Node.FS.File):
            stripped.append(l)
            continue

        if not SCons.Util.is_String(l):
            l = str(l)

        for stripprefix in stripprefixes:
            lsp = len(stripprefix)
            if l[:lsp] == stripprefix:
                l = l[lsp:]
                # Do not strip more than one prefix
                break

        for stripsuffix in stripsuffixes:
            lss = len(stripsuffix)
            if l[-lss:] == stripsuffix:
                l = l[:-lss]
                # Do not strip more than one suffix
                break

        stripped.append(l)

    return c(prefix, stripped, suffix, env)

def _defines(prefix, defs, suffix, env, c=_concat_ixes):
    """A wrapper around _concat_ixes that turns a list or string
    into a list of C preprocessor command-line definitions.
    """
    if SCons.Util.is_List(defs):
        l = []
        for d in defs:
            if SCons.Util.is_List(d) or type(d) is types.TupleType:
                l.append(str(d[0]) + '=' + str(d[1]))
            else:
                l.append(str(d))
    elif SCons.Util.is_Dict(defs):
        # The items in a dictionary are stored in random order, but
        # if the order of the command-line options changes from
        # invocation to invocation, then the signature of the command
        # line will change and we'll get random unnecessary rebuilds.
        # Consequently, we have to sort the keys to ensure a
        # consistent order...
        l = []
        keys = defs.keys()
        keys.sort()
        for k in keys:
            v = defs[k]
            if v is None:
                l.append(str(k))
            else:
                l.append(str(k) + '=' + str(v))
    else:
        l = [str(defs)]
    return c(prefix, env.subst_path(l), suffix, env)
    
class NullCmdGenerator:
    """This is a callable class that can be used in place of other
    command generators if you don't want them to do anything.

    The __call__ method for this class simply returns the thing
    you instantiated it with.

    Example usage:
    env["DO_NOTHING"] = NullCmdGenerator
    env["LINKCOM"] = "${DO_NOTHING('$LINK $SOURCES $TARGET')}"
    """

    def __init__(self, cmd):
        self.cmd = cmd

    def __call__(self, target, source, env, for_signature=None):
        return self.cmd

class Variable_Method_Caller:
    """A class for finding a construction variable on the stack and
    calling one of its methods.

    We use this to support "construction variables" in our string
    eval()s that actually stand in for methods--specifically, use
    of "RDirs" in call to _concat that should actually execute the
    "TARGET.RDirs" method.  (We used to support this by creating a little
    "build dictionary" that mapped RDirs to the method, but this got in
    the way of Memoizing construction environments, because we had to
    create new environment objects to hold the variables.)
    """
    def __init__(self, variable, method):
        self.variable = variable
        self.method = method
    def __call__(self, *args, **kw):
        try: 1/0
        except ZeroDivisionError: 
            # Don't start iterating with the current stack-frame to
            # prevent creating reference cycles (f_back is safe).
            frame = sys.exc_info()[2].tb_frame.f_back
        variable = self.variable
        while frame:
            if frame.f_locals.has_key(variable):
                v = frame.f_locals[variable]
                if v:
                    method = getattr(v, self.method)
                    return apply(method, args, kw)
            frame = frame.f_back
        return None

ConstructionEnvironment = {
    'BUILDERS'      : {},
    'SCANNERS'      : [],
    'CONFIGUREDIR'  : '#/.sconf_temp',
    'CONFIGURELOG'  : '#/config.log',
    'CPPSUFFIXES'   : SCons.Tool.CSuffixes,
    'DSUFFIXES'     : SCons.Tool.DSuffixes,
    'ENV'           : {},
    'IDLSUFFIXES'   : SCons.Tool.IDLSuffixes,
    'LATEXSUFFIXES' : SCons.Tool.LaTeXSuffixes,
    '_concat'       : _concat,
    '_defines'      : _defines,
    '_stripixes'    : _stripixes,
    '_LIBFLAGS'     : '${_concat(LIBLINKPREFIX, LIBS, LIBLINKSUFFIX, __env__)}',
    '_LIBDIRFLAGS'  : '$( ${_concat(LIBDIRPREFIX, LIBPATH, LIBDIRSUFFIX, __env__, RDirs, TARGET, SOURCE)} $)',
    '_CPPINCFLAGS'  : '$( ${_concat(INCPREFIX, CPPPATH, INCSUFFIX, __env__, RDirs, TARGET, SOURCE)} $)',
    '_CPPDEFFLAGS'  : '${_defines(CPPDEFPREFIX, CPPDEFINES, CPPDEFSUFFIX, __env__)}',
    'TEMPFILE'      : NullCmdGenerator,
    'Dir'           : Variable_Method_Caller('TARGET', 'Dir'),
    'Dirs'          : Variable_Method_Caller('TARGET', 'Dirs'),
    'File'          : Variable_Method_Caller('TARGET', 'File'),
    'RDirs'         : Variable_Method_Caller('TARGET', 'RDirs'),
}
