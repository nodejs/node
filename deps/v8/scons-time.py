#!/usr/bin/env python
#
# scons-time - run SCons timings and collect statistics
#
# A script for running a configuration through SCons with a standard
# set of invocations to collect timing and memory statistics and to
# capture the results in a consistent set of output files for display
# and analysis.
#

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

from __future__ import nested_scopes

__revision__ = "src/script/scons-time.py 3842 2008/12/20 22:59:52 scons"

import getopt
import glob
import os
import os.path
import re
import shutil
import string
import sys
import tempfile
import time

try:
    False
except NameError:
    # Pre-2.2 Python has no False keyword.
    import __builtin__
    __builtin__.False = not 1

try:
    True
except NameError:
    # Pre-2.2 Python has no True keyword.
    import __builtin__
    __builtin__.True = not 0

def make_temp_file(**kw):
    try:
        result = tempfile.mktemp(**kw)
        try:
            result = os.path.realpath(result)
        except AttributeError:
            # Python 2.1 has no os.path.realpath() method.
            pass
    except TypeError:
        try:
            save_template = tempfile.template
            prefix = kw['prefix']
            del kw['prefix']
            tempfile.template = prefix
            result = tempfile.mktemp(**kw)
        finally:
            tempfile.template = save_template
    return result

class Plotter:
    def increment_size(self, largest):
        """
        Return the size of each horizontal increment line for a specified
        maximum value.  This returns a value that will provide somewhere
        between 5 and 9 horizontal lines on the graph, on some set of
        boundaries that are multiples of 10/100/1000/etc.
        """
        i = largest / 5
        if not i:
            return largest
        multiplier = 1
        while i >= 10:
            i = i / 10
            multiplier = multiplier * 10
        return i * multiplier

    def max_graph_value(self, largest):
        # Round up to next integer.
        largest = int(largest) + 1
        increment = self.increment_size(largest)
        return ((largest + increment - 1) / increment) * increment

class Line:
    def __init__(self, points, type, title, label, comment, fmt="%s %s"):
        self.points = points
        self.type = type
        self.title = title
        self.label = label
        self.comment = comment
        self.fmt = fmt

    def print_label(self, inx, x, y):
        if self.label:
            print 'set label %s "%s" at %s,%s right' % (inx, self.label, x, y)

    def plot_string(self):
        if self.title:
            title_string = 'title "%s"' % self.title
        else:
            title_string = 'notitle'
        return "'-' %s with lines lt %s" % (title_string, self.type)

    def print_points(self, fmt=None):
        if fmt is None:
            fmt = self.fmt
        if self.comment:
            print '# %s' % self.comment
        for x, y in self.points:
            # If y is None, it usually represents some kind of break
            # in the line's index number.  We might want to represent
            # this some way rather than just drawing the line straight
            # between the two points on either side.
            if not y is None:
                print fmt % (x, y)
        print 'e'

    def get_x_values(self):
        return [ p[0] for p in self.points ]

    def get_y_values(self):
        return [ p[1] for p in self.points ]

class Gnuplotter(Plotter):

    def __init__(self, title, key_location):
        self.lines = []
        self.title = title
        self.key_location = key_location

    def line(self, points, type, title=None, label=None, comment=None, fmt='%s %s'):
        if points:
            line = Line(points, type, title, label, comment, fmt)
            self.lines.append(line)

    def plot_string(self, line):
        return line.plot_string()

    def vertical_bar(self, x, type, label, comment):
        if self.get_min_x() <= x and x <= self.get_max_x():
            points = [(x, 0), (x, self.max_graph_value(self.get_max_y()))]
            self.line(points, type, label, comment)

    def get_all_x_values(self):
        result = []
        for line in self.lines:
            result.extend(line.get_x_values())
        return filter(lambda r: not r is None, result)

    def get_all_y_values(self):
        result = []
        for line in self.lines:
            result.extend(line.get_y_values())
        return filter(lambda r: not r is None, result)

    def get_min_x(self):
        try:
            return self.min_x
        except AttributeError:
            try:
                self.min_x = min(self.get_all_x_values())
            except ValueError:
                self.min_x = 0
            return self.min_x

    def get_max_x(self):
        try:
            return self.max_x
        except AttributeError:
            try:
                self.max_x = max(self.get_all_x_values())
            except ValueError:
                self.max_x = 0
            return self.max_x

    def get_min_y(self):
        try:
            return self.min_y
        except AttributeError:
            try:
                self.min_y = min(self.get_all_y_values())
            except ValueError:
                self.min_y = 0
            return self.min_y

    def get_max_y(self):
        try:
            return self.max_y
        except AttributeError:
            try:
                self.max_y = max(self.get_all_y_values())
            except ValueError:
                self.max_y = 0
            return self.max_y

    def draw(self):

        if not self.lines:
            return

        if self.title:
            print 'set title "%s"' % self.title
        print 'set key %s' % self.key_location

        min_y = self.get_min_y()
        max_y = self.max_graph_value(self.get_max_y())
        range = max_y - min_y
        incr = range / 10.0
        start = min_y + (max_y / 2.0) + (2.0 * incr)
        position = [ start - (i * incr) for i in xrange(5) ]

        inx = 1
        for line in self.lines:
            line.print_label(inx, line.points[0][0]-1,
                             position[(inx-1) % len(position)])
            inx += 1

        plot_strings = [ self.plot_string(l) for l in self.lines ]
        print 'plot ' + ', \\\n     '.join(plot_strings)

        for line in self.lines:
            line.print_points()



def untar(fname):
    import tarfile
    tar = tarfile.open(name=fname, mode='r')
    for tarinfo in tar:
        tar.extract(tarinfo)
    tar.close()

def unzip(fname):
    import zipfile
    zf = zipfile.ZipFile(fname, 'r')
    for name in zf.namelist():
        dir = os.path.dirname(name)
        try:
            os.makedirs(dir)
        except:
            pass
        open(name, 'w').write(zf.read(name))

def read_tree(dir):
    def read_files(arg, dirname, fnames):
        for fn in fnames:
            fn = os.path.join(dirname, fn)
            if os.path.isfile(fn):
                open(fn, 'rb').read()
    os.path.walk('.', read_files, None)

def redirect_to_file(command, log):
    return '%s > %s 2>&1' % (command, log)

def tee_to_file(command, log):
    return '%s 2>&1 | tee %s' % (command, log)


    
class SConsTimer:
    """
    Usage: scons-time SUBCOMMAND [ARGUMENTS]
    Type "scons-time help SUBCOMMAND" for help on a specific subcommand.

    Available subcommands:
        func            Extract test-run data for a function
        help            Provides help
        mem             Extract --debug=memory data from test runs
        obj             Extract --debug=count data from test runs
        time            Extract --debug=time data from test runs
        run             Runs a test configuration
    """

    name = 'scons-time'
    name_spaces = ' '*len(name)

    def makedict(**kw):
        return kw

    default_settings = makedict(
        aegis               = 'aegis',
        aegis_project       = None,
        chdir               = None,
        config_file         = None,
        initial_commands    = [],
        key_location        = 'bottom left',
        orig_cwd            = os.getcwd(),
        outdir              = None,
        prefix              = '',
        python              = '"%s"' % sys.executable,
        redirect            = redirect_to_file,
        scons               = None,
        scons_flags         = '--debug=count --debug=memory --debug=time --debug=memoizer',
        scons_lib_dir       = None,
        scons_wrapper       = None,
        startup_targets     = '--help',
        subdir              = None,
        subversion_url      = None,
        svn                 = 'svn',
        svn_co_flag         = '-q',
        tar                 = 'tar',
        targets             = '',
        targets0            = None,
        targets1            = None,
        targets2            = None,
        title               = None,
        unzip               = 'unzip',
        verbose             = False,
        vertical_bars       = [],

        unpack_map = {
            '.tar.gz'       : (untar,   '%(tar)s xzf %%s'),
            '.tgz'          : (untar,   '%(tar)s xzf %%s'),
            '.tar'          : (untar,   '%(tar)s xf %%s'),
            '.zip'          : (unzip,   '%(unzip)s %%s'),
        },
    )

    run_titles = [
        'Startup',
        'Full build',
        'Up-to-date build',
    ]

    run_commands = [
        '%(python)s %(scons_wrapper)s %(scons_flags)s --profile=%(prof0)s %(targets0)s',
        '%(python)s %(scons_wrapper)s %(scons_flags)s --profile=%(prof1)s %(targets1)s',
        '%(python)s %(scons_wrapper)s %(scons_flags)s --profile=%(prof2)s %(targets2)s',
    ]

    stages = [
        'pre-read',
        'post-read',
        'pre-build',
        'post-build',
    ]

    stage_strings = {
        'pre-read'      : 'Memory before reading SConscript files:',
        'post-read'     : 'Memory after reading SConscript files:',
        'pre-build'     : 'Memory before building targets:',
        'post-build'    : 'Memory after building targets:',
    }

    memory_string_all = 'Memory '

    default_stage = stages[-1]

    time_strings = {
        'total'         : 'Total build time',
        'SConscripts'   : 'Total SConscript file execution time',
        'SCons'         : 'Total SCons execution time',
        'commands'      : 'Total command execution time',
    }
    
    time_string_all = 'Total .* time'

    #

    def __init__(self):
        self.__dict__.update(self.default_settings)

    # Functions for displaying and executing commands.

    def subst(self, x, dictionary):
        try:
            return x % dictionary
        except TypeError:
            # x isn't a string (it's probably a Python function),
            # so just return it.
            return x

    def subst_variables(self, command, dictionary):
        """
        Substitutes (via the format operator) the values in the specified
        dictionary into the specified command.

        The command can be an (action, string) tuple.  In all cases, we
        perform substitution on strings and don't worry if something isn't
        a string.  (It's probably a Python function to be executed.)
        """
        try:
            command + ''
        except TypeError:
            action = command[0]
            string = command[1]
            args = command[2:]
        else:
            action = command
            string = action
            args = (())
        action = self.subst(action, dictionary)
        string = self.subst(string, dictionary)
        return (action, string, args)

    def _do_not_display(self, msg, *args):
        pass

    def display(self, msg, *args):
        """
        Displays the specified message.

        Each message is prepended with a standard prefix of our name
        plus the time.
        """
        if callable(msg):
            msg = msg(*args)
        else:
            msg = msg % args
        if msg is None:
            return
        fmt = '%s[%s]: %s\n'
        sys.stdout.write(fmt % (self.name, time.strftime('%H:%M:%S'), msg))

    def _do_not_execute(self, action, *args):
        pass

    def execute(self, action, *args):
        """
        Executes the specified action.

        The action is called if it's a callable Python function, and
        otherwise passed to os.system().
        """
        if callable(action):
            action(*args)
        else:
            os.system(action % args)

    def run_command_list(self, commands, dict):
        """
        Executes a list of commands, substituting values from the
        specified dictionary.
        """
        commands = [ self.subst_variables(c, dict) for c in commands ]
        for action, string, args in commands:
            self.display(string, *args)
            sys.stdout.flush()
            status = self.execute(action, *args)
            if status:
                sys.exit(status)

    def log_display(self, command, log):
        command = self.subst(command, self.__dict__)
        if log:
            command = self.redirect(command, log)
        return command

    def log_execute(self, command, log):
        command = self.subst(command, self.__dict__)
        output = os.popen(command).read()
        if self.verbose:
            sys.stdout.write(output)
        open(log, 'wb').write(output)

    #

    def archive_splitext(self, path):
        """
        Splits an archive name into a filename base and extension.

        This is like os.path.splitext() (which it calls) except that it
        also looks for '.tar.gz' and treats it as an atomic extensions.
        """
        if path.endswith('.tar.gz'):
            return path[:-7], path[-7:]
        else:
            return os.path.splitext(path)

    def args_to_files(self, args, tail=None):
        """
        Takes a list of arguments, expands any glob patterns, and
        returns the last "tail" files from the list.
        """
        files = []
        for a in args:
            g = glob.glob(a)
            g.sort()
            files.extend(g)

        if tail:
            files = files[-tail:]

        return files

    def ascii_table(self, files, columns,
                    line_function, file_function=lambda x: x,
                    *args, **kw):

        header_fmt = ' '.join(['%12s'] * len(columns))
        line_fmt = header_fmt + '    %s'

        print header_fmt % columns

        for file in files:
            t = line_function(file, *args, **kw)
            if t is None:
                t = []
            diff = len(columns) - len(t)
            if diff > 0:
                t += [''] * diff
            t.append(file_function(file))
            print line_fmt % tuple(t)

    def collect_results(self, files, function, *args, **kw):
        results = {}

        for file in files:
            base = os.path.splitext(file)[0]
            run, index = string.split(base, '-')[-2:]

            run = int(run)
            index = int(index)

            value = function(file, *args, **kw)

            try:
                r = results[index]
            except KeyError:
                r = []
                results[index] = r
            r.append((run, value))

        return results

    def doc_to_help(self, obj):
        """
        Translates an object's __doc__ string into help text.

        This strips a consistent number of spaces from each line in the
        help text, essentially "outdenting" the text to the left-most
        column.
        """
        doc = obj.__doc__
        if doc is None:
            return ''
        return self.outdent(doc)

    def find_next_run_number(self, dir, prefix):
        """
        Returns the next run number in a directory for the specified prefix.

        Examines the contents the specified directory for files with the
        specified prefix, extracts the run numbers from each file name,
        and returns the next run number after the largest it finds.
        """
        x = re.compile(re.escape(prefix) + '-([0-9]+).*')
        matches = map(lambda e, x=x: x.match(e), os.listdir(dir))
        matches = filter(None, matches)
        if not matches:
            return 0
        run_numbers = map(lambda m: int(m.group(1)), matches)
        return int(max(run_numbers)) + 1

    def gnuplot_results(self, results, fmt='%s %.3f'):
        """
        Prints out a set of results in Gnuplot format.
        """
        gp = Gnuplotter(self.title, self.key_location)

        indices = results.keys()
        indices.sort()

        for i in indices:
            try:
                t = self.run_titles[i]
            except IndexError:
                t = '??? %s ???' % i
            results[i].sort()
            gp.line(results[i], i+1, t, None, t, fmt=fmt)

        for bar_tuple in self.vertical_bars:
            try:
                x, type, label, comment = bar_tuple
            except ValueError:
                x, type, label = bar_tuple
                comment = label
            gp.vertical_bar(x, type, label, comment)

        gp.draw()

    def logfile_name(self, invocation):
        """
        Returns the absolute path of a log file for the specificed
        invocation number.
        """
        name = self.prefix_run + '-%d.log' % invocation
        return os.path.join(self.outdir, name)

    def outdent(self, s):
        """
        Strip as many spaces from each line as are found at the beginning
        of the first line in the list.
        """
        lines = s.split('\n')
        if lines[0] == '':
            lines = lines[1:]
        spaces = re.match(' *', lines[0]).group(0)
        def strip_initial_spaces(l, s=spaces):
            if l.startswith(spaces):
                l = l[len(spaces):]
            return l
        return '\n'.join([ strip_initial_spaces(l) for l in lines ]) + '\n'

    def profile_name(self, invocation):
        """
        Returns the absolute path of a profile file for the specified
        invocation number.
        """
        name = self.prefix_run + '-%d.prof' % invocation
        return os.path.join(self.outdir, name)

    def set_env(self, key, value):
        os.environ[key] = value

    #

    def get_debug_times(self, file, time_string=None):
        """
        Fetch times from the --debug=time strings in the specified file.
        """
        if time_string is None:
            search_string = self.time_string_all
        else:
            search_string = time_string
        contents = open(file).read()
        if not contents:
            sys.stderr.write('file %s has no contents!\n' % repr(file))
            return None
        result = re.findall(r'%s: ([\d\.]*)' % search_string, contents)[-4:]
        result = [ float(r) for r in result ]
        if not time_string is None:
            try:
                result = result[0]
            except IndexError:
                sys.stderr.write('file %s has no results!\n' % repr(file))
                return None
        return result

    def get_function_profile(self, file, function):
        """
        Returns the file, line number, function name, and cumulative time.
        """
        try:
            import pstats
        except ImportError, e:
            sys.stderr.write('%s: func: %s\n' % (self.name, e))
            sys.stderr.write('%s  This version of Python is missing the profiler.\n' % self.name_spaces)
            sys.stderr.write('%s  Cannot use the "func" subcommand.\n' % self.name_spaces)
            sys.exit(1)
        statistics = pstats.Stats(file).stats
        matches = [ e for e in statistics.items() if e[0][2] == function ]
        r = matches[0]
        return r[0][0], r[0][1], r[0][2], r[1][3]

    def get_function_time(self, file, function):
        """
        Returns just the cumulative time for the specified function.
        """
        return self.get_function_profile(file, function)[3]

    def get_memory(self, file, memory_string=None):
        """
        Returns a list of integers of the amount of memory used.  The
        default behavior is to return all the stages.
        """
        if memory_string is None:
            search_string = self.memory_string_all
        else:
            search_string = memory_string
        lines = open(file).readlines()
        lines = [ l for l in lines if l.startswith(search_string) ][-4:]
        result = [ int(l.split()[-1]) for l in lines[-4:] ]
        if len(result) == 1:
            result = result[0]
        return result

    def get_object_counts(self, file, object_name, index=None):
        """
        Returns the counts of the specified object_name.
        """
        object_string = ' ' + object_name + '\n'
        lines = open(file).readlines()
        line = [ l for l in lines if l.endswith(object_string) ][0]
        result = [ int(field) for field in line.split()[:4] ]
        if not index is None:
            result = result[index]
        return result

    #

    command_alias = {}

    def execute_subcommand(self, argv):
        """
        Executes the do_*() function for the specified subcommand (argv[0]).
        """
        if not argv:
            return
        cmdName = self.command_alias.get(argv[0], argv[0])
        try:
            func = getattr(self, 'do_' + cmdName)
        except AttributeError:
            return self.default(argv)
        try:
            return func(argv)
        except TypeError, e:
            sys.stderr.write("%s %s: %s\n" % (self.name, cmdName, e))
            import traceback
            traceback.print_exc(file=sys.stderr)
            sys.stderr.write("Try '%s help %s'\n" % (self.name, cmdName))

    def default(self, argv):
        """
        The default behavior for an unknown subcommand.  Prints an
        error message and exits.
        """
        sys.stderr.write('%s: Unknown subcommand "%s".\n' % (self.name, argv[0]))
        sys.stderr.write('Type "%s help" for usage.\n' % self.name)
        sys.exit(1)

    #

    def do_help(self, argv):
        """
        """
        if argv[1:]:
            for arg in argv[1:]:
                try:
                    func = getattr(self, 'do_' + arg)
                except AttributeError:
                    sys.stderr.write('%s: No help for "%s"\n' % (self.name, arg))
                else:
                    try:
                        help = getattr(self, 'help_' + arg)
                    except AttributeError:
                        sys.stdout.write(self.doc_to_help(func))
                        sys.stdout.flush()
                    else:
                        help()
        else:
            doc = self.doc_to_help(self.__class__)
            if doc:
                sys.stdout.write(doc)
            sys.stdout.flush()
            return None

    #

    def help_func(self):
        help = """\
        Usage: scons-time func [OPTIONS] FILE [...]

          -C DIR, --chdir=DIR           Change to DIR before looking for files
          -f FILE, --file=FILE          Read configuration from specified FILE
          --fmt=FORMAT, --format=FORMAT Print data in specified FORMAT
          --func=NAME, --function=NAME  Report time for function NAME
          -h, --help                    Print this help and exit
          -p STRING, --prefix=STRING    Use STRING as log file/profile prefix
          -t NUMBER, --tail=NUMBER      Only report the last NUMBER files
          --title=TITLE                 Specify the output plot TITLE
        """
        sys.stdout.write(self.outdent(help))
        sys.stdout.flush()

    def do_func(self, argv):
        """
        """
        format = 'ascii'
        function_name = '_main'
        tail = None

        short_opts = '?C:f:hp:t:'

        long_opts = [
            'chdir=',
            'file=',
            'fmt=',
            'format=',
            'func=',
            'function=',
            'help',
            'prefix=',
            'tail=',
            'title=',
        ]

        opts, args = getopt.getopt(argv[1:], short_opts, long_opts)

        for o, a in opts:
            if o in ('-C', '--chdir'):
                self.chdir = a
            elif o in ('-f', '--file'):
                self.config_file = a
            elif o in ('--fmt', '--format'):
                format = a
            elif o in ('--func', '--function'):
                function_name = a
            elif o in ('-?', '-h', '--help'):
                self.do_help(['help', 'func'])
                sys.exit(0)
            elif o in ('--max',):
                max_time = int(a)
            elif o in ('-p', '--prefix'):
                self.prefix = a
            elif o in ('-t', '--tail'):
                tail = int(a)
            elif o in ('--title',):
                self.title = a

        if self.config_file:
            execfile(self.config_file, self.__dict__)

        if self.chdir:
            os.chdir(self.chdir)

        if not args:

            pattern = '%s*.prof' % self.prefix
            args = self.args_to_files([pattern], tail)

            if not args:
                if self.chdir:
                    directory = self.chdir
                else:
                    directory = os.getcwd()

                sys.stderr.write('%s: func: No arguments specified.\n' % self.name)
                sys.stderr.write('%s  No %s*.prof files found in "%s".\n' % (self.name_spaces, self.prefix, directory))
                sys.stderr.write('%s  Type "%s help func" for help.\n' % (self.name_spaces, self.name))
                sys.exit(1)

        else:

            args = self.args_to_files(args, tail)

        cwd_ = os.getcwd() + os.sep

        if format == 'ascii':

            def print_function_timing(file, func):
                try:
                    f, line, func, time = self.get_function_profile(file, func)
                except ValueError, e:
                    sys.stderr.write("%s: func: %s: %s\n" % (self.name, file, e))
                else:
                    if f.startswith(cwd_):
                        f = f[len(cwd_):]
                    print "%.3f %s:%d(%s)" % (time, f, line, func)

            for file in args:
                print_function_timing(file, function_name)

        elif format == 'gnuplot':

            results = self.collect_results(args, self.get_function_time,
                                           function_name)

            self.gnuplot_results(results)

        else:

            sys.stderr.write('%s: func: Unknown format "%s".\n' % (self.name, format))
            sys.exit(1)

    #

    def help_mem(self):
        help = """\
        Usage: scons-time mem [OPTIONS] FILE [...]

          -C DIR, --chdir=DIR           Change to DIR before looking for files
          -f FILE, --file=FILE          Read configuration from specified FILE
          --fmt=FORMAT, --format=FORMAT Print data in specified FORMAT
          -h, --help                    Print this help and exit
          -p STRING, --prefix=STRING    Use STRING as log file/profile prefix
          --stage=STAGE                 Plot memory at the specified stage:
                                          pre-read, post-read, pre-build,
                                          post-build (default: post-build)
          -t NUMBER, --tail=NUMBER      Only report the last NUMBER files
          --title=TITLE                 Specify the output plot TITLE
        """
        sys.stdout.write(self.outdent(help))
        sys.stdout.flush()

    def do_mem(self, argv):

        format = 'ascii'
        logfile_path = lambda x: x
        stage = self.default_stage
        tail = None

        short_opts = '?C:f:hp:t:'

        long_opts = [
            'chdir=',
            'file=',
            'fmt=',
            'format=',
            'help',
            'prefix=',
            'stage=',
            'tail=',
            'title=',
        ]

        opts, args = getopt.getopt(argv[1:], short_opts, long_opts)

        for o, a in opts:
            if o in ('-C', '--chdir'):
                self.chdir = a
            elif o in ('-f', '--file'):
                self.config_file = a
            elif o in ('--fmt', '--format'):
                format = a
            elif o in ('-?', '-h', '--help'):
                self.do_help(['help', 'mem'])
                sys.exit(0)
            elif o in ('-p', '--prefix'):
                self.prefix = a
            elif o in ('--stage',):
                if not a in self.stages:
                    sys.stderr.write('%s: mem: Unrecognized stage "%s".\n' % (self.name, a))
                    sys.exit(1)
                stage = a
            elif o in ('-t', '--tail'):
                tail = int(a)
            elif o in ('--title',):
                self.title = a

        if self.config_file:
            execfile(self.config_file, self.__dict__)

        if self.chdir:
            os.chdir(self.chdir)
            logfile_path = lambda x, c=self.chdir: os.path.join(c, x)

        if not args:

            pattern = '%s*.log' % self.prefix
            args = self.args_to_files([pattern], tail)

            if not args:
                if self.chdir:
                    directory = self.chdir
                else:
                    directory = os.getcwd()

                sys.stderr.write('%s: mem: No arguments specified.\n' % self.name)
                sys.stderr.write('%s  No %s*.log files found in "%s".\n' % (self.name_spaces, self.prefix, directory))
                sys.stderr.write('%s  Type "%s help mem" for help.\n' % (self.name_spaces, self.name))
                sys.exit(1)

        else:

            args = self.args_to_files(args, tail)

        cwd_ = os.getcwd() + os.sep

        if format == 'ascii':

            self.ascii_table(args, tuple(self.stages), self.get_memory, logfile_path)

        elif format == 'gnuplot':

            results = self.collect_results(args, self.get_memory,
                                           self.stage_strings[stage])

            self.gnuplot_results(results)

        else:

            sys.stderr.write('%s: mem: Unknown format "%s".\n' % (self.name, format))
            sys.exit(1)

        return 0

    #

    def help_obj(self):
        help = """\
        Usage: scons-time obj [OPTIONS] OBJECT FILE [...]

          -C DIR, --chdir=DIR           Change to DIR before looking for files
          -f FILE, --file=FILE          Read configuration from specified FILE
          --fmt=FORMAT, --format=FORMAT Print data in specified FORMAT
          -h, --help                    Print this help and exit
          -p STRING, --prefix=STRING    Use STRING as log file/profile prefix
          --stage=STAGE                 Plot memory at the specified stage:
                                          pre-read, post-read, pre-build,
                                          post-build (default: post-build)
          -t NUMBER, --tail=NUMBER      Only report the last NUMBER files
          --title=TITLE                 Specify the output plot TITLE
        """
        sys.stdout.write(self.outdent(help))
        sys.stdout.flush()

    def do_obj(self, argv):

        format = 'ascii'
        logfile_path = lambda x: x
        stage = self.default_stage
        tail = None

        short_opts = '?C:f:hp:t:'

        long_opts = [
            'chdir=',
            'file=',
            'fmt=',
            'format=',
            'help',
            'prefix=',
            'stage=',
            'tail=',
            'title=',
        ]

        opts, args = getopt.getopt(argv[1:], short_opts, long_opts)

        for o, a in opts:
            if o in ('-C', '--chdir'):
                self.chdir = a
            elif o in ('-f', '--file'):
                self.config_file = a
            elif o in ('--fmt', '--format'):
                format = a
            elif o in ('-?', '-h', '--help'):
                self.do_help(['help', 'obj'])
                sys.exit(0)
            elif o in ('-p', '--prefix'):
                self.prefix = a
            elif o in ('--stage',):
                if not a in self.stages:
                    sys.stderr.write('%s: obj: Unrecognized stage "%s".\n' % (self.name, a))
                    sys.stderr.write('%s       Type "%s help obj" for help.\n' % (self.name_spaces, self.name))
                    sys.exit(1)
                stage = a
            elif o in ('-t', '--tail'):
                tail = int(a)
            elif o in ('--title',):
                self.title = a

        if not args:
            sys.stderr.write('%s: obj: Must specify an object name.\n' % self.name)
            sys.stderr.write('%s       Type "%s help obj" for help.\n' % (self.name_spaces, self.name))
            sys.exit(1)

        object_name = args.pop(0)

        if self.config_file:
            execfile(self.config_file, self.__dict__)

        if self.chdir:
            os.chdir(self.chdir)
            logfile_path = lambda x, c=self.chdir: os.path.join(c, x)

        if not args:

            pattern = '%s*.log' % self.prefix
            args = self.args_to_files([pattern], tail)

            if not args:
                if self.chdir:
                    directory = self.chdir
                else:
                    directory = os.getcwd()

                sys.stderr.write('%s: obj: No arguments specified.\n' % self.name)
                sys.stderr.write('%s  No %s*.log files found in "%s".\n' % (self.name_spaces, self.prefix, directory))
                sys.stderr.write('%s  Type "%s help obj" for help.\n' % (self.name_spaces, self.name))
                sys.exit(1)

        else:

            args = self.args_to_files(args, tail)

        cwd_ = os.getcwd() + os.sep

        if format == 'ascii':

            self.ascii_table(args, tuple(self.stages), self.get_object_counts, logfile_path, object_name)

        elif format == 'gnuplot':

            stage_index = 0
            for s in self.stages:
                if stage == s:
                    break
                stage_index = stage_index + 1

            results = self.collect_results(args, self.get_object_counts,
                                           object_name, stage_index)

            self.gnuplot_results(results)

        else:

            sys.stderr.write('%s: obj: Unknown format "%s".\n' % (self.name, format))
            sys.exit(1)

        return 0

    #

    def help_run(self):
        help = """\
        Usage: scons-time run [OPTIONS] [FILE ...]

          --aegis=PROJECT               Use SCons from the Aegis PROJECT
          --chdir=DIR                   Name of unpacked directory for chdir
          -f FILE, --file=FILE          Read configuration from specified FILE
          -h, --help                    Print this help and exit
          -n, --no-exec                 No execute, just print command lines
          --number=NUMBER               Put output in files for run NUMBER
          --outdir=OUTDIR               Put output files in OUTDIR
          -p STRING, --prefix=STRING    Use STRING as log file/profile prefix
          --python=PYTHON               Time using the specified PYTHON
          -q, --quiet                   Don't print command lines
          --scons=SCONS                 Time using the specified SCONS
          --svn=URL, --subversion=URL   Use SCons from Subversion URL
          -v, --verbose                 Display output of commands
        """
        sys.stdout.write(self.outdent(help))
        sys.stdout.flush()

    def do_run(self, argv):
        """
        """
        run_number_list = [None]

        short_opts = '?f:hnp:qs:v'

        long_opts = [
            'aegis=',
            'file=',
            'help',
            'no-exec',
            'number=',
            'outdir=',
            'prefix=',
            'python=',
            'quiet',
            'scons=',
            'svn=',
            'subdir=',
            'subversion=',
            'verbose',
        ]

        opts, args = getopt.getopt(argv[1:], short_opts, long_opts)

        for o, a in opts:
            if o in ('--aegis',):
                self.aegis_project = a
            elif o in ('-f', '--file'):
                self.config_file = a
            elif o in ('-?', '-h', '--help'):
                self.do_help(['help', 'run'])
                sys.exit(0)
            elif o in ('-n', '--no-exec'):
                self.execute = self._do_not_execute
            elif o in ('--number',):
                run_number_list = self.split_run_numbers(a)
            elif o in ('--outdir',):
                self.outdir = a
            elif o in ('-p', '--prefix'):
                self.prefix = a
            elif o in ('--python',):
                self.python = a
            elif o in ('-q', '--quiet'):
                self.display = self._do_not_display
            elif o in ('-s', '--subdir'):
                self.subdir = a
            elif o in ('--scons',):
                self.scons = a
            elif o in ('--svn', '--subversion'):
                self.subversion_url = a
            elif o in ('-v', '--verbose'):
                self.redirect = tee_to_file
                self.verbose = True
                self.svn_co_flag = ''

        if not args and not self.config_file:
            sys.stderr.write('%s: run: No arguments or -f config file specified.\n' % self.name)
            sys.stderr.write('%s  Type "%s help run" for help.\n' % (self.name_spaces, self.name))
            sys.exit(1)

        if self.config_file:
            execfile(self.config_file, self.__dict__)

        if args:
            self.archive_list = args

        archive_file_name = os.path.split(self.archive_list[0])[1]

        if not self.subdir:
            self.subdir = self.archive_splitext(archive_file_name)[0]

        if not self.prefix:
            self.prefix = self.archive_splitext(archive_file_name)[0]

        prepare = None
        if self.subversion_url:
            prepare = self.prep_subversion_run
        elif self.aegis_project:
            prepare = self.prep_aegis_run

        for run_number in run_number_list:
            self.individual_run(run_number, self.archive_list, prepare)

    def split_run_numbers(self, s):
        result = []
        for n in s.split(','):
            try:
                x, y = n.split('-')
            except ValueError:
                result.append(int(n))
            else:
                result.extend(range(int(x), int(y)+1))
        return result

    def scons_path(self, dir):
        return os.path.join(dir, 'src', 'script', 'scons.py')

    def scons_lib_dir_path(self, dir):
        return os.path.join(dir, 'src', 'engine')

    def prep_aegis_run(self, commands, removals):
        self.aegis_tmpdir = make_temp_file(prefix = self.name + '-aegis-')
        removals.append((shutil.rmtree, 'rm -rf %%s', self.aegis_tmpdir))

        self.aegis_parent_project = os.path.splitext(self.aegis_project)[0]
        self.scons = self.scons_path(self.aegis_tmpdir)
        self.scons_lib_dir = self.scons_lib_dir_path(self.aegis_tmpdir)

        commands.extend([
            'mkdir %(aegis_tmpdir)s',
            (lambda: os.chdir(self.aegis_tmpdir), 'cd %(aegis_tmpdir)s'),
            '%(aegis)s -cp -ind -p %(aegis_parent_project)s .',
            '%(aegis)s -cp -ind -p %(aegis_project)s -delta %(run_number)s .',
        ])

    def prep_subversion_run(self, commands, removals):
        self.svn_tmpdir = make_temp_file(prefix = self.name + '-svn-')
        removals.append((shutil.rmtree, 'rm -rf %%s', self.svn_tmpdir))

        self.scons = self.scons_path(self.svn_tmpdir)
        self.scons_lib_dir = self.scons_lib_dir_path(self.svn_tmpdir)

        commands.extend([
            'mkdir %(svn_tmpdir)s',
            '%(svn)s co %(svn_co_flag)s -r %(run_number)s %(subversion_url)s %(svn_tmpdir)s',
        ])

    def individual_run(self, run_number, archive_list, prepare=None):
        """
        Performs an individual run of the default SCons invocations.
        """

        commands = []
        removals = []

        if prepare:
            prepare(commands, removals)

        save_scons              = self.scons
        save_scons_wrapper      = self.scons_wrapper
        save_scons_lib_dir      = self.scons_lib_dir

        if self.outdir is None:
            self.outdir = self.orig_cwd
        elif not os.path.isabs(self.outdir):
            self.outdir = os.path.join(self.orig_cwd, self.outdir)

        if self.scons is None:
            self.scons = self.scons_path(self.orig_cwd)

        if self.scons_lib_dir is None:
            self.scons_lib_dir = self.scons_lib_dir_path(self.orig_cwd)

        if self.scons_wrapper is None:
            self.scons_wrapper = self.scons

        if not run_number:
            run_number = self.find_next_run_number(self.outdir, self.prefix)

        self.run_number = str(run_number)

        self.prefix_run = self.prefix + '-%03d' % run_number

        if self.targets0 is None:
            self.targets0 = self.startup_targets
        if self.targets1 is None:
            self.targets1 = self.targets
        if self.targets2 is None:
            self.targets2 = self.targets

        self.tmpdir = make_temp_file(prefix = self.name + '-')

        commands.extend([
            'mkdir %(tmpdir)s',

            (os.chdir, 'cd %%s', self.tmpdir),
        ])

        for archive in archive_list:
            if not os.path.isabs(archive):
                archive = os.path.join(self.orig_cwd, archive)
            if os.path.isdir(archive):
                dest = os.path.split(archive)[1]
                commands.append((shutil.copytree, 'cp -r %%s %%s', archive, dest))
            else:
                suffix = self.archive_splitext(archive)[1]
                unpack_command = self.unpack_map.get(suffix)
                if not unpack_command:
                    dest = os.path.split(archive)[1]
                    commands.append((shutil.copyfile, 'cp %%s %%s', archive, dest))
                else:
                    commands.append(unpack_command + (archive,))

        commands.extend([
            (os.chdir, 'cd %%s', self.subdir),
        ])

        commands.extend(self.initial_commands)

        commands.extend([
            (lambda: read_tree('.'),
            'find * -type f | xargs cat > /dev/null'),

            (self.set_env, 'export %%s=%%s',
             'SCONS_LIB_DIR', self.scons_lib_dir),

            '%(python)s %(scons_wrapper)s --version',
        ])

        index = 0
        for run_command in self.run_commands:
            setattr(self, 'prof%d' % index, self.profile_name(index))
            c = (
                self.log_execute,
                self.log_display,
                run_command,
                self.logfile_name(index),
            )
            commands.append(c)
            index = index + 1

        commands.extend([
            (os.chdir, 'cd %%s', self.orig_cwd),
        ])

        if not os.environ.get('PRESERVE'):
            commands.extend(removals)

            commands.append((shutil.rmtree, 'rm -rf %%s', self.tmpdir))

        self.run_command_list(commands, self.__dict__)

        self.scons              = save_scons
        self.scons_lib_dir      = save_scons_lib_dir
        self.scons_wrapper      = save_scons_wrapper

    #

    def help_time(self):
        help = """\
        Usage: scons-time time [OPTIONS] FILE [...]

          -C DIR, --chdir=DIR           Change to DIR before looking for files
          -f FILE, --file=FILE          Read configuration from specified FILE
          --fmt=FORMAT, --format=FORMAT Print data in specified FORMAT
          -h, --help                    Print this help and exit
          -p STRING, --prefix=STRING    Use STRING as log file/profile prefix
          -t NUMBER, --tail=NUMBER      Only report the last NUMBER files
          --which=TIMER                 Plot timings for TIMER:  total,
                                          SConscripts, SCons, commands.
        """
        sys.stdout.write(self.outdent(help))
        sys.stdout.flush()

    def do_time(self, argv):

        format = 'ascii'
        logfile_path = lambda x: x
        tail = None
        which = 'total'

        short_opts = '?C:f:hp:t:'

        long_opts = [
            'chdir=',
            'file=',
            'fmt=',
            'format=',
            'help',
            'prefix=',
            'tail=',
            'title=',
            'which=',
        ]

        opts, args = getopt.getopt(argv[1:], short_opts, long_opts)

        for o, a in opts:
            if o in ('-C', '--chdir'):
                self.chdir = a
            elif o in ('-f', '--file'):
                self.config_file = a
            elif o in ('--fmt', '--format'):
                format = a
            elif o in ('-?', '-h', '--help'):
                self.do_help(['help', 'time'])
                sys.exit(0)
            elif o in ('-p', '--prefix'):
                self.prefix = a
            elif o in ('-t', '--tail'):
                tail = int(a)
            elif o in ('--title',):
                self.title = a
            elif o in ('--which',):
                if not a in self.time_strings.keys():
                    sys.stderr.write('%s: time: Unrecognized timer "%s".\n' % (self.name, a))
                    sys.stderr.write('%s  Type "%s help time" for help.\n' % (self.name_spaces, self.name))
                    sys.exit(1)
                which = a

        if self.config_file:
            execfile(self.config_file, self.__dict__)

        if self.chdir:
            os.chdir(self.chdir)
            logfile_path = lambda x, c=self.chdir: os.path.join(c, x)

        if not args:

            pattern = '%s*.log' % self.prefix
            args = self.args_to_files([pattern], tail)

            if not args:
                if self.chdir:
                    directory = self.chdir
                else:
                    directory = os.getcwd()

                sys.stderr.write('%s: time: No arguments specified.\n' % self.name)
                sys.stderr.write('%s  No %s*.log files found in "%s".\n' % (self.name_spaces, self.prefix, directory))
                sys.stderr.write('%s  Type "%s help time" for help.\n' % (self.name_spaces, self.name))
                sys.exit(1)

        else:

            args = self.args_to_files(args, tail)

        cwd_ = os.getcwd() + os.sep

        if format == 'ascii':

            columns = ("Total", "SConscripts", "SCons", "commands")
            self.ascii_table(args, columns, self.get_debug_times, logfile_path)

        elif format == 'gnuplot':

            results = self.collect_results(args, self.get_debug_times,
                                           self.time_strings[which])

            self.gnuplot_results(results, fmt='%s %.6f')

        else:

            sys.stderr.write('%s: time: Unknown format "%s".\n' % (self.name, format))
            sys.exit(1)

if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], 'h?V', ['help', 'version'])

    ST = SConsTimer()

    for o, a in opts:
        if o in ('-?', '-h', '--help'):
            ST.do_help(['help'])
            sys.exit(0)
        elif o in ('-V', '--version'):
            sys.stdout.write('scons-time version\n')
            sys.exit(0)

    if not args:
        sys.stderr.write('Type "%s help" for usage.\n' % ST.name)
        sys.exit(1)

    ST.execute_subcommand(args)
