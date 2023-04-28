import unittest
import sys, os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             '..', '..', 'tools')))
from js2c import NormalizeFileName

class Js2ctest(unittest.TestCase):
    def testNormalizeFileName(self):
        self.assertEqual(NormalizeFileName('dir/mod.js'), 'mod')
        self.assertEqual(NormalizeFileName('deps/mod.js'), 'internal/deps/mod')
        self.assertEqual(NormalizeFileName('mod.js'), 'mod')

if __name__ == '__main__':
    unittest.main()
