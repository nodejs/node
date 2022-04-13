// Flags:  --enable-source-maps

'use strict';
require('../common');
try {
  require('../fixtures/source-map/typescript-throw');
} catch (err) {
  setTimeout(() => {
    console.info(err);
  }, 10);
}
