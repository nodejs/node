/* eslint-disable no-unused-vars */
'use strict';

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

// A subscription can be scoped with `using`
{
  const channel = dc.channel('test-subscribe-dispose-using');
  const subscriber = common.mustCall((message) => {
    assert.deepStrictEqual(message, { foo: 'bar' });
  });

  {
    using subscription = channel.subscribe(subscriber);

    assert.ok(channel.hasSubscribers);
    channel.publish({ foo: 'bar' });
  }

  assert.ok(!channel.hasSubscribers);
  channel.publish({ foo: 'bar' });
}

// The first subscribe on a channel goes through the inactive prototype and
// must still return a working disposable
{
  const channel = dc.channel('test-subscribe-dispose-first');
  const subscriber = common.mustNotCall();

  const subscription = channel.subscribe(subscriber);
  assert.ok(channel.hasSubscribers);

  subscription[Symbol.dispose]();
  assert.ok(!channel.hasSubscribers);

  channel.publish({ foo: 'bar' });
}

// Disposing more than once is a no-op
{
  const channel = dc.channel('test-subscribe-dispose-idempotent');
  const subscriber = common.mustNotCall();

  const subscription = channel.subscribe(subscriber);
  subscription[Symbol.dispose]();
  subscription[Symbol.dispose]();

  assert.ok(!channel.hasSubscribers);
}

// Disposing twice must not remove another registration of the same handler
{
  const channel = dc.channel('test-subscribe-dispose-duplicate');
  const subscriber = common.mustCall((message) => {
    assert.deepStrictEqual(message, { foo: 'bar' });
  });

  const first = channel.subscribe(subscriber);
  const second = channel.subscribe(subscriber);

  first[Symbol.dispose]();
  first[Symbol.dispose]();

  assert.ok(channel.hasSubscribers);
  channel.publish({ foo: 'bar' });

  second[Symbol.dispose]();
  assert.ok(!channel.hasSubscribers);
}

// Disposing after an explicit unsubscribe does not throw
{
  const channel = dc.channel('test-subscribe-dispose-after-unsubscribe');
  const subscriber = common.mustNotCall();

  const subscription = channel.subscribe(subscriber);

  assert.ok(channel.unsubscribe(subscriber));
  subscription[Symbol.dispose]();

  assert.ok(!channel.hasSubscribers);
}

// The module level subscribe returns the same disposable
{
  const name = 'test-subscribe-dispose-module-level';
  const subscriber = common.mustNotCall();

  const subscription = dc.subscribe(name, subscriber);
  assert.ok(dc.hasSubscribers(name));

  subscription[Symbol.dispose]();
  assert.ok(!dc.hasSubscribers(name));
}

// Invalid subscribers still throw
{
  const channel = dc.channel('test-subscribe-dispose-invalid');

  assert.throws(() => {
    channel.subscribe(null);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
}

// tracingChannel disposes every handler it subscribed
{
  const channel = dc.tracingChannel('test-subscribe-dispose-tracing');
  const handler = common.mustNotCall();

  const subscription = channel.subscribe({
    start: handler,
    end: handler,
    asyncStart: handler,
    asyncEnd: handler,
    error: handler,
  });

  assert.ok(channel.start.hasSubscribers);
  assert.ok(channel.end.hasSubscribers);
  assert.ok(channel.asyncStart.hasSubscribers);
  assert.ok(channel.asyncEnd.hasSubscribers);
  assert.ok(channel.error.hasSubscribers);

  subscription[Symbol.dispose]();
  subscription[Symbol.dispose]();

  assert.ok(!channel.hasSubscribers);
  assert.ok(!channel.start.hasSubscribers);
  assert.ok(!channel.end.hasSubscribers);
  assert.ok(!channel.asyncStart.hasSubscribers);
  assert.ok(!channel.asyncEnd.hasSubscribers);
  assert.ok(!channel.error.hasSubscribers);
}

// tracingChannel handles partial subscriber sets
{
  const channel = dc.tracingChannel('test-subscribe-dispose-tracing-partial');
  const handler = common.mustNotCall();

  {
    using subscription = channel.subscribe({ start: handler });
    assert.ok(channel.start.hasSubscribers);
  }
  assert.ok(!channel.hasSubscribers);

  {
    using subscription = channel.subscribe({ asyncEnd: handler });
    assert.ok(channel.asyncEnd.hasSubscribers);
  }
  assert.ok(!channel.hasSubscribers);
}

// boundedChannel disposes every handler it subscribed
{
  const channel = dc.boundedChannel('test-subscribe-dispose-bounded');
  const handler = common.mustNotCall();

  const subscription = channel.subscribe({
    start: handler,
    end: handler,
  });

  assert.ok(channel.start.hasSubscribers);
  assert.ok(channel.end.hasSubscribers);

  subscription[Symbol.dispose]();
  subscription[Symbol.dispose]();

  assert.ok(!channel.hasSubscribers);
  assert.ok(!channel.start.hasSubscribers);
  assert.ok(!channel.end.hasSubscribers);
}

// boundedChannel handles partial subscriber sets
{
  const channel = dc.boundedChannel('test-subscribe-dispose-bounded-partial');
  const handler = common.mustNotCall();

  {
    using subscription = channel.subscribe({ start: handler });

    assert.ok(channel.start.hasSubscribers);
    assert.ok(!channel.end.hasSubscribers);
  }

  assert.ok(!channel.hasSubscribers);
}
