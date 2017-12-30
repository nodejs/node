'use strict';

// test HTTP throughput in fragmented header case
const common = require('../common.js');
const net = require('net');

const bench = common.createBenchmark(main, {
  len:  [1, 4, 8, 16, 32, 64, 128],
  n:  [5, 50, 500, 2000],
  type: ['send'],
});


function main({ len, n }) {
  var todo = [];
  const headers = [];
  // Chose 7 because 9 showed "Connection error" / "Connection closed"
  // An odd number could result in a better length dispersion.
  for (var i = 7; i <= 7 * 7 * 7; i *= 7)
    headers.push('o'.repeat(i));

  function WriteHTTPHeaders(channel, has_keep_alive, extra_header_count) {
    todo = [];
    todo.push('GET / HTTP/1.1');
    todo.push('Host: localhost');
    todo.push('Connection: keep-alive');
    todo.push('Accept: text/html,application/xhtml+xml,' +
              'application/xml;q=0.9,image/webp,*/*;q=0.8');
    todo.push('User-Agent: Mozilla/5.0 (X11; Linux x86_64) ' +
              'AppleWebKit/537.36 (KHTML, like Gecko) ' +
              'Chrome/39.0.2171.71 Safari/537.36');
    todo.push('Accept-Encoding: gzip, deflate, sdch');
    todo.push('Accept-Language: en-US,en;q=0.8');
    for (var i = 0; i < extra_header_count; i++) {
      // Utilize first three powers of a small integer for an odd cycle and
      // because the fourth power of some integers overloads the server.
      todo.push(`X-Header-${i}: ${headers[i % 3]}`);
    }
    todo.push('');
    todo.push('');
    todo = todo.join('\r\n');
    // Using odd numbers in many places may increase length coverage.
    const chunksize = 37;
    for (i = 0; i < todo.length; i += chunksize) {
      const cur = todo.slice(i, i + chunksize);
      channel.write(cur);
    }
  }

  const min = 10;
  var size = 0;
  const mod = 317;
  const mult = 17;
  const add = 11;
  var count = 0;
  const PIPE = process.env.PIPE_NAME;
  var socket = net.connect(PIPE, function() {
    bench.start();
    WriteHTTPHeaders(socket, 1, len);
    socket.setEncoding('utf8');
    socket.on('data', function(d) {
      var did = false;
      var pattern = 'HTTP/1.1 200 OK\r\n';
      if ((d.length === pattern.length && d === pattern) ||
          (d.length > pattern.length &&
           d.slice(0, pattern.length) === pattern)) {
        did = true;
      } else {
        pattern = 'HTTP/1.1 ';
        if ((d.length === pattern.length && d === pattern) ||
            (d.length > pattern.length &&
             d.slice(0, pattern.length) === pattern)) {
          did = true;
        }
      }
      size = (size * mult + add) % mod;
      if (did) {
        count += 1;
        if (count === n) {
          bench.end(count);
          process.exit(0);
        } else {
          WriteHTTPHeaders(socket, 1, min + size);
        }
      }
    });
    socket.on('close', function() {
      console.log('Connection closed');
    });

    socket.on('error', function() {
      throw new Error('Connection error');
    });
  });
}
