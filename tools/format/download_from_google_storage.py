#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download files from Google Storage based on SHA1 sums."""


import hashlib
import optparse
import os
import Queue
import re
import shutil
import stat
import sys
import tarfile
import threading
import time

import subprocess2


GSUTIL_DEFAULT_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'gsutil.py')
# Maps sys.platform to what we actually want to call them.
PLATFORM_MAPPING = {
    'cygwin': 'win',
    'darwin': 'mac',
    'linux2': 'linux',
    'win32': 'win',
}


class FileNotFoundError(IOError):
  pass


class InvalidFileError(IOError):
  pass


class InvalidPlatformError(Exception):
  pass


def GetNormalizedPlatform():
  """Returns the result of sys.platform accounting for cygwin.
  Under cygwin, this will always return "win32" like the native Python."""
  if sys.platform == 'cygwin':
    return 'win32'
  return sys.platform

# Common utilities
class Gsutil(object):
  """Call gsutil with some predefined settings.  This is a convenience object,
  and is also immutable."""
  def __init__(self, path, boto_path=None, timeout=None, version='4.15'):
    if not os.path.exists(path):
      raise FileNotFoundError('GSUtil not found in %s' % path)
    self.path = path
    self.timeout = timeout
    self.boto_path = boto_path
    self.version = version

  def get_sub_env(self):
    env = os.environ.copy()
    if self.boto_path == os.devnull:
      env['AWS_CREDENTIAL_FILE'] = ''
      env['BOTO_CONFIG'] = ''
    elif self.boto_path:
      env['AWS_CREDENTIAL_FILE'] = self.boto_path
      env['BOTO_CONFIG'] = self.boto_path

    return env

  def call(self, *args):
    cmd = [sys.executable, self.path, '--force-version', self.version]
    cmd.extend(args)
    return subprocess2.call(cmd, env=self.get_sub_env(), timeout=self.timeout)

  def check_call(self, *args):
    cmd = [sys.executable, self.path, '--force-version', self.version]
    cmd.extend(args)
    ((out, err), code) = subprocess2.communicate(
        cmd,
        stdout=subprocess2.PIPE,
        stderr=subprocess2.PIPE,
        env=self.get_sub_env(),
        timeout=self.timeout)

    # Parse output.
    status_code_match = re.search('status=([0-9]+)', err)
    if status_code_match:
      return (int(status_code_match.group(1)), out, err)
    if ('You are attempting to access protected data with '
          'no configured credentials.' in err):
      return (403, out, err)
    if 'matched no objects' in err:
      return (404, out, err)
    return (code, out, err)


def check_platform(target):
  """Checks if any parent directory of target matches (win|mac|linux)."""
  assert os.path.isabs(target)
  root, target_name = os.path.split(target)
  if not target_name:
    return None
  if target_name in ('linux', 'mac', 'win'):
    return target_name
  return check_platform(root)


def get_sha1(filename):
  sha1 = hashlib.sha1()
  with open(filename, 'rb') as f:
    while True:
      # Read in 1mb chunks, so it doesn't all have to be loaded into memory.
      chunk = f.read(1024*1024)
      if not chunk:
        break
      sha1.update(chunk)
  return sha1.hexdigest()


# Download-specific code starts here

def enumerate_work_queue(input_filename, work_queue, directory,
                         recursive, ignore_errors, output, sha1_file,
                         auto_platform):
  if sha1_file:
    if not os.path.exists(input_filename):
      if not ignore_errors:
        raise FileNotFoundError('%s not found.' % input_filename)
      print >> sys.stderr, '%s not found.' % input_filename
    with open(input_filename, 'rb') as f:
      sha1_match = re.match('^([A-Za-z0-9]{40})$', f.read(1024).rstrip())
      if sha1_match:
        work_queue.put((sha1_match.groups(1)[0], output))
        return 1
    if not ignore_errors:
      raise InvalidFileError('No sha1 sum found in %s.' % input_filename)
    print >> sys.stderr, 'No sha1 sum found in %s.' % input_filename
    return 0

  if not directory:
    work_queue.put((input_filename, output))
    return 1

  work_queue_size = 0
  for root, dirs, files in os.walk(input_filename):
    if not recursive:
      for item in dirs[:]:
        dirs.remove(item)
    else:
      for exclude in ['.svn', '.git']:
        if exclude in dirs:
          dirs.remove(exclude)
    for filename in files:
      full_path = os.path.join(root, filename)
      if full_path.endswith('.sha1'):
        if auto_platform:
          # Skip if the platform does not match.
          target_platform = check_platform(os.path.abspath(full_path))
          if not target_platform:
            err = ('--auto_platform passed in but no platform name found in '
                   'the path of %s' % full_path)
            if not ignore_errors:
              raise InvalidFileError(err)
            print >> sys.stderr, err
            continue
          current_platform = PLATFORM_MAPPING[sys.platform]
          if current_platform != target_platform:
            continue

        with open(full_path, 'rb') as f:
          sha1_match = re.match('^([A-Za-z0-9]{40})$', f.read(1024).rstrip())
        if sha1_match:
          work_queue.put(
              (sha1_match.groups(1)[0], full_path.replace('.sha1', '')))
          work_queue_size += 1
        else:
          if not ignore_errors:
            raise InvalidFileError('No sha1 sum found in %s.' % filename)
          print >> sys.stderr, 'No sha1 sum found in %s.' % filename
  return work_queue_size


def _validate_tar_file(tar, prefix):
  def _validate(tarinfo):
    """Returns false if the tarinfo is something we explicitly forbid."""
    if tarinfo.issym() or tarinfo.islnk():
      return False
    if '..' in tarinfo.name or not tarinfo.name.startswith(prefix):
      return False
    return True
  return all(map(_validate, tar.getmembers()))

def _downloader_worker_thread(thread_num, q, force, base_url,
                              gsutil, out_q, ret_codes, verbose, extract,
                              delete=True):
  while True:
    input_sha1_sum, output_filename = q.get()
    if input_sha1_sum is None:
      return
    if os.path.exists(output_filename) and not force:
      if get_sha1(output_filename) == input_sha1_sum:
        if verbose:
          out_q.put(
              '%d> File %s exists and SHA1 matches. Skipping.' % (
                  thread_num, output_filename))
        continue
    # Check if file exists.
    file_url = '%s/%s' % (base_url, input_sha1_sum)
    (code, _, err) = gsutil.check_call('ls', file_url)
    if code != 0:
      if code == 404:
        out_q.put('%d> File %s for %s does not exist, skipping.' % (
            thread_num, file_url, output_filename))
        ret_codes.put((1, 'File %s for %s does not exist.' % (
            file_url, output_filename)))
      else:
        # Other error, probably auth related (bad ~/.boto, etc).
        out_q.put('%d> Failed to fetch file %s for %s, skipping. [Err: %s]' % (
            thread_num, file_url, output_filename, err))
        ret_codes.put((1, 'Failed to fetch file %s for %s. [Err: %s]' % (
            file_url, output_filename, err)))
      continue
    # Fetch the file.
    out_q.put('%d> Downloading %s...' % (thread_num, output_filename))
    try:
      if delete:
        os.remove(output_filename)  # Delete the file if it exists already.
    except OSError:
      if os.path.exists(output_filename):
        out_q.put('%d> Warning: deleting %s failed.' % (
            thread_num, output_filename))
    code, _, err = gsutil.check_call('cp', file_url, output_filename)
    if code != 0:
      out_q.put('%d> %s' % (thread_num, err))
      ret_codes.put((code, err))
      continue

    remote_sha1 = get_sha1(output_filename)
    if remote_sha1 != input_sha1_sum:
      msg = ('%d> ERROR remote sha1 (%s) does not match expected sha1 (%s).' %
             (thread_num, remote_sha1, input_sha1_sum))
      out_q.put(msg)
      ret_codes.put((20, msg))
      continue

    if extract:
      if (not tarfile.is_tarfile(output_filename)
          or not output_filename.endswith('.tar.gz')):
        out_q.put('%d> Error: %s is not a tar.gz archive.' % (
                  thread_num, output_filename))
        ret_codes.put((1, '%s is not a tar.gz archive.' % (output_filename)))
        continue
      with tarfile.open(output_filename, 'r:gz') as tar:
        dirname = os.path.dirname(os.path.abspath(output_filename))
        extract_dir = output_filename[0:len(output_filename)-7]
        if not _validate_tar_file(tar, os.path.basename(extract_dir)):
          out_q.put('%d> Error: %s contains files outside %s.' % (
                    thread_num, output_filename, extract_dir))
          ret_codes.put((1, '%s contains invalid entries.' % (output_filename)))
          continue
        if os.path.exists(extract_dir):
          try:
            shutil.rmtree(extract_dir)
            out_q.put('%d> Removed %s...' % (thread_num, extract_dir))
          except OSError:
            out_q.put('%d> Warning: Can\'t delete: %s' % (
                      thread_num, extract_dir))
            ret_codes.put((1, 'Can\'t delete %s.' % (extract_dir)))
            continue
        out_q.put('%d> Extracting %d entries from %s to %s' %
                  (thread_num, len(tar.getmembers()),output_filename,
                   extract_dir))
        tar.extractall(path=dirname)
    # Set executable bit.
    if sys.platform == 'cygwin':
      # Under cygwin, mark all files as executable. The executable flag in
      # Google Storage will not be set when uploading from Windows, so if
      # this script is running under cygwin and we're downloading an
      # executable, it will be unrunnable from inside cygwin without this.
      st = os.stat(output_filename)
      os.chmod(output_filename, st.st_mode | stat.S_IEXEC)
    elif sys.platform != 'win32':
      # On non-Windows platforms, key off of the custom header
      # "x-goog-meta-executable".
      code, out, _ = gsutil.check_call('stat', file_url)
      if code != 0:
        out_q.put('%d> %s' % (thread_num, err))
        ret_codes.put((code, err))
      elif re.search(r'executable:\s*1', out):
        st = os.stat(output_filename)
        os.chmod(output_filename, st.st_mode | stat.S_IEXEC)

def printer_worker(output_queue):
  while True:
    line = output_queue.get()
    # Its plausible we want to print empty lines.
    if line is None:
      break
    print line


def download_from_google_storage(
    input_filename, base_url, gsutil, num_threads, directory, recursive,
    force, output, ignore_errors, sha1_file, verbose, auto_platform, extract):
  # Start up all the worker threads.
  all_threads = []
  download_start = time.time()
  stdout_queue = Queue.Queue()
  work_queue = Queue.Queue()
  ret_codes = Queue.Queue()
  ret_codes.put((0, None))
  for thread_num in range(num_threads):
    t = threading.Thread(
        target=_downloader_worker_thread,
        args=[thread_num, work_queue, force, base_url,
              gsutil, stdout_queue, ret_codes, verbose, extract])
    t.daemon = True
    t.start()
    all_threads.append(t)
  printer_thread = threading.Thread(target=printer_worker, args=[stdout_queue])
  printer_thread.daemon = True
  printer_thread.start()

  # Enumerate our work queue.
  work_queue_size = enumerate_work_queue(
      input_filename, work_queue, directory, recursive,
      ignore_errors, output, sha1_file, auto_platform)
  for _ in all_threads:
    work_queue.put((None, None))  # Used to tell worker threads to stop.

  # Wait for all downloads to finish.
  for t in all_threads:
    t.join()
  stdout_queue.put(None)
  printer_thread.join()

  # See if we ran into any errors.
  max_ret_code = 0
  for ret_code, message in ret_codes.queue:
    max_ret_code = max(ret_code, max_ret_code)
    if message:
      print >> sys.stderr, message
  if verbose and not max_ret_code:
    print 'Success!'

  if verbose:
    print 'Downloading %d files took %1f second(s)' % (
        work_queue_size, time.time() - download_start)
  return max_ret_code


def main(args):
  usage = ('usage: %prog [options] target\n'
           'Target must be:\n'
           '  (default) a sha1 sum ([A-Za-z0-9]{40}).\n'
           '  (-s or --sha1_file) a .sha1 file, containing a sha1 sum on '
           'the first line.\n'
           '  (-d or --directory) A directory to scan for .sha1 files.')
  parser = optparse.OptionParser(usage)
  parser.add_option('-o', '--output',
                    help='Specify the output file name. Defaults to: '
                         '(a) Given a SHA1 hash, the name is the SHA1 hash. '
                         '(b) Given a .sha1 file or directory, the name will '
                         'match (.*).sha1.')
  parser.add_option('-b', '--bucket',
                    help='Google Storage bucket to fetch from.')
  parser.add_option('-e', '--boto',
                    help='Specify a custom boto file.')
  parser.add_option('-c', '--no_resume', action='store_true',
                    help='Resume download if file is partially downloaded.')
  parser.add_option('-f', '--force', action='store_true',
                    help='Force download even if local file exists.')
  parser.add_option('-i', '--ignore_errors', action='store_true',
                    help='Don\'t throw error if we find an invalid .sha1 file.')
  parser.add_option('-r', '--recursive', action='store_true',
                    help='Scan folders recursively for .sha1 files. '
                         'Must be used with -d/--directory')
  parser.add_option('-t', '--num_threads', default=1, type='int',
                    help='Number of downloader threads to run.')
  parser.add_option('-d', '--directory', action='store_true',
                    help='The target is a directory.  '
                         'Cannot be used with -s/--sha1_file.')
  parser.add_option('-s', '--sha1_file', action='store_true',
                    help='The target is a file containing a sha1 sum.  '
                         'Cannot be used with -d/--directory.')
  parser.add_option('-g', '--config', action='store_true',
                    help='Alias for "gsutil config".  Run this if you want '
                         'to initialize your saved Google Storage '
                         'credentials.  This will create a read-only '
                         'credentials file in ~/.boto.depot_tools.')
  parser.add_option('-n', '--no_auth', action='store_true',
                    help='Skip auth checking.  Use if it\'s known that the '
                         'target bucket is a public bucket.')
  parser.add_option('-p', '--platform',
                    help='A regular expression that is compared against '
                         'Python\'s sys.platform. If this option is specified, '
                         'the download will happen only if there is a match.')
  parser.add_option('-a', '--auto_platform',
                    action='store_true',
                    help='Detects if any parent folder of the target matches '
                         '(linux|mac|win).  If so, the script will only '
                         'process files that are in the paths that '
                         'that matches the current platform.')
  parser.add_option('-u', '--extract',
                    action='store_true',
                    help='Extract a downloaded tar.gz file. '
                         'Leaves the tar.gz file around for sha1 verification'
                         'If a directory with the same name as the tar.gz '
                         'file already exists, is deleted (to get a '
                         'clean state in case of update.)')
  parser.add_option('-v', '--verbose', action='store_true', default=True,
                    help='DEPRECATED: Defaults to True.  Use --no-verbose '
                         'to suppress.')
  parser.add_option('-q', '--quiet', action='store_false', dest='verbose',
                    help='Suppresses diagnostic and progress information.')

  (options, args) = parser.parse_args()

  # Make sure we should run at all based on platform matching.
  if options.platform:
    if options.auto_platform:
      parser.error('--platform can not be specified with --auto_platform')
    if not re.match(options.platform, GetNormalizedPlatform()):
      if options.verbose:
        print('The current platform doesn\'t match "%s", skipping.' %
              options.platform)
      return 0

  # Set the boto file to /dev/null if we don't need auth.
  if options.no_auth:
    if (set(('http_proxy', 'https_proxy')).intersection(
        env.lower() for env in os.environ) and
        'NO_AUTH_BOTO_CONFIG' not in os.environ):
      print >> sys.stderr, ('NOTICE: You have PROXY values set in your '
                            'environment, but gsutil in depot_tools does not '
                            '(yet) obey them.')
      print >> sys.stderr, ('Also, --no_auth prevents the normal BOTO_CONFIG '
                            'environment variable from being used.')
      print >> sys.stderr, ('To use a proxy in this situation, please supply '
                            'those settings in a .boto file pointed to by '
                            'the NO_AUTH_BOTO_CONFIG environment var.')
    options.boto = os.environ.get('NO_AUTH_BOTO_CONFIG', os.devnull)

  # Make sure gsutil exists where we expect it to.
  if os.path.exists(GSUTIL_DEFAULT_PATH):
    gsutil = Gsutil(GSUTIL_DEFAULT_PATH,
                    boto_path=options.boto)
  else:
    parser.error('gsutil not found in %s, bad depot_tools checkout?' %
                 GSUTIL_DEFAULT_PATH)

  # Passing in -g/--config will run our copy of GSUtil, then quit.
  if options.config:
    print '===Note from depot_tools==='
    print 'If you do not have a project ID, enter "0" when asked for one.'
    print '===End note from depot_tools==='
    print
    return gsutil.call('config')

  if not args:
    parser.error('Missing target.')
  if len(args) > 1:
    parser.error('Too many targets.')
  if not options.bucket:
    parser.error('Missing bucket.  Specify bucket with --bucket.')
  if options.sha1_file and options.directory:
    parser.error('Both --directory and --sha1_file are specified, '
                 'can only specify one.')
  if options.recursive and not options.directory:
    parser.error('--recursive specified but --directory not specified.')
  if options.output and options.directory:
    parser.error('--directory is specified, so --output has no effect.')
  if (not (options.sha1_file or options.directory)
      and options.auto_platform):
    parser.error('--auto_platform must be specified with either '
                 '--sha1_file or --directory')

  input_filename = args[0]

  # Set output filename if not specified.
  if not options.output and not options.directory:
    if not options.sha1_file:
      # Target is a sha1 sum, so output filename would also be the sha1 sum.
      options.output = input_filename
    elif options.sha1_file:
      # Target is a .sha1 file.
      if not input_filename.endswith('.sha1'):
        parser.error('--sha1_file is specified, but the input filename '
                     'does not end with .sha1, and no --output is specified. '
                     'Either make sure the input filename has a .sha1 '
                     'extension, or specify --output.')
      options.output = input_filename[:-5]
    else:
      parser.error('Unreachable state.')

  # Check if output file already exists.
  if not options.directory and not options.force and not options.no_resume:
    if os.path.exists(options.output):
      parser.error('Output file %s exists and --no_resume is specified.'
                   % options.output)

  base_url = 'gs://%s' % options.bucket

  return download_from_google_storage(
      input_filename, base_url, gsutil, options.num_threads, options.directory,
      options.recursive, options.force, options.output, options.ignore_errors,
      options.sha1_file, options.verbose, options.auto_platform,
      options.extract)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
