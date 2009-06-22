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

__revision__ = "src/engine/SCons/Script/Main.py 3842 2008/12/20 22:59:52 scons"

import os
import os.path
import string
import sys
import time
import traceback

# Strip the script directory from sys.path() so on case-insensitive
# (Windows) systems Python doesn't think that the "scons" script is the
# "SCons" package.  Replace it with our own version directory so, if
# if they're there, we pick up the right version of the build engine
# modules.
#sys.path = [os.path.join(sys.prefix,
#                         'lib',
#                         'scons-%d' % SCons.__version__)] + sys.path[1:]

import SCons.CacheDir
import SCons.Debug
import SCons.Defaults
import SCons.Environment
import SCons.Errors
import SCons.Job
import SCons.Node
import SCons.Node.FS
import SCons.SConf
import SCons.Script
import SCons.Taskmaster
import SCons.Util
import SCons.Warnings

import SCons.Script.Interactive

def fetch_win32_parallel_msg():
    # A subsidiary function that exists solely to isolate this import
    # so we don't have to pull it in on all platforms, and so that an
    # in-line "import" statement in the _main() function below doesn't
    # cause warnings about local names shadowing use of the 'SCons'
    # globl in nest scopes and UnboundLocalErrors and the like in some
    # versions (2.1) of Python.
    import SCons.Platform.win32
    return SCons.Platform.win32.parallel_msg

#

class SConsPrintHelpException(Exception):
    pass

display = SCons.Util.display
progress_display = SCons.Util.DisplayEngine()

first_command_start = None
last_command_end = None

class Progressor:
    prev = ''
    count = 0
    target_string = '$TARGET'

    def __init__(self, obj, interval=1, file=None, overwrite=False):
        if file is None:
            file = sys.stdout

        self.obj = obj
        self.file = file
        self.interval = interval
        self.overwrite = overwrite

        if callable(obj):
            self.func = obj
        elif SCons.Util.is_List(obj):
            self.func = self.spinner
        elif string.find(obj, self.target_string) != -1:
            self.func = self.replace_string
        else:
            self.func = self.string

    def write(self, s):
        self.file.write(s)
        self.file.flush()
        self.prev = s

    def erase_previous(self):
        if self.prev:
            length = len(self.prev)
            if self.prev[-1] in ('\n', '\r'):
                length = length - 1
            self.write(' ' * length + '\r')
            self.prev = ''

    def spinner(self, node):
        self.write(self.obj[self.count % len(self.obj)])

    def string(self, node):
        self.write(self.obj)

    def replace_string(self, node):
        self.write(string.replace(self.obj, self.target_string, str(node)))

    def __call__(self, node):
        self.count = self.count + 1
        if (self.count % self.interval) == 0:
            if self.overwrite:
                self.erase_previous()
            self.func(node)

ProgressObject = SCons.Util.Null()

def Progress(*args, **kw):
    global ProgressObject
    ProgressObject = apply(Progressor, args, kw)

# Task control.
#

_BuildFailures = []

def GetBuildFailures():
    return _BuildFailures

class BuildTask(SCons.Taskmaster.Task):
    """An SCons build task."""
    progress = ProgressObject

    def display(self, message):
        display('scons: ' + message)

    def prepare(self):
        self.progress(self.targets[0])
        return SCons.Taskmaster.Task.prepare(self)

    def needs_execute(self):
        target = self.targets[0]
        if target.get_state() == SCons.Node.executing:
            return True
        else:
            if self.top and target.has_builder():
                display("scons: `%s' is up to date." % str(self.node))
            return False

    def execute(self):
        if print_time:
            start_time = time.time()
            global first_command_start
            if first_command_start is None:
                first_command_start = start_time
        SCons.Taskmaster.Task.execute(self)
        if print_time:
            global cumulative_command_time
            global last_command_end
            finish_time = time.time()
            last_command_end = finish_time
            cumulative_command_time = cumulative_command_time+finish_time-start_time
            sys.stdout.write("Command execution time: %f seconds\n"%(finish_time-start_time))

    def do_failed(self, status=2):
        _BuildFailures.append(self.exception[1])
        global exit_status
        global this_build_status
        if self.options.ignore_errors:
            SCons.Taskmaster.Task.executed(self)
        elif self.options.keep_going:
            SCons.Taskmaster.Task.fail_continue(self)
            exit_status = status
            this_build_status = status
        else:
            SCons.Taskmaster.Task.fail_stop(self)
            exit_status = status
            this_build_status = status
            
    def executed(self):
        t = self.targets[0]
        if self.top and not t.has_builder() and not t.side_effect:
            if not t.exists():
                errstr="Do not know how to make target `%s'." % t
                sys.stderr.write("scons: *** " + errstr)
                if not self.options.keep_going:
                    sys.stderr.write("  Stop.")
                sys.stderr.write("\n")
                try:
                    raise SCons.Errors.BuildError(t, errstr)
                except KeyboardInterrupt:
                    raise
                except:
                    self.exception_set()
                self.do_failed()
            else:
                print "scons: Nothing to be done for `%s'." % t
                SCons.Taskmaster.Task.executed(self)
        else:
            SCons.Taskmaster.Task.executed(self)

    def failed(self):
        # Handle the failure of a build task.  The primary purpose here
        # is to display the various types of Errors and Exceptions
        # appropriately.
        exc_info = self.exc_info()
        try:
            t, e, tb = exc_info
        except ValueError:
            t, e = exc_info
            tb = None

        if t is None:
            # The Taskmaster didn't record an exception for this Task;
            # see if the sys module has one.
            try:
                t, e, tb = sys.exc_info()[:]
            except ValueError:
                t, e = exc_info
                tb = None
                
        # Deprecated string exceptions will have their string stored
        # in the first entry of the tuple.
        if e is None:
            e = t

        buildError = SCons.Errors.convert_to_BuildError(e)
        if not buildError.node:
            buildError.node = self.node

        node = buildError.node
        if not SCons.Util.is_List(node):
                node = [ node ]
        nodename = string.join(map(str, node), ', ')

        errfmt = "scons: *** [%s] %s\n"
        sys.stderr.write(errfmt % (nodename, buildError))

        if (buildError.exc_info[2] and buildError.exc_info[1] and 
           # TODO(1.5)
           #not isinstance(
           #    buildError.exc_info[1], 
           #    (EnvironmentError, SCons.Errors.StopError, SCons.Errors.UserError))):
           not isinstance(buildError.exc_info[1], EnvironmentError) and
           not isinstance(buildError.exc_info[1], SCons.Errors.StopError) and
           not isinstance(buildError.exc_info[1], SCons.Errors.UserError)):
            type, value, trace = buildError.exc_info
            traceback.print_exception(type, value, trace)
        elif tb and print_stacktrace:
            sys.stderr.write("scons: internal stack trace:\n")
            traceback.print_tb(tb, file=sys.stderr)

        self.exception = (e, buildError, tb) # type, value, traceback
        self.do_failed(buildError.exitstatus)

        self.exc_clear()

    def postprocess(self):
        if self.top:
            t = self.targets[0]
            for tp in self.options.tree_printers:
                tp.display(t)
            if self.options.debug_includes:
                tree = t.render_include_tree()
                if tree:
                    print
                    print tree
        SCons.Taskmaster.Task.postprocess(self)

    def make_ready(self):
        """Make a task ready for execution"""
        SCons.Taskmaster.Task.make_ready(self)
        if self.out_of_date and self.options.debug_explain:
            explanation = self.out_of_date[0].explain()
            if explanation:
                sys.stdout.write("scons: " + explanation)

class CleanTask(SCons.Taskmaster.Task):
    """An SCons clean task."""
    def fs_delete(self, path, pathstr, remove=1):
        try:
            if os.path.exists(path):
                if os.path.isfile(path):
                    if remove: os.unlink(path)
                    display("Removed " + pathstr)
                elif os.path.isdir(path) and not os.path.islink(path):
                    # delete everything in the dir
                    entries = os.listdir(path)
                    # Sort for deterministic output (os.listdir() Can
                    # return entries in a random order).
                    entries.sort()
                    for e in entries:
                        p = os.path.join(path, e)
                        s = os.path.join(pathstr, e)
                        if os.path.isfile(p):
                            if remove: os.unlink(p)
                            display("Removed " + s)
                        else:
                            self.fs_delete(p, s, remove)
                    # then delete dir itself
                    if remove: os.rmdir(path)
                    display("Removed directory " + pathstr)
        except (IOError, OSError), e:
            print "scons: Could not remove '%s':" % pathstr, e.strerror

    def show(self):
        target = self.targets[0]
        if (target.has_builder() or target.side_effect) and not target.noclean:
            for t in self.targets:
                if not t.isdir():
                    display("Removed " + str(t))
        if SCons.Environment.CleanTargets.has_key(target):
            files = SCons.Environment.CleanTargets[target]
            for f in files:
                self.fs_delete(f.abspath, str(f), 0)

    def remove(self):
        target = self.targets[0]
        if (target.has_builder() or target.side_effect) and not target.noclean:
            for t in self.targets:
                try:
                    removed = t.remove()
                except OSError, e:
                    # An OSError may indicate something like a permissions
                    # issue, an IOError would indicate something like
                    # the file not existing.  In either case, print a
                    # message and keep going to try to remove as many
                    # targets aa possible.
                    print "scons: Could not remove '%s':" % str(t), e.strerror
                else:
                    if removed:
                        display("Removed " + str(t))
        if SCons.Environment.CleanTargets.has_key(target):
            files = SCons.Environment.CleanTargets[target]
            for f in files:
                self.fs_delete(f.abspath, str(f))

    execute = remove

    # We want the Taskmaster to update the Node states (and therefore
    # handle reference counts, etc.), but we don't want to call
    # back to the Node's post-build methods, which would do things
    # we don't want, like store .sconsign information.
    executed = SCons.Taskmaster.Task.executed_without_callbacks

    # Have the taskmaster arrange to "execute" all of the targets, because
    # we'll figure out ourselves (in remove() or show() above) whether
    # anything really needs to be done.
    make_ready = SCons.Taskmaster.Task.make_ready_all

    def prepare(self):
        pass

class QuestionTask(SCons.Taskmaster.Task):
    """An SCons task for the -q (question) option."""
    def prepare(self):
        pass
    
    def execute(self):
        if self.targets[0].get_state() != SCons.Node.up_to_date or \
           (self.top and not self.targets[0].exists()):
            global exit_status
            global this_build_status
            exit_status = 1
            this_build_status = 1
            self.tm.stop()

    def executed(self):
        pass


class TreePrinter:
    def __init__(self, derived=False, prune=False, status=False):
        self.derived = derived
        self.prune = prune
        self.status = status
    def get_all_children(self, node):
        return node.all_children()
    def get_derived_children(self, node):
        children = node.all_children(None)
        return filter(lambda x: x.has_builder(), children)
    def display(self, t):
        if self.derived:
            func = self.get_derived_children
        else:
            func = self.get_all_children
        s = self.status and 2 or 0
        SCons.Util.print_tree(t, func, prune=self.prune, showtags=s)


def python_version_string():
    return string.split(sys.version)[0]

def python_version_unsupported(version=sys.version_info):
    return version < (1, 5, 2)

def python_version_deprecated(version=sys.version_info):
    return version < (2, 2, 0)


# Global variables

print_objects = 0
print_memoizer = 0
print_stacktrace = 0
print_time = 0
sconscript_time = 0
cumulative_command_time = 0
exit_status = 0 # final exit status, assume success by default
this_build_status = 0 # "exit status" of an individual build
num_jobs = None
delayed_warnings = []

class FakeOptionParser:
    """
    A do-nothing option parser, used for the initial OptionsParser variable.

    During normal SCons operation, the OptionsParser is created right
    away by the main() function.  Certain tests scripts however, can
    introspect on different Tool modules, the initialization of which
    can try to add a new, local option to an otherwise uninitialized
    OptionsParser object.  This allows that introspection to happen
    without blowing up.

    """
    class FakeOptionValues:
        def __getattr__(self, attr):
            return None
    values = FakeOptionValues()
    def add_local_option(self, *args, **kw):
        pass

OptionsParser = FakeOptionParser()

def AddOption(*args, **kw):
    if not kw.has_key('default'):
        kw['default'] = None
    result = apply(OptionsParser.add_local_option, args, kw)
    return result

def GetOption(name):
    return getattr(OptionsParser.values, name)

def SetOption(name, value):
    return OptionsParser.values.set_option(name, value)

#
class Stats:
    def __init__(self):
        self.stats = []
        self.labels = []
        self.append = self.do_nothing
        self.print_stats = self.do_nothing
    def enable(self, outfp):
        self.outfp = outfp
        self.append = self.do_append
        self.print_stats = self.do_print
    def do_nothing(self, *args, **kw):
        pass

class CountStats(Stats):
    def do_append(self, label):
        self.labels.append(label)
        self.stats.append(SCons.Debug.fetchLoggedInstances())
    def do_print(self):
        stats_table = {}
        for s in self.stats:
            for n in map(lambda t: t[0], s):
                stats_table[n] = [0, 0, 0, 0]
        i = 0
        for s in self.stats:
            for n, c in s:
                stats_table[n][i] = c
            i = i + 1
        keys = stats_table.keys()
        keys.sort()
        self.outfp.write("Object counts:\n")
        pre = ["   "]
        post = ["   %s\n"]
        l = len(self.stats)
        fmt1 = string.join(pre + [' %7s']*l + post, '')
        fmt2 = string.join(pre + [' %7d']*l + post, '')
        labels = self.labels[:l]
        labels.append(("", "Class"))
        self.outfp.write(fmt1 % tuple(map(lambda x: x[0], labels)))
        self.outfp.write(fmt1 % tuple(map(lambda x: x[1], labels)))
        for k in keys:
            r = stats_table[k][:l] + [k]
            self.outfp.write(fmt2 % tuple(r))

count_stats = CountStats()

class MemStats(Stats):
    def do_append(self, label):
        self.labels.append(label)
        self.stats.append(SCons.Debug.memory())
    def do_print(self):
        fmt = 'Memory %-32s %12d\n'
        for label, stats in map(None, self.labels, self.stats):
            self.outfp.write(fmt % (label, stats))

memory_stats = MemStats()

# utility functions

def _scons_syntax_error(e):
    """Handle syntax errors. Print out a message and show where the error
    occurred.
    """
    etype, value, tb = sys.exc_info()
    lines = traceback.format_exception_only(etype, value)
    for line in lines:
        sys.stderr.write(line+'\n')
    sys.exit(2)

def find_deepest_user_frame(tb):
    """
    Find the deepest stack frame that is not part of SCons.

    Input is a "pre-processed" stack trace in the form
    returned by traceback.extract_tb() or traceback.extract_stack()
    """
    
    tb.reverse()

    # find the deepest traceback frame that is not part
    # of SCons:
    for frame in tb:
        filename = frame[0]
        if string.find(filename, os.sep+'SCons'+os.sep) == -1:
            return frame
    return tb[0]

def _scons_user_error(e):
    """Handle user errors. Print out a message and a description of the
    error, along with the line number and routine where it occured. 
    The file and line number will be the deepest stack frame that is
    not part of SCons itself.
    """
    global print_stacktrace
    etype, value, tb = sys.exc_info()
    if print_stacktrace:
        traceback.print_exception(etype, value, tb)
    filename, lineno, routine, dummy = find_deepest_user_frame(traceback.extract_tb(tb))
    sys.stderr.write("\nscons: *** %s\n" % value)
    sys.stderr.write('File "%s", line %d, in %s\n' % (filename, lineno, routine))
    sys.exit(2)

def _scons_user_warning(e):
    """Handle user warnings. Print out a message and a description of
    the warning, along with the line number and routine where it occured.
    The file and line number will be the deepest stack frame that is
    not part of SCons itself.
    """
    etype, value, tb = sys.exc_info()
    filename, lineno, routine, dummy = find_deepest_user_frame(traceback.extract_tb(tb))
    sys.stderr.write("\nscons: warning: %s\n" % e)
    sys.stderr.write('File "%s", line %d, in %s\n' % (filename, lineno, routine))

def _scons_internal_warning(e):
    """Slightly different from _scons_user_warning in that we use the
    *current call stack* rather than sys.exc_info() to get our stack trace.
    This is used by the warnings framework to print warnings."""
    filename, lineno, routine, dummy = find_deepest_user_frame(traceback.extract_stack())
    sys.stderr.write("\nscons: warning: %s\n" % e[0])
    sys.stderr.write('File "%s", line %d, in %s\n' % (filename, lineno, routine))

def _scons_internal_error():
    """Handle all errors but user errors. Print out a message telling
    the user what to do in this case and print a normal trace.
    """
    print 'internal error'
    traceback.print_exc()
    sys.exit(2)

def _SConstruct_exists(dirname='', repositories=[], filelist=None):
    """This function checks that an SConstruct file exists in a directory.
    If so, it returns the path of the file. By default, it checks the
    current directory.
    """
    if not filelist:
        filelist = ['SConstruct', 'Sconstruct', 'sconstruct']
    for file in filelist:
        sfile = os.path.join(dirname, file)
        if os.path.isfile(sfile):
            return sfile
        if not os.path.isabs(sfile):
            for rep in repositories:
                if os.path.isfile(os.path.join(rep, sfile)):
                    return sfile
    return None

def _set_debug_values(options):
    global print_memoizer, print_objects, print_stacktrace, print_time

    debug_values = options.debug

    if "count" in debug_values:
        # All of the object counts are within "if __debug__:" blocks,
        # which get stripped when running optimized (with python -O or
        # from compiled *.pyo files).  Provide a warning if __debug__ is
        # stripped, so it doesn't just look like --debug=count is broken.
        enable_count = False
        if __debug__: enable_count = True
        if enable_count:
            count_stats.enable(sys.stdout)
        else:
            msg = "--debug=count is not supported when running SCons\n" + \
                  "\twith the python -O option or optimized (.pyo) modules."
            SCons.Warnings.warn(SCons.Warnings.NoObjectCountWarning, msg)
    if "dtree" in debug_values:
        options.tree_printers.append(TreePrinter(derived=True))
    options.debug_explain = ("explain" in debug_values)
    if "findlibs" in debug_values:
        SCons.Scanner.Prog.print_find_libs = "findlibs"
    options.debug_includes = ("includes" in debug_values)
    print_memoizer = ("memoizer" in debug_values)
    if "memory" in debug_values:
        memory_stats.enable(sys.stdout)
    print_objects = ("objects" in debug_values)
    if "presub" in debug_values:
        SCons.Action.print_actions_presub = 1
    if "stacktrace" in debug_values:
        print_stacktrace = 1
    if "stree" in debug_values:
        options.tree_printers.append(TreePrinter(status=True))
    if "time" in debug_values:
        print_time = 1
    if "tree" in debug_values:
        options.tree_printers.append(TreePrinter())

def _create_path(plist):
    path = '.'
    for d in plist:
        if os.path.isabs(d):
            path = d
        else:
            path = path + '/' + d
    return path

def _load_site_scons_dir(topdir, site_dir_name=None):
    """Load the site_scons dir under topdir.
    Adds site_scons to sys.path, imports site_scons/site_init.py,
    and adds site_scons/site_tools to default toolpath."""
    if site_dir_name:
        err_if_not_found = True       # user specified: err if missing
    else:
        site_dir_name = "site_scons"
        err_if_not_found = False
        
    site_dir = os.path.join(topdir.path, site_dir_name)
    if not os.path.exists(site_dir):
        if err_if_not_found:
            raise SCons.Errors.UserError, "site dir %s not found."%site_dir
        return

    site_init_filename = "site_init.py"
    site_init_modname = "site_init"
    site_tools_dirname = "site_tools"
    sys.path = [os.path.abspath(site_dir)] + sys.path
    site_init_file = os.path.join(site_dir, site_init_filename)
    site_tools_dir = os.path.join(site_dir, site_tools_dirname)
    if os.path.exists(site_init_file):
        import imp
        try:
            fp, pathname, description = imp.find_module(site_init_modname,
                                                        [site_dir])
            try:
                imp.load_module(site_init_modname, fp, pathname, description)
            finally:
                if fp:
                    fp.close()
        except ImportError, e:
            sys.stderr.write("Can't import site init file '%s': %s\n"%(site_init_file, e))
            raise
        except Exception, e:
            sys.stderr.write("Site init file '%s' raised exception: %s\n"%(site_init_file, e))
            raise
    if os.path.exists(site_tools_dir):
        SCons.Tool.DefaultToolpath.append(os.path.abspath(site_tools_dir))

def version_string(label, module):
    version = module.__version__
    build = module.__build__
    if build:
        if build[0] != '.':
            build = '.' + build
        version = version + build
    fmt = "\t%s: v%s, %s, by %s on %s\n"
    return fmt % (label,
                  version,
                  module.__date__,
                  module.__developer__,
                  module.__buildsys__)

def _main(parser):
    global exit_status
    global this_build_status

    options = parser.values

    # Here's where everything really happens.

    # First order of business:  set up default warnings and then
    # handle the user's warning options, so that we can issue (or
    # suppress) appropriate warnings about anything that might happen,
    # as configured by the user.

    default_warnings = [ SCons.Warnings.CorruptSConsignWarning,
                         SCons.Warnings.DeprecatedWarning,
                         SCons.Warnings.DuplicateEnvironmentWarning,
                         SCons.Warnings.FutureReservedVariableWarning,
                         SCons.Warnings.LinkWarning,
                         SCons.Warnings.MissingSConscriptWarning,
                         SCons.Warnings.NoMD5ModuleWarning,
                         SCons.Warnings.NoMetaclassSupportWarning,
                         SCons.Warnings.NoObjectCountWarning,
                         SCons.Warnings.NoParallelSupportWarning,
                         SCons.Warnings.MisleadingKeywordsWarning,
                         SCons.Warnings.ReservedVariableWarning,
                         SCons.Warnings.StackSizeWarning,
                       ]

    for warning in default_warnings:
        SCons.Warnings.enableWarningClass(warning)
    SCons.Warnings._warningOut = _scons_internal_warning
    SCons.Warnings.process_warn_strings(options.warn)

    # Now that we have the warnings configuration set up, we can actually
    # issue (or suppress) any warnings about warning-worthy things that
    # occurred while the command-line options were getting parsed.
    try:
        dw = options.delayed_warnings
    except AttributeError:
        pass
    else:
        delayed_warnings.extend(dw)
    for warning_type, message in delayed_warnings:
        SCons.Warnings.warn(warning_type, message)

    if options.diskcheck:
        SCons.Node.FS.set_diskcheck(options.diskcheck)

    # Next, we want to create the FS object that represents the outside
    # world's file system, as that's central to a lot of initialization.
    # To do this, however, we need to be in the directory from which we
    # want to start everything, which means first handling any relevant
    # options that might cause us to chdir somewhere (-C, -D, -U, -u).
    if options.directory:
        cdir = _create_path(options.directory)
        try:
            os.chdir(cdir)
        except OSError:
            sys.stderr.write("Could not change directory to %s\n" % cdir)

    target_top = None
    if options.climb_up:
        target_top = '.'  # directory to prepend to targets
        script_dir = os.getcwd()  # location of script
        while script_dir and not _SConstruct_exists(script_dir,
                                                    options.repository,
                                                    options.file):
            script_dir, last_part = os.path.split(script_dir)
            if last_part:
                target_top = os.path.join(last_part, target_top)
            else:
                script_dir = ''
        if script_dir and script_dir != os.getcwd():
            display("scons: Entering directory `%s'" % script_dir)
            os.chdir(script_dir)

    # Now that we're in the top-level SConstruct directory, go ahead
    # and initialize the FS object that represents the file system,
    # and make it the build engine default.
    fs = SCons.Node.FS.get_default_fs()

    for rep in options.repository:
        fs.Repository(rep)

    # Now that we have the FS object, the next order of business is to
    # check for an SConstruct file (or other specified config file).
    # If there isn't one, we can bail before doing any more work.
    scripts = []
    if options.file:
        scripts.extend(options.file)
    if not scripts:
        sfile = _SConstruct_exists(repositories=options.repository,
                                   filelist=options.file)
        if sfile:
            scripts.append(sfile)

    if not scripts:
        if options.help:
            # There's no SConstruct, but they specified -h.
            # Give them the options usage now, before we fail
            # trying to read a non-existent SConstruct file.
            raise SConsPrintHelpException
        raise SCons.Errors.UserError, "No SConstruct file found."

    if scripts[0] == "-":
        d = fs.getcwd()
    else:
        d = fs.File(scripts[0]).dir
    fs.set_SConstruct_dir(d)

    _set_debug_values(options)
    SCons.Node.implicit_cache = options.implicit_cache
    SCons.Node.implicit_deps_changed = options.implicit_deps_changed
    SCons.Node.implicit_deps_unchanged = options.implicit_deps_unchanged

    if options.no_exec:
        SCons.SConf.dryrun = 1
        SCons.Action.execute_actions = None
    if options.question:
        SCons.SConf.dryrun = 1
    if options.clean:
        SCons.SConf.SetBuildType('clean')
    if options.help:
        SCons.SConf.SetBuildType('help')
    SCons.SConf.SetCacheMode(options.config)
    SCons.SConf.SetProgressDisplay(progress_display)

    if options.no_progress or options.silent:
        progress_display.set_mode(0)

    if options.site_dir:
        _load_site_scons_dir(d, options.site_dir)
    elif not options.no_site_dir:
        _load_site_scons_dir(d)
        
    if options.include_dir:
        sys.path = options.include_dir + sys.path

    # That should cover (most of) the options.  Next, set up the variables
    # that hold command-line arguments, so the SConscript files that we
    # read and execute have access to them.
    targets = []
    xmit_args = []
    for a in parser.largs:
        if a[0] == '-':
            continue
        if '=' in a:
            xmit_args.append(a)
        else:
            targets.append(a)
    SCons.Script._Add_Targets(targets + parser.rargs)
    SCons.Script._Add_Arguments(xmit_args)

    # If stdout is not a tty, replace it with a wrapper object to call flush
    # after every write.
    #
    # Tty devices automatically flush after every newline, so the replacement
    # isn't necessary.  Furthermore, if we replace sys.stdout, the readline
    # module will no longer work.  This affects the behavior during
    # --interactive mode.  --interactive should only be used when stdin and
    # stdout refer to a tty.
    if not sys.stdout.isatty():
        sys.stdout = SCons.Util.Unbuffered(sys.stdout)
    if not sys.stderr.isatty():
        sys.stderr = SCons.Util.Unbuffered(sys.stderr)

    memory_stats.append('before reading SConscript files:')
    count_stats.append(('pre-', 'read'))

    # And here's where we (finally) read the SConscript files.

    progress_display("scons: Reading SConscript files ...")

    start_time = time.time()
    try:
        for script in scripts:
            SCons.Script._SConscript._SConscript(fs, script)
    except SCons.Errors.StopError, e:
        # We had problems reading an SConscript file, such as it
        # couldn't be copied in to the VariantDir.  Since we're just
        # reading SConscript files and haven't started building
        # things yet, stop regardless of whether they used -i or -k
        # or anything else.
        sys.stderr.write("scons: *** %s  Stop.\n" % e)
        exit_status = 2
        sys.exit(exit_status)
    global sconscript_time
    sconscript_time = time.time() - start_time

    progress_display("scons: done reading SConscript files.")

    memory_stats.append('after reading SConscript files:')
    count_stats.append(('post-', 'read'))

    # Re-{enable,disable} warnings in case they disabled some in
    # the SConscript file.
    #
    # We delay enabling the PythonVersionWarning class until here so that,
    # if they explicity disabled it in either in the command line or in
    # $SCONSFLAGS, or in the SConscript file, then the search through
    # the list of deprecated warning classes will find that disabling
    # first and not issue the warning.
    SCons.Warnings.enableWarningClass(SCons.Warnings.PythonVersionWarning)
    SCons.Warnings.process_warn_strings(options.warn)

    # Now that we've read the SConscript files, we can check for the
    # warning about deprecated Python versions--delayed until here
    # in case they disabled the warning in the SConscript files.
    if python_version_deprecated():
        msg = "Support for pre-2.2 Python (%s) is deprecated.\n" + \
              "    If this will cause hardship, contact dev@scons.tigris.org."
        SCons.Warnings.warn(SCons.Warnings.PythonVersionWarning,
                            msg % python_version_string())

    if not options.help:
        SCons.SConf.CreateConfigHBuilder(SCons.Defaults.DefaultEnvironment())

    # Now re-parse the command-line options (any to the left of a '--'
    # argument, that is) with any user-defined command-line options that
    # the SConscript files may have added to the parser object.  This will
    # emit the appropriate error message and exit if any unknown option
    # was specified on the command line.

    parser.preserve_unknown_options = False
    parser.parse_args(parser.largs, options)

    if options.help:
        help_text = SCons.Script.help_text
        if help_text is None:
            # They specified -h, but there was no Help() inside the
            # SConscript files.  Give them the options usage.
            raise SConsPrintHelpException
        else:
            print help_text
            print "Use scons -H for help about command-line options."
        exit_status = 0
        return

    # Change directory to the top-level SConstruct directory, then tell
    # the Node.FS subsystem that we're all done reading the SConscript
    # files and calling Repository() and VariantDir() and changing
    # directories and the like, so it can go ahead and start memoizing
    # the string values of file system nodes.

    fs.chdir(fs.Top)

    SCons.Node.FS.save_strings(1)

    # Now that we've read the SConscripts we can set the options
    # that are SConscript settable:
    SCons.Node.implicit_cache = options.implicit_cache
    SCons.Node.FS.set_duplicate(options.duplicate)
    fs.set_max_drift(options.max_drift)

    SCons.Job.explicit_stack_size = options.stack_size

    if options.md5_chunksize:
        SCons.Node.FS.File.md5_chunksize = options.md5_chunksize

    platform = SCons.Platform.platform_module()

    if options.interactive:
        SCons.Script.Interactive.interact(fs, OptionsParser, options,
                                          targets, target_top)

    else:

        # Build the targets
        nodes = _build_targets(fs, options, targets, target_top)
        if not nodes:
            exit_status = 2

def _build_targets(fs, options, targets, target_top):

    global this_build_status
    this_build_status = 0

    progress_display.set_mode(not (options.no_progress or options.silent))
    display.set_mode(not options.silent)
    SCons.Action.print_actions          = not options.silent
    SCons.Action.execute_actions        = not options.no_exec
    SCons.Node.FS.do_store_info         = not options.no_exec
    SCons.SConf.dryrun                  = options.no_exec

    if options.diskcheck:
        SCons.Node.FS.set_diskcheck(options.diskcheck)

    SCons.CacheDir.cache_enabled = not options.cache_disable
    SCons.CacheDir.cache_debug = options.cache_debug
    SCons.CacheDir.cache_force = options.cache_force
    SCons.CacheDir.cache_show = options.cache_show

    if options.no_exec:
        CleanTask.execute = CleanTask.show
    else:
        CleanTask.execute = CleanTask.remove

    lookup_top = None
    if targets or SCons.Script.BUILD_TARGETS != SCons.Script._build_plus_default:
        # They specified targets on the command line or modified
        # BUILD_TARGETS in the SConscript file(s), so if they used -u,
        # -U or -D, we have to look up targets relative to the top,
        # but we build whatever they specified.
        if target_top:
            lookup_top = fs.Dir(target_top)
            target_top = None

        targets = SCons.Script.BUILD_TARGETS
    else:
        # There are no targets specified on the command line,
        # so if they used -u, -U or -D, we may have to restrict
        # what actually gets built.
        d = None
        if target_top:
            if options.climb_up == 1:
                # -u, local directory and below
                target_top = fs.Dir(target_top)
                lookup_top = target_top
            elif options.climb_up == 2:
                # -D, all Default() targets
                target_top = None
                lookup_top = None
            elif options.climb_up == 3:
                # -U, local SConscript Default() targets
                target_top = fs.Dir(target_top)
                def check_dir(x, target_top=target_top):
                    if hasattr(x, 'cwd') and not x.cwd is None:
                        cwd = x.cwd.srcnode()
                        return cwd == target_top
                    else:
                        # x doesn't have a cwd, so it's either not a target,
                        # or not a file, so go ahead and keep it as a default
                        # target and let the engine sort it out:
                        return 1                
                d = filter(check_dir, SCons.Script.DEFAULT_TARGETS)
                SCons.Script.DEFAULT_TARGETS[:] = d
                target_top = None
                lookup_top = None

        targets = SCons.Script._Get_Default_Targets(d, fs)

    if not targets:
        sys.stderr.write("scons: *** No targets specified and no Default() targets found.  Stop.\n")
        return None

    def Entry(x, ltop=lookup_top, ttop=target_top, fs=fs):
        if isinstance(x, SCons.Node.Node):
            node = x
        else:
            node = None
            # Why would ltop be None? Unfortunately this happens.
            if ltop == None: ltop = ''
            # Curdir becomes important when SCons is called with -u, -C,
            # or similar option that changes directory, and so the paths
            # of targets given on the command line need to be adjusted.
            curdir = os.path.join(os.getcwd(), str(ltop))
            for lookup in SCons.Node.arg2nodes_lookups:
                node = lookup(x, curdir=curdir)
                if node != None:
                    break
            if node is None:
                node = fs.Entry(x, directory=ltop, create=1)
        if ttop and not node.is_under(ttop):
            if isinstance(node, SCons.Node.FS.Dir) and ttop.is_under(node):
                node = ttop
            else:
                node = None
        return node

    nodes = filter(None, map(Entry, targets))

    task_class = BuildTask      # default action is to build targets
    opening_message = "Building targets ..."
    closing_message = "done building targets."
    if options.keep_going:
        failure_message = "done building targets (errors occurred during build)."
    else:
        failure_message = "building terminated because of errors."
    if options.question:
        task_class = QuestionTask
    try:
        if options.clean:
            task_class = CleanTask
            opening_message = "Cleaning targets ..."
            closing_message = "done cleaning targets."
            if options.keep_going:
                failure_message = "done cleaning targets (errors occurred during clean)."
            else:
                failure_message = "cleaning terminated because of errors."
    except AttributeError:
        pass

    task_class.progress = ProgressObject

    if options.random:
        def order(dependencies):
            """Randomize the dependencies."""
            import random
            # This is cribbed from the implementation of
            # random.shuffle() in Python 2.X.
            d = dependencies
            for i in xrange(len(d)-1, 0, -1):
                j = int(random.random() * (i+1))
                d[i], d[j] = d[j], d[i]
            return d
    else:
        def order(dependencies):
            """Leave the order of dependencies alone."""
            return dependencies

    if options.taskmastertrace_file == '-':
        tmtrace = sys.stdout
    elif options.taskmastertrace_file:
        tmtrace = open(options.taskmastertrace_file, 'wb')
    else:
        tmtrace = None
    taskmaster = SCons.Taskmaster.Taskmaster(nodes, task_class, order, tmtrace)

    # Let the BuildTask objects get at the options to respond to the
    # various print_* settings, tree_printer list, etc.
    BuildTask.options = options

    global num_jobs
    num_jobs = options.num_jobs
    jobs = SCons.Job.Jobs(num_jobs, taskmaster)
    if num_jobs > 1:
        msg = None
        if jobs.num_jobs == 1:
            msg = "parallel builds are unsupported by this version of Python;\n" + \
                  "\tignoring -j or num_jobs option.\n"
        elif sys.platform == 'win32':
            msg = fetch_win32_parallel_msg()
        if msg:
            SCons.Warnings.warn(SCons.Warnings.NoParallelSupportWarning, msg)

    memory_stats.append('before building targets:')
    count_stats.append(('pre-', 'build'))

    def jobs_postfunc(
        jobs=jobs,
        options=options,
        closing_message=closing_message,
        failure_message=failure_message
        ):
        if jobs.were_interrupted():
            progress_display("scons: Build interrupted.")
            global exit_status
            global this_build_status
            exit_status = 2
            this_build_status = 2

        if this_build_status:
            progress_display("scons: " + failure_message)
        else:
            progress_display("scons: " + closing_message)
        if not options.no_exec:
            if jobs.were_interrupted():
                progress_display("scons: writing .sconsign file.")
            SCons.SConsign.write()

    progress_display("scons: " + opening_message)
    jobs.run(postfunc = jobs_postfunc)

    memory_stats.append('after building targets:')
    count_stats.append(('post-', 'build'))

    return nodes

def _exec_main(parser, values):
    sconsflags = os.environ.get('SCONSFLAGS', '')
    all_args = string.split(sconsflags) + sys.argv[1:]

    options, args = parser.parse_args(all_args, values)

    if type(options.debug) == type([]) and "pdb" in options.debug:
        import pdb
        pdb.Pdb().runcall(_main, parser)
    elif options.profile_file:
        try:
            from cProfile import Profile
        except ImportError, e:
            from profile import Profile

        # Some versions of Python 2.4 shipped a profiler that had the
        # wrong 'c_exception' entry in its dispatch table.  Make sure
        # we have the right one.  (This may put an unnecessary entry
        # in the table in earlier versions of Python, but its presence
        # shouldn't hurt anything).
        try:
            dispatch = Profile.dispatch
        except AttributeError:
            pass
        else:
            dispatch['c_exception'] = Profile.trace_dispatch_return

        prof = Profile()
        try:
            prof.runcall(_main, parser)
        except SConsPrintHelpException, e:
            prof.dump_stats(options.profile_file)
            raise e
        except SystemExit:
            pass
        prof.dump_stats(options.profile_file)
    else:
        _main(parser)

def main():
    global OptionsParser
    global exit_status
    global first_command_start

    # Check up front for a Python version we do not support.  We
    # delay the check for deprecated Python versions until later,
    # after the SConscript files have been read, in case they
    # disable that warning.
    if python_version_unsupported():
        msg = "scons: *** SCons version %s does not run under Python version %s.\n"
        sys.stderr.write(msg % (SCons.__version__, python_version_string()))
        sys.exit(1)

    parts = ["SCons by Steven Knight et al.:\n"]
    try:
        import __main__
        parts.append(version_string("script", __main__))
    except (ImportError, AttributeError):
        # On Windows there is no scons.py, so there is no
        # __main__.__version__, hence there is no script version.
        pass 
    parts.append(version_string("engine", SCons))
    parts.append("Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation")
    version = string.join(parts, '')

    import SConsOptions
    parser = SConsOptions.Parser(version)
    values = SConsOptions.SConsValues(parser.get_default_values())

    OptionsParser = parser
    
    try:
        _exec_main(parser, values)
    except SystemExit, s:
        if s:
            exit_status = s
    except KeyboardInterrupt:
        print("scons: Build interrupted.")
        sys.exit(2)
    except SyntaxError, e:
        _scons_syntax_error(e)
    except SCons.Errors.InternalError:
        _scons_internal_error()
    except SCons.Errors.UserError, e:
        _scons_user_error(e)
    except SConsPrintHelpException:
        parser.print_help()
        exit_status = 0
    except SCons.Errors.BuildError, e:
        exit_status = e.exitstatus
    except:
        # An exception here is likely a builtin Python exception Python
        # code in an SConscript file.  Show them precisely what the
        # problem was and where it happened.
        SCons.Script._SConscript.SConscript_exception()
        sys.exit(2)

    memory_stats.print_stats()
    count_stats.print_stats()

    if print_objects:
        SCons.Debug.listLoggedInstances('*')
        #SCons.Debug.dumpLoggedInstances('*')

    if print_memoizer:
        SCons.Memoize.Dump("Memoizer (memory cache) hits and misses:")

    # Dump any development debug info that may have been enabled.
    # These are purely for internal debugging during development, so
    # there's no need to control them with --debug= options; they're
    # controlled by changing the source code.
    SCons.Debug.dump_caller_counts()
    SCons.Taskmaster.dump_stats()

    if print_time:
        total_time = time.time() - SCons.Script.start_time
        if num_jobs == 1:
            ct = cumulative_command_time
        else:
            if last_command_end is None or first_command_start is None:
                ct = 0.0
            else:
                ct = last_command_end - first_command_start
        scons_time = total_time - sconscript_time - ct
        print "Total build time: %f seconds"%total_time
        print "Total SConscript file execution time: %f seconds"%sconscript_time
        print "Total SCons execution time: %f seconds"%scons_time
        print "Total command execution time: %f seconds"%ct

    sys.exit(exit_status)
