'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const fs = require('fs');
const writeFile = fs.writeFile;

const bench = common.createBenchmark(main, {
  dur: [5],
  file: [''],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 64 * 1024, 1024 * 1024]
});

function main(conf) {
  const dur = +conf.dur * 1000;
  const size = +conf.size;
  const file = (conf.file.length === 0 ? filename : conf.file);
  var writes = 0;
  var ended = false;
  var encoding;
  var data;

  try { fs.unlinkSync(file); } catch (e) {}
  process.on('exit', function() {
    try { fs.unlinkSync(file); } catch (e) {}
  });

  setTimeout(function() {
    bench.end(writes);
    ended = true;
  }, dur);

  switch (conf.type) {
    case 'buf':
      data = Buffer.alloc(size, 'b');
      (function() {
        function afterWrite(err) {
          if (err)
            throw err;
          if (ended)
            return;
          ++writes;
          writeFile(file, data, afterWrite);
        }
        bench.start();
        writeFile(file, data, afterWrite);
      })();
      return;
    case 'asc':
      data = new Array(size + 1).join('a');
      encoding = 'ascii';
      break;
    case 'utf':
      data = new Array(Math.ceil(size / 2) + 1).join('ü');
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  (function() {
    function afterWrite(err) {
      if (err)
        throw err;
      if (ended)
        return;
      ++writes;
      writeFile(file, data, encoding, afterWrite);
    }
    bench.start();
    writeFile(file, data, encoding, afterWrite);
  })();
}
