'use strict';
require('../common');
const spawn = require('child_process').spawn;

const child = spawn(process.execPath,
                    [ '--inspect', 'no-such-script.js' ],
                    { 'stdio': 'inherit' });

function signalHandler(value) {
  child.kill();
  process.exit(1);
}

process.on('SIGINT', signalHandler);
process.on('SIGTERM', signalHandler);
