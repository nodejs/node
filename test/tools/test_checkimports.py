import unittest
import sys
from contextlib import contextmanager
from os import path
sys.path.append(path.abspath(path.join(path.dirname(__file__),
                                       '..', '..', 'tools')))
try:
  from StringIO import StringIO
except ImportError:
  from io import StringIO

from checkimports import is_valid

@contextmanager
def captured_output():
  tmp_out, tmp_err = StringIO(), StringIO()
  old_out, old_err = sys.stdout, sys.stderr
  try:
    sys.stdout, sys.stderr = tmp_out, tmp_err
    yield sys.stdout, sys.stderr
  finally:
    sys.stdout, sys.stderr = old_out, old_err
    tmp_out.close()
    tmp_err.close()

class CheckImportsTest(unittest.TestCase):
  fixturesDir = path.join(path.dirname(__file__), '..', '..',
                          'test', 'fixtures', 'tools', 'checkimports')

  def test_unused_and_unsorted(self):
    with captured_output() as (out, err):
      self.assertEqual(is_valid(path.join(self.fixturesDir, 'invalid.cc')),
                       False)
      output = out.getvalue()
      self.assertIn('does not use "Local"', output);
      self.assertIn('using statements aren\'t sorted in', output);
      self.assertIn('Line 1: Actual: v8::MaybeLocal, Expected: v8::Array',
                    output);
      self.assertIn('Line 2: Actual: v8::Array, Expected: v8::Local',
                    output);
      self.assertIn('Line 3: Actual: v8::Local, Expected: v8::MaybeLocal',
                    output);

  def test_unused_complex(self):
    with captured_output() as (out, err):
      self.assertEqual(is_valid(path.join(self.fixturesDir, 'maybe.cc')),
                       False)
      output = out.getvalue()
      self.assertIn('does not use "Local"', output);

  def test_unused_simple(self):
    with captured_output() as (out, err):
      self.assertEqual(is_valid(path.join(self.fixturesDir, 'unused.cc')),
                       False)
      output = out.getvalue()
      self.assertIn('does not use "Context"', output);

  def test_unsorted(self):
    with captured_output() as (out, err):
      self.assertEqual(is_valid(path.join(self.fixturesDir, 'unsorted.cc')),
                       False)
      output = out.getvalue()
      self.assertIn('using statements aren\'t sorted in', output);
      self.assertIn('Line 1: Actual: v8::MaybeLocal, Expected: v8::Array',
                    output);
      self.assertIn('Line 2: Actual: v8::Array, Expected: v8::MaybeLocal',
                    output);

  def test_valid(self):
    with captured_output() as (out, err):
      self.assertEqual(is_valid(path.join(self.fixturesDir, 'valid.cc')),
                       True)
      output = out.getvalue()
      self.assertEqual(output, '');

if __name__ == '__main__':
  unittest.main()
