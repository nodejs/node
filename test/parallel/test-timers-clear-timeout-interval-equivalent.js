'use strict';
const common = require('../common');

// This test makes sure that timers created with setTimeout can be disarmed by
// clearInterval and that timers created with setInterval can be disarmed by
// clearTimeout.
//
// This behavior is documented in the HTML Living Standard:
//
// * Refs: https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval

// Disarm interval with clearTimeout.
const interval = setInterval(common.mustNotCall(), 1);
clearTimeout(interval);

// Disarm timeout with clearInterval.
const timeout = setTimeout(common.mustNotCall(), 1);
clearInterval(timeout);
