'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

function run(input, expectation) {
  const node = spawn('node');

  node.stderr.on('data', common.mustCall((data) => {
    assert.ok(data.includes(expectation));
  }));

  node.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 1);
  }));

  node.stdin.write(input);
  node.stdin.end();
}

run('delete Array.prototype[Symbol.iterator];',
    'TypeError: Found non-callable @@iterator');

run('const ArrayIteratorPrototype =' +
      'Object.getPrototypeOf(Array.prototype[Symbol.iterator]());' +
      'delete ArrayIteratorPrototype.next;',
    'TypeError: fn is not a function');
