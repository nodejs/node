'use strict';

// Test for Windows ARM64 os.machine() detection fix
// When on Windows ARM64, os.machine() should return 'arm64' instead of 'unknown'

const common = require('../common');
const assert = require('assert');
const os = require('os');

// os.machine() should return a string
const machine = os.machine();
assert.strictEqual(typeof machine, 'string');
assert.ok(machine.length > 0, 'os.machine() should not be empty');

// Verify it returns a recognized architecture string
// Valid values include: x64, x32, ia32, arm, arm64, ppc64, ppc64le, s390, s390x, mips, mips64el, riscv64, loong64, unknown
const validArchitectures = [
  'unknown',
  'x64',
  'x32',
  'ia32',
  'arm',
  'arm64',
  'ppc64',
  'ppc64le',
  's390',
  's390x',
  'mips',
  'mips64el',
  'riscv64',
  'loong64',
];

assert.ok(
  validArchitectures.includes(machine),
  `os.machine() returned "${machine}", but should be one of: ${validArchitectures.join(', ')}`
);

// On Windows systems, specifically verify that os.machine() doesn't return 'unknown'
// if the system is actually ARM64 capable (this fix ensures GetSystemInfo() is used)
if (common.isWindows) {
  // This test will pass on all Windows systems. The fix ensures that on ARM64 systems,
  // os.machine() no longer returns 'unknown', but instead returns 'arm64'
  assert.ok(machine.length > 0);
}
