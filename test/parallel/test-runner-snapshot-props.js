'use strict';
require('../common');

const assert = require('node:assert');
const { resolve } = require('node:path');
const { describe, test } = require('node:test');


// These props are needed to support jest-like snapshots (which is not itself a feature in Core).

const rootName = 'props for snapshots';
describe(rootName, () => {
  test('test context has "file" property', (ctx) => {
    assert.strictEqual(ctx.file, resolve(__filename));
  });

  const nestedName = '"fullName" contains both case and ancestor names';
  test(nestedName, (ctx) => {
    assert.match(ctx.fullName, new RegExp(rootName));
    assert.match(ctx.fullName, new RegExp(nestedName));

    // Ensure root appears before tip
    assert.ok(ctx.fullName.indexOf(rootName) < ctx.fullName.indexOf(nestedName));
  });
});
