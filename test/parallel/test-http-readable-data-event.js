'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const helloWorld = 'Hello World!';
const helloAgainLater = 'Hello again later!';

let next = null;

const server = http.createServer((req, res) => {
  res.writeHead(200, {
    'Content-Length': `${(helloWorld.length + helloAgainLater.length)}`
  });

  // We need to make sure the data is flushed
  // before writing again
  next = () => {
    res.end(helloAgainLater);
    next = () => { };
  };

  res.write(helloWorld);
}).listen(0, function() {
  const opts = {
    hostname: 'localhost',
    port: server.address().port,
    path: '/'
  };

  const expectedData = [helloWorld, helloAgainLater];
  const expectedRead = [helloWorld, null, helloAgainLater, null, null];

  const req = http.request(opts, (res) => {
    res.on('error', common.mustNotCall());

    res.on('readable', common.mustCall(() => {
      let data;

      do {
        data = res.read();
        assert.strictEqual(data, expectedRead.shift());
        next();
      } while (data !== null);
    }, 3));

    res.setEncoding('utf8');
    res.on('data', common.mustCall((data) => {
      assert.strictEqual(data, expectedData.shift());
    }, 2));

    res.on('end', common.mustCall(() => {
      server.close();
    }));
  });

  req.end();
});
