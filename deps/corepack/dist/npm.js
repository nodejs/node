#!/usr/bin/env node
require('./lib/corepack.cjs').runMain(['npm', ...process.argv.slice(2)]);