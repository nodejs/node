process.mixin(require("./common"));
var events = require('events');

var callbacks_called = [ ];

var e = new events.EventEmitter();

function callback1() {
    callbacks_called.push("callback1");
    e.addListener("foo", callback2);
    e.removeListener("foo", callback1);
}

function callback2() {
    callbacks_called.push("callback2");
    e.removeListener("foo", callback2);
}

e.addListener("foo", callback1);
assert.equal(1, e.listeners("foo").length);

e.emit("foo");
assert.equal(1, e.listeners("foo").length);
assert.deepEqual(["callback1"], callbacks_called);

e.emit("foo");
assert.equal(0, e.listeners("foo").length);
assert.deepEqual(["callback1", "callback2"], callbacks_called);

e.emit("foo");
assert.equal(0, e.listeners("foo").length);
assert.deepEqual(["callback1", "callback2"], callbacks_called);
