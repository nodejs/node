common = require("../common");
assert = common.assert
var events = require('events');


count = 0;

function listener1 () {
  console.log('listener1');
  count++;
}

function listener2 () {
  console.log('listener2');
  count++;
}

function listener3 () {
  console.log('listener3');
  count++;
}

e1 = new events.EventEmitter();
e1.addListener("hello", listener1);
e1.removeListener("hello", listener1);
assert.deepEqual([], e1.listeners('hello'));

e2 = new events.EventEmitter();
e2.addListener("hello", listener1);
e2.removeListener("hello", listener2);
assert.deepEqual([listener1], e2.listeners('hello'));

e3 = new events.EventEmitter();
e3.addListener("hello", listener1);
e3.addListener("hello", listener2);
e3.removeListener("hello", listener1);
assert.deepEqual([listener2], e3.listeners('hello'));



