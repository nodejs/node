var path = require('path');
var fs = require('fs');

module.exports = mkdirP.mkdirp = mkdirP.mkdirP = mkdirP;

function mkdirP (p, mode, f) {
    var cb = f || function () {};
    if (typeof mode === 'string') mode = parseInt(mode, 8);
    p = path.resolve(p);

    fs.mkdir(p, mode, function (er) {
        if (!er) return cb();
        switch (er.code) {
            case 'ENOENT':
                mkdirP(path.dirname(p), mode, function (er) {
                    if (er) cb(er);
                    else mkdirP(p, mode, cb);
                });
                break;

            case 'EEXIST':
                fs.stat(p, function (er2, stat) {
                    // if the stat fails, then that's super weird.
                    // let the original EEXIST be the failure reason.
                    if (er2 || !stat.isDirectory()) cb(er)
                    else if ((stat.mode & 0777) !== mode) fs.chmod(p, mode, cb);
                    else cb();
                });
                break;

            default:
                cb(er);
                break;
        }
    });
}
