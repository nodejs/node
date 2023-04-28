'use strict';

// Test that monkeypatching console._stdout and console._stderr works.
const common = require('../common');

const { Writable } = require('stream');

const streamToNowhere = new Writable({ write: common.mustCall() });
const anotherStreamToNowhere = new Writable({ write: common.mustCall() });

// Overriding the lazy-loaded _stdout and _stderr properties this way is what we
// are testing. Don't change this to be a Console instance from calling a
// constructor. It has to be the global `console` object.
console._stdout = streamToNowhere;
console._stderr = anotherStreamToNowhere;

console.log('fhqwhgads');
console.error('fhqwhgads');
