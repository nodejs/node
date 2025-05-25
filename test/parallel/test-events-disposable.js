'use strict';

const common = require('../common');
const { strictEqual } = require('assert');
const { EventEmitter } = require('events');

const emitter = new EventEmitter();

{
  // Verifiy that the disposable stack removes the handlers
  // when the stack is disposed.
  // TODO(@jasnell): DisposableStack is not recognized by eslint?
  using ds = new DisposableStack();  // eslint-disable-line no-undef
  ds.use(emitter.use('foo', common.mustCall()));
  ds.use(emitter.use('bar', common.mustCall()));
  ds.use(emitter.useOnce('baz', common.mustNotCall()));
  emitter.emit('foo');
  emitter.emit('bar');
  strictEqual(emitter.listenerCount('foo'), 1);
  strictEqual(emitter.listenerCount('bar'), 1);
}
emitter.emit('foo');
emitter.emit('bar');
emitter.emit('baz');
strictEqual(emitter.listenerCount('foo'), 0);
strictEqual(emitter.listenerCount('bar'), 0);
