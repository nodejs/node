import '../common/index.mjs';
import assert from 'assert';
import { fork } from 'child_process';
import { once } from 'events';
import { fileURLToPath } from 'url';

if (process.argv[2] !== 'child') {
  const filename = fileURLToPath(import.meta.url);
  const cp = fork(filename, ['child']);
  const message = 'Hello World';
  cp.send(message);

  const [received] = await once(cp, 'message');
  assert.deepStrictEqual(received, message);

  cp.disconnect();
  await once(cp, 'exit');
} else {
  process.on('message', (msg) => process.send(msg));
}
