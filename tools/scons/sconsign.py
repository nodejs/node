#! /usr/bin/env python
#
# SCons - a Software Constructor
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

__revision__ = "src/script/sconsign.py 3842 2008/12/20 22:59:52 scons"

__version__ = "1.2.0"

__build__ = "r3842"

__buildsys__ = "scons-dev"

__date__ = "2008/12/20 22:59:52"

__developer__ = "scons"

import os
import os.path
import sys
import time

##############################################################################
# BEGIN STANDARD SCons SCRIPT HEADER
#
# This is the cut-and-paste logic so that a self-contained script can
# interoperate correctly with different SCons versions and installation
# locations for the engine.  If you modify anything in this section, you
# should also change other scripts that use this same header.
##############################################################################

# Strip the script directory from sys.path() so on case-insensitive
# (WIN32) systems Python doesn't think that the "scons" script is the
# "SCons" package.  Replace it with our own library directories
# (version-specific first, in case they installed by hand there,
# followed by generic) so we pick up the right version of the build
# engine modules if they're in either directory.

script_dir = sys.path[0]

if script_dir in sys.path:
    sys.path.remove(script_dir)

libs = []

if os.environ.has_key("SCONS_LIB_DIR"):
    libs.append(os.environ["SCONS_LIB_DIR"])

local_version = 'scons-local-' + __version__
local = 'scons-local'
if script_dir:
    local_version = os.path.join(script_dir, local_version)
    local = os.path.join(script_dir, local)
libs.append(os.path.abspath(local_version))
libs.append(os.path.abspath(local))

scons_version = 'scons-%s' % __version__

prefs = []

if sys.platform == 'win32':
    # sys.prefix is (likely) C:\Python*;
    # check only C:\Python*.
    prefs.append(sys.prefix)
    prefs.append(os.path.join(sys.prefix, 'Lib', 'site-packages'))
else:
    # On other (POSIX) platforms, things are more complicated due to
    # the variety of path names and library locations.  Try to be smart
    # about it.
    if script_dir == 'bin':
        # script_dir is `pwd`/bin;
        # check `pwd`/lib/scons*.
        prefs.append(os.getcwd())
    else:
        if script_dir == '.' or script_dir == '':
            script_dir = os.getcwd()
        head, tail = os.path.split(script_dir)
        if tail == "bin":
            # script_dir is /foo/bin;
            # check /foo/lib/scons*.
            prefs.append(head)

    head, tail = os.path.split(sys.prefix)
    if tail == "usr":
        # sys.prefix is /foo/usr;
        # check /foo/usr/lib/scons* first,
        # then /foo/usr/local/lib/scons*.
        prefs.append(sys.prefix)
        prefs.append(os.path.join(sys.prefix, "local"))
    elif tail == "local":
        h, t = os.path.split(head)
        if t == "usr":
            # sys.prefix is /foo/usr/local;
            # check /foo/usr/local/lib/scons* first,
            # then /foo/usr/lib/scons*.
            prefs.append(sys.prefix)
            prefs.append(head)
        else:
            # sys.prefix is /foo/local;
            # check only /foo/local/lib/scons*.
            prefs.append(sys.prefix)
    else:
        # sys.prefix is /foo (ends in neither /usr or /local);
        # check only /foo/lib/scons*.
        prefs.append(sys.prefix)

    temp = map(lambda x: os.path.join(x, 'lib'), prefs)
    temp.extend(map(lambda x: os.path.join(x,
                                           'lib',
                                           'python' + sys.version[:3],
                                           'site-packages'),
                           prefs))
    prefs = temp

    # Add the parent directory of the current python's library to the
    # preferences.  On SuSE-91/AMD64, for example, this is /usr/lib64,
    # not /usr/lib.
    try:
        libpath = os.__file__
    except AttributeError:
        pass
    else:
        # Split /usr/libfoo/python*/os.py to /usr/libfoo/python*.
        libpath, tail = os.path.split(libpath)
        # Split /usr/libfoo/python* to /usr/libfoo
        libpath, tail = os.path.split(libpath)
        # Check /usr/libfoo/scons*.
        prefs.append(libpath)

# Look first for 'scons-__version__' in all of our preference libs,
# then for 'scons'.
libs.extend(map(lambda x: os.path.join(x, scons_version), prefs))
libs.extend(map(lambda x: os.path.join(x, 'scons'), prefs))

sys.path = libs + sys.path

##############################################################################
# END STANDARD SCons SCRIPT HEADER
##############################################################################

import cPickle
import imp
import string
import whichdb

import SCons.SConsign

def my_whichdb(filename):
    if filename[-7:] == ".dblite":
        return "SCons.dblite"
    try:
        f = open(filename + ".dblite", "rb")
        f.close()
        return "SCons.dblite"
    except IOError:
        pass
    return _orig_whichdb(filename)

_orig_whichdb = whichdb.whichdb
whichdb.whichdb = my_whichdb

def my_import(mname):
    if '.' in mname:
        i = string.rfind(mname, '.')
        parent = my_import(mname[:i])
        fp, pathname, description = imp.find_module(mname[i+1:],
                                                    parent.__path__)
    else:
        fp, pathname, description = imp.find_module(mname)
    return imp.load_module(mname, fp, pathname, description)

class Flagger:
    default_value = 1
    def __setitem__(self, item, value):
        self.__dict__[item] = value
        self.default_value = 0
    def __getitem__(self, item):
        return self.__dict__.get(item, self.default_value)

Do_Call = None
Print_Directories = []
Print_Entries = []
Print_Flags = Flagger()
Verbose = 0
Readable = 0

def default_mapper(entry, name):
    try:
        val = eval("entry."+name)
    except:
        val = None
    return str(val)

def map_action(entry, name):
    try:
        bact = entry.bact
        bactsig = entry.bactsig
    except AttributeError:
        return None
    return '%s [%s]' % (bactsig, bact)

def map_timestamp(entry, name):
    try:
        timestamp = entry.timestamp
    except AttributeError:
        timestamp = None
    if Readable and timestamp:
        return "'" + time.ctime(timestamp) + "'"
    else:
        return str(timestamp)

def map_bkids(entry, name):
    try:
        bkids = entry.bsources + entry.bdepends + entry.bimplicit
        bkidsigs = entry.bsourcesigs + entry.bdependsigs + entry.bimplicitsigs
    except AttributeError:
        return None
    result = []
    for i in xrange(len(bkids)):
        result.append(nodeinfo_string(bkids[i], bkidsigs[i], "        "))
    if result == []:
        return None
    return string.join(result, "\n        ")

map_field = {
    'action'    : map_action,
    'timestamp' : map_timestamp,
    'bkids'     : map_bkids,
}

map_name = {
    'implicit'  : 'bkids',
}

def field(name, entry, verbose=Verbose):
    if not Print_Flags[name]:
        return None
    fieldname = map_name.get(name, name)
    mapper = map_field.get(fieldname, default_mapper)
    val = mapper(entry, name)
    if verbose:
        val = name + ": " + val
    return val

def nodeinfo_raw(name, ninfo, prefix=""):
    # This just formats the dictionary, which we would normally use str()
    # to do, except that we want the keys sorted for deterministic output.
    d = ninfo.__dict__
    try:
        keys = ninfo.field_list + ['_version_id']
    except AttributeError:
        keys = d.keys()
        keys.sort()
    l = []
    for k in keys:
        l.append('%s: %s' % (repr(k), repr(d.get(k))))
    if '\n' in name:
        name = repr(name)
    return name + ': {' + string.join(l, ', ') + '}'

def nodeinfo_cooked(name, ninfo, prefix=""):
    try:
        field_list = ninfo.field_list
    except AttributeError:
        field_list = []
    f = lambda x, ni=ninfo, v=Verbose: field(x, ni, v)
    if '\n' in name:
        name = repr(name)
    outlist = [name+':'] + filter(None, map(f, field_list))
    if Verbose:
        sep = '\n    ' + prefix
    else:
        sep = ' '
    return string.join(outlist, sep)

nodeinfo_string = nodeinfo_cooked

def printfield(name, entry, prefix=""):
    outlist = field("implicit", entry, 0)
    if outlist:
        if Verbose:
            print "    implicit:"
        print "        " + outlist
    outact = field("action", entry, 0)
    if outact:
        if Verbose:
            print "    action: " + outact
        else:
            print "        " + outact

def printentries(entries, location):
    if Print_Entries:
        for name in Print_Entries:
            try:
                entry = entries[name]
            except KeyError:
                sys.stderr.write("sconsign: no entry `%s' in `%s'\n" % (name, location))
            else:
                try:
                    ninfo = entry.ninfo
                except AttributeError:
                    print name + ":"
                else:
                    print nodeinfo_string(name, entry.ninfo)
                printfield(name, entry.binfo)
    else:
        names = entries.keys()
        names.sort()
        for name in names:
            entry = entries[name]
            try:
                ninfo = entry.ninfo
            except AttributeError:
                print name + ":"
            else:
                print nodeinfo_string(name, entry.ninfo)
            printfield(name, entry.binfo)

class Do_SConsignDB:
    def __init__(self, dbm_name, dbm):
        self.dbm_name = dbm_name
        self.dbm = dbm

    def __call__(self, fname):
        # The *dbm modules stick their own file suffixes on the names
        # that are passed in.  This is causes us to jump through some
        # hoops here to be able to allow the user
        try:
            # Try opening the specified file name.  Example:
            #   SPECIFIED                  OPENED BY self.dbm.open()
            #   ---------                  -------------------------
            #   .sconsign               => .sconsign.dblite
            #   .sconsign.dblite        => .sconsign.dblite.dblite
            db = self.dbm.open(fname, "r")
        except (IOError, OSError), e:
            print_e = e
            try:
                # That didn't work, so try opening the base name,
                # so that if the actually passed in 'sconsign.dblite'
                # (for example), the dbm module will put the suffix back
                # on for us and open it anyway.
                db = self.dbm.open(os.path.splitext(fname)[0], "r")
            except (IOError, OSError):
                # That didn't work either.  See if the file name
                # they specified just exists (independent of the dbm
                # suffix-mangling).
                try:
                    open(fname, "r")
                except (IOError, OSError), e:
                    # Nope, that file doesn't even exist, so report that
                    # fact back.
                    print_e = e
                sys.stderr.write("sconsign: %s\n" % (print_e))
                return
        except KeyboardInterrupt:
            raise
        except cPickle.UnpicklingError:
            sys.stderr.write("sconsign: ignoring invalid `%s' file `%s'\n" % (self.dbm_name, fname))
            return
        except Exception, e:
            sys.stderr.write("sconsign: ignoring invalid `%s' file `%s': %s\n" % (self.dbm_name, fname, e))
            return

        if Print_Directories:
            for dir in Print_Directories:
                try:
                    val = db[dir]
                except KeyError:
                    sys.stderr.write("sconsign: no dir `%s' in `%s'\n" % (dir, args[0]))
                else:
                    self.printentries(dir, val)
        else:
            keys = db.keys()
            keys.sort()
            for dir in keys:
                self.printentries(dir, db[dir])

    def printentries(self, dir, val):
        print '=== ' + dir + ':'
        printentries(cPickle.loads(val), dir)

def Do_SConsignDir(name):
    try:
        fp = open(name, 'rb')
    except (IOError, OSError), e:
        sys.stderr.write("sconsign: %s\n" % (e))
        return
    try:
        sconsign = SCons.SConsign.Dir(fp)
    except KeyboardInterrupt:
        raise
    except cPickle.UnpicklingError:
        sys.stderr.write("sconsign: ignoring invalid .sconsign file `%s'\n" % (name))
        return
    except Exception, e:
        sys.stderr.write("sconsign: ignoring invalid .sconsign file `%s': %s\n" % (name, e))
        return
    printentries(sconsign.entries, args[0])

##############################################################################

import getopt

helpstr = """\
Usage: sconsign [OPTIONS] FILE [...]
Options:
  -a, --act, --action         Print build action information.
  -c, --csig                  Print content signature information.
  -d DIR, --dir=DIR           Print only info about DIR.
  -e ENTRY, --entry=ENTRY     Print only info about ENTRY.
  -f FORMAT, --format=FORMAT  FILE is in the specified FORMAT.
  -h, --help                  Print this message and exit.
  -i, --implicit              Print implicit dependency information.
  -r, --readable              Print timestamps in human-readable form.
  --raw                       Print raw Python object representations.
  -s, --size                  Print file sizes.
  -t, --timestamp             Print timestamp information.
  -v, --verbose               Verbose, describe each field.
"""

opts, args = getopt.getopt(sys.argv[1:], "acd:e:f:hirstv",
                            ['act', 'action',
                             'csig', 'dir=', 'entry=',
                             'format=', 'help', 'implicit',
                             'raw', 'readable',
                             'size', 'timestamp', 'verbose'])


for o, a in opts:
    if o in ('-a', '--act', '--action'):
        Print_Flags['action'] = 1
    elif o in ('-c', '--csig'):
        Print_Flags['csig'] = 1
    elif o in ('-d', '--dir'):
        Print_Directories.append(a)
    elif o in ('-e', '--entry'):
        Print_Entries.append(a)
    elif o in ('-f', '--format'):
        Module_Map = {'dblite'   : 'SCons.dblite',
                      'sconsign' : None}
        dbm_name = Module_Map.get(a, a)
        if dbm_name:
            try:
                dbm = my_import(dbm_name)
            except:
                sys.stderr.write("sconsign: illegal file format `%s'\n" % a)
                print helpstr
                sys.exit(2)
            Do_Call = Do_SConsignDB(a, dbm)
        else:
            Do_Call = Do_SConsignDir
    elif o in ('-h', '--help'):
        print helpstr
        sys.exit(0)
    elif o in ('-i', '--implicit'):
        Print_Flags['implicit'] = 1
    elif o in ('--raw',):
        nodeinfo_string = nodeinfo_raw
    elif o in ('-r', '--readable'):
        Readable = 1
    elif o in ('-s', '--size'):
        Print_Flags['size'] = 1
    elif o in ('-t', '--timestamp'):
        Print_Flags['timestamp'] = 1
    elif o in ('-v', '--verbose'):
        Verbose = 1

if Do_Call:
    for a in args:
        Do_Call(a)
else:
    for a in args:
        dbm_name = whichdb.whichdb(a)
        if dbm_name:
            Map_Module = {'SCons.dblite' : 'dblite'}
            dbm = my_import(dbm_name)
            Do_SConsignDB(Map_Module.get(dbm_name, dbm_name), dbm)(a)
        else:
            Do_SConsignDir(a)

sys.exit(0)
