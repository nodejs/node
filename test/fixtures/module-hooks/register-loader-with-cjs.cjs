'use strict';
const {register} = require('node:module');
const fixtures = require('../../common/fixtures.js');

register(
  fixtures.fileURL('es-module-loaders/hooks-initialize.mjs'),
);
register(
  fixtures.fileURL('es-module-loaders/loader-load-foo-or-42.mjs'),
);

import('node:os').then((result) => {
  console.log(JSON.stringify(result));
});
