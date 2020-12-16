#!/usr/bin/env node
require('./corepack').runMain(['pnpm', 'pnpm', ...process.argv.slice(2)]);
