'use strict';
const { mustCall } = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');

const child = spawn(process.execPath, [
  '--interactive',
], {
  cwd: fixtures.path('es-modules', 'pkgimports'),
});

child.stdin.end(
  'try{require("#test");await import("#test")}catch{process.exit(-1)}'
);

child.on('exit', mustCall((code) => {
  assert.strictEqual(code, 0);
}));
