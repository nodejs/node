import sys
import os
import unittest
import subprocess


class ConfigureTests(unittest.TestCase):
  def setUp(self):
    self.working_dir = os.path.abspath(
      os.path.join(
        os.path.dirname(__file__),
        '..', '..'
      )
    )

  def test_ninja(self):
    subprocess.check_call(
      './configure --ninja',
      cwd=self.working_dir,
      shell=True,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE
    )


if (__name__ == '__main__' and
    sys.platform in ['linux', 'linux2', 'darwin', 'cygwin']):

  unittest.main()
