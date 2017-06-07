'use strict';
// Ref: https://github.com/nodejs/node/issues/2734

require('../common');
const vm = require('vm');
const assert = require('assert');


const sandbox = Object.create(null, {
  prop: {
    get: function() { return 'foo';}
  }
});

const context = vm.createContext(sandbox);

const script = `
              Object.getOwnPropertyDescriptor(this, 'prop');
              `;

const result = vm.runInContext(script, context);

// accessor property is flattened to data property with the value
// returned by the getter and 'writable: true'
assert.strictEqual(result.value, undefined); // returns 'foo'
