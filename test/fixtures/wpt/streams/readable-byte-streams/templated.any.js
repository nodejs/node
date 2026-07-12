// META: global=window,worker,shadowrealm
// META: script=../resources/test-utils.js
// META: script=../resources/rs-test-templates.js
'use strict';

templatedRSEmpty('ReadableStream with byte source (empty)', () => {
  return new ReadableStream({ type: 'bytes' });
});

templatedRSEmptyReader('ReadableStream with byte source (empty) default reader', () => {
  const stream = new ReadableStream({ type: 'bytes' });
  const reader = stream.getReader();
  return { stream, reader, read: () => reader.read() };
});

templatedRSEmptyReader('ReadableStream with byte source (empty) BYOB reader', () => {
  const stream = new ReadableStream({ type: 'bytes' });
  const reader = stream.getReader({ mode: 'byob' });
  return { stream, reader, read: () => reader.read(new Uint8Array([0])) };
});

templatedRSThrowAfterCloseOrError('ReadableStream with byte source', (extras) => {
  return new ReadableStream({ type: 'bytes', ...extras });
});
