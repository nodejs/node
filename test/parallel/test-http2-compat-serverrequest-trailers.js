'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerRequest should have getter for trailers & rawTrailers

const expectedTrailers = {
  'x-foo': 'xOxOxOx, OxOxOxO, xOxOxOx, OxOxOxO',
  'x-foo-test': 'test, test'
};

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    let data = '';
    request.setEncoding('utf8');
    request.on('data', common.mustCallAtLeast((chunk) => data += chunk));
    request.on('end', common.mustCall(() => {
      const trailers = request.trailers;
      for (const [name, value] of Object.entries(expectedTrailers)) {
        assert.strictEqual(trailers[name], value);
      }
      assert.deepStrictEqual([
        'x-foo',
        'xOxOxOx',
        'x-foo',
        'OxOxOxO',
        'x-foo',
        'xOxOxOx',
        'x-foo',
        'OxOxOxO',
        'x-foo-test',
        'test, test'
      ], request.rawTrailers);
      assert.strictEqual(data, 'test\ntest');
      response.end();
    }));
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'POST',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers, {
      getTrailers(trailers) {
        trailers['x-fOo'] = 'xOxOxOx';
        trailers['x-foO'] = 'OxOxOxO';
        trailers['X-fOo'] = 'xOxOxOx';
        trailers['X-foO'] = 'OxOxOxO';
        trailers['x-foo-test'] = 'test, test';
      }
    });
    request.resume();
    request.on('end', common.mustCall(function() {
      server.close();
      client.close();
    }));
    request.write('test\n');
    request.end('test');
  }));
}));
