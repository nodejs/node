'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');

if (!common.isWindows) {
  common.skip('Windows only');
}

const normalizeDeviceNameTests = [
  { input: 'CON', expected: 'CON' },
  { input: 'con', expected: 'con' },
  { input: 'CON:', expected: '.\\CON:.' },
  { input: 'con:', expected: '.\\con:.' },
  { input: 'CON:.', expected: '.\\CON:.' },
  { input: 'coN:', expected: '.\\coN:.' },
  { input: 'LPT9.foo', expected: 'LPT9.foo' },
  { input: 'COM9:', expected: '.\\COM9:.' },
  { input: 'COM9.', expected: '.\\COM9.' },
  { input: 'C:COM9', expected: 'C:COM9' },
  { input: 'C:\\COM9', expected: 'C:\\COM9' },
  { input: 'CON:./foo', expected: '.\\CON:foo' },
  { input: 'CON:/foo', expected: '.\\CON:foo' },
  { input: 'CON:../foo', expected: '.\\CON:..\\foo' },
  { input: 'CON:/../foo', expected: '.\\CON:..\\foo' },
  { input: 'CON:./././foo', expected: '.\\CON:foo' },
  { input: 'CON:..', expected: '.\\CON:..' },
  { input: 'CON:..\\', expected: '.\\CON:..\\' },
  { input: 'CON:..\\..', expected: '.\\CON:..\\..' },
  { input: 'CON:..\\..\\', expected: '.\\CON:..\\..\\' },
  { input: 'CON:..\\..\\foo', expected: '.\\CON:..\\..\\foo' },
  { input: 'CON:..\\..\\foo\\', expected: '.\\CON:..\\..\\foo\\' },
  { input: 'CON:..\\..\\foo\\bar', expected: '.\\CON:..\\..\\foo\\bar' },
  { input: 'CON:..\\..\\foo\\bar\\', expected: '.\\CON:..\\..\\foo\\bar\\' },
  { input: 'COM1:a:b:c', expected: '.\\COM1:a:b:c' },
  { input: 'COM1:a:b:c/', expected: '.\\COM1:a:b:c\\' },
  { input: 'c:lpt1', expected: 'c:lpt1' },
  { input: 'c:\\lpt1', expected: 'c:\\lpt1' },

  // Reserved device names with path traversal
  { input: 'CON:.\\..\\..\\foo', expected: '.\\CON:..\\..\\foo' },
  { input: 'PRN:.\\..\\bar', expected: '.\\PRN:..\\bar' },
  { input: 'AUX:/../../baz', expected: '.\\AUX:..\\..\\baz' },

  { input: 'COM1:', expected: '.\\COM1:.' },
  { input: 'COM9:', expected: '.\\COM9:.' },
  { input: 'COM¹:', expected: '.\\COM¹:.' },
  { input: 'COM²:', expected: '.\\COM²:.' },
  { input: 'COM³:', expected: '.\\COM³:.' },
  { input: 'COM1:.\\..\\..\\foo', expected: '.\\COM1:..\\..\\foo' },
  { input: 'COM¹:.\\..\\..\\foo', expected: '.\\COM¹:..\\..\\foo' },
  { input: 'LPT1:', expected: '.\\LPT1:.' },
  { input: 'LPT¹:', expected: '.\\LPT¹:.' },
  { input: 'LPT²:', expected: '.\\LPT²:.' },
  { input: 'LPT³:', expected: '.\\LPT³:.' },
  { input: 'LPT9:', expected: '.\\LPT9:.' },
  { input: 'LPT1:.\\..\\..\\foo', expected: '.\\LPT1:..\\..\\foo' },
  { input: 'LPT¹:.\\..\\..\\foo', expected: '.\\LPT¹:..\\..\\foo' },
  { input: 'LpT5:/another/path', expected: '.\\LpT5:another\\path' },

  { input: 'C:\\foo', expected: 'C:\\foo' },
  { input: 'D:bar', expected: 'D:bar' },

  { input: 'CON', expected: 'CON' },
  { input: 'CON.TXT', expected: 'CON.TXT' },
  { input: 'COM10:', expected: '.\\COM10:' },
  { input: 'LPT10:', expected: '.\\LPT10:' },
  { input: 'CONNINGTOWER:', expected: '.\\CONNINGTOWER:' },
  { input: 'AUXILIARYDEVICE:', expected: '.\\AUXILIARYDEVICE:' },
  { input: 'NULLED:', expected: '.\\NULLED:' },
  { input: 'PRNINTER:', expected: '.\\PRNINTER:' },

  { input: 'CON:\\..\\..\\windows\\system32', expected: '.\\CON:..\\..\\windows\\system32' },
  { input: 'PRN:.././../etc/passwd', expected: '.\\PRN:..\\..\\etc\\passwd' },

  // Test with trailing slashes
  { input: 'CON:\\', expected: '.\\CON:.\\' },
  { input: 'COM1:\\foo\\bar\\', expected: '.\\COM1:foo\\bar\\' },

  // Test cases from original vulnerability reports or similar scenarios
  { input: 'COM1:.\\..\\..\\foo.js', expected: '.\\COM1:..\\..\\foo.js' },
  { input: 'LPT1:.\\..\\..\\another.txt', expected: '.\\LPT1:..\\..\\another.txt' },

  // Paths with device names not at the beginning
  { input: 'C:\\CON', expected: 'C:\\CON' },
  { input: 'C:\\path\\to\\COM1:', expected: 'C:\\path\\to\\COM1:' },

  // Device name followed by multiple colons
  { input: 'CON::', expected: '.\\CON::' },
  { input: 'COM1:::foo', expected: '.\\COM1:::foo' },

  // Device name with mixed path separators
  { input: 'AUX:/foo\\bar/baz', expected: '.\\AUX:foo\\bar\\baz' },
];

for (const { input, expected } of normalizeDeviceNameTests) {
  const actual = path.win32.normalize(input);
  assert.strictEqual(actual, expected,
                     `path.win32.normalize(${JSON.stringify(input)}) === ${JSON.stringify(expected)}, but got ${JSON.stringify(actual)}`);
}

assert.strictEqual(path.win32.normalize('CON:foo/../bar'), '.\\CON:bar');

// This should NOT be prefixed because 'c:' is treated as a drive letter.
assert.strictEqual(path.win32.normalize('c:COM1:'), 'c:COM1:');
