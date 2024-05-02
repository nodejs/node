'use strict';

const common = require('../common');
const { promises: fs } = require('fs');

async function doOpen() {
  const fh = await fs.open(__filename);
  fh.on('close', common.mustCall());
  await fh[Symbol.asyncDispose]();
}

doOpen().then(common.mustCall());
