'use strict';

var cp = require('child_process');
var parse = require('./lib/parse');
var enoent = require('./lib/enoent');

function spawn(command, args, options) {
    var parsed;
    var spawned;

    // Parse the arguments
    parsed = parse(command, args, options);

    // Spawn the child process
    spawned = cp.spawn(parsed.command, parsed.args, parsed.options);

    // Hook into child process "exit" event to emit an error if the command
    // does not exists, see: https://github.com/IndigoUnited/node-cross-spawn/issues/16
    enoent.hookChildProcess(spawned, parsed);

    return spawned;
}

module.exports = spawn;
module.exports.spawn = spawn;
module.exports._parse = parse;
module.exports._enoent = enoent;
