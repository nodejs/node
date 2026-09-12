'use strict';

// The writer's closed promise stays pending until the sink's abort()
// algorithm settles, even when it is first observed after the stream has
// already reached the 'errored' state.

const common = require('../common');
const assert = require('assert');
const { setImmediate: immediate } = require('timers/promises');
const { WritableStream } = require('stream/web');

async function main() {
  const error = new Error('boom');
  const { promise: abortComplete, resolve: finishAbort } = Promise.withResolvers();
  const ws = new WritableStream({
    abort: common.mustCall((reason) => {
      assert.strictEqual(reason, error);
      return abortComplete;
    }),
  });
  const writer = ws.getWriter();
  const aborted = writer.abort(error);

  // Lets the stream finish erroring and call the sink's abort().
  await immediate();

  let closedSettled = false;
  const closed = writer.closed.catch(common.mustCall((reason) => {
    closedSettled = true;
    assert.strictEqual(reason, error);
  }));

  await immediate();
  assert.strictEqual(closedSettled, false);

  finishAbort();
  await aborted;
  await closed;
  assert.strictEqual(closedSettled, true);
}

main().then(common.mustCall());
