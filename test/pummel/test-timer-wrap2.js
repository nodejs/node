'use strict';
// Flags: --expose_internals
var common = require('../common');
var assert = require('assert');

// Test that allocating a timer does not increase the loop's reference
// count.

var Timer = require('binding/timer_wrap').Timer;
var t = new Timer();
