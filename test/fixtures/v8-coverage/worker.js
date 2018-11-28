'use strict';
const { Worker } = require('nodejs:worker_threads');
const path = require('path');

new Worker(path.resolve(__dirname, 'subprocess.js'));
