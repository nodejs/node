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

__revision__ = "src/engine/SCons/Memoize.py 3842 2008/12/20 22:59:52 scons"

__doc__ = """Memoizer

A metaclass implementation to count hits and misses of the computed
values that various methods cache in memory.

Use of this modules assumes that wrapped methods be coded to cache their
values in a consistent way.  Here is an example of wrapping a method
that returns a computed value, with no input parameters:

    memoizer_counters = []                                      # Memoization

    memoizer_counters.append(SCons.Memoize.CountValue('foo'))   # Memoization

    def foo(self):

        try:                                                    # Memoization
            return self._memo['foo']                            # Memoization
        except KeyError:                                        # Memoization
            pass                                                # Memoization

        result = self.compute_foo_value()

        self._memo['foo'] = result                              # Memoization

        return result

Here is an example of wrapping a method that will return different values
based on one or more input arguments:

    def _bar_key(self, argument):                               # Memoization
        return argument                                         # Memoization

    memoizer_counters.append(SCons.Memoize.CountDict('bar', _bar_key)) # Memoization

    def bar(self, argument):

        memo_key = argument                                     # Memoization
        try:                                                    # Memoization
            memo_dict = self._memo['bar']                       # Memoization
        except KeyError:                                        # Memoization
            memo_dict = {}                                      # Memoization
            self._memo['dict'] = memo_dict                      # Memoization
        else:                                                   # Memoization
            try:                                                # Memoization
                return memo_dict[memo_key]                      # Memoization
            except KeyError:                                    # Memoization
                pass                                            # Memoization

        result = self.compute_bar_value(argument)

        memo_dict[memo_key] = result                            # Memoization

        return result

At one point we avoided replicating this sort of logic in all the methods
by putting it right into this module, but we've moved away from that at
present (see the "Historical Note," below.).

Deciding what to cache is tricky, because different configurations
can have radically different performance tradeoffs, and because the
tradeoffs involved are often so non-obvious.  Consequently, deciding
whether or not to cache a given method will likely be more of an art than
a science, but should still be based on available data from this module.
Here are some VERY GENERAL guidelines about deciding whether or not to
cache return values from a method that's being called a lot:

    --  The first question to ask is, "Can we change the calling code
        so this method isn't called so often?"  Sometimes this can be
        done by changing the algorithm.  Sometimes the *caller* should
        be memoized, not the method you're looking at.

    --  The memoized function should be timed with multiple configurations
        to make sure it doesn't inadvertently slow down some other
        configuration.

    --  When memoizing values based on a dictionary key composed of
        input arguments, you don't need to use all of the arguments
        if some of them don't affect the return values.

Historical Note:  The initial Memoizer implementation actually handled
the caching of values for the wrapped methods, based on a set of generic
algorithms for computing hashable values based on the method's arguments.
This collected caching logic nicely, but had two drawbacks:

    Running arguments through a generic key-conversion mechanism is slower
    (and less flexible) than just coding these things directly.  Since the
    methods that need memoized values are generally performance-critical,
    slowing them down in order to collect the logic isn't the right
    tradeoff.

    Use of the memoizer really obscured what was being called, because
    all the memoized methods were wrapped with re-used generic methods.
    This made it more difficult, for example, to use the Python profiler
    to figure out how to optimize the underlying methods.
"""

import new

# A flag controlling whether or not we actually use memoization.
use_memoizer = None

CounterList = []

class Counter:
    """
    Base class for counting memoization hits and misses.

    We expect that the metaclass initialization will have filled in
    the .name attribute that represents the name of the function
    being counted.
    """
    def __init__(self, method_name):
        """
        """
        self.method_name = method_name
        self.hit = 0
        self.miss = 0
        CounterList.append(self)
    def display(self):
        fmt = "    %7d hits %7d misses    %s()"
        print fmt % (self.hit, self.miss, self.name)
    def __cmp__(self, other):
        try:
            return cmp(self.name, other.name)
        except AttributeError:
            return 0

class CountValue(Counter):
    """
    A counter class for simple, atomic memoized values.

    A CountValue object should be instantiated in a class for each of
    the class's methods that memoizes its return value by simply storing
    the return value in its _memo dictionary.

    We expect that the metaclass initialization will fill in the
    .underlying_method attribute with the method that we're wrapping.
    We then call the underlying_method method after counting whether
    its memoized value has already been set (a hit) or not (a miss).
    """
    def __call__(self, *args, **kw):
        obj = args[0]
        if obj._memo.has_key(self.method_name):
            self.hit = self.hit + 1
        else:
            self.miss = self.miss + 1
        return apply(self.underlying_method, args, kw)

class CountDict(Counter):
    """
    A counter class for memoized values stored in a dictionary, with
    keys based on the method's input arguments.

    A CountDict object is instantiated in a class for each of the
    class's methods that memoizes its return value in a dictionary,
    indexed by some key that can be computed from one or more of
    its input arguments.

    We expect that the metaclass initialization will fill in the
    .underlying_method attribute with the method that we're wrapping.
    We then call the underlying_method method after counting whether the
    computed key value is already present in the memoization dictionary
    (a hit) or not (a miss).
    """
    def __init__(self, method_name, keymaker):
        """
        """
        Counter.__init__(self, method_name)
        self.keymaker = keymaker
    def __call__(self, *args, **kw):
        obj = args[0]
        try:
            memo_dict = obj._memo[self.method_name]
        except KeyError:
            self.miss = self.miss + 1
        else:
            key = apply(self.keymaker, args, kw)
            if memo_dict.has_key(key):
                self.hit = self.hit + 1
            else:
                self.miss = self.miss + 1
        return apply(self.underlying_method, args, kw)

class Memoizer:
    """Object which performs caching of method calls for its 'primary'
    instance."""

    def __init__(self):
        pass

# Find out if we support metaclasses (Python 2.2 and later).

class M:
    def __init__(cls, name, bases, cls_dict):
        cls.use_metaclass = 1
        def fake_method(self):
            pass
        new.instancemethod(fake_method, None, cls)

try:
    class A:
        __metaclass__ = M

    use_metaclass = A.use_metaclass
except AttributeError:
    use_metaclass = None
    reason = 'no metaclasses'
except TypeError:
    use_metaclass = None
    reason = 'new.instancemethod() bug'
else:
    del A

del M

if not use_metaclass:

    def Dump(title):
        pass

    try:
        class Memoized_Metaclass(type):
            # Just a place-holder so pre-metaclass Python versions don't
            # have to have special code for the Memoized classes.
            pass
    except TypeError:
        class Memoized_Metaclass:
            # A place-holder so pre-metaclass Python versions don't
            # have to have special code for the Memoized classes.
            pass

    def EnableMemoization():
        import SCons.Warnings
        msg = 'memoization is not supported in this version of Python (%s)'
        raise SCons.Warnings.NoMetaclassSupportWarning, msg % reason

else:

    def Dump(title=None):
        if title:
            print title
        CounterList.sort()
        for counter in CounterList:
            counter.display()

    class Memoized_Metaclass(type):
        def __init__(cls, name, bases, cls_dict):
            super(Memoized_Metaclass, cls).__init__(name, bases, cls_dict)

            for counter in cls_dict.get('memoizer_counters', []):
                method_name = counter.method_name

                counter.name = cls.__name__ + '.' + method_name
                counter.underlying_method = cls_dict[method_name]

                replacement_method = new.instancemethod(counter, None, cls)
                setattr(cls, method_name, replacement_method)

    def EnableMemoization():
        global use_memoizer
        use_memoizer = 1
