"""SCons.Debug

Code for debugging SCons internal things.  Not everything here is
guaranteed to work all the way back to Python 1.5.2, and shouldn't be
needed by most users.

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

__revision__ = "src/engine/SCons/Debug.py 3842 2008/12/20 22:59:52 scons"

import os
import string
import sys

# Recipe 14.10 from the Python Cookbook.
try:
    import weakref
except ImportError:
    def logInstanceCreation(instance, name=None):
        pass
else:
    def logInstanceCreation(instance, name=None):
        if name is None:
            name = instance.__class__.__name__
        if not tracked_classes.has_key(name):
            tracked_classes[name] = []
        tracked_classes[name].append(weakref.ref(instance))



tracked_classes = {}

def string_to_classes(s):
    if s == '*':
        c = tracked_classes.keys()
        c.sort()
        return c
    else:
        return string.split(s)

def fetchLoggedInstances(classes="*"):
    classnames = string_to_classes(classes)
    return map(lambda cn: (cn, len(tracked_classes[cn])), classnames)
  
def countLoggedInstances(classes, file=sys.stdout):
    for classname in string_to_classes(classes):
        file.write("%s: %d\n" % (classname, len(tracked_classes[classname])))

def listLoggedInstances(classes, file=sys.stdout):
    for classname in string_to_classes(classes):
        file.write('\n%s:\n' % classname)
        for ref in tracked_classes[classname]:
            obj = ref()
            if obj is not None:
                file.write('    %s\n' % repr(obj))

def dumpLoggedInstances(classes, file=sys.stdout):
    for classname in string_to_classes(classes):
        file.write('\n%s:\n' % classname)
        for ref in tracked_classes[classname]:
            obj = ref()
            if obj is not None:
                file.write('    %s:\n' % obj)
                for key, value in obj.__dict__.items():
                    file.write('        %20s : %s\n' % (key, value))



if sys.platform[:5] == "linux":
    # Linux doesn't actually support memory usage stats from getrusage().
    def memory():
        mstr = open('/proc/self/stat').read()
        mstr = string.split(mstr)[22]
        return int(mstr)
else:
    try:
        import resource
    except ImportError:
        try:
            import win32process
            import win32api
        except ImportError:
            def memory():
                return 0
        else:
            def memory():
                process_handle = win32api.GetCurrentProcess()
                memory_info = win32process.GetProcessMemoryInfo( process_handle )
                return memory_info['PeakWorkingSetSize']
    else:
        def memory():
            res = resource.getrusage(resource.RUSAGE_SELF)
            return res[4]

# returns caller's stack
def caller_stack(*backlist):
    import traceback
    if not backlist:
        backlist = [0]
    result = []
    for back in backlist:
        tb = traceback.extract_stack(limit=3+back)
        key = tb[0][:3]
        result.append('%s:%d(%s)' % func_shorten(key))
    return result

caller_bases = {}
caller_dicts = {}

# trace a caller's stack
def caller_trace(back=0):
    import traceback
    tb = traceback.extract_stack(limit=3+back)
    tb.reverse()
    callee = tb[1][:3]
    caller_bases[callee] = caller_bases.get(callee, 0) + 1
    for caller in tb[2:]:
        caller = callee + caller[:3]
        try:
            entry = caller_dicts[callee]
        except KeyError:
            caller_dicts[callee] = entry = {}
        entry[caller] = entry.get(caller, 0) + 1
        callee = caller

# print a single caller and its callers, if any
def _dump_one_caller(key, file, level=0):
    l = []
    for c,v in caller_dicts[key].items():
        l.append((-v,c))
    l.sort()
    leader = '      '*level
    for v,c in l:
        file.write("%s  %6d %s:%d(%s)\n" % ((leader,-v) + func_shorten(c[-3:])))
        if caller_dicts.has_key(c):
            _dump_one_caller(c, file, level+1)

# print each call tree
def dump_caller_counts(file=sys.stdout):
    keys = caller_bases.keys()
    keys.sort()
    for k in keys:
        file.write("Callers of %s:%d(%s), %d calls:\n"
                    % (func_shorten(k) + (caller_bases[k],)))
        _dump_one_caller(k, file)

shorten_list = [
    ( '/scons/SCons/',          1),
    ( '/src/engine/SCons/',     1),
    ( '/usr/lib/python',        0),
]

if os.sep != '/':
   def platformize(t):
       return (string.replace(t[0], '/', os.sep), t[1])
   shorten_list = map(platformize, shorten_list)
   del platformize

def func_shorten(func_tuple):
    f = func_tuple[0]
    for t in shorten_list:
        i = string.find(f, t[0])
        if i >= 0:
            if t[1]:
                i = i + len(t[0])
            return (f[i:],)+func_tuple[1:]
    return func_tuple


TraceFP = {}
if sys.platform == 'win32':
    TraceDefault = 'con'
else:
    TraceDefault = '/dev/tty'

def Trace(msg, file=None, mode='w'):
    """Write a trace message to a file.  Whenever a file is specified,
    it becomes the default for the next call to Trace()."""
    global TraceDefault
    if file is None:
        file = TraceDefault
    else:
        TraceDefault = file
    try:
        fp = TraceFP[file]
    except KeyError:
        try:
            fp = TraceFP[file] = open(file, mode)
        except TypeError:
            # Assume we were passed an open file pointer.
            fp = file
    fp.write(msg)
    fp.flush()
