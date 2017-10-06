'use strict';

var path = require('path');
var which = require('which');
var LRU = require('lru-cache');

var commandCache = new LRU({ max: 50, maxAge: 30 * 1000 });  // Cache just for 30sec
var hasSepInPathRegExp = new RegExp(process.platform === 'win32' ? /[\/\\]/ : /\//);

function resolveCommand(command, noExtension) {
    var resolved;

    // If command looks like a file path, make it absolute to make it canonical
    // and also to circuvent a bug in which, see: https://github.com/npm/node-which/issues/33
    if (hasSepInPathRegExp.test(command)) {
        command = path.resolve(command);
    }

    noExtension = !!noExtension;
    resolved = commandCache.get(command + '!' + noExtension);

    // Check if its resolved in the cache
    if (commandCache.has(command)) {
        return commandCache.get(command);
    }

    try {
        resolved = !noExtension ?
            which.sync(command) :
            which.sync(command, { pathExt: path.delimiter + (process.env.PATHEXT || '') });
    } catch (e) { /* empty */ }

    commandCache.set(command + '!' + noExtension, resolved);

    return resolved;
}

module.exports = resolveCommand;
