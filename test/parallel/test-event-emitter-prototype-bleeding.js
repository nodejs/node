'use strict';
var util = require('util');
var EventEmitter = require('events').EventEmitter;

var TestClass = function () {};
TestClass.prototype = new EventEmitter;

function listener_n1() {
	// This one is okay to be called!
}
function listener_n2() {
	throw new Error("This one should not be called!")
}

var ok = new TestClass();
var broken = new TestClass();
broken.on('end', listener_n2);
ok.on('end', listener_n1);
ok.emit('end');