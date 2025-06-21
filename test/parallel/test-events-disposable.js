'use strict';

const common = require('../common');
const { strictEqual, throws } = require('assert');
const { EventEmitter } = require('events');

const emitter = new EventEmitter();

{
  // Verify that the disposable stack removes the handlers
  // when the stack is disposed.
  using ds = new DisposableStack();
  ds.use(emitter.addDisposableListener('foo', common.mustCall()));
  ds.use(emitter.addDisposableListener('bar', common.mustCall()));
  ds.use(emitter.addDisposableListener('baz', common.mustNotCall()),
         { once: true });
  emitter.emit('foo');
  emitter.emit('bar');
  strictEqual(emitter.listenerCount('foo'), 1);
  strictEqual(emitter.listenerCount('bar'), 1);

  // The disposer returned by addDisposableListener can be called manually.
  const disposer = emitter.addDisposableListener('foo', common.mustNotCall());
  strictEqual(emitter.listenerCount('foo'), 2);
  disposer.dispose();
  strictEqual(emitter.listenerCount('foo'), 1);
  // Disposer is callable multiple times without error.
  disposer.dispose();
}
emitter.emit('foo');
emitter.emit('bar');
emitter.emit('baz');
strictEqual(emitter.listenerCount('foo'), 0);
strictEqual(emitter.listenerCount('bar'), 0);

// ============================================================================
// Type checking on inputs
throws(() => emitter.addDisposableListener('foo', 'not a function'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

throws(() => emitter.addDisposableListener('foo', () => {}, ''), {
  code: 'ERR_INVALID_ARG_TYPE',
});

throws(() => emitter.addDisposableListener('foo', () => {}, { once: '' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
