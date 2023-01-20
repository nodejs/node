'use strict';

const common = require('../common');
const { finished, addAbortSignal } = require('stream');
const { ReadableStream, WritableStream } = require('stream/web');
const assert = require('assert');

{
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('hello');
      controller.close();
    }
  });

  const reader = rs.getReader();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  finished(rs, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
  }));

  ac.abort();

  assert.rejects(reader.read(), 'AbortError: The operation was aborted.').then(common.mustCall());
}

{
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.enqueue('b');
      controller.enqueue('c');
      controller.close();
    }
  });

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  assert.rejects((async () => {
    for await (const chunk of rs) {
      if (chunk === 'b') {
        ac.abort();
      }
    }
  })(), /AbortError/);
}

{
  const values = [];
  const ws = new WritableStream({
    write(chunk) {
      values.push(chunk);
    }
  });

  finished(ws, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.deepStrictEqual(values, ['a']);
  }));

  const ac = new AbortController();

  addAbortSignal(ac.signal, ws);

  const writer = ws.getWriter();

  writer.write('a').then(() => {
    ac.abort();
  });
}
