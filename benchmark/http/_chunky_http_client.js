'use strict';

// test HTTP throughput in fragmented header case
var common = require('../common.js');
var net = require('net');
var test = require('../../test/common.js');

var bench = common.createBenchmark(main, {
  len:  [1, 4, 8, 16, 32, 64, 128],
  num:  [5, 50, 500, 2000],
  type: ['send'],
});


function main(conf) {
  var len = +conf.len;
  var num = +conf.num;
  var todo = [];
  var headers = [];
  // Chose 7 because 9 showed "Connection error" / "Connection closed"
  // An odd number could result in a better length dispersion.
  for (var i = 7; i <= 7 * 7 * 7; i *= 7)
    headers.push(Array(i + 1).join('o'));

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
      todo.push('X-Header-' + i + ': ' + headers[i % 3]);
    }
    todo.push('');
    todo.push('');
    todo = todo.join('\r\n');
    // Using odd numbers in many places may increase length coverage.
    var chunksize = 37;
    for (i = 0; i < todo.length; i += chunksize) {
      var cur = todo.slice(i, i + chunksize);
      channel.write(cur);
    }
  }

  var min = 10;
  var size = 0;
  var mod = 317;
  var mult = 17;
  var add = 11;
  var count = 0;
  var PIPE = test.PIPE;
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
        if (count === num) {
          bench.end(count);
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
