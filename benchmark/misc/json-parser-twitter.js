'use strict';

// Benchmark using the twitter.json fixture (~631 KB, 100 tweets).
// Uses a lower default n than json-parser.js due to payload size.

const common = require('../common.js');
const assert = require('assert');
const { readFileSync } = require('fs');
const { join } = require('path');
const { Parser } = require('node:json');

const json = readFileSync(join(__dirname, '../fixtures/twitter.json'), 'utf8');

// Project 6 fields out of 23 per tweet; skip nested entities, user, etc.
const schema = {
  type: 'object',
  properties: {
    statuses: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          id_str: { type: 'string' },
          text: { type: 'string' },
          created_at: { type: 'string' },
          retweet_count: { type: 'integer' },
          favorite_count: { type: 'integer' },
          lang: { type: 'string' },
        },
      },
    },
  },
};

const bench = common.createBenchmark(main, {
  method: ['node', 'v8'],
  n: [10],
});

function main({ method, n }) {
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
