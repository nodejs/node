// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const { buildEmbedderGraph } = internalBinding('heap_utils');

const first = {};
const second = {};
const bigint = BigInt('123456789012345678901234567890');
const sameBigint = BigInt('123456789012345678901234567890');
const graph = buildEmbedderGraph(first, first, second, bigint, sameBigint);

function findNodes(value) {
  return graph.filter((node) => Object.hasOwn(node, 'value') &&
                                Object.is(node.value, value));
}

assert.strictEqual(findNodes(first).length, 1);
assert.strictEqual(findNodes(second).length, 1);
assert.strictEqual(findNodes(bigint).length, 1);
