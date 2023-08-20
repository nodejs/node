#!/usr/bin/env node
require('./lib/corepack.cjs').runMain(['npx', ...process.argv.slice(2)]);