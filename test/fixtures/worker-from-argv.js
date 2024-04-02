'use strict';
const {Worker} = require('worker_threads');
new Worker(process.argv[2]).on('exit', process.exit);
