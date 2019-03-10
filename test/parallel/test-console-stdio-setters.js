'use strict';

// Test that monkeypatching console._stdout and console._stderr works.
const common = require('../common');

const { Writable } = require('stream');
const { Console } = require('console');

const streamToNowhere = new Writable({ write: common.mustCall() });
const anotherStreamToNowhere = new Writable({ write: common.mustCall() });
const myConsole = new Console(process.stdout);

// Overriding the _stdout and _stderr properties this way is what we are
// testing. Don't change this to be done via arguments passed to the constructor
// above.
myConsole._stdout = streamToNowhere;
myConsole._stderr = anotherStreamToNowhere;

myConsole.log('fhqwhgads');
myConsole.error('fhqwhgads');
