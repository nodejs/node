#!/usr/bin/env node
require('./lib/corepack.cjs').runMain(['pnpm', ...process.argv.slice(2)]);