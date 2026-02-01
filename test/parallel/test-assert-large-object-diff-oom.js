// Flags: --max-old-space-size=512
'use strict';

// Regression test: assert.strictEqual should not OOM when comparing objects
// with many converging paths to shared objects. Such objects cause exponential
// growth in util.inspect output, which previously led to OOM during error
// message generation.

require('../common');
const assert = require('assert');

// Test: should throw AssertionError, not OOM
{
  const { doc1, doc2 } = createTestObjects();

  assert.throws(
    () => assert.strictEqual(doc1, doc2),
    (err) => {
      assert.ok(err instanceof assert.AssertionError);
      // Message should be bounded (fix truncates inspect output at 2MB)
      assert.ok(err.message.length < 5 * 1024 * 1024);
      return true;
    }
  );
}

// Creates objects where many paths converge on shared objects, causing
// exponential growth in util.inspect output at high depths.
function createTestObjects() {
  const base = createBase();

  const s1 = createSchema(base, 's1');
  const s2 = createSchema(base, 's2');
  base.schemas.s1 = s1;
  base.schemas.s2 = s2;

  const doc1 = createDoc(s1, base);
  const doc2 = createDoc(s2, base);

  // Populated refs create additional converging paths
  for (let i = 0; i < 2; i++) {
    const ps = createSchema(base, 'p' + i);
    base.schemas['p' + i] = ps;
    doc1.$__.pop['r' + i] = { value: createDoc(ps, base), opts: { base, schema: ps } };
  }

  // Cross-link creates more converging paths
  doc1.$__.pop.r0.value.$__parent = doc2;

  return { doc1, doc2 };
}

function createBase() {
  const base = { types: {}, schemas: {} };
  for (let i = 0; i < 4; i++) {
    base.types['t' + i] = {
      base,
      caster: { base },
      opts: { base, validators: [{ base }, { base }] }
    };
  }
  return base;
}

function createSchema(base, name) {
  const schema = { name, base, paths: {}, children: [] };
  for (let i = 0; i < 6; i++) {
    schema.paths['f' + i] = {
      schema, base,
      type: base.types['t' + (i % 4)],
      caster: base.types['t' + (i % 4)].caster,
      opts: { schema, base, validators: [{ schema, base }] }
    };
  }
  for (let i = 0; i < 2; i++) {
    const child = { name: name + '_c' + i, base, parent: schema, paths: {} };
    for (let j = 0; j < 3; j++) {
      child.paths['cf' + j] = { schema: child, base, type: base.types['t' + (j % 4)] };
    }
    schema.children.push(child);
  }
  return schema;
}

function createDoc(schema, base) {
  const doc = { schema, base, $__: { scopes: {}, pop: {} } };
  for (let i = 0; i < 6; i++) {
    doc.$__.scopes['p' + i] = { schema, base, type: base.types['t' + (i % 4)] };
  }
  return doc;
}
