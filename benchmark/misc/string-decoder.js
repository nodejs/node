var common = require('../common.js');
var StringDecoder = require('string_decoder').StringDecoder;

var bench = common.createBenchmark(main, {
  encoding: ['ascii', 'utf8', 'base64-utf8', 'base64-ascii'],
  inlen: [32, 128, 1024],
  chunk: [16, 64, 256, 1024],
  n: [25e4]
});

var UTF_ALPHA = 'Blåbærsyltetøy';
var ASC_ALPHA = 'Blueberry jam';

function main(conf) {
  var encoding = conf.encoding;
  var inLen = conf.inlen | 0;
  var chunkLen = conf.chunk | 0;
  var n = conf.n | 0;

  var alpha;
  var chunks = [];
  var str = '';
  var isBase64 = (encoding === 'base64-ascii' || encoding === 'base64-utf8');

  if (encoding === 'ascii' || encoding === 'base64-ascii')
    alpha = ASC_ALPHA;
  else if (encoding === 'utf8' || encoding === 'base64-utf8')
    alpha = UTF_ALPHA;
  else
    throw new Error('Bad encoding');

  var sd = new StringDecoder(isBase64 ? 'base64' : encoding);

  for (var i = 0; i < inLen; ++i) {
    if (i > 0 && (i % chunkLen) === 0 && !isBase64) {
      chunks.push(new Buffer(str, encoding));
      str = '';
    }
    str += alpha[i % alpha.length];
  }
  if (str.length > 0 && !isBase64)
    chunks.push(new Buffer(str, encoding));
  if (isBase64) {
    str = new Buffer(str, 'utf8').toString('base64');
    while (str.length > 0) {
      var len = Math.min(chunkLen, str.length);
      chunks.push(new Buffer(str.substring(0, len), 'utf8'));
      str = str.substring(len);
    }
  }

  var nChunks = chunks.length;

  bench.start();
  for (var i = 0; i < n; ++i) {
    for (var j = 0; j < nChunks; ++j)
      sd.write(chunks[j]);
  }
  bench.end(n);
}
