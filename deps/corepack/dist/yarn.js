#!/usr/bin/env node
require('./corepack').runMain(['yarn', 'yarn', ...process.argv.slice(2)]);
