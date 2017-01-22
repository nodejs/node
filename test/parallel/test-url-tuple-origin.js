'use strict';
require('../common');
const assert = require('assert');
const url = require('url');

// verify via originFor
const originFor = url.originFor;
const exampleURL = 'https://example.com';

assert.strictEqual(originFor(exampleURL).scheme, 'https');
assert.strictEqual(originFor(exampleURL).host, 'example.com');
assert.strictEqual(originFor(exampleURL).port, undefined);
assert.strictEqual(originFor(`${exampleURL}:1234`).port, 1234);
assert.strictEqual(originFor(exampleURL).domain, null);
assert.strictEqual(originFor(exampleURL).effectiveDomain, 'example.com');
assert.strictEqual(originFor(exampleURL).toString(), exampleURL);
assert.strictEqual(originFor(exampleURL).toString(false), exampleURL);
