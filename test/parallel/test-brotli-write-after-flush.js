// Flags: --expose-brotli
'use strict';
const common = require('../common');
const assert = require('assert');
const brotli = require('brotli');

const comp = new brotli.Compress();
const decomp = new brotli.Decompress();

comp.pipe(decomp);

let output = '';
const input = 'A line of data\n';
decomp.setEncoding('utf8');
decomp.on('data', (c) => output += c);
decomp.on('end', common.mustCall(() => {
  assert.strictEqual(output, input);
  assert.strictEqual(comp._flushFlag,
                     brotli.constants.BROTLI_OPERATION_PROCESS);
}));

// make sure that flush/write doesn't trigger an assert failure
comp.flush();
comp.write(input);
comp.end();
comp.read(0);
