'use strict';

const common = require('../common');
const {
  ReadableStream,
  WritableStream,
  TransformStream,
  finished,
  CountQueuingStrategy,
} = require('stream/web');
const { Readable } = require('stream');
const assert = require('assert');

/* Errors */

{
  assert.throws(() => {
    finished();
  }, /ERR_MISSING_ARGS/);
}

{
  assert.throws(() => {
    finished('NOT_A_STREAM', common.mustNotCall());
  }, /ERR_INVALID_ARG_TYPE/);
}

{
  const stream = new ReadableStream();
  assert.throws(() => {
    finished(stream, 'NOT_A_FUNC');
  }, /ERR_INVALID_ARG_TYPE/);
}

{
  const nonWebStream = new Readable();
  assert.throws(() => {
    finished(nonWebStream, common.mustNotCall());
  }, /ERR_INVALID_ARG_TYPE/);
}

{
  const rs = new ReadableStream();
  const ws = new WritableStream();
  const ts = new TransformStream();
  assert.doesNotThrow(() => {
    finished(rs);
    finished(ws);
    finished(ts);
  });
}

/* ReadableStreams */

{
  const rs = new ReadableStream();

  finished(rs, common.mustSucceed());

  rs.cancel();
}

{
  const rs = new ReadableStream();

  const removeCallbacks = finished(rs, common.mustNotCall());
  removeCallbacks();

  rs.cancel();
}

{
  const rs = new ReadableStream();

  finished(rs, common.mustNotCall());
  const removeCallbacks = finished(rs, common.mustNotCall());
  removeCallbacks();

  rs.cancel();
}

{
  const rs = new ReadableStream({
    start(controller) {
      controller.close();
    },
  });

  finished(rs, common.mustCall());

  async function readEntireStream(s) {
    const r = s.getReader();
    let result;
    do {
      result = await r.read();
    } while (!result.done);
  }

  readEntireStream(rs).then(common.mustCall());
}

/* WritableStreams */

{
  const ws = new WritableStream();

  finished(ws, common.mustSucceed());

  ws.close();
}

{
  const ws = new WritableStream();

  finished(ws, common.mustSucceed());

  ws.abort();
}

{
  const ws = new WritableStream();
  const removeCallbacks = finished(ws, common.mustNotCall());
  removeCallbacks();

  ws.close();
}

{
  const ws = new WritableStream();

  finished(ws, common.mustNotCall());
  const removeCallbacks = finished(ws, common.mustNotCall());
  removeCallbacks();

  ws.close();
}

/* TransformStreams */

{
  const ts = new TransformStream();

  finished(ts, common.mustSucceed());

  ts.readable.cancel();
}

{
  const ts = new TransformStream();

  finished(ts, common.mustSucceed());

  ts.readable.abort();
}

{
  const ts = new TransformStream();

  const removeCallbacks = finished(ts, common.mustNotCall());
  removeCallbacks();

  ts.readable.cancel();
}

{
  const ts = new TransformStream();

  finished(ts, common.mustNotCall());
  const removeCallbacks = finished(ts, common.mustNotCall());
  removeCallbacks();

  ts.readable.cancel();
}
