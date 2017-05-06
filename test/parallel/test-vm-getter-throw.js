'use strict';

require('../common');
const assert = require('assert');

const vm = require('vm');

const x = {};

const code = `
  var foo = 1;
  Object.defineProperty(this, "bar", {
    enumerable: true,
    get: function () { throw 42 }
  });
  var baz = 2;
`;
vm.runInNewContext(code, x);

// bar is skipped, see known_issues/test-vm-attributes-property-not-on-sandbox.js
assert.deepEqual(x, { foo: 1, baz: 2 });
