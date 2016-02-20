'use strict';
var common = require('../common');

var bench = common.createBenchmark(main, {
  encoding: ['utf8', 'base64'],
  len: [1, 2, 4, 16, 64, 256], // x16
  n: [5e6]
});

// 16 chars each
var chars = [
  'hello brendan!!!', // 1 byte
  'ΰαβγδεζηθικλμνξο', // 2 bytes
  '挰挱挲挳挴挵挶挷挸挹挺挻挼挽挾挿', // 3 bytes
  '𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕𠴕𠵼𠵿𠸎𠸏𠹷𠺝𠺢' // 4 bytes
];

function main(conf) {
  var n = conf.n | 0;
  var len = conf.len | 0;
  var encoding = conf.encoding;

  var strings = [];
  for (var string of chars) {
    // Strings must be built differently, depending on encoding
    var data = buildString(string, len);
    if (encoding === 'utf8') {
      strings.push(data);
    } else if (encoding === 'base64') {
      // Base64 strings will be much longer than their UTF8 counterparts
      strings.push(new Buffer(data, 'utf8').toString('base64'));
    }
  }

  // Check the result to ensure it is *properly* optimized
  var results = strings.map(function(val) {
    return Buffer.byteLength(val, encoding);
  });

  bench.start();
  for (var i = 0; i < n; i++) {
    var index = n % strings.length;
    // Go!
    var r = Buffer.byteLength(strings[index], encoding);

    if (r !== results[index])
      throw new Error('incorrect return value');
  }
  bench.end(n);
}

function buildString(str, times) {
  if (times == 1) return str;

  return str + buildString(str, times - 1);
}
