'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const { NGHTTP2_CANCEL } = http2.constants;

function getAbortController(req) {
  const sym = Object.getOwnPropertySymbols(req)
    .find((s) => s.description === 'kAbortController');
  return req[sym];
}

function withServer(onRequest, onListen) {
  const server = http2.createServer(onRequest);
  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    onListen(server, client);
  }));
}

// The signal is created lazily, the same object is returned on every access,
// and it is not aborted when the request and response complete normally.
{
  const body = 'hello world';
  withServer(common.mustCall((req, res) => {
    assert.strictEqual(getAbortController(req), null);
    const { signal } = req;
    assert.ok(signal instanceof AbortSignal);
    assert.strictEqual(req.signal, signal);
    assert.strictEqual(signal.aborted, false);
    signal.onabort = common.mustNotCall();

    let data = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => data += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(data, body);
      assert.strictEqual(signal.aborted, false);
      res.end('ok');
    }));
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.aborted, false);
      assert.strictEqual(signal.aborted, false);
    }));
  }), common.mustCall((server, client) => {
    const req = client.request({ ':method': 'POST' });
    req.resume();
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end(body);
  }));
}

// A signal first accessed after the stream completed normally is not aborted.
{
  withServer(common.mustCall((req, res) => {
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, false);
    }));
    res.end();
  }), common.mustCall((server, client) => {
    const req = client.request();
    req.resume();
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// The signal is not aborted when the stream is reset after the response was
// sent completely, even though the request body is incomplete. In HTTP/1 the
// signal is likewise detached once the response finishes.
{
  withServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustNotCall();
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, false);
    }));
    res.end();
  }), common.mustCall((server, client) => {
    const req = client.request({ ':method': 'POST' });
    req.write('partial');
    req.resume();
    req.on('response', common.mustCall());
    req.on('end', common.mustCall(() => {
      req.close(NGHTTP2_CANCEL);
    }));
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// Resetting one stream aborts only that stream's signal, not the signal of
// another stream on the same session.
{
  const requests = [];
  withServer(common.mustCall((req, res) => {
    requests.push({ req, res });
    if (requests.length < 2)
      return;

    const [cancelled, other] = requests;
    other.req.signal.onabort = common.mustNotCall();
    cancelled.req.signal.onabort = common.mustCall(() => {
      assert.strictEqual(other.req.signal.aborted, false);
      other.res.end('ok');
    });
    cancelled.req.on('close', common.mustCall(() => {
      assert.strictEqual(cancelled.req.aborted, true);
      assert.strictEqual(cancelled.req.signal.aborted, true);
    }));
    cancelled.res.on('close', common.mustCall());
    cancelled.req.stream.session.emit('both-received');
  }, 2), common.mustCall((server, client) => {
    const cancelled = client.request({ ':path': '/cancelled' });
    cancelled.on('error', common.mustNotCall());
    cancelled.on('close', common.mustCall(() => {
      assert.strictEqual(cancelled.rstCode, NGHTTP2_CANCEL);
    }));

    const other = client.request({ ':path': '/other' });
    other.resume();
    other.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
    }));
    other.on('close', common.mustCall(() => {
      assert.strictEqual(other.rstCode, 0);
      client.close();
      server.close();
    }));

    server.once('session', common.mustCall((session) => {
      session.once('both-received', common.mustCall(() => {
        cancelled.close(NGHTTP2_CANCEL);
      }));
    }));
  }));
}

// An 'aborted' listener observes an aborted signal, whether the signal was
// accessed before the stream was reset or is first accessed in the listener.
{
  let pending = 2;
  withServer(common.mustCall((req, res) => {
    if (req.url === '/early')
      assert.strictEqual(req.signal.aborted, false);
    req.on('aborted', common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, true);
    }));
    req.stream.session.emit('received', req.url);
  }, 2), common.mustCall((server, client) => {
    const requests = {
      '/early': client.request({ ':path': '/early' }),
      '/late': client.request({ ':path': '/late' }),
    };
    for (const req of Object.values(requests)) {
      req.on('close', common.mustCall(() => {
        if (--pending === 0) {
          client.close();
          server.close();
        }
      }));
    }
    server.once('session', common.mustCall((session) => {
      session.on('received', common.mustCall((url) => {
        requests[url].close(NGHTTP2_CANCEL);
      }, 2));
    }));
  }));
}

// The signal is not aborted when the response completes with trailers, or
// when the headers are flushed before the response is ended.
{
  withServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustNotCall();
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, false);
    }));
    if (req.url === '/trailers') {
      res.addTrailers({ 'x-trailer': 'value' });
      res.end('body');
    } else {
      res.flushHeaders();
      res.end('body');
    }
  }, 2), common.mustCall((server, client) => {
    const withTrailers = client.request({ ':path': '/trailers' });
    withTrailers.resume();
    withTrailers.on('trailers', common.mustCall((trailers) => {
      assert.strictEqual(trailers['x-trailer'], 'value');
    }));
    withTrailers.on('close', common.mustCall(() => {
      const flushed = client.request({ ':path': '/flushed' });
      flushed.resume();
      flushed.on('close', common.mustCall(() => {
        client.close();
        server.close();
      }));
    }));
  }));
}

// The signal is aborted when the stream is destroyed after the writable side
// finished but before the frame that ends the stream was sent: with a body
// and trailers, with trailers only, and with neither.
{
  const cases = {
    '/body-and-trailers'(res, cb) {
      res.addTrailers({ 'x-trailer': 'value' });
      res.end('body', cb);
    },
    '/trailers-only'(res, cb) {
      res.flushHeaders();
      res.addTrailers({ 'x-trailer': 'value' });
      res.end(cb);
    },
    '/empty'(res, cb) {
      res.flushHeaders();
      res.end(cb);
    },
  };
  const paths = Object.keys(cases);
  withServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustCall();
    cases[req.url](res, common.mustCall(() => {
      assert.strictEqual(res.stream.writableFinished, true);
      assert.strictEqual(req.signal.aborted, false);
      res.destroy();
    }));
  }, paths.length), common.mustCall((server, client) => {
    let pending = paths.length;
    for (const path of paths) {
      const req = client.request({ ':path': path });
      req.resume();
      req.on('trailers', common.mustNotCall());
      req.on('error', () => {});
      req.on('close', common.mustCall(() => {
        if (--pending === 0) {
          client.close();
          server.close();
        }
      }));
    }
  }));
}

// A signal first accessed after the stream was reset is already aborted.
{
  withServer(common.mustCall((req, res) => {
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, true);
    }));
    req.stream.session.emit('received');
  }), common.mustCall((server, client) => {
    const req = client.request();
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
    server.once('session', common.mustCall((session) => {
      session.once('received', common.mustCall(() => {
        req.close(NGHTTP2_CANCEL);
      }));
    }));
  }));
}

// Destroying the response before it completes aborts the signal.
{
  withServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustCall();
    res.destroy();
  }), common.mustCall((server, client) => {
    const req = client.request();
    req.on('error', () => {});
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// A HEAD request reset before the response is sent aborts the signal.
{
  withServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustCall();
    req.stream.session.emit('received');
  }), common.mustCall((server, client) => {
    const req = client.request({ ':method': 'HEAD' });
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
    server.once('session', common.mustCall((session) => {
      session.once('received', common.mustCall(() => {
        req.close(NGHTTP2_CANCEL);
      }));
    }));
  }));
}

// When the connection is lost, the signals of all pending streams on it are
// aborted.
{
  const aborted = [];
  withServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustCall(() => {
      aborted.push(req.url);
      if (aborted.length === 2)
        req.stream.session.emit('all-aborted');
    });
    if (req.url === '/second')
      req.stream.session.emit('both-received');
  }, 2), common.mustCall((server, client) => {
    client.request({ ':path': '/first' }).on('error', () => {});
    client.request({ ':path': '/second' }).on('error', () => {});
    server.once('session', common.mustCall((session) => {
      session.once('both-received', common.mustCall(() => {
        client.destroy();
      }));
      session.once('all-aborted', common.mustCall(() => {
        assert.deepStrictEqual(aborted.sort(), ['/first', '/second']);
        server.close();
      }));
    }));
  }));
}
