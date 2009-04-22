"""SCons.Script.SConscript

This module defines the Python API provided to SConscript and SConstruct
files.

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

__revision__ = "src/engine/SCons/Script/SConscript.py 3842 2008/12/20 22:59:52 scons"

import SCons
import SCons.Action
import SCons.Builder
import SCons.Defaults
import SCons.Environment
import SCons.Errors
import SCons.Node
import SCons.Node.Alias
import SCons.Node.FS
import SCons.Platform
import SCons.SConf
import SCons.Script.Main
import SCons.Tool
import SCons.Util

import os
import os.path
import re
import string
import sys
import traceback
import types
import UserList

# The following variables used to live in this module.  Some
# SConscript files out there may have referred to them directly as
# SCons.Script.SConscript.*.  This is now supported by some special
# handling towards the bottom of the SConscript.__init__.py module.
#Arguments = {}
#ArgList = []
#BuildTargets = TargetList()
#CommandLineTargets = []
#DefaultTargets = []

class SConscriptReturn(Exception):
    pass

launch_dir = os.path.abspath(os.curdir)

GlobalDict = None

# global exports set by Export():
global_exports = {}

# chdir flag
sconscript_chdir = 1

def get_calling_namespaces():
    """Return the locals and globals for the function that called
    into this module in the current call stack."""
    try: 1/0
    except ZeroDivisionError: 
        # Don't start iterating with the current stack-frame to
        # prevent creating reference cycles (f_back is safe).
        frame = sys.exc_info()[2].tb_frame.f_back

    # Find the first frame that *isn't* from this file.  This means
    # that we expect all of the SCons frames that implement an Export()
    # or SConscript() call to be in this file, so that we can identify
    # the first non-Script.SConscript frame as the user's local calling
    # environment, and the locals and globals dictionaries from that
    # frame as the calling namespaces.  See the comment below preceding
    # the DefaultEnvironmentCall block for even more explanation.
    while frame.f_globals.get("__name__") == __name__:
        frame = frame.f_back

    return frame.f_locals, frame.f_globals


def compute_exports(exports):
    """Compute a dictionary of exports given one of the parameters
    to the Export() function or the exports argument to SConscript()."""

    loc, glob = get_calling_namespaces()

    retval = {}
    try:
        for export in exports:
            if SCons.Util.is_Dict(export):
                retval.update(export)
            else:
                try:
                    retval[export] = loc[export]
                except KeyError:
                    retval[export] = glob[export]
    except KeyError, x:
        raise SCons.Errors.UserError, "Export of non-existent variable '%s'"%x

    return retval

class Frame:
    """A frame on the SConstruct/SConscript call stack"""
    def __init__(self, fs, exports, sconscript):
        self.globals = BuildDefaultGlobals()
        self.retval = None
        self.prev_dir = fs.getcwd()
        self.exports = compute_exports(exports)  # exports from the calling SConscript
        # make sure the sconscript attr is a Node.
        if isinstance(sconscript, SCons.Node.Node):
            self.sconscript = sconscript
        elif sconscript == '-':
            self.sconscript = None
        else:
            self.sconscript = fs.File(str(sconscript))

# the SConstruct/SConscript call stack:
call_stack = []

# For documentation on the methods in this file, see the scons man-page

def Return(*vars, **kw):
    retval = []
    try:
        fvars = SCons.Util.flatten(vars)
        for var in fvars:
            for v in string.split(var):
                retval.append(call_stack[-1].globals[v])
    except KeyError, x:
        raise SCons.Errors.UserError, "Return of non-existent variable '%s'"%x

    if len(retval) == 1:
        call_stack[-1].retval = retval[0]
    else:
        call_stack[-1].retval = tuple(retval)

    stop = kw.get('stop', True)

    if stop:
        raise SConscriptReturn


stack_bottom = '% Stack boTTom %' # hard to define a variable w/this name :)

def _SConscript(fs, *files, **kw):
    top = fs.Top
    sd = fs.SConstruct_dir.rdir()
    exports = kw.get('exports', [])

    # evaluate each SConscript file
    results = []
    for fn in files:
        call_stack.append(Frame(fs, exports, fn))
        old_sys_path = sys.path
        try:
            SCons.Script.sconscript_reading = SCons.Script.sconscript_reading + 1
            if fn == "-":
                exec sys.stdin in call_stack[-1].globals
            else:
                if isinstance(fn, SCons.Node.Node):
                    f = fn
                else:
                    f = fs.File(str(fn))
                _file_ = None

                # Change directory to the top of the source
                # tree to make sure the os's cwd and the cwd of
                # fs match so we can open the SConscript.
                fs.chdir(top, change_os_dir=1)
                if f.rexists():
                    _file_ = open(f.rfile().get_abspath(), "r")
                elif f.has_src_builder():
                    # The SConscript file apparently exists in a source
                    # code management system.  Build it, but then clear
                    # the builder so that it doesn't get built *again*
                    # during the actual build phase.
                    f.build()
                    f.built()
                    f.builder_set(None)
                    if f.exists():
                        _file_ = open(f.get_abspath(), "r")
                if _file_:
                    # Chdir to the SConscript directory.  Use a path
                    # name relative to the SConstruct file so that if
                    # we're using the -f option, we're essentially
                    # creating a parallel SConscript directory structure
                    # in our local directory tree.
                    #
                    # XXX This is broken for multiple-repository cases
                    # where the SConstruct and SConscript files might be
                    # in different Repositories.  For now, cross that
                    # bridge when someone comes to it.
                    try:
                        src_dir = kw['src_dir']
                    except KeyError:
                        ldir = fs.Dir(f.dir.get_path(sd))
                    else:
                        ldir = fs.Dir(src_dir)
                        if not ldir.is_under(f.dir):
                            # They specified a source directory, but
                            # it's above the SConscript directory.
                            # Do the sensible thing and just use the
                            # SConcript directory.
                            ldir = fs.Dir(f.dir.get_path(sd))
                    try:
                        fs.chdir(ldir, change_os_dir=sconscript_chdir)
                    except OSError:
                        # There was no local directory, so we should be
                        # able to chdir to the Repository directory.
                        # Note that we do this directly, not through
                        # fs.chdir(), because we still need to
                        # interpret the stuff within the SConscript file
                        # relative to where we are logically.
                        fs.chdir(ldir, change_os_dir=0)
                        # TODO Not sure how to handle src_dir here
                        os.chdir(f.rfile().dir.get_abspath())

                    # Append the SConscript directory to the beginning
                    # of sys.path so Python modules in the SConscript
                    # directory can be easily imported.
                    sys.path = [ f.dir.get_abspath() ] + sys.path

                    # This is the magic line that actually reads up
                    # and executes the stuff in the SConscript file.
                    # The locals for this frame contain the special
                    # bottom-of-the-stack marker so that any
                    # exceptions that occur when processing this
                    # SConscript can base the printed frames at this
                    # level and not show SCons internals as well.
                    call_stack[-1].globals.update({stack_bottom:1})
                    old_file = call_stack[-1].globals.get('__file__')
                    try:
                        del call_stack[-1].globals['__file__']
                    except KeyError:
                        pass
                    try:
                        try:
                            exec _file_ in call_stack[-1].globals
                        except SConscriptReturn:
                            pass
                    finally:
                        if old_file is not None:
                            call_stack[-1].globals.update({__file__:old_file})
                else:
                    SCons.Warnings.warn(SCons.Warnings.MissingSConscriptWarning,
                             "Ignoring missing SConscript '%s'" % f.path)

        finally:
            SCons.Script.sconscript_reading = SCons.Script.sconscript_reading - 1
            sys.path = old_sys_path
            frame = call_stack.pop()
            try:
                fs.chdir(frame.prev_dir, change_os_dir=sconscript_chdir)
            except OSError:
                # There was no local directory, so chdir to the
                # Repository directory.  Like above, we do this
                # directly.
                fs.chdir(frame.prev_dir, change_os_dir=0)
                rdir = frame.prev_dir.rdir()
                rdir._create()  # Make sure there's a directory there.
                try:
                    os.chdir(rdir.get_abspath())
                except OSError, e:
                    # We still couldn't chdir there, so raise the error,
                    # but only if actions are being executed.
                    #
                    # If the -n option was used, the directory would *not*
                    # have been created and we should just carry on and
                    # let things muddle through.  This isn't guaranteed
                    # to work if the SConscript files are reading things
                    # from disk (for example), but it should work well
                    # enough for most configurations.
                    if SCons.Action.execute_actions:
                        raise e

            results.append(frame.retval)

    # if we only have one script, don't return a tuple
    if len(results) == 1:
        return results[0]
    else:
        return tuple(results)

def SConscript_exception(file=sys.stderr):
    """Print an exception stack trace just for the SConscript file(s).
    This will show users who have Python errors where the problem is,
    without cluttering the output with all of the internal calls leading
    up to where we exec the SConscript."""
    exc_type, exc_value, exc_tb = sys.exc_info()
    tb = exc_tb
    while tb and not tb.tb_frame.f_locals.has_key(stack_bottom):
        tb = tb.tb_next
    if not tb:
        # We did not find our exec statement, so this was actually a bug
        # in SCons itself.  Show the whole stack.
        tb = exc_tb
    stack = traceback.extract_tb(tb)
    try:
        type = exc_type.__name__
    except AttributeError:
        type = str(exc_type)
        if type[:11] == "exceptions.":
            type = type[11:]
    file.write('%s: %s:\n' % (type, exc_value))
    for fname, line, func, text in stack:
        file.write('  File "%s", line %d:\n' % (fname, line))
        file.write('    %s\n' % text)

def annotate(node):
    """Annotate a node with the stack frame describing the
    SConscript file and line number that created it."""
    tb = sys.exc_info()[2]
    while tb and not tb.tb_frame.f_locals.has_key(stack_bottom):
        tb = tb.tb_next
    if not tb:
        # We did not find any exec of an SConscript file: what?!
        raise SCons.Errors.InternalError, "could not find SConscript stack frame"
    node.creator = traceback.extract_stack(tb)[0]

# The following line would cause each Node to be annotated using the
# above function.  Unfortunately, this is a *huge* performance hit, so
# leave this disabled until we find a more efficient mechanism.
#SCons.Node.Annotate = annotate

class SConsEnvironment(SCons.Environment.Base):
    """An Environment subclass that contains all of the methods that
    are particular to the wrapper SCons interface and which aren't
    (or shouldn't be) part of the build engine itself.

    Note that not all of the methods of this class have corresponding
    global functions, there are some private methods.
    """

    #
    # Private methods of an SConsEnvironment.
    #
    def _exceeds_version(self, major, minor, v_major, v_minor):
        """Return 1 if 'major' and 'minor' are greater than the version
        in 'v_major' and 'v_minor', and 0 otherwise."""
        return (major > v_major or (major == v_major and minor > v_minor))

    def _get_major_minor_revision(self, version_string):
        """Split a version string into major, minor and (optionally)
        revision parts.

        This is complicated by the fact that a version string can be
        something like 3.2b1."""
        version = string.split(string.split(version_string, ' ')[0], '.')
        v_major = int(version[0])
        v_minor = int(re.match('\d+', version[1]).group())
        if len(version) >= 3:
            v_revision = int(re.match('\d+', version[2]).group())
        else:
            v_revision = 0
        return v_major, v_minor, v_revision

    def _get_SConscript_filenames(self, ls, kw):
        """
        Convert the parameters passed to # SConscript() calls into a list
        of files and export variables.  If the parameters are invalid,
        throws SCons.Errors.UserError. Returns a tuple (l, e) where l
        is a list of SConscript filenames and e is a list of exports.
        """
        exports = []

        if len(ls) == 0:
            try:
                dirs = kw["dirs"]
            except KeyError:
                raise SCons.Errors.UserError, \
                      "Invalid SConscript usage - no parameters"

            if not SCons.Util.is_List(dirs):
                dirs = [ dirs ]
            dirs = map(str, dirs)

            name = kw.get('name', 'SConscript')

            files = map(lambda n, name = name: os.path.join(n, name), dirs)

        elif len(ls) == 1:

            files = ls[0]

        elif len(ls) == 2:

            files   = ls[0]
            exports = self.Split(ls[1])

        else:

            raise SCons.Errors.UserError, \
                  "Invalid SConscript() usage - too many arguments"

        if not SCons.Util.is_List(files):
            files = [ files ]

        if kw.get('exports'):
            exports.extend(self.Split(kw['exports']))

        variant_dir = kw.get('variant_dir') or kw.get('build_dir')
        if variant_dir:
            if len(files) != 1:
                raise SCons.Errors.UserError, \
                    "Invalid SConscript() usage - can only specify one SConscript with a variant_dir"
            duplicate = kw.get('duplicate', 1)
            src_dir = kw.get('src_dir')
            if not src_dir:
                src_dir, fname = os.path.split(str(files[0]))
                files = [os.path.join(str(variant_dir), fname)]
            else:
                if not isinstance(src_dir, SCons.Node.Node):
                    src_dir = self.fs.Dir(src_dir)
                fn = files[0]
                if not isinstance(fn, SCons.Node.Node):
                    fn = self.fs.File(fn)
                if fn.is_under(src_dir):
                    # Get path relative to the source directory.
                    fname = fn.get_path(src_dir)
                    files = [os.path.join(str(variant_dir), fname)]
                else:
                    files = [fn.abspath]
                kw['src_dir'] = variant_dir
            self.fs.VariantDir(variant_dir, src_dir, duplicate)

        return (files, exports)

    #
    # Public methods of an SConsEnvironment.  These get
    # entry points in the global name space so they can be called
    # as global functions.
    #

    def Configure(self, *args, **kw):
        if not SCons.Script.sconscript_reading:
            raise SCons.Errors.UserError, "Calling Configure from Builders is not supported."
        kw['_depth'] = kw.get('_depth', 0) + 1
        return apply(SCons.Environment.Base.Configure, (self,)+args, kw)

    def Default(self, *targets):
        SCons.Script._Set_Default_Targets(self, targets)

    def EnsureSConsVersion(self, major, minor, revision=0):
        """Exit abnormally if the SCons version is not late enough."""
        scons_ver = self._get_major_minor_revision(SCons.__version__)
        if scons_ver < (major, minor, revision):
            if revision:
                scons_ver_string = '%d.%d.%d' % (major, minor, revision)
            else:
                scons_ver_string = '%d.%d' % (major, minor)
            print "SCons %s or greater required, but you have SCons %s" % \
                  (scons_ver_string, SCons.__version__)
            sys.exit(2)

    def EnsurePythonVersion(self, major, minor):
        """Exit abnormally if the Python version is not late enough."""
        try:
            v_major, v_minor, v_micro, release, serial = sys.version_info
            python_ver = (v_major, v_minor)
        except AttributeError:
            python_ver = self._get_major_minor_revision(sys.version)[:2]
        if python_ver < (major, minor):
            v = string.split(sys.version, " ", 1)[0]
            print "Python %d.%d or greater required, but you have Python %s" %(major,minor,v)
            sys.exit(2)

    def Exit(self, value=0):
        sys.exit(value)

    def Export(self, *vars):
        for var in vars:
            global_exports.update(compute_exports(self.Split(var)))

    def GetLaunchDir(self):
        global launch_dir
        return launch_dir

    def GetOption(self, name):
        name = self.subst(name)
        return SCons.Script.Main.GetOption(name)

    def Help(self, text):
        text = self.subst(text, raw=1)
        SCons.Script.HelpFunction(text)

    def Import(self, *vars):
        try:
            frame = call_stack[-1]
            globals = frame.globals
            exports = frame.exports
            for var in vars:
                var = self.Split(var)
                for v in var:
                    if v == '*':
                        globals.update(global_exports)
                        globals.update(exports)
                    else:
                        if exports.has_key(v):
                            globals[v] = exports[v]
                        else:
                            globals[v] = global_exports[v]
        except KeyError,x:
            raise SCons.Errors.UserError, "Import of non-existent variable '%s'"%x

    def SConscript(self, *ls, **kw):
        def subst_element(x, subst=self.subst):
            if SCons.Util.is_List(x):
                x = map(subst, x)
            else:
                x = subst(x)
            return x
        ls = map(subst_element, ls)
        subst_kw = {}
        for key, val in kw.items():
            if SCons.Util.is_String(val):
                val = self.subst(val)
            elif SCons.Util.is_List(val):
                result = []
                for v in val:
                    if SCons.Util.is_String(v):
                        v = self.subst(v)
                    result.append(v)
                val = result
            subst_kw[key] = val

        files, exports = self._get_SConscript_filenames(ls, subst_kw)
        subst_kw['exports'] = exports
        return apply(_SConscript, [self.fs,] + files, subst_kw)

    def SConscriptChdir(self, flag):
        global sconscript_chdir
        sconscript_chdir = flag

    def SetOption(self, name, value):
        name = self.subst(name)
        SCons.Script.Main.SetOption(name, value)

#
#
#
SCons.Environment.Environment = SConsEnvironment

def Configure(*args, **kw):
    if not SCons.Script.sconscript_reading:
        raise SCons.Errors.UserError, "Calling Configure from Builders is not supported."
    kw['_depth'] = 1
    return apply(SCons.SConf.SConf, args, kw)

# It's very important that the DefaultEnvironmentCall() class stay in this
# file, with the get_calling_namespaces() function, the compute_exports()
# function, the Frame class and the SConsEnvironment.Export() method.
# These things make up the calling stack leading up to the actual global
# Export() or SConscript() call that the user issued.  We want to allow
# users to export local variables that they define, like so:
#
#       def func():
#           x = 1
#           Export('x')
#
# To support this, the get_calling_namespaces() function assumes that
# the *first* stack frame that's not from this file is the local frame
# for the Export() or SConscript() call.

_DefaultEnvironmentProxy = None

def get_DefaultEnvironmentProxy():
    global _DefaultEnvironmentProxy
    if not _DefaultEnvironmentProxy:
        default_env = SCons.Defaults.DefaultEnvironment()
        _DefaultEnvironmentProxy = SCons.Environment.NoSubstitutionProxy(default_env)
    return _DefaultEnvironmentProxy

class DefaultEnvironmentCall:
    """A class that implements "global function" calls of
    Environment methods by fetching the specified method from the
    DefaultEnvironment's class.  Note that this uses an intermediate
    proxy class instead of calling the DefaultEnvironment method
    directly so that the proxy can override the subst() method and
    thereby prevent expansion of construction variables (since from
    the user's point of view this was called as a global function,
    with no associated construction environment)."""
    def __init__(self, method_name, subst=0):
        self.method_name = method_name
        if subst:
            self.factory = SCons.Defaults.DefaultEnvironment
        else:
            self.factory = get_DefaultEnvironmentProxy
    def __call__(self, *args, **kw):
        env = self.factory()
        method = getattr(env, self.method_name)
        return apply(method, args, kw)


def BuildDefaultGlobals():
    """
    Create a dictionary containing all the default globals for
    SConstruct and SConscript files.
    """

    global GlobalDict
    if GlobalDict is None:
        GlobalDict = {}

        import SCons.Script
        d = SCons.Script.__dict__
        def not_a_module(m, d=d, mtype=type(SCons.Script)):
             return type(d[m]) != mtype
        for m in filter(not_a_module, dir(SCons.Script)):
             GlobalDict[m] = d[m]

    return GlobalDict.copy()
