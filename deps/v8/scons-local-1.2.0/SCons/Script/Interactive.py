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

__revision__ = "src/engine/SCons/Script/Interactive.py 3842 2008/12/20 22:59:52 scons"

__doc__ = """
SCons interactive mode
"""

# TODO:
#
# This has the potential to grow into something with a really big life
# of its own, which might or might not be a good thing.  Nevertheless,
# here are some enhancements that will probably be requested some day
# and are worth keeping in mind (assuming this takes off):
# 
# - A command to re-read / re-load the SConscript files.  This may
#   involve allowing people to specify command-line options (e.g. -f,
#   -I, --no-site-dir) that affect how the SConscript files are read.
#
# - Additional command-line options on the "build" command.
#
#   Of the supported options that seemed to make sense (after a quick
#   pass through the list), the ones that seemed likely enough to be
#   used are listed in the man page and have explicit test scripts.
#
#   These had code changed in Script/Main.py to support them, but didn't
#   seem likely to be used regularly, so had no test scripts added:
#
#       build --diskcheck=*
#       build --implicit-cache=*
#       build --implicit-deps-changed=*
#       build --implicit-deps-unchanged=*
#
#   These look like they should "just work" with no changes to the
#   existing code, but like those above, look unlikely to be used and
#   therefore had no test scripts added:
#
#       build --random
#
#   These I'm not sure about.  They might be useful for individual
#   "build" commands, and may even work, but they seem unlikely enough
#   that we'll wait until they're requested before spending any time on
#   writing test scripts for them, or investigating whether they work.
#
#       build -q [???  is there a useful analog to the exit status?]
#       build --duplicate=
#       build --profile=
#       build --max-drift=
#       build --warn=*
#       build --Y
#
# - Most of the SCons command-line options that the "build" command
#   supports should be settable as default options that apply to all
#   subsequent "build" commands.  Maybe a "set {option}" command that
#   maps to "SetOption('{option}')".
#
# - Need something in the 'help' command that prints the -h output.
#
# - A command to run the configure subsystem separately (must see how
#   this interacts with the new automake model).
#
# - Command-line completion of target names; maybe even of SCons options?
#   Completion is something that's supported by the Python cmd module,
#   so this should be doable without too much trouble.
#

import cmd
import copy
import os
import re
import shlex
import string
import sys

try:
    import readline
except ImportError:
    pass

class SConsInteractiveCmd(cmd.Cmd):
    """\
    build [TARGETS]         Build the specified TARGETS and their dependencies.
                            'b' is a synonym.
    clean [TARGETS]         Clean (remove) the specified TARGETS and their
                            dependencies.  'c' is a synonym.
    exit                    Exit SCons interactive mode.
    help [COMMAND]          Prints help for the specified COMMAND.  'h' and
                            '?' are synonyms.
    shell [COMMANDLINE]     Execute COMMANDLINE in a subshell.  'sh' and '!'
                            are synonyms.
    version                 Prints SCons version information.
    """

    synonyms = {
        'b'     : 'build',
        'c'     : 'clean',
        'h'     : 'help',
        'scons' : 'build',
        'sh'    : 'shell',
    }

    def __init__(self, **kw):
        cmd.Cmd.__init__(self)
        for key, val in kw.items():
            setattr(self, key, val)

        if sys.platform == 'win32':
            self.shell_variable = 'COMSPEC'
        else:
            self.shell_variable = 'SHELL'

    def default(self, argv):
        print "*** Unknown command: %s" % argv[0]

    def onecmd(self, line):
        line = string.strip(line)
        if not line:
            print self.lastcmd
            return self.emptyline()
        self.lastcmd = line
        if line[0] == '!':
            line = 'shell ' + line[1:]
        elif line[0] == '?':
            line = 'help ' + line[1:]
        if os.sep == '\\':
            line = string.replace(line, '\\', '\\\\')
        argv = shlex.split(line)
        argv[0] = self.synonyms.get(argv[0], argv[0])
        if not argv[0]:
            return self.default(line)
        else:
            try:
                func = getattr(self, 'do_' + argv[0])
            except AttributeError:
                return self.default(argv)
            return func(argv)

    def do_build(self, argv):
        """\
        build [TARGETS]         Build the specified TARGETS and their
                                dependencies.  'b' is a synonym.
        """
        import SCons.Node
        import SCons.SConsign
        import SCons.Script.Main

        options = copy.deepcopy(self.options)

        options, targets = self.parser.parse_args(argv[1:], values=options)

        SCons.Script.COMMAND_LINE_TARGETS = targets

        if targets:
            SCons.Script.BUILD_TARGETS = targets
        else:
            # If the user didn't specify any targets on the command line,
            # use the list of default targets.
            SCons.Script.BUILD_TARGETS = SCons.Script._build_plus_default

        nodes = SCons.Script.Main._build_targets(self.fs,
                                                 options,
                                                 targets,
                                                 self.target_top)

        if not nodes:
            return

        # Call each of the Node's alter_targets() methods, which may
        # provide additional targets that ended up as part of the build
        # (the canonical example being a VariantDir() when we're building
        # from a source directory) and which we therefore need their
        # state cleared, too.
        x = []
        for n in nodes:
            x.extend(n.alter_targets()[0])
        nodes.extend(x)

        # Clean up so that we can perform the next build correctly.
        #
        # We do this by walking over all the children of the targets,
        # and clearing their state.
        #
        # We currently have to re-scan each node to find their
        # children, because built nodes have already been partially
        # cleared and don't remember their children.  (In scons
        # 0.96.1 and earlier, this wasn't the case, and we didn't
        # have to re-scan the nodes.)
        #
        # Because we have to re-scan each node, we can't clear the
        # nodes as we walk over them, because we may end up rescanning
        # a cleared node as we scan a later node.  Therefore, only
        # store the list of nodes that need to be cleared as we walk
        # the tree, and clear them in a separate pass.
        #
        # XXX: Someone more familiar with the inner workings of scons
        # may be able to point out a more efficient way to do this.

        SCons.Script.Main.progress_display("scons: Clearing cached node information ...")

        seen_nodes = {}

        def get_unseen_children(node, parent, seen_nodes=seen_nodes):
            def is_unseen(node, seen_nodes=seen_nodes):
                return not seen_nodes.has_key(node)
            return filter(is_unseen, node.children(scan=1))

        def add_to_seen_nodes(node, parent, seen_nodes=seen_nodes):
            seen_nodes[node] = 1

            # If this file is in a VariantDir and has a
            # corresponding source file in the source tree, remember the
            # node in the source tree, too.  This is needed in
            # particular to clear cached implicit dependencies on the
            # source file, since the scanner will scan it if the
            # VariantDir was created with duplicate=0.
            try:
                rfile_method = node.rfile
            except AttributeError:
                return
            else:
                rfile = rfile_method()
            if rfile != node:
                seen_nodes[rfile] = 1

        for node in nodes:
            walker = SCons.Node.Walker(node,
                                        kids_func=get_unseen_children,
                                        eval_func=add_to_seen_nodes)
            n = walker.next()
            while n:
                n = walker.next()

        for node in seen_nodes.keys():
            # Call node.clear() to clear most of the state
            node.clear()
            # node.clear() doesn't reset node.state, so call
            # node.set_state() to reset it manually
            node.set_state(SCons.Node.no_state)
            node.implicit = None

            # Debug:  Uncomment to verify that all Taskmaster reference
            # counts have been reset to zero.
            #if node.ref_count != 0:
            #    from SCons.Debug import Trace
            #    Trace('node %s, ref_count %s !!!\n' % (node, node.ref_count))

        SCons.SConsign.Reset()
        SCons.Script.Main.progress_display("scons: done clearing node information.")

    def do_clean(self, argv):
        """\
        clean [TARGETS]         Clean (remove) the specified TARGETS
                                and their dependencies.  'c' is a synonym.
        """
        return self.do_build(['build', '--clean'] + argv[1:])

    def do_EOF(self, argv):
        print
        self.do_exit(argv)

    def _do_one_help(self, arg):
        try:
            # If help_<arg>() exists, then call it.
            func = getattr(self, 'help_' + arg)
        except AttributeError:
            try:
                func = getattr(self, 'do_' + arg)
            except AttributeError:
                doc = None
            else:
                doc = self._doc_to_help(func)
            if doc:
                sys.stdout.write(doc + '\n')
                sys.stdout.flush()
        else:
            doc = self.strip_initial_spaces(func())
            if doc:
                sys.stdout.write(doc + '\n')
                sys.stdout.flush()

    def _doc_to_help(self, obj):
        doc = obj.__doc__
        if doc is None:
            return ''
        return self._strip_initial_spaces(doc)

    def _strip_initial_spaces(self, s):
        #lines = s.split('\n')
        lines = string.split(s, '\n')
        spaces = re.match(' *', lines[0]).group(0)
        #def strip_spaces(l):
        #    if l.startswith(spaces):
        #        l = l[len(spaces):]
        #    return l
        #return '\n'.join([ strip_spaces(l) for l in lines ])
        def strip_spaces(l, spaces=spaces):
            if l[:len(spaces)] == spaces:
                l = l[len(spaces):]
            return l
        lines = map(strip_spaces, lines)
        return string.join(lines, '\n')

    def do_exit(self, argv):
        """\
        exit                    Exit SCons interactive mode.
        """
        sys.exit(0)

    def do_help(self, argv):
        """\
        help [COMMAND]          Prints help for the specified COMMAND.  'h'
                                and '?' are synonyms.
        """
        if argv[1:]:
            for arg in argv[1:]:
                if self._do_one_help(arg):
                    break
        else:
            # If bare 'help' is called, print this class's doc
            # string (if it has one).
            doc = self._doc_to_help(self.__class__)
            if doc:
                sys.stdout.write(doc + '\n')
                sys.stdout.flush()

    def do_shell(self, argv):
        """\
        shell [COMMANDLINE]     Execute COMMANDLINE in a subshell.  'sh' and
                                '!' are synonyms.
        """
        import subprocess
        argv = argv[1:]
        if not argv:
            argv = os.environ[self.shell_variable]
        try:
            p = subprocess.Popen(argv)
        except EnvironmentError, e:
            sys.stderr.write('scons: %s: %s\n' % (argv[0], e.strerror))
        else:
            p.wait()

    def do_version(self, argv):
        """\
        version                 Prints SCons version information.
        """
        sys.stdout.write(self.parser.version + '\n')

def interact(fs, parser, options, targets, target_top):
    c = SConsInteractiveCmd(prompt = 'scons>>> ',
                            fs = fs,
                            parser = parser,
                            options = options,
                            targets = targets,
                            target_top = target_top)
    c.cmdloop()
