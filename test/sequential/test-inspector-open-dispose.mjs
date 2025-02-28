import * as common from '../common/index.mjs';
import assert from 'node:assert';
import net from 'node:net';
import url from 'node:url';
import { fork } from 'node:child_process';

common.skipIfInspectorDisabled();
if (process.env.BE_CHILD) {
  await beChild();
} else {
  let firstPort;

  const filename = url.fileURLToPath(import.meta.url);
  const child = fork(filename, { env: { ...process.env, BE_CHILD: 1 } });

  child.once('message', common.mustCall((msg) => {
    assert.strictEqual(msg.cmd, 'started');
    assert.strictEqual(msg.url, undefined);

    child.send({ cmd: 'open' });
    child.once('message', common.mustCall(wasOpenedHandler));
  }));

  function wasOpenedHandler(msg) {
    assert.strictEqual(msg.cmd, 'url');
    const { port } = url.parse(msg.url);
    ping(port, common.mustSucceed(() => {
      child.send({ cmd: 'dispose' });
      child.once('message', common.mustCall(wasDisposedWhenOpenHandler));
      firstPort = port;
    }));
  }

  function wasDisposedWhenOpenHandler(msg) {
    assert.strictEqual(msg.cmd, 'url');
    assert.strictEqual(msg.url, undefined);
    ping(firstPort, (err) => {
      assert(err, 'expected ping to inspector port to fail');
      child.send({ cmd: 'dispose' });
      child.once('message', common.mustCall(wasReDisposedHandler));
    });
  }

  function wasReDisposedHandler(msg) {
    assert.strictEqual(msg.cmd, 'url');
    assert.strictEqual(msg.url, undefined);
    process.exit();
  }
}

function ping(port, callback) {
  net.connect({ port, family: 4 })
    .on('connect', function() { close(this); })
    .on('error', function(err) { close(this, err); });

  function close(self, err) {
    self.end();
    self.on('close', () => callback(err));
  }
}

async function beChild() {
  const inspector = await import('node:inspector');
  let inspectorDisposer;
  process.send({ cmd: 'started' });

  process.on('message', (msg) => {
    if (msg.cmd === 'open') {
      inspectorDisposer = inspector.open(0, false, undefined);
    }
    if (msg.cmd === 'dispose') {
      inspectorDisposer[Symbol.dispose]();
    }
    process.send({ cmd: 'url', url: inspector.url() });
  });
}
