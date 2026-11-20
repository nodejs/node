// Flags: --expose-gc
'use strict';

const common = require('../common');
const { gcUntil } = require('../common/gc');
const { Writable } = require('node:stream');
const { setImmediate: nextTurn } = require('node:timers/promises');

async function checkIdleWriter() {
  const sink = new Writable({
    highWaterMark: 64 * 1024,
    write(chunk, encoding, done) { setImmediate(done); },
    writev(entries, done) { setImmediate(done); },
  });
  const writer = Writable.toWeb(sink).getWriter();
  const state = sink._writableState;
  const original = state.onwrite;
  state.onwrite = (error) => original(error);
  const replaced = new WeakRef(state.onwrite);
  const chunk = Buffer.alloc(1024);
  for (let i = 0; i < 129; i++) await writer.write(chunk);
  while (sink.writableLength > 0) await nextTurn();
  state.onwrite = original;

  await gcUntil('release replaced onwrite', () => replaced.deref() === undefined);
  await writer.close();
}

checkIdleWriter().then(common.mustCall());
