// Flags: --expose-internals
import * as common from '../common/index.mjs';
import assert from 'assert';

import util from 'internal/webstreams/util';

import { Readable } from 'stream';

import * as dc from 'diagnostics_channel';

{
  const readable = Readable.toWeb(Readable.from([1]));

  const channel = dc.channel('stream.web.done');
  const subscriber = common.mustCall(({ stream }) => {
    assert.strictEqual(readable, stream);
    assert.strictEqual(readable[util.kState].state, 'closed');
  });
  channel.subscribe(subscriber);

  const reader = readable.getReader();
  let result;

  while (!result?.done) {
    result = await reader.read();
  }

  channel.unsubscribe(subscriber);
}

{
  const readable = Readable.toWeb(Readable.from([1]));

  const channel = dc.channel('stream.web.done');
  const subscriber = common.mustCall(({ stream }) => {
    assert.strictEqual(readable, stream);
    assert.strictEqual(readable[util.kState].state, 'closed');
  });
  channel.subscribe(subscriber);

  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) { /* drain */ }

  channel.unsubscribe(subscriber);
}

{
  const readable = new ReadableStream({
    type: 'bytes',
    start(controller) {
      controller.enqueue(new Uint8Array([1, 2, 3]));
      controller.close();
    },
  });

  const channel = dc.channel('stream.web.done');
  const subscriber = common.mustCall(({ stream }) => {
    assert.strictEqual(readable, stream);
    assert.strictEqual(readable[util.kState].state, 'closed');
  });
  channel.subscribe(subscriber);

  const reader = readable.getReader({ mode: 'byob' });
  let result;
  while (!result?.done) {
    result = await reader.read(new Uint8Array(8));
  }

  channel.unsubscribe(subscriber);
}
