#!/usr/bin/env node
require('./corepack').runMain(['yarn', 'yarnpkg', ...process.argv.slice(2)]);
