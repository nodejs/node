var common = require('../common');
var assert = require('assert');
var vm = require('vm');

var sbx = {};
sbx.window = sbx;

sbx = vm.createContext(sbx);

sbx.test = 123;

assert.equal(sbx.window.window.window.window.window.test, 123);