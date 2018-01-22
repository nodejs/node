'use strict';
const common = require('../common.js');
const StringDecoder = require('string_decoder').StringDecoder;

const bench = common.createBenchmark(main, {
  encoding: ['ascii', 'utf8', 'base64-utf8', 'base64-ascii', 'utf16le'],
  inLen: [32, 128, 1024, 4096],
  chunkLen: [16, 64, 256, 1024],
  n: [25e5]
});

const UTF8_ALPHA = 'Blåbærsyltetøy';
const ASC_ALPHA = 'Blueberry jam';
const UTF16_BUF = Buffer.from('Blåbærsyltetøy', 'utf16le');

function main({ encoding, inLen, chunkLen, n }) {
  var alpha;
  var buf;
  const chunks = [];
  var str = '';
  const isBase64 = (encoding === 'base64-ascii' || encoding === 'base64-utf8');
  var i;

  if (encoding === 'ascii' || encoding === 'base64-ascii')
    alpha = ASC_ALPHA;
  else if (encoding === 'utf8' || encoding === 'base64-utf8')
    alpha = UTF8_ALPHA;
  else if (encoding === 'utf16le') {
    buf = UTF16_BUF;
    str = Buffer.alloc(0);
  } else
    throw new Error('Bad encoding');

  const sd = new StringDecoder(isBase64 ? 'base64' : encoding);

  for (i = 0; i < inLen; ++i) {
    if (i > 0 && (i % chunkLen) === 0 && !isBase64) {
      if (alpha) {
        chunks.push(Buffer.from(str, encoding));
        str = '';
      } else {
        chunks.push(str);
        str = Buffer.alloc(0);
      }
    }
    if (alpha)
      str += alpha[i % alpha.length];
    else {
      var start = i;
      var end = i + 2;
      if (i % 2 !== 0) {
        ++start;
        ++end;
      }
      str = Buffer.concat([
        str,
        buf.slice(start % buf.length, end % buf.length)
      ]);
    }
  }

  if (!alpha) {
    if (str.length > 0)
      chunks.push(str);
  } else if (str.length > 0 && !isBase64)
    chunks.push(Buffer.from(str, encoding));

  if (isBase64) {
    str = Buffer.from(str, 'utf8').toString('base64');
    while (str.length > 0) {
      const len = Math.min(chunkLen, str.length);
      chunks.push(Buffer.from(str.substring(0, len), 'utf8'));
      str = str.substring(len);
    }
  }

  const nChunks = chunks.length;

  bench.start();
  for (i = 0; i < n; ++i) {
    for (var j = 0; j < nChunks; ++j)
      sd.write(chunks[j]);
  }
  bench.end(n);
}
