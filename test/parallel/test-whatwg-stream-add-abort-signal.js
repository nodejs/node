'use strict';

const common = require('../common');
const assert = require('assert');
const { 
  addAbortSignal, 
  ReadableStream, 
  WritableStream, 
  TransformStream, 
  finished,
} = require("stream/web");

{
  assert.throws(() => {
    addAbortSignal('INVALID_SIGNAL');
  }, /ERR_INVALID_ARG_TYPE/);

  const ac = new AbortController();
  assert.throws(() => {
    addAbortSignal(ac.signal, 'INVALID_STREAM');
  }, /ERR_INVALID_ARG_TYPE/);
}

{
  const rs = new ReadableStream();
  const ac = new AbortController()
  assert.deepStrictEqual(rs, addAbortSignal(ac.signal, rs));
}

{
  const rs = new ReadableStream();
  finished(rs, common.mustSucceed())

  const ac = new AbortController();
  addAbortSignal(ac.signal, rs);

  ac.abort();
}

{
  const ws = new WritableStream();
  finished(ws, common.mustSucceed())

  const ac = new AbortController();
  addAbortSignal(ac.signal, ws);

  ac.abort();
}
  
{
  const ts = new TransformStream();
  finished(ts, common.mustSucceed())

  const ac = new AbortController();
  addAbortSignal(ac.signal, ts);

  ac.abort();
}
  