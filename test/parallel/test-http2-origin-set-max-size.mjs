import {
  expectsError,
  hasCrypto,
  mustCall,
  mustNotCall,
  mustSucceed,
  skip,
} from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto)
  skip('missing crypto');

const {
  createSecureServer,
  connect,
} = await import('node:http2');

const key = fixtures.readKey('agent8-key.pem', 'binary');
const cert = fixtures.readKey('agent8-cert.pem', 'binary');
const ca = fixtures.readKey('fake-startcom-root-cert.pem', 'binary');

const server = createSecureServer({ key, cert });
server.on('session', (session) => {
  let i = 0;
  const timer = setInterval(() => {
    try {
      session.origin(...Array.from({ length: 10 }, () => `https://o${i++}.example.com`));
    } catch {
      clearInterval(timer);
    }
  }, 10);

  session.on('close', () => {
    clearInterval(timer);
  });
});

await new Promise((resolve) => server.listen(0, resolve));

// Test fantasist values
[Symbol(), '0', 1n, {}, [], true, false, /s/, () => {}].forEach((maxOriginSetSize) => {
  assert.throws(
    () => connect(`https://localhost:${server.address().port}`, { ca, maxOriginSetSize }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});
[NaN, -1].forEach((maxOriginSetSize) => {
  assert.throws(
    () => connect(`https://localhost:${server.address().port}`, { ca, maxOriginSetSize }),
    { code: 'ERR_OUT_OF_RANGE' },
  );
});

await new Promise((resolve) => server.getConnections(mustSucceed((count) => {
  assert.strictEqual(count, 0);
  resolve();
})));

// Test default value
await new Promise((resolve) => {
  const client = connect(`https://localhost:${server.address().port}`, { ca });

  client.on('origin', mustCall(12)); // Default value is 128, the first 12 frames should pass, the 13th one should error
  client.on('error', expectsError({
    code: 'ERR_HTTP2_TOO_MANY_ORIGINS',
  }));
  client.on('goaway', mustNotCall());
  client.on('close', resolve);
});

// Test non-default values
await Promise.all([-0, 9, 1.5].map((maxOriginSetSize) => new Promise((resolve) => {
  const client = connect(`https://localhost:${server.address().port}`, { ca, maxOriginSetSize });

  client.on('origin', mustNotCall()); // The server send 10 origins on the first frame, that's already too many.
  client.on('error', expectsError({
    code: 'ERR_HTTP2_TOO_MANY_ORIGINS',
  }));
  client.on('goaway', mustNotCall());
  client.on('close', resolve);
})));


// Test values higher than the default value
await Promise.all([512, Infinity].map((maxOriginSetSize) => new Promise((resolve) => {
  const client = connect(`https://localhost:${server.address().port}`, { ca, maxOriginSetSize });

  client.on('origin', mustCall(() => {
    if (client.originSet.length > 128) {
      client.destroy();
    }
  }, 13));
  client.on('error', mustNotCall());
  client.on('goaway', mustNotCall());
  client.on('close', resolve);
})));

server.close();
