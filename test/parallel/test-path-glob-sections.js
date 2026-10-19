'use strict';

require('../common');
const assert = require('node:assert');
const { posix, win32 } = require('node:path');

for (const implementation of [posix, win32]) {
  for (const count of [1, 2, 4, 8, 9, 10, 16]) {
    for (const prefix of ['part', '\u00e9tage', '\u4e2d']) {
      const names = Array.from({ length: count - 1 }, (_, index) => `${prefix}${index}`);
      const pattern = ['**', ...names.flatMap((name) => [name, '**']), 'file.js'].join('/');
      const parts = ['root', ...names.flatMap((name) => [name, 'dir']), 'file.js'];
      assert.strictEqual(implementation.matchesGlob(parts.join(implementation.sep), pattern), true);
      if (names.length) {
        parts[parts.length - 3] = 'absent';
      } else {
        parts[parts.length - 1] = 'file.txt';
      }
      assert.strictEqual(implementation.matchesGlob(parts.join(implementation.sep), pattern), false);
    }
  }
  assert.strictEqual(implementation.matchesGlob('root/.hidden/src/lib/file.js', '**/src/**/file.js'), false);
  assert.strictEqual(implementation.matchesGlob('root/src/./lib/file.js', '**/src/**/file.js'), true);
  assert.strictEqual(implementation.matchesGlob('root/src/lib/file.js/', '**/src/**/file.js'), true);
}
