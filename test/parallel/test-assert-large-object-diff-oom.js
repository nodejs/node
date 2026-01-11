// Flags: --max-old-space-size=512
'use strict';

// Test that assert.strictEqual does not OOM when comparing objects
// that produce large util.inspect output.
//
// This is a regression test for an issue where objects with many unique
// paths converging on shared objects can cause exponential growth in
// util.inspect output, leading to OOM during assertion error generation.
//
// The fix adds a 2MB limit to inspect output in assertion_error.js

require('../common');
const assert = require('assert');

// Create an object graph where many unique paths converge on shared objects.
// This delays circular reference detection and creates exponential growth
// in util.inspect output at high depths.

function createBase() {
  const base = {
    id: 'base',
    models: {},
    schemas: {},
    types: {},
  };

  for (let i = 0; i < 5; i++) {
    base.types[`type_${i}`] = {
      name: `type_${i}`,
      base,
      caster: { base, name: `type_${i}_caster` },
      options: {
        base,
        validators: [
          { base, name: 'v1' },
          { base, name: 'v2' },
          { base, name: 'v3' },
        ],
      },
    };
  }

  return base;
}

function createSchema(base, name) {
  const schema = {
    name,
    base,
    paths: {},
    tree: {},
    virtuals: {},
  };

  for (let i = 0; i < 10; i++) {
    schema.paths[`field_${i}`] = {
      path: `field_${i}`,
      schema,
      instance: base.types[`type_${i % 5}`],
      options: {
        type: base.types[`type_${i % 5}`],
        validators: [
          { validator: () => true, base, schema },
          { validator: () => true, base, schema },
        ],
      },
      caster: base.types[`type_${i % 5}`].caster,
    };
  }

  schema.childSchemas = [];
  for (let i = 0; i < 3; i++) {
    const child = { name: `${name}_child_${i}`, base, schema, paths: {} };
    for (let j = 0; j < 5; j++) {
      child.paths[`child_field_${j}`] = {
        path: `child_field_${j}`,
        schema: child,
        instance: base.types[`type_${j % 5}`],
        options: { base, schema: child },
      };
    }
    schema.childSchemas.push(child);
  }

  return schema;
}

function createDocument(schema, base) {
  const doc = {
    $__: { activePaths: {}, pathsToScopes: {}, populated: {} },
    _doc: { name: 'test' },
    _schema: schema,
    _base: base,
  };

  for (let i = 0; i < 10; i++) {
    doc.$__.pathsToScopes[`path_${i}`] = {
      schema,
      base,
      type: base.types[`type_${i % 5}`],
    };
  }

  for (let i = 0; i < 3; i++) {
    const populatedSchema = createSchema(base, `Populated_${i}`);
    base.schemas[`Populated_${i}`] = populatedSchema;

    doc.$__.populated[`ref_${i}`] = {
      value: {
        $__: { pathsToScopes: {}, populated: {} },
        _doc: { id: i },
        _schema: populatedSchema,
        _base: base,
      },
      options: { path: `ref_${i}`, model: `Model_${i}`, base },
      schema: populatedSchema,
    };

    for (let j = 0; j < 5; j++) {
      doc.$__.populated[`ref_${i}`].value.$__.pathsToScopes[`field_${j}`] = {
        schema: populatedSchema,
        base,
        type: base.types[`type_${j % 5}`],
      };
    }
  }

  return doc;
}

class Document {
  constructor(schema, base) {
    Object.assign(this, createDocument(schema, base));
  }
}

Object.defineProperty(Document.prototype, 'schema', {
  get() { return this._schema; },
  enumerable: true,
});

Object.defineProperty(Document.prototype, 'base', {
  get() { return this._base; },
  enumerable: true,
});

// Setup test objects
const base = createBase();
const schema1 = createSchema(base, 'Schema1');
const schema2 = createSchema(base, 'Schema2');
base.schemas.Schema1 = schema1;
base.schemas.Schema2 = schema2;

const doc1 = new Document(schema1, base);
const doc2 = new Document(schema2, base);
doc2.$__.populated.ref_0.value.$__parent = doc1;

// The actual OOM test: assert.strictEqual should NOT crash
// when comparing objects with large inspect output.
// It should throw an AssertionError with a reasonable message size.
{
  const actual = doc2.$__.populated.ref_0.value.$__parent;
  const expected = doc2;

  // This assertion is expected to fail (they are different objects)
  // but it should NOT cause an OOM crash
  assert.throws(
    () => assert.strictEqual(actual, expected, 'Objects should be equal'),
    (err) => {
      // Should get an AssertionError, not an OOM crash
      assert.ok(err instanceof assert.AssertionError,
        'Expected AssertionError');

      // Message should exist and be reasonable (not hundreds of MB)
      // The fix limits inspect output to 2MB, so message should be bounded
      const maxExpectedSize = 5 * 1024 * 1024; // 5MB (2MB * 2 + overhead)
      assert.ok(err.message.length < maxExpectedSize,
        `Error message too large: ${(err.message.length / 1024 / 1024).toFixed(2)} MB`);

      return true;
    }
  );
}
