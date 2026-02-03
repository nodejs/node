// Flags: --unhandled-rejections=strict
'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');

process.on('unhandledRejection', common.mustNotCall);

process.on('uncaughtException', common.mustCall((err) => {
  assert.ok(err.message.includes('foo'));
}));


async function readFile() {
  return fs.promises.readFile(__filename);
}

async function crash() {
  throw new Error('foo');
}


async function main() {
  crash();
  readFile();
}

main();
