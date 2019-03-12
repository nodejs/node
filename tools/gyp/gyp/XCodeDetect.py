import subprocess
import sys


def run(*cmd_args):
  return subprocess.check_output(cmd_args, stderr=subprocess.PIPE).decode('utf-8')


class XCodeDetect(object):
  """Simplify access to Xcode information."""
  _cache = {}

  @staticmethod
  def Version():
    if 'Version' not in XCodeDetect._cache:
      version = ''
      try:
        lines = run('xcodebuild', '-version').splitlines()
        version = ''.join(lines[0].decode('utf-8').split()[-1].split('.'))
        version = (version + '0' * (3 - len(version))).zfill(4)
      except subprocess.CalledProcessError:
        pass
      try:
        lines = run('pkgutil', '--pkg-info=com.apple.pkg.CLTools_Executables').splitlines()
        for l in lines:
          n, v = l.split(': ', 1)
          if n != 'version':
            continue
          parts = v.split('.',4)
          version = '%s%s%s%s' % tuple(parts[0:4])
          break
      except subprocess.CalledProcessError:
        pass
      XCodeDetect._cache['Version'] = version
    return XCodeDetect._cache['Version']


  @staticmethod
  def SDKVersion():
    if 'SDKVersion' not in XCodeDetect._cache:
      out = ''
      try:
        out = run('xcrun', '--show-sdk-version')
      except subprocess.CalledProcessError:
        pass
      try:
        out = run('xcodebuild', '-version', '-sdk', '', 'SDKVersion')
      except subprocess.CalledProcessError:
        pass
      XCodeDetect._cache['SDKVersion'] = out.strip()
    return XCodeDetect._cache['SDKVersion']


  @staticmethod
  def HasIPhoneSDK():
    if not sys.platform == 'darwin':
      return False

    if 'HasIPhoneSDK' not in XCodeDetect._cache:
      try:
        out = run('xcrun', '--sdk', 'iphoneos', '--show-sdk-path')
      except subprocess.CalledProcessError:
        out = 1
      XCodeDetect._cache['HasIPhoneSDK'] = out == 0
    return XCodeDetect._cache['HasIPhoneSDK']
