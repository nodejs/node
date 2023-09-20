#!/usr/bin/env node
require('./lib/corepack.cjs').runMain(['pnpx', ...process.argv.slice(2)]);