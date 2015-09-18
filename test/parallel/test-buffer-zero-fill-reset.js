'use strict';

require('../common');
const assert = require('assert');


function testUint8Array(ui) {
  const length = ui.length;
  for (let i = 0; i < length; i++)
    if (ui[i] !== 0) return false;
  return true;
}


for (let i = 0; i < 100; i++) {
  new Buffer(0);
  let ui = new Uint8Array(65);
  assert.ok(testUint8Array(ui), 'Uint8Array is not zero-filled');
}
