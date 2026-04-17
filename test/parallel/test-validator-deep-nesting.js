'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('five-level nested object path', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      a: {
        type: 'object',
        properties: {
          b: {
            type: 'object',
            properties: {
              c: {
                type: 'object',
                properties: {
                  d: {
                    type: 'object',
                    properties: {
                      e: { type: 'string' },
                    },
                  },
                },
              },
            },
          },
        },
      },
    },
  });

  const result = schema.validate({ a: { b: { c: { d: { e: 42 } } } } });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 1);
  assert.strictEqual(result.errors[0].path, 'a.b.c.d.e');
  assert.strictEqual(result.errors[0].code, 'INVALID_TYPE');
});

test('deeply nested arrays of arrays', () => {
  const schema = new Schema({
    type: 'array',
    items: {
      type: 'array',
      items: {
        type: 'array',
        items: { type: 'number' },
      },
    },
  });

  const result = schema.validate([[[1, 2], [3, 'bad']]]);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, '[0][1][1]');
  assert.strictEqual(result.errors[0].code, 'INVALID_TYPE');
});

test('mixed arrays and objects deep path', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      teams: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            members: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  addresses: {
                    type: 'array',
                    items: {
                      type: 'object',
                      properties: {
                        zip: { type: 'string', minLength: 5 },
                      },
                    },
                  },
                },
              },
            },
          },
        },
      },
    },
  });

  const result = schema.validate({
    teams: [
      {
        members: [
          { addresses: [{ zip: '12345' }] },
          { addresses: [{ zip: '12345' }, { zip: '1' }] },
        ],
      },
    ],
  });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(
    result.errors[0].path,
    'teams[0].members[1].addresses[1].zip',
  );
  assert.strictEqual(result.errors[0].code, 'STRING_TOO_SHORT');
});

test('multiple errors at different deep paths', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      outer: {
        type: 'object',
        required: ['x', 'y'],
        properties: {
          x: {
            type: 'object',
            properties: {
              value: { type: 'number', minimum: 0 },
            },
          },
          y: {
            type: 'object',
            properties: {
              value: { type: 'string', minLength: 3 },
            },
          },
        },
      },
    },
  });

  const result = schema.validate({
    outer: {
      x: { value: -5 },
      y: { value: 'a' },
    },
  });
  assert.strictEqual(result.valid, false);
  const paths = result.errors.map((e) => e.path);
  assert.ok(paths.includes('outer.x.value'));
  assert.ok(paths.includes('outer.y.value'));
});

test('deeply nested applyDefaults fills missing leaves', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      a: {
        type: 'object',
        properties: {
          b: {
            type: 'object',
            properties: {
              c: {
                type: 'object',
                properties: {
                  d: { type: 'string', default: 'deep' },
                },
              },
            },
          },
        },
      },
    },
  });

  const result = schema.applyDefaults({ a: { b: { c: {} } } });
  assert.strictEqual(result.a.b.c.d, 'deep');
});
