'use strict';
const required = require('pkg');
import('pkg').then(({ default: imported }) => {
  postMessage({ require: required, import: imported, resolve: require.resolve('pkg') });
});
