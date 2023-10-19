#!/usr/bin/env node
require('./lib/corepack.cjs').runMain(['yarn', ...process.argv.slice(2)]);