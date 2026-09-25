'use strict';
const common = require('../common.js');
const { ReadableStream } = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
  type: ['normal', 'byob'],
});

async function main({ n, type }) {
  switch (type) {
    case 'normal': {
      const rs = new ReadableStream({
        pull: function(controller) {
          controller.enqueue('a');
        },
      });
      const reader = rs.getReader();
      let x = null;
      bench.start();
      for (let i = 0; i < n; i++) {
        const { value } = await reader.read();
        x = value;
      }
      bench.end(n);
      console.assert(x);
      break;
    }
    case 'byob': {
      const encode = new TextEncoder();
      const rs = new ReadableStream({
        type: 'bytes',
        pull: function(controller) {
          controller.enqueue(encode.encode('a'));
        },
      });
      const reader = rs.getReader({ mode: 'byob' });
      let x = null;
      bench.start();
      for (let i = 0; i < n; i++) {
        const { value } = await reader.read(new Uint8Array(1));
        x = value;
      }
      bench.end(n);
      console.assert(x);
      break;
    }
  }
}
