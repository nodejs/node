#!/usr/bin/env python
# encoding: utf-8
# Copyright 2009 Baptiste Lepilleur and The JsonCpp Authors
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.
# See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

from __future__ import print_function
from dircache import listdir
import re
import fnmatch
import os.path


# These fnmatch expressions are used by default to prune the directory tree
# while doing the recursive traversal in the glob_impl method of glob function.
prune_dirs = '.git .bzr .hg .svn _MTN _darcs CVS SCCS '

# These fnmatch expressions are used by default to exclude files and dirs
# while doing the recursive traversal in the glob_impl method of glob function.
##exclude_pats = prune_pats + '*~ #*# .#* %*% ._* .gitignore .cvsignore vssver.scc .DS_Store'.split()

# These ant_glob expressions are used by default to exclude files and dirs and also prune the directory tree
# while doing the recursive traversal in the glob_impl method of glob function.
default_excludes = '''
**/*~
**/#*#
**/.#*
**/%*%
**/._*
**/CVS
**/CVS/**
**/.cvsignore
**/SCCS
**/SCCS/**
**/vssver.scc
**/.svn
**/.svn/**
**/.git
**/.git/**
**/.gitignore
**/.bzr
**/.bzr/**
**/.hg
**/.hg/**
**/_MTN
**/_MTN/**
**/_darcs
**/_darcs/**
**/.DS_Store '''

DIR = 1
FILE = 2
DIR_LINK = 4
FILE_LINK = 8
LINKS = DIR_LINK | FILE_LINK
ALL_NO_LINK = DIR | FILE
ALL = DIR | FILE | LINKS

_ANT_RE = re.compile(r'(/\*\*/)|(\*\*/)|(/\*\*)|(\*)|(/)|([^\*/]*)')

def ant_pattern_to_re(ant_pattern):
    """Generates a regular expression from the ant pattern.
    Matching convention:
    **/a: match 'a', 'dir/a', 'dir1/dir2/a'
    a/**/b: match 'a/b', 'a/c/b', 'a/d/c/b'
    *.py: match 'script.py' but not 'a/script.py'
    """
    rex = ['^']
    next_pos = 0
    sep_rex = r'(?:/|%s)' % re.escape(os.path.sep)
##    print 'Converting', ant_pattern
    for match in _ANT_RE.finditer(ant_pattern):
##        print 'Matched', match.group()
##        print match.start(0), next_pos
        if match.start(0) != next_pos:
            raise ValueError("Invalid ant pattern")
        if match.group(1): # /**/
            rex.append(sep_rex + '(?:.*%s)?' % sep_rex)
        elif match.group(2): # **/
            rex.append('(?:.*%s)?' % sep_rex)
        elif match.group(3): # /**
            rex.append(sep_rex + '.*')
        elif match.group(4): # *
            rex.append('[^/%s]*' % re.escape(os.path.sep))
        elif match.group(5): # /
            rex.append(sep_rex)
        else: # somepath
            rex.append(re.escape(match.group(6)))
        next_pos = match.end()
    rex.append('$')
    return re.compile(''.join(rex))

def _as_list(l):
    if isinstance(l, basestring):
        return l.split()
    return l

def glob(dir_path,
         includes = '**/*',
         excludes = default_excludes,
         entry_type = FILE,
         prune_dirs = prune_dirs,
         max_depth = 25):
    include_filter = [ant_pattern_to_re(p) for p in _as_list(includes)]
    exclude_filter = [ant_pattern_to_re(p) for p in _as_list(excludes)]
    prune_dirs = [p.replace('/',os.path.sep) for p in _as_list(prune_dirs)]
    dir_path = dir_path.replace('/',os.path.sep)
    entry_type_filter = entry_type

    def is_pruned_dir(dir_name):
        for pattern in prune_dirs:
            if fnmatch.fnmatch(dir_name, pattern):
                return True
        return False

    def apply_filter(full_path, filter_rexs):
        """Return True if at least one of the filter regular expression match full_path."""
        for rex in filter_rexs:
            if rex.match(full_path):
                return True
        return False

    def glob_impl(root_dir_path):
        child_dirs = [root_dir_path]
        while child_dirs:
            dir_path = child_dirs.pop()
            for entry in listdir(dir_path):
                full_path = os.path.join(dir_path, entry)
##                print 'Testing:', full_path,
                is_dir = os.path.isdir(full_path)
                if is_dir and not is_pruned_dir(entry): # explore child directory ?
##                    print '===> marked for recursion',
                    child_dirs.append(full_path)
                included = apply_filter(full_path, include_filter)
                rejected = apply_filter(full_path, exclude_filter)
                if not included or rejected: # do not include entry ?
##                    print '=> not included or rejected'
                    continue
                link = os.path.islink(full_path)
                is_file = os.path.isfile(full_path)
                if not is_file and not is_dir:
##                    print '=> unknown entry type'
                    continue
                if link:
                    entry_type = is_file and FILE_LINK or DIR_LINK
                else:
                    entry_type = is_file and FILE or DIR
##                print '=> type: %d' % entry_type, 
                if (entry_type & entry_type_filter) != 0:
##                    print ' => KEEP'
                    yield os.path.join(dir_path, entry)
##                else:
##                    print ' => TYPE REJECTED'
    return list(glob_impl(dir_path))


if __name__ == "__main__":
    import unittest

    class AntPatternToRETest(unittest.TestCase):
##        def test_conversion(self):
##            self.assertEqual('^somepath$', ant_pattern_to_re('somepath').pattern)

        def test_matching(self):
            test_cases = [ ('path',
                             ['path'],
                             ['somepath', 'pathsuffix', '/path', '/path']),
                           ('*.py',
                             ['source.py', 'source.ext.py', '.py'],
                             ['path/source.py', '/.py', 'dir.py/z', 'z.pyc', 'z.c']),
                           ('**/path',
                             ['path', '/path', '/a/path', 'c:/a/path', '/a/b/path', '//a/path', '/a/path/b/path'],
                             ['path/', 'a/path/b', 'dir.py/z', 'somepath', 'pathsuffix', 'a/somepath']),
                           ('path/**',
                             ['path/a', 'path/path/a', 'path//'],
                             ['path', 'somepath/a', 'a/path', 'a/path/a', 'pathsuffix/a']),
                           ('/**/path',
                             ['/path', '/a/path', '/a/b/path/path', '/path/path'],
                             ['path', 'path/', 'a/path', '/pathsuffix', '/somepath']),
                           ('a/b',
                             ['a/b'],
                             ['somea/b', 'a/bsuffix', 'a/b/c']),
                           ('**/*.py',
                             ['script.py', 'src/script.py', 'a/b/script.py', '/a/b/script.py'],
                             ['script.pyc', 'script.pyo', 'a.py/b']),
                           ('src/**/*.py',
                             ['src/a.py', 'src/dir/a.py'],
                             ['a/src/a.py', '/src/a.py']),
                           ]
            for ant_pattern, accepted_matches, rejected_matches in list(test_cases):
                def local_path(paths):
                    return [ p.replace('/',os.path.sep) for p in paths ]
                test_cases.append((ant_pattern, local_path(accepted_matches), local_path(rejected_matches)))
            for ant_pattern, accepted_matches, rejected_matches in test_cases:
                rex = ant_pattern_to_re(ant_pattern)
                print('ant_pattern:', ant_pattern, ' => ', rex.pattern)
                for accepted_match in accepted_matches:
                    print('Accepted?:', accepted_match)
                    self.assertTrue(rex.match(accepted_match) is not None)
                for rejected_match in rejected_matches:
                    print('Rejected?:', rejected_match)
                    self.assertTrue(rex.match(rejected_match) is None)

    unittest.main()
