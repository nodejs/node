"""SCons.Executor

A module for executing actions with specific lists of target and source
Nodes.

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

__revision__ = "src/engine/SCons/Executor.py 3842 2008/12/20 22:59:52 scons"

import string

from SCons.Debug import logInstanceCreation
import SCons.Errors
import SCons.Memoize


class Executor:
    """A class for controlling instances of executing an action.

    This largely exists to hold a single association of an action,
    environment, list of environment override dictionaries, targets
    and sources for later processing as needed.
    """

    if SCons.Memoize.use_memoizer:
        __metaclass__ = SCons.Memoize.Memoized_Metaclass

    memoizer_counters = []

    def __init__(self, action, env=None, overridelist=[{}],
                 targets=[], sources=[], builder_kw={}):
        if __debug__: logInstanceCreation(self, 'Executor.Executor')
        self.set_action_list(action)
        self.pre_actions = []
        self.post_actions = []
        self.env = env
        self.overridelist = overridelist
        self.targets = targets
        self.sources = sources[:]
        self.sources_need_sorting = False
        self.builder_kw = builder_kw
        self._memo = {}

    def set_action_list(self, action):
        import SCons.Util
        if not SCons.Util.is_List(action):
            if not action:
                import SCons.Errors
                raise SCons.Errors.UserError, "Executor must have an action."
            action = [action]
        self.action_list = action

    def get_action_list(self):
        return self.pre_actions + self.action_list + self.post_actions

    memoizer_counters.append(SCons.Memoize.CountValue('get_build_env'))

    def get_build_env(self):
        """Fetch or create the appropriate build Environment
        for this Executor.
        """
        try:
            return self._memo['get_build_env']
        except KeyError:
            pass

        # Create the build environment instance with appropriate
        # overrides.  These get evaluated against the current
        # environment's construction variables so that users can
        # add to existing values by referencing the variable in
        # the expansion.
        overrides = {}
        for odict in self.overridelist:
            overrides.update(odict)

        import SCons.Defaults
        env = self.env or SCons.Defaults.DefaultEnvironment()
        build_env = env.Override(overrides)

        self._memo['get_build_env'] = build_env

        return build_env

    def get_build_scanner_path(self, scanner):
        """Fetch the scanner path for this executor's targets and sources.
        """
        env = self.get_build_env()
        try:
            cwd = self.targets[0].cwd
        except (IndexError, AttributeError):
            cwd = None
        return scanner.path(env, cwd, self.targets, self.get_sources())

    def get_kw(self, kw={}):
        result = self.builder_kw.copy()
        result.update(kw)
        return result

    def do_nothing(self, target, kw):
        return 0

    def do_execute(self, target, kw):
        """Actually execute the action list."""
        env = self.get_build_env()
        kw = self.get_kw(kw)
        status = 0
        for act in self.get_action_list():
            status = apply(act, (self.targets, self.get_sources(), env), kw)
            if isinstance(status, SCons.Errors.BuildError):
                status.executor = self
                raise status
            elif status:
                msg = "Error %s" % status
                raise SCons.Errors.BuildError(
                    errstr=msg, 
                    node=self.targets,
                    executor=self, 
                    action=act)
        return status

    # use extra indirection because with new-style objects (Python 2.2
    # and above) we can't override special methods, and nullify() needs
    # to be able to do this.

    def __call__(self, target, **kw):
        return self.do_execute(target, kw)

    def cleanup(self):
        self._memo = {}

    def add_sources(self, sources):
        """Add source files to this Executor's list.  This is necessary
        for "multi" Builders that can be called repeatedly to build up
        a source file list for a given target."""
        self.sources.extend(sources)
        self.sources_need_sorting = True

    def get_sources(self):
        if self.sources_need_sorting:
            self.sources = SCons.Util.uniquer_hashables(self.sources)
            self.sources_need_sorting = False
        return self.sources

    def prepare(self):
        """
        Preparatory checks for whether this Executor can go ahead
        and (try to) build its targets.
        """
        for s in self.get_sources():
            if s.missing():
                msg = "Source `%s' not found, needed by target `%s'."
                raise SCons.Errors.StopError, msg % (s, self.targets[0])

    def add_pre_action(self, action):
        self.pre_actions.append(action)

    def add_post_action(self, action):
        self.post_actions.append(action)

    # another extra indirection for new-style objects and nullify...

    def my_str(self):
        env = self.get_build_env()
        get = lambda action, t=self.targets, s=self.get_sources(), e=env: \
                     action.genstring(t, s, e)
        return string.join(map(get, self.get_action_list()), "\n")


    def __str__(self):
        return self.my_str()

    def nullify(self):
        self.cleanup()
        self.do_execute = self.do_nothing
        self.my_str     = lambda S=self: ''

    memoizer_counters.append(SCons.Memoize.CountValue('get_contents'))

    def get_contents(self):
        """Fetch the signature contents.  This is the main reason this
        class exists, so we can compute this once and cache it regardless
        of how many target or source Nodes there are.
        """
        try:
            return self._memo['get_contents']
        except KeyError:
            pass
        env = self.get_build_env()
        get = lambda action, t=self.targets, s=self.get_sources(), e=env: \
                     action.get_contents(t, s, e)
        result = string.join(map(get, self.get_action_list()), "")
        self._memo['get_contents'] = result
        return result

    def get_timestamp(self):
        """Fetch a time stamp for this Executor.  We don't have one, of
        course (only files do), but this is the interface used by the
        timestamp module.
        """
        return 0

    def scan_targets(self, scanner):
        self.scan(scanner, self.targets)

    def scan_sources(self, scanner):
        if self.sources:
            self.scan(scanner, self.get_sources())

    def scan(self, scanner, node_list):
        """Scan a list of this Executor's files (targets or sources) for
        implicit dependencies and update all of the targets with them.
        This essentially short-circuits an N*M scan of the sources for
        each individual target, which is a hell of a lot more efficient.
        """
        env = self.get_build_env()

        deps = []
        if scanner:
            for node in node_list:
                node.disambiguate()
                s = scanner.select(node)
                if not s:
                    continue
                path = self.get_build_scanner_path(s)
                deps.extend(node.get_implicit_deps(env, s, path))
        else:
            kw = self.get_kw()
            for node in node_list:
                node.disambiguate()
                scanner = node.get_env_scanner(env, kw)
                if not scanner:
                    continue
                scanner = scanner.select(node)
                if not scanner:
                    continue
                path = self.get_build_scanner_path(scanner)
                deps.extend(node.get_implicit_deps(env, scanner, path))

        deps.extend(self.get_implicit_deps())

        for tgt in self.targets:
            tgt.add_to_implicit(deps)

    def _get_unignored_sources_key(self, ignore=()):
        return tuple(ignore)

    memoizer_counters.append(SCons.Memoize.CountDict('get_unignored_sources', _get_unignored_sources_key))

    def get_unignored_sources(self, ignore=()):
        ignore = tuple(ignore)
        try:
            memo_dict = self._memo['get_unignored_sources']
        except KeyError:
            memo_dict = {}
            self._memo['get_unignored_sources'] = memo_dict
        else:
            try:
                return memo_dict[ignore]
            except KeyError:
                pass

        sourcelist = self.get_sources()
        if ignore:
            idict = {}
            for i in ignore:
                idict[i] = 1
            sourcelist = filter(lambda s, i=idict: not i.has_key(s), sourcelist)

        memo_dict[ignore] = sourcelist

        return sourcelist

    def _process_sources_key(self, func, ignore=()):
        return (func, tuple(ignore))

    memoizer_counters.append(SCons.Memoize.CountDict('process_sources', _process_sources_key))

    def process_sources(self, func, ignore=()):
        memo_key = (func, tuple(ignore))
        try:
            memo_dict = self._memo['process_sources']
        except KeyError:
            memo_dict = {}
            self._memo['process_sources'] = memo_dict
        else:
            try:
                return memo_dict[memo_key]
            except KeyError:
                pass

        result = map(func, self.get_unignored_sources(ignore))

        memo_dict[memo_key] = result

        return result

    def get_implicit_deps(self):
        """Return the executor's implicit dependencies, i.e. the nodes of
        the commands to be executed."""
        result = []
        build_env = self.get_build_env()
        for act in self.get_action_list():
            result.extend(act.get_implicit_deps(self.targets, self.get_sources(), build_env))
        return result

nullenv = None

def get_NullEnvironment():
    """Use singleton pattern for Null Environments."""
    global nullenv

    import SCons.Util
    class NullEnvironment(SCons.Util.Null):
        import SCons.CacheDir
        _CacheDir_path = None
        _CacheDir = SCons.CacheDir.CacheDir(None)
        def get_CacheDir(self):
            return self._CacheDir

    if not nullenv:
        nullenv = NullEnvironment()
    return nullenv

class Null:
    """A null Executor, with a null build Environment, that does
    nothing when the rest of the methods call it.

    This might be able to disapper when we refactor things to
    disassociate Builders from Nodes entirely, so we're not
    going to worry about unit tests for this--at least for now.
    """
    def __init__(self, *args, **kw):
        if __debug__: logInstanceCreation(self, 'Executor.Null')
        self.targets = kw['targets']
    def get_build_env(self):
        return get_NullEnvironment()
    def get_build_scanner_path(self):
        return None
    def cleanup(self):
        pass
    def prepare(self):
        pass
    def get_unignored_sources(self, *args, **kw):
        return tuple(())
    def get_action_list(self):
        return []
    def __call__(self, *args, **kw):
        return 0
    def get_contents(self):
        return ''

    def _morph(self):
        """Morph this Null executor to a real Executor object."""
        self.__class__ = Executor
        self.__init__([], targets=self.targets)            

    # The following methods require morphing this Null Executor to a
    # real Executor object.

    def add_pre_action(self, action):
        self._morph()
        self.add_pre_action(action)
    def add_post_action(self, action):
        self._morph()
        self.add_post_action(action)
    def set_action_list(self, action):
        self._morph()
        self.set_action_list(action)


