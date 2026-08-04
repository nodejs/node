// Flags: --experimental-vfs
'use strict';

// Buffer encoding for watch(): filename arrives as a Buffer.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const vfs = require('node:vfs');

(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/bf.txt', 'a');
  const watcher = myVfs.watch('/bf.txt', { interval: 25, encoding: 'buffer' });
  const changed = once(watcher, 'change');
  myVfs.writeFileSync('/bf.txt', 'longer-content-changed');
  const [eventType, filename] = await changed;
  assert.strictEqual(eventType, 'change');
  assert.deepStrictEqual(filename, Buffer.from('bf.txt'));
  watcher.close();
})().then(common.mustCall());
