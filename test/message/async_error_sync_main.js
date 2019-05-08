'use strict';
require('../common');
const four = require('../fixtures/async-error');

async function main() {
  try {
    await four();
  } catch (e) {
    console.error(e);
  }
}

main();
