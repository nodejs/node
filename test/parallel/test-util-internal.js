'use strict';
// Flags: --expose_internals

const common = require('../common');
const assert = require('assert');
const internalUtil = require('internal/util');
const spawnSync = require('child_process').spawnSync;

function getHiddenValue(obj, name) {
  return function() {
    internalUtil.getHiddenValue(obj, name);
  };
}

assert.throws(getHiddenValue(), /obj must be an object/);
assert.throws(getHiddenValue(null, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(undefined, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue('bar', 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(85, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue({}), /name must be a string/);
assert.throws(getHiddenValue({}, null), /name must be a string/);
assert.throws(getHiddenValue({}, []), /name must be a string/);
assert.deepEqual(internalUtil.getHiddenValue({}, 'foo'), undefined);

let arrowMessage;

try {
  require('../fixtures/syntax/bad_syntax');
} catch (err) {
  arrowMessage = internalUtil.getHiddenValue(err, 'arrowMessage');
}

assert(/bad_syntax\.js:1/.test(arrowMessage));

const args = [
  '--expose-internals',
  '-e',
  "require('internal/util').error('foo %d', 5)"
];
const result = spawnSync(process.argv[0], args, { encoding: 'utf8' });
assert.strictEqual(result.stderr.indexOf('%'), -1);
assert(/foo 5/.test(result.stderr));
