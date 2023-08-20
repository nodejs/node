'use strict';

const common = require('../../common');

const binding = require(`./build/${common.buildType}/binding`);

// Create an AsyncWrap object.
const timer = setTimeout(common.mustNotCall(), 1);
timer.unref();

// Stress-test the heap profiler.
binding.test();

clearTimeout(timer);
