'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.Server(common.mustCall(function(req, res) {
  const time = req.url[req.url.length - 1];
  switch (time) {
    case '0': res.redirect('https://google.com/', 307, 'foo'); break;
    case '1': res.redirect('https://alipay.com/', 301); break;
    case '2': res.redirect('https://nodejs.org/'); break;
    case '3': res.redirect('https://foo.bar/', common.mustCall(function() {}));
  }
}, 4));

const finished = [];
server.listen(0, function() {
  for (let i = 0; i < 4; i++) {
    const time = i.toString();
    http.get({ port: this.address().port, path: `/?time=${time}` }, function(res) {
      finished.push(res);

      let data = '';

      res.on('data', function(chunk) {
        data += chunk;
      });

      res.on('end', () => {
        switch (time) {
          case '0':
            assert.strictEqual(data, 'foo');
            assert.strictEqual(res.statusCode, 307);
            assert.strictEqual(res.headers.location, 'https://google.com/');
            break;

          case '1':
            assert.strictEqual(data, '');
            assert.strictEqual(res.statusCode, 301);
            assert.strictEqual(res.headers.location, 'https://alipay.com/');
            break;

          case '2':
            assert.strictEqual(data, '');
            assert.strictEqual(res.statusCode, 302);
            assert.strictEqual(res.headers.location, 'https://nodejs.org/');
            break;

          case '3':
            assert.strictEqual(data, '');
            assert.strictEqual(res.statusCode, 302);
            assert.strictEqual(res.headers.location, 'https://foo.bar/');
            break;

          default: break;
        }
      });

      if (finished.length >= 4) server.close();
    });
  }
});
