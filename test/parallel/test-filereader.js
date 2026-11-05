// Flags: --expose-internals --experimental-web-worker
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createFileList } = require('internal/filereader');

{
  assert.strictEqual(typeof FileReader, 'function');
  assert.strictEqual(typeof FileList, 'function');
  assert.strictEqual(typeof ProgressEvent, 'function');
  assert.strictEqual(typeof FileReaderSync, 'undefined');
}

{
  const reader = new FileReader();
  assert.strictEqual(reader.readyState, FileReader.EMPTY);
  assert.strictEqual(reader.result, null);
  assert.strictEqual(reader.error, null);
  for (const name of [
    'onloadstart', 'onprogress', 'onload', 'onabort', 'onerror', 'onloadend',
  ]) {
    assert.strictEqual(reader[name], null);
  }
  assert.strictEqual(FileReader.EMPTY, 0);
  assert.strictEqual(FileReader.LOADING, 1);
  assert.strictEqual(FileReader.DONE, 2);
  assert.strictEqual(FileReader.prototype.EMPTY, 0);
  assert.throws(() => {
    reader.readAsText({});
  }, { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => {
    FileReader.prototype.readAsText.call({}, new Blob());
  }, { code: 'ERR_INVALID_THIS' });
  reader.abort();
  assert.strictEqual(reader.readyState, FileReader.EMPTY);
  assert.strictEqual(reader.result, null);
}

{
  const reader = new FileReader();
  const events = [];
  reader.addEventListener('loadstart', common.mustCall(() => {
    events.push('loadstart');
    assert.strictEqual(reader.readyState, FileReader.LOADING);
    assert.strictEqual(reader.result, null);
  }));
  reader.addEventListener('progress', common.mustCall(() => {
    events.push('progress');
    assert.strictEqual(reader.readyState, FileReader.LOADING);
    assert.strictEqual(reader.result, null);
  }));
  reader.addEventListener('load', common.mustCall(() => {
    events.push('load');
    assert.strictEqual(reader.readyState, FileReader.DONE);
    assert.strictEqual(reader.result, 'TEST');
  }));
  reader.addEventListener('loadend', common.mustCall(() => {
    events.push('loadend');
    assert.deepStrictEqual(events, ['loadstart', 'progress', 'load', 'loadend']);
  }));
  reader.readAsText(new Blob(['TEST']));
}

{
  const reader = new FileReader();
  const events = [];
  reader.onloadstart = common.mustCall(() => events.push('loadstart'));
  reader.onprogress = common.mustNotCall();
  reader.onload = common.mustCall(() => {
    events.push('load');
    assert.strictEqual(reader.result, '');
  });
  reader.onloadend = common.mustCall(() => {
    events.push('loadend');
    assert.deepStrictEqual(events, ['loadstart', 'load', 'loadend']);
  });
  reader.readAsText(new Blob());
}

{
  const reader = new FileReader();
  reader.onload = common.mustCall(() => {
    assert.strictEqual(reader.result, '\u4554\u5453');
  });
  reader.readAsText(new Blob(['TEST']), 'UTF-16');
}

{
  const data = [0xFE, 0xFF, 0x00, 0x68, 0x00, 0x65, 0x00, 0x6C, 0x00, 0x6C, 0x00, 0x6F];
  const reader = new FileReader();
  reader.onload = common.mustCall(() => {
    assert.strictEqual(reader.result, 'hello');
  });
  reader.readAsText(new Blob([new Uint8Array(data)], {
    type: 'text/plain;charset=UTF-16BE',
  }));
}

{
  const reader = new FileReader();
  reader.onload = common.mustCall(() => {
    assert.strictEqual(reader.result, 'data:text/plain;base64,VEVTVA==');
    assert.strictEqual(reader.result instanceof ArrayBuffer, false);
  });
  reader.readAsDataURL(new Blob(['TEST'], { type: 'text/plain' }));
}

{
  const reader = new FileReader();
  reader.onload = common.mustCall(() => {
    assert.strictEqual(
      reader.result,
      'data:;base64,VEVTVA==');
  });
  reader.readAsDataURL(new Blob(['TEST']));
}

{
  const reader = new FileReader();
  reader.onload = common.mustCall(() => {
    assert.ok(reader.result instanceof ArrayBuffer);
    assert.deepStrictEqual(
      new Uint8Array(reader.result),
      new Uint8Array([84, 69, 83, 84]));
  });
  reader.readAsArrayBuffer(new Blob(['TEST']));
}

{
  const reader = new FileReader();
  reader.onload = common.mustCall(() => {
    assert.strictEqual(reader.result, '\xcf\x83');
  });
  reader.readAsBinaryString(new Blob(['σ']));
}

{
  const reader = new FileReader();
  const blob = new Blob(['TEST THE ABORT METHOD']);
  reader.onloadstart = common.mustCall(() => {
    assert.strictEqual(reader.readyState, FileReader.LOADING);
    reader.onabort = common.mustCall((event) => {
      assert.strictEqual(event.type, 'abort');
      assert.strictEqual(event.bubbles, false);
      assert.strictEqual(event.cancelable, false);
      assert.ok(event instanceof ProgressEvent);
      assert.strictEqual(reader.readyState, FileReader.DONE);
      assert.strictEqual(reader.result, null);
    });
    reader.onloadend = common.mustCall(() => {
      assert.strictEqual(reader.readyState, FileReader.DONE);
    });
    reader.onload = common.mustNotCall();
    reader.abort();
  });
  reader.readAsText(blob);
}

{
  const reader = new FileReader();
  reader.readAsText(new Blob(['one']));
  assert.throws(() => {
    reader.readAsText(new Blob(['two']));
  }, { name: 'InvalidStateError' });
}

{
  const first = new FileReader();
  first.onloadstart = common.mustCall(() => {
    first.abort();
    first.onloadstart = null;
    first.onloadend = common.mustCall(() => {
      assert.strictEqual(first.result, 'second');
    });
    first.readAsText(new Blob(['second']));
  });
  first.readAsText(new Blob(['first']));
}

{
  assert.throws(() => new FileList(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });
  const file = new File(['hello'], 'hello.txt');
  const list = createFileList([file]);
  assert.ok(list instanceof FileList);
  assert.strictEqual(list.length, 1);
  assert.strictEqual(list.item(0), file);
  assert.strictEqual(list[0], file);
  assert.strictEqual(list.item(1), null);
  const empty = createFileList([]);
  assert.strictEqual(empty.length, 0);
  assert.strictEqual(empty.item(0), null);
}

{
  const event = new ProgressEvent('progress', {
    lengthComputable: true,
    loaded: 3,
    total: 4,
    bubbles: true,
  });
  assert.strictEqual(event.type, 'progress');
  assert.strictEqual(event.lengthComputable, true);
  assert.strictEqual(event.loaded, 3);
  assert.strictEqual(event.total, 4);
  assert.strictEqual(event.bubbles, true);
  assert.strictEqual(event.cancelable, false);
  assert.throws(() => new ProgressEvent('load', { loaded: Infinity }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => ProgressEvent.prototype.loaded, {
    code: 'ERR_INVALID_THIS',
  });
}

if (common.hasCrypto) {
  const source = `
    const sync = new FileReaderSync();
    const blob = new Blob(['test']);
    const empty = new Blob();
    postMessage({
      hasReader: typeof FileReader === 'function',
      text: sync.readAsText(blob),
      empty: sync.readAsText(empty),
      binary: sync.readAsBinaryString(blob),
      url: sync.readAsDataURL(empty),
      bytes: sync.readAsArrayBuffer(blob).byteLength,
    });
  `;
  const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
  const worker = new Worker(url);
  URL.revokeObjectURL(url);
  worker.onerror = common.mustNotCall('worker failed');
  worker.onmessage = common.mustCall(({ data }) => {
    worker.terminate();
    assert.strictEqual(data.hasReader, true);
    assert.strictEqual(data.text, 'test');
    assert.strictEqual(data.empty, '');
    assert.strictEqual(data.binary, 'test');
    assert.strictEqual(data.url.indexOf('data:'), 0);
    assert.strictEqual(data.bytes, 4);
  });
}
