'use strict';

require('../common');
var assert = require('assert');

var vm = require('vm');

var symbol = Symbol();

function Document() {
  this[symbol] = 'foo';
}

Document.prototype.getSymbolValue = function() {
  return this[symbol];
};

var context = new Document();
vm.createContext(context);

assert.equal(context.getSymbolValue(), 'foo',
    'should return symbol-keyed value from the outside');

assert.equal(vm.runInContext('this.getSymbolValue()', context), 'foo',
    'should return symbol-keyed value from the inside');
