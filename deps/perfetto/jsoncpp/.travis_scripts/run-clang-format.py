#!/usr/bin/env python
"""A wrapper script around clang-format, suitable for linting multiple files
and to use for continuous integration.
This is an alternative API for the clang-format command line.
It runs over multiple files and directories in parallel.
A diff output is produced and a sensible exit code is returned.

NOTE: pulled from https://github.com/Sarcasm/run-clang-format, which is
licensed under the MIT license.
"""

from __future__ import print_function, unicode_literals

import argparse
import codecs
import difflib
import fnmatch
import io
import multiprocessing
import os
import signal
import subprocess
import sys
import traceback

from functools import partial

try:
    from subprocess import DEVNULL  # py3k
except ImportError:
    DEVNULL = open(os.devnull, "wb")


DEFAULT_EXTENSIONS = 'c,h,C,H,cpp,hpp,cc,hh,c++,h++,cxx,hxx'


class ExitStatus:
    SUCCESS = 0
    DIFF = 1
    TROUBLE = 2


def list_files(files, recursive=False, extensions=None, exclude=None):
    if extensions is None:
        extensions = []
    if exclude is None:
        exclude = []

    out = []
    for file in files:
        if recursive and os.path.isdir(file):
            for dirpath, dnames, fnames in os.walk(file):
                fpaths = [os.path.join(dirpath, fname) for fname in fnames]
                for pattern in exclude:
                    # os.walk() supports trimming down the dnames list
                    # by modifying it in-place,
                    # to avoid unnecessary directory listings.
                    dnames[:] = [
                        x for x in dnames
                        if
                        not fnmatch.fnmatch(os.path.join(dirpath, x), pattern)
                    ]
                    fpaths = [
                        x for x in fpaths if not fnmatch.fnmatch(x, pattern)
                    ]
                for f in fpaths:
                    ext = os.path.splitext(f)[1][1:]
                    if ext in extensions:
                        out.append(f)
        else:
            out.append(file)
    return out


def make_diff(file, original, reformatted):
    return list(
        difflib.unified_diff(
            original,
            reformatted,
            fromfile='{}\t(original)'.format(file),
            tofile='{}\t(reformatted)'.format(file),
            n=3))


class DiffError(Exception):
    def __init__(self, message, errs=None):
        super(DiffError, self).__init__(message)
        self.errs = errs or []


class UnexpectedError(Exception):
    def __init__(self, message, exc=None):
        super(UnexpectedError, self).__init__(message)
        self.formatted_traceback = traceback.format_exc()
        self.exc = exc


def run_clang_format_diff_wrapper(args, file):
    try:
        ret = run_clang_format_diff(args, file)
        return ret
    except DiffError:
        raise
    except Exception as e:
        raise UnexpectedError('{}: {}: {}'.format(file, e.__class__.__name__,
                                                  e), e)


def run_clang_format_diff(args, file):
    try:
        with io.open(file, 'r', encoding='utf-8') as f:
            original = f.readlines()
    except IOError as exc:
        raise DiffError(str(exc))
    invocation = [args.clang_format_executable, file]

    # Use of utf-8 to decode the process output.
    #
    # Hopefully, this is the correct thing to do.
    #
    # It's done due to the following assumptions (which may be incorrect):
    # - clang-format will returns the bytes read from the files as-is,
    #   without conversion, and it is already assumed that the files use utf-8.
    # - if the diagnostics were internationalized, they would use utf-8:
    #   > Adding Translations to Clang
    #   >
    #   > Not possible yet!
    #   > Diagnostic strings should be written in UTF-8,
    #   > the client can translate to the relevant code page if needed.
    #   > Each translation completely replaces the format string
    #   > for the diagnostic.
    #   > -- http://clang.llvm.org/docs/InternalsManual.html#internals-diag-translation
    #
    # It's not pretty, due to Python 2 & 3 compatibility.
    encoding_py3 = {}
    if sys.version_info[0] >= 3:
        encoding_py3['encoding'] = 'utf-8'

    try:
        proc = subprocess.Popen(
            invocation,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            **encoding_py3)
    except OSError as exc:
        raise DiffError(
            "Command '{}' failed to start: {}".format(
                subprocess.list2cmdline(invocation), exc
            )
        )
    proc_stdout = proc.stdout
    proc_stderr = proc.stderr
    if sys.version_info[0] < 3:
        # make the pipes compatible with Python 3,
        # reading lines should output unicode
        encoding = 'utf-8'
        proc_stdout = codecs.getreader(encoding)(proc_stdout)
        proc_stderr = codecs.getreader(encoding)(proc_stderr)
    # hopefully the stderr pipe won't get full and block the process
    outs = list(proc_stdout.readlines())
    errs = list(proc_stderr.readlines())
    proc.wait()
    if proc.returncode:
        raise DiffError(
            "Command '{}' returned non-zero exit status {}".format(
                subprocess.list2cmdline(invocation), proc.returncode
            ),
            errs,
        )
    return make_diff(file, original, outs), errs


def bold_red(s):
    return '\x1b[1m\x1b[31m' + s + '\x1b[0m'


def colorize(diff_lines):
    def bold(s):
        return '\x1b[1m' + s + '\x1b[0m'

    def cyan(s):
        return '\x1b[36m' + s + '\x1b[0m'

    def green(s):
        return '\x1b[32m' + s + '\x1b[0m'

    def red(s):
        return '\x1b[31m' + s + '\x1b[0m'

    for line in diff_lines:
        if line[:4] in ['--- ', '+++ ']:
            yield bold(line)
        elif line.startswith('@@ '):
            yield cyan(line)
        elif line.startswith('+'):
            yield green(line)
        elif line.startswith('-'):
            yield red(line)
        else:
            yield line


def print_diff(diff_lines, use_color):
    if use_color:
        diff_lines = colorize(diff_lines)
    if sys.version_info[0] < 3:
        sys.stdout.writelines((l.encode('utf-8') for l in diff_lines))
    else:
        sys.stdout.writelines(diff_lines)


def print_trouble(prog, message, use_colors):
    error_text = 'error:'
    if use_colors:
        error_text = bold_red(error_text)
    print("{}: {} {}".format(prog, error_text, message), file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--clang-format-executable',
        metavar='EXECUTABLE',
        help='path to the clang-format executable',
        default='clang-format')
    parser.add_argument(
        '--extensions',
        help='comma separated list of file extensions (default: {})'.format(
            DEFAULT_EXTENSIONS),
        default=DEFAULT_EXTENSIONS)
    parser.add_argument(
        '-r',
        '--recursive',
        action='store_true',
        help='run recursively over directories')
    parser.add_argument('files', metavar='file', nargs='+')
    parser.add_argument(
        '-q',
        '--quiet',
        action='store_true')
    parser.add_argument(
        '-j',
        metavar='N',
        type=int,
        default=0,
        help='run N clang-format jobs in parallel'
        ' (default number of cpus + 1)')
    parser.add_argument(
        '--color',
        default='auto',
        choices=['auto', 'always', 'never'],
        help='show colored diff (default: auto)')
    parser.add_argument(
        '-e',
        '--exclude',
        metavar='PATTERN',
        action='append',
        default=[],
        help='exclude paths matching the given glob-like pattern(s)'
        ' from recursive search')

    args = parser.parse_args()

    # use default signal handling, like diff return SIGINT value on ^C
    # https://bugs.python.org/issue14229#msg156446
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    try:
        signal.SIGPIPE
    except AttributeError:
        # compatibility, SIGPIPE does not exist on Windows
        pass
    else:
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)

    colored_stdout = False
    colored_stderr = False
    if args.color == 'always':
        colored_stdout = True
        colored_stderr = True
    elif args.color == 'auto':
        colored_stdout = sys.stdout.isatty()
        colored_stderr = sys.stderr.isatty()

    version_invocation = [args.clang_format_executable, str("--version")]
    try:
        subprocess.check_call(version_invocation, stdout=DEVNULL)
    except subprocess.CalledProcessError as e:
        print_trouble(parser.prog, str(e), use_colors=colored_stderr)
        return ExitStatus.TROUBLE
    except OSError as e:
        print_trouble(
            parser.prog,
            "Command '{}' failed to start: {}".format(
                subprocess.list2cmdline(version_invocation), e
            ),
            use_colors=colored_stderr,
        )
        return ExitStatus.TROUBLE

    retcode = ExitStatus.SUCCESS
    files = list_files(
        args.files,
        recursive=args.recursive,
        exclude=args.exclude,
        extensions=args.extensions.split(','))

    if not files:
        return

    njobs = args.j
    if njobs == 0:
        njobs = multiprocessing.cpu_count() + 1
    njobs = min(len(files), njobs)

    if njobs == 1:
        # execute directly instead of in a pool,
        # less overhead, simpler stacktraces
        it = (run_clang_format_diff_wrapper(args, file) for file in files)
        pool = None
    else:
        pool = multiprocessing.Pool(njobs)
        it = pool.imap_unordered(
            partial(run_clang_format_diff_wrapper, args), files)
    while True:
        try:
            outs, errs = next(it)
        except StopIteration:
            break
        except DiffError as e:
            print_trouble(parser.prog, str(e), use_colors=colored_stderr)
            retcode = ExitStatus.TROUBLE
            sys.stderr.writelines(e.errs)
        except UnexpectedError as e:
            print_trouble(parser.prog, str(e), use_colors=colored_stderr)
            sys.stderr.write(e.formatted_traceback)
            retcode = ExitStatus.TROUBLE
            # stop at the first unexpected error,
            # something could be very wrong,
            # don't process all files unnecessarily
            if pool:
                pool.terminate()
            break
        else:
            sys.stderr.writelines(errs)
            if outs == []:
                continue
            if not args.quiet:
                print_diff(outs, use_color=colored_stdout)
            if retcode == ExitStatus.SUCCESS:
                retcode = ExitStatus.DIFF
    return retcode


if __name__ == '__main__':
    sys.exit(main())