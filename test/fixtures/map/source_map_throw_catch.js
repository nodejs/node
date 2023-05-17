// Flags:  --enable-source-maps

'use strict';
require('../../common');
Error.stackTraceLimit = 2;

try {
  require('../source-map/typescript-throw');
} catch (err) {
  setTimeout(() => {
    console.info(err);
  }, 10);
}
