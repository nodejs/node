'use strict';

const common = require('../common.js');
const assert = require('assert');
const { Parser } = require('node:json');

const payloads = {
  flat_small: {
    json: '{"name":"Alice","age":30,"active":true,"score":9.5}',
    schema: {
      type: 'object',
      properties: {
        name: { type: 'string' },
        age: { type: 'integer' },
        active: { type: 'boolean' },
        score: { type: 'number' },
      },
    },
  },
  flat_large: {
    json: JSON.stringify({
      id: 1, name: 'Bob', email: 'bob@example.com', age: 25,
      address: '123 Main St', city: 'Anytown', state: 'NY', zip: '10001',
      country: 'US', phone: '555-1234', fax: '555-5678', website: 'http://example.com',
      company: 'Acme', department: 'Engineering', title: 'Engineer',
      manager: 'Carol', team: 'Backend', score: 8.2, active: true, tags: 'foo,bar',
    }),
    schema: {
      type: 'object',
      properties: {
        id: { type: 'integer' },
        name: { type: 'string' },
        score: { type: 'number' },
        active: { type: 'boolean' },
      },
    },
  },
  nested: {
    json: '{"user":{"id":42,"name":"Carol","roles":["admin","user"]},"meta":{"version":2,"debug":false}}',
    schema: {
      type: 'object',
      properties: {
        user: {
          type: 'object',
          properties: {
            id: { type: 'integer' },
            name: { type: 'string' },
          },
        },
      },
    },
  },
  array: {
    json: JSON.stringify(
      Array.from({ length: 10 }, (_, i) => ({
        id: i + 1, name: `Item ${i + 1}`, value: i * 1.5, active: i % 2 === 0,
        description: `Description for item ${i + 1}`, extra: 'ignored',
      })),
    ),
    schema: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          id: { type: 'integer' },
          name: { type: 'string' },
          value: { type: 'number' },
          active: { type: 'boolean' },
        },
      },
    },
  },
};

const bench = common.createBenchmark(main, {
  method: ['node', 'v8'],
  payload: Object.keys(payloads),
  n: [1e4],
});

function main({ method, payload, n }) {
  const { json, schema } = payloads[payload];

  if (method === 'node') {
    const parser = new Parser(schema);
    let result = parser.parse(json);  // Warm up / avoid dead code elimination
    bench.start();
    for (let i = 0; i < n; ++i) {
      result = parser.parse(json);
    }
    bench.end(n);
    assert.ok(result);
  } else {
    let result = JSON.parse(json);
    bench.start();
    for (let i = 0; i < n; ++i) {
      result = JSON.parse(json);
    }
    bench.end(n);
    assert.ok(result);
  }
}
