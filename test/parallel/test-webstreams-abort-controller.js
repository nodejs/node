'use strict';

const common = require('../common');
const { finished, addAbortSignal } = require('stream');
const { ReadableStream, WritableStream } = require('stream/web');
const assert = require('assert');

function createTestReadableStream() {
  return new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.enqueue('b');
      controller.enqueue('c');
      controller.close();
    }
  });
}

function createTestWritableStream(values) {
  return new WritableStream({
    write(chunk) {
      values.push(chunk);
    }
  });
}

{
  const rs = createTestReadableStream();

  const reader = rs.getReader();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  finished(rs, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(reader.read(), /AbortError/).then(common.mustCall());
    assert.rejects(reader.closed, /AbortError/).then(common.mustCall());
  }));

  reader.read().then(common.mustCall((result) => {
    assert.strictEqual(result.value, 'a');
    ac.abort();
  }));
}

{
  const rs = createTestReadableStream();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  assert.rejects((async () => {
    for await (const chunk of rs) {
      if (chunk === 'b') {
        ac.abort();
      }
    }
  })(), /AbortError/).then(common.mustCall());
}

{
  const rs1 = createTestReadableStream();

  const rs2 = createTestReadableStream();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs1);
  addAbortSignal(ac.signal, rs2);

  const reader1 = rs1.getReader();
  const reader2 = rs2.getReader();

  finished(rs1, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(reader1.read(), /AbortError/).then(common.mustCall());
    assert.rejects(reader1.closed, /AbortError/).then(common.mustCall());
  }));

  finished(rs2, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(reader2.read(), /AbortError/).then(common.mustCall());
    assert.rejects(reader2.closed, /AbortError/).then(common.mustCall());
  }));

  ac.abort();
}

{
  const rs = createTestReadableStream();

  const { 0: rs1, 1: rs2 } = rs.tee();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  const reader1 = rs1.getReader();
  const reader2 = rs2.getReader();

  finished(rs1, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(reader1.read(), /AbortError/).then(common.mustCall());
    assert.rejects(reader1.closed, /AbortError/).then(common.mustCall());
  }));

  finished(rs2, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(reader2.read(), /AbortError/).then(common.mustCall());
    assert.rejects(reader2.closed, /AbortError/).then(common.mustCall());
  }));

  ac.abort();
}

{
  const values = [];
  const ws = createTestWritableStream(values);

  const ac = new AbortController();

  addAbortSignal(ac.signal, ws);

  const writer = ws.getWriter();

  finished(ws, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.deepStrictEqual(values, ['a']);
    assert.rejects(writer.write('b'), /AbortError/).then(common.mustCall());
    assert.rejects(writer.closed, /AbortError/).then(common.mustCall());
  }));

  writer.write('a').then(() => {
    ac.abort();
  });
}

{
  const values = [];

  const ws1 = createTestWritableStream(values);
  const ws2 = createTestWritableStream(values);

  const ac = new AbortController();

  addAbortSignal(ac.signal, ws1);
  addAbortSignal(ac.signal, ws2);

  const writer1 = ws1.getWriter();
  const writer2 = ws2.getWriter();

  finished(ws1, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(writer1.write('a'), /AbortError/).then(common.mustCall());
    assert.rejects(writer1.closed, /AbortError/).then(common.mustCall());
  }));

  finished(ws2, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(writer2.write('a'), /AbortError/).then(common.mustCall());
    assert.rejects(writer2.closed, /AbortError/).then(common.mustCall());
  }));

  ac.abort();
}
