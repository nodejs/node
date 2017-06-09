// test the throughput of the fs.WriteStream class.
'use strict';

var path = require('path');
var common = require('../common.js');
var filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
var fs = require('fs');
var filesize = 1000 * 1024 * 1024;
var assert = require('assert');

var type, encoding, size;

var bench = common.createBenchmark(main, {
  type: ['buf', 'asc', 'utf'],
  size: [1024, 4096, 65535, 1024 * 1024]
});

function main(conf) {
  type = conf.type;
  size = +conf.size;

  switch (type) {
    case 'buf':
      encoding = null;
      break;
    case 'asc':
      encoding = 'ascii';
      break;
    case 'utf':
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  makeFile(runTest);
}

function runTest() {
  assert(fs.statSync(filename).size === filesize);
  var rs = fs.createReadStream(filename, {
    highWaterMark: size,
    encoding: encoding
  });

  rs.on('open', function() {
    bench.start();
  });

  var bytes = 0;
  rs.on('data', function(chunk) {
    bytes += chunk.length;
  });

  rs.on('end', function() {
    try { fs.unlinkSync(filename); } catch (e) {}
    // MB/sec
    bench.end(bytes / (1024 * 1024));
  });
}

function makeFile() {
  var buf = Buffer.allocUnsafe(filesize / 1024);
  if (encoding === 'utf8') {
    // ü
    for (var i = 0; i < buf.length; i++) {
      buf[i] = i % 2 === 0 ? 0xC3 : 0xBC;
    }
  } else if (encoding === 'ascii') {
    buf.fill('a');
  } else {
    buf.fill('x');
  }

  try { fs.unlinkSync(filename); } catch (e) {}
  var w = 1024;
  var ws = fs.createWriteStream(filename);
  ws.on('close', runTest);
  ws.on('drain', write);
  write();
  function write() {
    do {
      w--;
    } while (false !== ws.write(buf) && w > 0);
    if (w === 0)
      ws.end();
  }
}
