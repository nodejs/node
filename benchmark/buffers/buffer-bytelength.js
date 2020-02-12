'use strict';
const common = require('../common');

const bench = common.createBenchmark(main, {
  encoding: ['utf8', 'base64', 'buffer'],
  len: [2, 16, 256], // x16
  n: [4e6]
});

// 16 chars each
const chars = [
  'hello brendan!!!', // 1 byte
  'ΰαβγδεζηθικλμνξο', // 2 bytes
  '挰挱挲挳挴挵挶挷挸挹挺挻挼挽挾挿', // 3 bytes
  '𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕𠴕𠵼𠵿𠸎𠸏𠹷𠺝𠺢', // 4 bytes
];

function main({ n, len, encoding }) {
  let strings = [];
  let results = [len * 16];
  if (encoding === 'buffer') {
    strings = [Buffer.alloc(len * 16, 'a')];
  } else {
    for (const string of chars) {
      // Strings must be built differently, depending on encoding
      const data = string.repeat(len);
      if (encoding === 'utf8') {
        strings.push(data);
      } else if (encoding === 'base64') {
        // Base64 strings will be much longer than their UTF8 counterparts
        strings.push(Buffer.from(data, 'utf8').toString('base64'));
      }
    }

    // Check the result to ensure it is *properly* optimized
    results = strings.map((val) => Buffer.byteLength(val, encoding));
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const index = n % strings.length;
    // Go!
    const r = Buffer.byteLength(strings[index], encoding);

    if (r !== results[index])
      throw new Error('incorrect return value');
  }
  bench.end(n);
}
