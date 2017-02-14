'use strict';

require('../common');
const assert = require('assert');

const vm = require('vm');

const symbol = Symbol();

function Document() {
  this[symbol] = 'foo';
}

Document.prototype.getSymbolValue = function() {
  return this[symbol];
};

const context = new Document();
vm.createContext(context);

assert.strictEqual(context.getSymbolValue(), 'foo',
                   'should return symbol-keyed value from the outside');

assert.strictEqual(vm.runInContext('this.getSymbolValue()', context), 'foo',
                   'should return symbol-keyed value from the inside');
