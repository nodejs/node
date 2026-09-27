// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../../common');
const assert = require('node:assert');

if (process.argv[2] === 'child') {
  const binding = require(`./build/${common.buildType}/test_typedarray`);
  const mutable = Uint8Array.from([65, 66, 67, 68]);
  binding.Detach(mutable);
  assert.strictEqual(binding.IsDetached(mutable.buffer), true);

  const buffer = Uint8Array.from([65, 66, 67, 68]).buffer.transferToImmutable();
  const view = new Uint8Array(buffer);

  // napi_detach_arraybuffer() must return an error or propagate an exception
  // when detaching an immutable ArrayBuffer, without aborting the process.
  assert.throws(() => binding.Detach(view), Error);
  assert.strictEqual(binding.IsDetached(buffer), false);
  assert.strictEqual(buffer.immutable, true);
  assert.deepStrictEqual([...view], [65, 66, 67, 68]);
} else {
  const { spawnSyncAndExitWithoutError } = require('../../common/child_process');
  const [command, options] = common.escapePOSIXShell`"${process.execPath}" --js-immutable-arraybuffer "${__filename}" child`;
  // Avoid core files while this known issue still aborts the child.
  const childCommand = common.isWindows ? command : `ulimit -c 0 && exec ${command}`;
  spawnSyncAndExitWithoutError(childCommand, { ...options, shell: true });
}
