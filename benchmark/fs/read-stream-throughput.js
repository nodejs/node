// test the throughput of the fs.WriteStream class.
'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(process.env.NODE_TMPDIR || __dirname,
                              `.removeme-benchmark-garbage-${process.pid}`);
const fs = require('fs');
const assert = require('assert');

let encodingType, encoding, size, filesize;

const bench = common.createBenchmark(main, {
  encodingType: ['buf', 'asc', 'utf'],
  filesize: [1000 * 1024 * 1024],
  size: [1024, 4096, 65535, 1024 * 1024]
});

function main(conf) {
  encodingType = conf.encodingType;
  size = conf.size;
  filesize = conf.filesize;

  switch (encodingType) {
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
      throw new Error(`invalid encodingType: ${encodingType}`);
  }

  makeFile();
}

function runTest() {
  assert(fs.statSync(filename).size === filesize);
  const rs = fs.createReadStream(filename, {
    highWaterMark: size,
    encoding: encoding
  });

  rs.on('open', () => {
    bench.start();
  });

  var bytes = 0;
  rs.on('data', (chunk) => {
    bytes += chunk.length;
  });

  rs.on('end', () => {
    try { fs.unlinkSync(filename); } catch {}
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

  try { fs.unlinkSync(filename); } catch {}
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
