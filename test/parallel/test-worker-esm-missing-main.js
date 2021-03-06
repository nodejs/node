'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const { Worker } = require('worker_threads');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const missing = path.join(tmpdir.path, 'does-not-exist.js');

const worker = new Worker(missing);

worker.on('error', common.mustCall((err) => {
  // eslint-disable-next-line node-core/no-unescaped-regexp-dot
  assert(/Cannot find module .+does-not-exist.js/.test(err.message), err);
}));
