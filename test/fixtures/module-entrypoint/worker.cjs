'use strict';

const { entrypoint } = require('node:module');
const { parentPort } = require('node:worker_threads');

parentPort.postMessage(entrypoint);
