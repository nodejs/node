'use strict';

const { Worker } = require('worker_threads');

new Worker('1', { eval: true });
