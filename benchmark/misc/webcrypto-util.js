'use strict';

const common = require('../common.js');

const inputs = [
  'arraybuffer',
  'uint8array',
  'dataview',
  'buffer',
  'sharedview',
  'empty-arraybuffer',
  'empty-uint8array',
  'empty-dataview',
  'detached-arraybuffer',
  'detached-uint8array',
  'detached-dataview',
];

const bench = common.createBenchmark(main, {
  op: [
    ...inputs.flatMap((input) => [`byteLength:${input}`, `bytes:${input}`]),
    'truncate:arraybuffer:100',
    'truncate:arraybuffer:128',
    'truncate:uint8array:100',
    'truncate:uint8array:128',
    'usages:0',
    'usages:1',
    'usages:2',
    'usages:4',
  ],
  n: [1e6],
}, { flags: ['--expose-internals'] });

function createInput(input) {
  const empty = input.startsWith('empty-');
  const detached = input.startsWith('detached-');
  const type = input.replace(/^(empty|detached)-/, '');
  const size = empty ? 0 : 32;
  const buffer = new ArrayBuffer(size + 16);
  let value;
  switch (type) {
    case 'arraybuffer':
      value = new ArrayBuffer(size);
      break;
    case 'uint8array':
      value = new Uint8Array(buffer, 8, size);
      break;
    case 'dataview':
      value = new DataView(buffer, 8, size);
      break;
    case 'buffer':
      value = Buffer.from(buffer, 8, size);
      break;
    case 'sharedview':
      value = new Uint8Array(new SharedArrayBuffer(size));
      break;
    default:
      throw new Error(`Unsupported input: ${input}`);
  }
  if (detached) {
    const backing = type === 'arraybuffer' ? value : buffer;
    structuredClone(backing, { transfer: [backing] });
  }
  return value;
}

function main({ n, op }) {
  const {
    getBufferSourceByteLength,
    getBufferSourceBytes,
    getUsagesFromMask,
    getUsagesMask,
    truncateToBitLength,
  } = require('internal/crypto/util');
  const [operation, input, length] = op.split(':');
  let run;
  switch (operation) {
    case 'byteLength': {
      const value = createInput(input);
      run = () => getBufferSourceByteLength(value);
      break;
    }
    case 'bytes': {
      const value = createInput(input);
      run = () => getBufferSourceBytes(value);
      break;
    }
    case 'truncate': {
      const value = createInput(input);
      const bits = Number(length);
      run = () => truncateToBitLength(bits, value);
      break;
    }
    case 'usages': {
      const usages = {
        0: [],
        1: ['sign'],
        2: ['sign', 'verify'],
        4: ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'],
      };
      const mask = getUsagesMask(usages[input]);
      run = () => getUsagesFromMask(mask);
      break;
    }
    default:
      throw new Error(`Unsupported operation: ${operation}`);
  }

  let result;
  bench.start();
  for (let i = 0; i < n; i++)
    result = run();
  bench.end(n);

  if (result === undefined)
    throw new Error('Missing benchmark result');
}
