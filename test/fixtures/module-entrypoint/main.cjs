'use strict';

const { entrypoint } = require('node:module');

console.log(JSON.stringify({
  entrypoint,
  matchesMain: require('node:url').pathToFileURL(__filename).href === entrypoint,
}));
