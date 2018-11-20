'use strict'
var fs = require('graceful-fs');
var chain = require('slide').chain;
var crypto = require('crypto');

var md5hex = function () {
    var hash = crypto.createHash('md5');
    for (var ii=0; ii<arguments.length; ++ii) hash.update(''+arguments[ii])
    return hash.digest('hex')
}
var invocations = 0;
var getTmpname = function (filename) {
    return filename + "." + md5hex(__filename, process.pid, ++invocations)
}

module.exports = function writeFile(filename, data, options, callback) {
    if (options instanceof Function) {
        callback = options;
        options = null;
    }
    if (!options) options = {};
    var tmpfile = getTmpname(filename);
    chain([
        [fs, fs.writeFile, tmpfile, data, options],
        options.chown && [fs, fs.chown, tmpfile, options.chown.uid, options.chown.gid],
        [fs, fs.rename, tmpfile, filename]
    ], function (err) {
        err ? fs.unlink(tmpfile, function () { callback(err) })
            : callback()
    })
}

module.exports.sync = function writeFileSync(filename, data, options) {
    if (!options) options = {};
    var tmpfile = getTmpname(filename);
    try {
        fs.writeFileSync(tmpfile, data, options);
        if (options.chown) fs.chownSync(tmpfile, options.chown.uid, options.chown.gid);
        fs.renameSync(tmpfile, filename);
    }
    catch (err) {
        try { fs.unlinkSync(tmpfile) } catch(e) {}
        throw err;
    }
}
