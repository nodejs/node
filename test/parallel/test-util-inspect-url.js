'use strict';

require('../common');
const assert = require('assert');
const { format, inspect } = require('util');

class TestURL extends URL {}

assert.strictEqual(inspect(TestURL.prototype), 'URL {}');
assert.strictEqual(
  inspect(TestURL.prototype, { customInspect: false }),
  'URL {}',
);

// Inspecting hidden properties must not throw when reaching the prototype.
inspect(TestURL, { showHidden: true });
inspect({ TestURL }, { showHidden: true });
format('%o', { TestURL });

const url = new TestURL('https://example.org/');
assert.strictEqual(
  inspect(url, { customInspect: false }),
  'https://example.org/',
);

class NonStringURL extends URL {
  get href() {
    return undefined;
  }
}

assert.strictEqual(
  inspect(new NonStringURL('https://example.org/'), { customInspect: false }),
  'NonStringURL [URL] {}',
);
