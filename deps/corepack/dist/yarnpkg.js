#!/usr/bin/env node
require('./lib/corepack.cjs').runMain(['yarnpkg', ...process.argv.slice(2)]);