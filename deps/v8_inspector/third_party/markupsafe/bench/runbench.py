#!/usr/bin/env python
"""
    Runs the benchmarks
"""
import sys
import os
import re
from subprocess import Popen

_filename_re = re.compile(r'^bench_(.*?)\.py$')
bench_directory = os.path.abspath(os.path.dirname(__file__))


def list_benchmarks():
    result = []
    for name in os.listdir(bench_directory):
        match = _filename_re.match(name)
        if match is not None:
            result.append(match.group(1))
    result.sort(key=lambda x: (x.startswith('logging_'), x.lower()))
    return result


def run_bench(name):
    sys.stdout.write('%-32s' % name)
    sys.stdout.flush()
    Popen([sys.executable, '-mtimeit', '-s',
           'from bench_%s import run' % name,
           'run()']).wait()


def main():
    print '=' * 80
    print 'Running benchmark for MarkupSafe'
    print '-' * 80
    os.chdir(bench_directory)
    for bench in list_benchmarks():
        run_bench(bench)
    print '-' * 80


if __name__ == '__main__':
    main()
