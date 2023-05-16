'use strict';

const {
  hasCrypto,
  mustCall,
  mustNotCall,
  skip
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');

const {
  deepStrictEqual,
  strictEqual,
  throws
} = require('assert');
const {
  createSecureServer,
  createServer,
  connect
} = require('http2');
const Countdown = require('../common/countdown');

const { readKey } = require('../common/fixtures');

const key = readKey('agent8-key.pem', 'binary');
const cert = readKey('agent8-cert.pem', 'binary');
const ca = readKey('fake-startcom-root-cert.pem', 'binary');

{
  const server = createSecureServer({ key, cert });
  server.on('stream', mustCall((stream) => {
    stream.session.origin('https://example.org/a/b/c',
                          new URL('https://example.com'));
    stream.respond();
    stream.end('ok');
  }));
  server.on('session', mustCall((session) => {
    session.origin('https://foo.org/a/b/c', new URL('https://bar.org'));

    // Won't error, but won't send anything
    session.origin();

    [0, true, {}, []].forEach((input) => {
      throws(
        () => session.origin(input),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          name: 'TypeError'
        }
      );
    });

    [new URL('foo://bar'), 'foo://bar'].forEach((input) => {
      throws(
        () => session.origin(input),
        {
          code: 'ERR_HTTP2_INVALID_ORIGIN',
          name: 'TypeError'
        }
      );
    });

    ['not a valid url'].forEach((input) => {
      throws(
        () => session.origin(input),
        {
          code: 'ERR_INVALID_URL',
          name: 'TypeError'
        }
      );
    });
    const longInput = `http://foo.bar${'a'.repeat(16383)}`;
    throws(
      () => session.origin(longInput),
      {
        code: 'ERR_HTTP2_ORIGIN_LENGTH',
        name: 'TypeError'
      }
    );
  }));

  server.listen(0, mustCall(() => {
    const originSet = [`https://localhost:${server.address().port}`];
    const client = connect(originSet[0], { ca });
    const checks = [
      ['https://foo.org', 'https://bar.org'],
      ['https://example.org', 'https://example.com'],
    ];

    const countdown = new Countdown(3, () => {
      client.close();
      server.close();
    });

    client.on('origin', mustCall((origins) => {
      const check = checks.shift();
      originSet.push(...check);
      deepStrictEqual(client.originSet, originSet);
      deepStrictEqual(origins, check);
      countdown.dec();
    }, 2));

    client.request().on('close', mustCall(() => countdown.dec())).resume();
  }));
}

// Test automatically sending origin on connection start
{
  const origins = ['https://foo.org/a/b/c', 'https://bar.org'];
  const server = createSecureServer({ key, cert, origins });
  server.on('stream', mustCall((stream) => {
    stream.respond();
    stream.end('ok');
  }));

  server.listen(0, mustCall(() => {
    const check = ['https://foo.org', 'https://bar.org'];
    const originSet = [`https://localhost:${server.address().port}`];
    const client = connect(originSet[0], { ca });

    const countdown = new Countdown(2, () => {
      client.close();
      server.close();
    });

    client.on('origin', mustCall((origins) => {
      originSet.push(...check);
      deepStrictEqual(client.originSet, originSet);
      deepStrictEqual(origins, check);
      countdown.dec();
    }));

    client.request().on('close', mustCall(() => countdown.dec())).resume();
  }));
}

// If return status is 421, the request origin must be removed from the
// originSet
{
  const server = createSecureServer({ key, cert });
  server.on('stream', mustCall((stream) => {
    stream.respond({ ':status': 421 });
    stream.end();
  }));
  server.on('session', mustCall((session) => {
    session.origin('https://foo.org');
  }));

  server.listen(0, mustCall(() => {
    const origin = `https://localhost:${server.address().port}`;
    const client = connect(origin, { ca });

    client.on('origin', mustCall((origins) => {
      deepStrictEqual(client.originSet, [origin, 'https://foo.org']);
      const req = client.request({ ':authority': 'foo.org' });
      req.on('response', mustCall((headers) => {
        strictEqual(headers[':status'], 421);
        deepStrictEqual(client.originSet, [origin]);
      }));
      req.resume();
      req.on('close', mustCall(() => {
        client.close();
        server.close();
      }));
    }, 1));
  }));
}

// Origin is ignored on plain text HTTP/2 connections... server will still
// send them, but client will ignore them.
{
  const server = createServer();
  server.on('stream', mustCall((stream) => {
    stream.session.origin('https://example.org',
                          new URL('https://example.com'));
    stream.respond();
    stream.end('ok');
  }));
  server.listen(0, mustCall(() => {
    const client = connect(`http://localhost:${server.address().port}`);
    client.on('origin', mustNotCall());
    strictEqual(client.originSet, undefined);
    const req = client.request();
    req.resume();
    req.on('close', mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}
