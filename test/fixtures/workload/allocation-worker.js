'use strict';

const { Worker } = require('worker_threads');
const path = require('path');
new Worker(path.join(__dirname, 'allocation.js'));
