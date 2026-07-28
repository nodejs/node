'use strict';
require('../../common');
const four = require('../async-error');

async function main() {
  try {
    await four();
  } catch (e) {
    console.error(e);
  }
}

queueMicrotask(main);
