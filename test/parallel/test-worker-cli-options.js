// Flags: --expose-internals
'use strict';
require('../common');
const { Worker } = require('worker_threads');

const CODE = `
// If the --expose-internals flag does not pass to worker
// require function will throw an error
require('internal/options');
`;
// Test if the flags is passed to worker threads
// See https://github.com/nodejs/node/issues/52825
new Worker(CODE, { eval: true });
new Worker(CODE, { eval: true, env: process.env, execArgv: ['--expose-internals'] });
new Worker(CODE, { eval: true, env: process.env });
new Worker(CODE, { eval: true, execArgv: ['--expose-internals'] });
