#!/usr/bin/env node
require('./corepack').runMain(['pnpm', 'pnpx', ...process.argv.slice(2)]);
