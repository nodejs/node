// test the throughput of the fs.WriteStream class.
'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const fs = require('fs');
const filesize = 1000 * 1024 * 1024;
const assert = require('assert');

var type, encoding, size;

const bench = common.createBenchmark(main, {
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
  const rs = fs.createReadStream(filename, {
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
  const buf = Buffer.allocUnsafe(filesize / 1024);
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
  const ws = fs.createWriteStream(filename);
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
