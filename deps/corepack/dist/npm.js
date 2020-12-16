#!/usr/bin/env node
require('./corepack').runMain(['npm', 'npm', ...process.argv.slice(2)]);
