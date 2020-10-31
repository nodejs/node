# Copyright 2010 Baptiste Lepilleur and The JsonCpp Authors
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.
# See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

from __future__ import print_function
import os.path
import sys

def fix_source_eol(path, is_dry_run = True, verbose = True, eol = '\n'):
    """Makes sure that all sources have the specified eol sequence (default: unix)."""
    if not os.path.isfile(path):
        raise ValueError('Path "%s" is not a file' % path)
    try:
        f = open(path, 'rb')
    except IOError as msg:
        print("%s: I/O Error: %s" % (file, str(msg)), file=sys.stderr)
        return False
    try:
        raw_lines = f.readlines()
    finally:
        f.close()
    fixed_lines = [line.rstrip('\r\n') + eol for line in raw_lines]
    if raw_lines != fixed_lines:
        print('%s =>' % path, end=' ')
        if not is_dry_run:
            f = open(path, "wb")
            try:
                f.writelines(fixed_lines)
            finally:
                f.close()
        if verbose:
            print(is_dry_run and ' NEED FIX' or ' FIXED')
    return True
##    
##    
##
##def _do_fix(is_dry_run = True):
##    from waftools import antglob
##    python_sources = antglob.glob('.',
##        includes = '**/*.py **/wscript **/wscript_build',
##        excludes = antglob.default_excludes + './waf.py',
##        prune_dirs = antglob.prune_dirs + 'waf-* ./build')
##    for path in python_sources:
##        _fix_python_source(path, is_dry_run)
##
##    cpp_sources = antglob.glob('.',
##        includes = '**/*.cpp **/*.h **/*.inl',
##        prune_dirs = antglob.prune_dirs + 'waf-* ./build')
##    for path in cpp_sources:
##        _fix_source_eol(path, is_dry_run)
##
##
##def dry_fix(context):
##    _do_fix(is_dry_run = True)
##
##def fix(context):
##    _do_fix(is_dry_run = False)
##
##def shutdown():
##    pass
##
##def check(context):
##    # Unit tests are run when "check" target is used
##    ut = UnitTest.unit_test()
##    ut.change_to_testfile_dir = True
##    ut.want_to_see_test_output = True
##    ut.want_to_see_test_error = True
##    ut.run()
##    ut.print_results()
