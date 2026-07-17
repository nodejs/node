'use strict';

require('../common');
const http = require('http');
const net = require('net');
const url = require('url');
const assert = require('assert');
const Countdown = require('../common/countdown');

// Response splitting example, credit: Amit Klein, Safebreach
const str = '/welcome?lang=bar%c4%8d%c4%8aContent-Length:%200%c4%8d%c4%8a%c' +
            '4%8d%c4%8aHTTP/1.1%20200%20OK%c4%8d%c4%8aContent-Length:%202' +
            '0%c4%8d%c4%8aLast-Modified:%20Mon,%2027%20Oct%202003%2014:50:18' +
            '%20GMT%c4%8d%c4%8aContent-Type:%20text/html%c4%8d%c4%8a%c4%8' +
            'd%c4%8a%3chtml%3eGotcha!%3c/html%3e';

// Response splitting example, credit: Сковорода Никита Андреевич (@ChALkeR)
const x = 'fooഊSet-Cookie: foo=barഊഊ<script>alert("Hi!")</script>';
const y = 'foo⠊Set-Cookie: foo=bar';

let count = 0;
const countdown = new Countdown(3, () => server.close());

function test(res, code, key, value) {
  const header = { [key]: value };
  assert.throws(
    () => res.writeHead(code, header),
    {
      code: 'ERR_INVALID_CHAR',
      name: 'TypeError',
      message: `Invalid character in header content ["${key}"]`
    }
  );
}

const server = http.createServer((req, res) => {
  switch (count++) {
    case 0: {
      const loc = url.parse(req.url, true).query.lang;
      test(res, 302, 'Location', `/foo?lang=${loc}`);
      break;
    }
    case 1:
      test(res, 200, 'foo', x);
      break;
    case 2:
      test(res, 200, 'foo', y);
      break;
    default:
      assert.fail('should not get to here.');
  }
  countdown.dec();
  res.end('ok');
});
server.listen(0, () => {
  const end = 'HTTP/1.1\r\nHost: example.com\r\n\r\n';
  const client = net.connect({ port: server.address().port }, () => {
    client.write(`GET ${str} ${end}`);
    client.write(`GET / ${end}`);
    client.write(`GET / ${end}`);
    client.end();
  });
});
