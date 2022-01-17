'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

// Test emit called by other context
const EE = new EventEmitter();

// Works as expected if the context has no `constructor.name`
{
  const ctx = Object.create(null);
  assert.throws(
    () => EE.emit.call(ctx, 'error', new Error('foo')),
    common.expectsError({ name: 'Error', message: 'foo' })
  );
}

assert.strictEqual(EE.emit.call({}, 'foo'), false);
