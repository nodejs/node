# Copyright 2009 Baptiste Lepilleur and The JsonCpp Authors
# Distributed under MIT license, or public domain if desired and
# recognized in your jurisdiction.
# See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

from __future__ import print_function
from __future__ import unicode_literals
from io import open
from glob import glob
import sys
import os
import os.path
import subprocess
import optparse

VALGRIND_CMD = 'valgrind --tool=memcheck --leak-check=yes --undef-value-errors=yes'

class TestProxy(object):
    def __init__(self, test_exe_path, use_valgrind=False):
        self.test_exe_path = os.path.normpath(os.path.abspath(test_exe_path))
        self.use_valgrind = use_valgrind

    def run(self, options):
        if self.use_valgrind:
            cmd = VALGRIND_CMD.split()
        else:
            cmd = []
        cmd.extend([self.test_exe_path, '--test-auto'] + options)
        try:
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except:
            print(cmd)
            raise
        stdout = process.communicate()[0]
        if process.returncode:
            return False, stdout
        return True, stdout

def runAllTests(exe_path, use_valgrind=False):
    test_proxy = TestProxy(exe_path, use_valgrind=use_valgrind)
    status, test_names = test_proxy.run(['--list-tests'])
    if not status:
        print("Failed to obtain unit tests list:\n" + test_names, file=sys.stderr)
        return 1
    test_names = [name.strip() for name in test_names.decode('utf-8').strip().split('\n')]
    failures = []
    for name in test_names:
        print('TESTING %s:' % name, end=' ')
        succeed, result = test_proxy.run(['--test', name])
        if succeed:
            print('OK')
        else:
            failures.append((name, result))
            print('FAILED')
    failed_count = len(failures)
    pass_count = len(test_names) - failed_count
    if failed_count:
        print()
        for name, result in failures:
            print(result)
        print('%d/%d tests passed (%d failure(s))' % (            pass_count, len(test_names), failed_count))
        return 1
    else:
        print('All %d tests passed' % len(test_names))
        return 0

def main():
    from optparse import OptionParser
    parser = OptionParser(usage="%prog [options] <path to test_lib_json.exe>")
    parser.add_option("--valgrind",
                  action="store_true", dest="valgrind", default=False,
                  help="run all the tests using valgrind to detect memory leaks")
    parser.enable_interspersed_args()
    options, args = parser.parse_args()

    if len(args) != 1:
        parser.error('Must provides at least path to test_lib_json executable.')
        sys.exit(1)

    exit_code = runAllTests(args[0], use_valgrind=options.valgrind)
    sys.exit(exit_code)

if __name__ == '__main__':
    main()
