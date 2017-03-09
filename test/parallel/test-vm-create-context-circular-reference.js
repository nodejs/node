'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

let sbx = {};
sbx.window = sbx;

sbx = vm.createContext(sbx);

sbx.test = 123;

assert.equal(sbx.window.window.window.window.window.test, 123);
