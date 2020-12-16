#!/usr/bin/env node
require('./corepack').runMain(['npm', 'npx', ...process.argv.slice(2)]);
