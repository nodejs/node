'use strict';

const common = require('../common');
const {
  Writable,
  Readable,
  destroy
} = require('stream');
const assert = require('assert');
const http = require('http');

{
  const r = new Readable({ read() {} });
  destroy(r);
  assert.strictEqual(r.destroyed, true);
  r.on('error', common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
  }));
  r.on('close', common.mustCall());
}

{
  const r = new Readable({ read() {} });
  destroy(r, new Error('asd'));
  assert.strictEqual(r.destroyed, true);
  r.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'asd');
  }));
  r.on('close', common.mustCall());
}

{
  const w = new Writable({ write() {} });
  destroy(w);
  assert.strictEqual(w.destroyed, true);
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
  }));
  w.on('close', common.mustCall());
}

{
  const w = new Writable({ write() {} });
  destroy(w, new Error('asd'));
  assert.strictEqual(w.destroyed, true);
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'asd');
  }));
  w.on('close', common.mustCall());
}

{
  const server = http.createServer((req, res) => {
    destroy(req);
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.name, 'AbortError');
    }));
    req.on('close', common.mustCall(() => {
      res.end('hello');
    }));
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port,
      agent: new http.Agent()
    });

    req.write('asd');
    req.on('response', (res) => {
      const buf = [];
      res.on('data', (data) => buf.push(data));
      res.on('end', common.mustCall(() => {
        assert.deepStrictEqual(
          Buffer.concat(buf),
          Buffer.from('hello')
        );
        server.close();
      }));
    });
  });
}

{
  const server = http.createServer((req, res) => {
    req
      .resume()
      .on('end', () => {
        destroy(req);
      })
      .on('error', common.mustNotCall());

    req.on('close', common.mustCall(() => {
      res.end('hello');
    }));
  });

  server.listen(0, () => {
    const req = http.request({
      port: server.address().port,
      agent: new http.Agent()
    });

    req.write('asd');
    req.on('response', (res) => {
      const buf = [];
      res.on('data', (data) => buf.push(data));
      res.on('end', common.mustCall(() => {
        assert.deepStrictEqual(
          Buffer.concat(buf),
          Buffer.from('hello')
        );
        server.close();
      }));
    });
  });
}
