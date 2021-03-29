'use strict';

const common = require('../common');
const { strictEqual } = require('assert');

let called = false;

requestAnimationFrame(common.mustCall((ts) => {
  called = true;
  strictEqual(typeof ts, 'number');
}));

strictEqual(called, false);

const req = requestAnimationFrame(common.mustNotCall());
cancelAnimationFrame(req);
