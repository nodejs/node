'use strict';

const common = require('../common');
const { Readable, Writable, Duplex, finished } = require('stream');

const bench = common.createBenchmark(main, {
  n: [1e5],
  streamType: ['readable', 'writable', 'duplex'],
});

function main({ n, streamType }) {
  bench.start();

  for (let i = 0; i < n; i++) {
    let stream;

    switch (streamType) {
      case 'readable':
        stream = new Readable({ read() { this.push(null); } });
        break;
      case 'writable':
        stream = new Writable({ write(chunk, enc, cb) { cb(); } });
        stream.end();
        break;
      case 'duplex':
        stream = new Duplex({
          read() { this.push(null); },
          write(chunk, enc, cb) { cb(); },
        });
        stream.end();
        break;
    }

    finished(stream, () => {});
  }

  bench.end(n);
}
