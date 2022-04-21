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

  for await (const _ of readable) {}

  channel.unsubscribe(subscriber);
}
