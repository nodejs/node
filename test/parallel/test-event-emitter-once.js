'use strict';
const common = require('../common');
const events = require('events');

const e = new events.EventEmitter();

e.once('hello', common.mustCall(function(a, b) {}));

e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');

const remove = function() {
  common.fail('once->foo should not be emitted');
};

e.once('foo', remove);
e.removeListener('foo', remove);
e.emit('foo');

e.once('e', common.mustCall(function() {
  e.emit('e');
}));

e.once('e', common.mustCall(function() {}));

e.emit('e');
