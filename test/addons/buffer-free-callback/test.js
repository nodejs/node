'use strict';
// Flags: --expose-gc

var assert = require('assert');
var binding = require('./build/Release/binding');
var buf = binding.alloc();
var slice = buf.slice(32);
buf = null;
binding.check(slice);
