import { test } from 'node:test';
import { once } from 'node:events';
import { connect } from 'node:net';

test('slow', async () => {
  // The host closes this connection after receiving fast-fail's bypassed
  // test:complete event, so this test cannot finish before that event arrives.
  const socket = connect(Number(process.argv[2]), '127.0.0.1');
  socket.resume();
  await once(socket, 'end');
});
