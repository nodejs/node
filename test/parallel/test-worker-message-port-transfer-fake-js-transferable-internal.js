'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs').promises;
const { MessageChannel } = require('worker_threads');
const { once } = require('events');

// Test that overriding the internal kTransfer method of a JSTransferable does
// not enable loading arbitrary code from internal Node.js core modules.

(async function() {
  const fh = await fs.open(__filename);
  assert.strictEqual(fh.constructor.name, 'FileHandle');

  const kTransfer = Object.getOwnPropertySymbols(Object.getPrototypeOf(fh))
    .find((symbol) => symbol.description === 'messaging_transfer_symbol');
  assert.strictEqual(typeof kTransfer, 'symbol');
  // Use a module:export pair that is not a registered transferable. net.Socket
  // and net.Server are real transferables now, so they would be constructed (and
  // then safely reject the bogus data); a nonexistent export keeps exercising
  // the "Unknown deserialize spec" allowlist rejection.
  fh[kTransfer] = () => {
    return {
      data: '✨',
      deserializeInfo: 'net:NotARealTransferable'
    };
  };

  const { port1, port2 } = new MessageChannel();
  port1.postMessage(fh, [ fh ]);
  port2.on('message', common.mustNotCall());

  const [ exception ] = await once(port2, 'messageerror');

  assert.strictEqual(exception.message,
                     'Unknown deserialize spec net:NotARealTransferable');
  port2.close();
})().then(common.mustCall());
