// Flags: --expose-internals

import '../common/index.mjs';
import assert from 'node:assert';
import errorsModule from 'internal/errors';


const { determineSpecificType } = errorsModule;

assert.strictEqual(
  determineSpecificType(1n),
  'type bigint (1n)',
);

assert.strictEqual(
  determineSpecificType(true),
  'type boolean (true)',
);
assert.strictEqual(
  determineSpecificType(false),
  'type boolean (false)',
);

assert.strictEqual(
  determineSpecificType(2),
  'type number (2)',
);

assert.strictEqual(
  determineSpecificType(NaN),
  'type number (NaN)',
);

assert.strictEqual(
  determineSpecificType(Infinity),
  'type number (Infinity)',
);

assert.strictEqual(
  determineSpecificType({ __proto__: null }),
  '[Object: null prototype] {}',
);

assert.strictEqual(
  determineSpecificType(''),
  "type string ('')",
);

assert.strictEqual(
  determineSpecificType(''),
  "type string ('')",
);
assert.strictEqual(
  determineSpecificType("''"),
  "type string (\"''\")",
);
assert.strictEqual(
  determineSpecificType('Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor'),
  "type string ('Lorem ipsum dolor sit ame...')",
);
assert.strictEqual(
  determineSpecificType("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor'"),
  "type string ('Lorem ipsum dolor sit ame...')",
);
assert.strictEqual(
  determineSpecificType("Lorem' ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor"),
  "type string (\"Lorem' ipsum dolor sit am...\")",
);

assert.strictEqual(
  determineSpecificType(Symbol('foo')),
  'type symbol (Symbol(foo))',
);

assert.strictEqual(
  determineSpecificType(function foo() {}),
  'function foo',
);

const implicitlyNamed = function() {}; // eslint-disable-line func-style
assert.strictEqual(
  determineSpecificType(implicitlyNamed),
  'function implicitlyNamed',
);
assert.strictEqual(
  determineSpecificType(() => {}),
  'function ',
);
function noName() {}
delete noName.name;
assert.strictEqual(
  noName.name,
  '',
);
assert.strictEqual(
  determineSpecificType(noName),
  'function ',
);

function * generatorFn() {}
assert.strictEqual(
  determineSpecificType(generatorFn),
  'function generatorFn',
);

async function asyncFn() {}
assert.strictEqual(
  determineSpecificType(asyncFn),
  'function asyncFn',
);

assert.strictEqual(
  determineSpecificType(null),
  'null',
);

assert.strictEqual(
  determineSpecificType(undefined),
  'undefined',
);

assert.strictEqual(
  determineSpecificType([]),
  'an instance of Array',
);

assert.strictEqual(
  determineSpecificType(new Array()),
  'an instance of Array',
);
assert.strictEqual(
  determineSpecificType(new BigInt64Array()),
  'an instance of BigInt64Array',
);
assert.strictEqual(
  determineSpecificType(new BigUint64Array()),
  'an instance of BigUint64Array',
);
assert.strictEqual(
  determineSpecificType(new Int8Array()),
  'an instance of Int8Array',
);
assert.strictEqual(
  determineSpecificType(new Int16Array()),
  'an instance of Int16Array',
);
assert.strictEqual(
  determineSpecificType(new Int32Array()),
  'an instance of Int32Array',
);
assert.strictEqual(
  determineSpecificType(new Float32Array()),
  'an instance of Float32Array',
);
assert.strictEqual(
  determineSpecificType(new Float64Array()),
  'an instance of Float64Array',
);
assert.strictEqual(
  determineSpecificType(new Uint8Array()),
  'an instance of Uint8Array',
);
assert.strictEqual(
  determineSpecificType(new Uint8ClampedArray()),
  'an instance of Uint8ClampedArray',
);
assert.strictEqual(
  determineSpecificType(new Uint16Array()),
  'an instance of Uint16Array',
);
assert.strictEqual(
  determineSpecificType(new Uint32Array()),
  'an instance of Uint32Array',
);

assert.strictEqual(
  determineSpecificType(new Date()),
  'an instance of Date',
);

assert.strictEqual(
  determineSpecificType(new Map()),
  'an instance of Map',
);
assert.strictEqual(
  determineSpecificType(new WeakMap()),
  'an instance of WeakMap',
);

assert.strictEqual(
  determineSpecificType({}),
  'an instance of Object',
);
assert.strictEqual(
  determineSpecificType(new Object()),
  'an instance of Object',
);

assert.strictEqual(
  determineSpecificType(Promise.resolve('foo')),
  'an instance of Promise',
);

assert.strictEqual(
  determineSpecificType(new Set()),
  'an instance of Set',
);
assert.strictEqual(
  determineSpecificType(new WeakSet()),
  'an instance of WeakSet',
);
