"use strict";

module.exports = exports = testpackage;

exports.usage = 'Tests that the staged package is valid';

var fs = require('fs');
var path = require('path');
var log = require('npmlog');
var existsAsync = fs.exists || path.exists;
var versioning = require('./util/versioning.js');
var testbinary = require('./testbinary.js');
var read = require('fs').createReadStream;
var zlib = require('zlib');

function testpackage(gyp, argv, callback) {
    var package_json = JSON.parse(fs.readFileSync('./package.json'));
    var opts = versioning.evaluate(package_json, gyp.opts);
    var tarball = opts.staged_tarball;
    existsAsync(tarball, function(found) {
        if (!found) {
            return callback(new Error("Cannot test package because " + tarball + " missing: run `node-pre-gyp package` first"));
        }
        var to = opts.module_path;
        var gunzip = zlib.createGunzip();
        var extracter = require('tar').Extract({ path: to, strip: 1 });
        function filter_func(entry) {
            // ensure directories are +x
            // https://github.com/mapnik/node-mapnik/issues/262
            entry.props.mode |= (entry.props.mode >>> 2) & parseInt('0111',8);
            log.info('install','unpacking ' + entry.path);
        }
        gunzip.on('error', callback);
        extracter.on('error', callback);
        extracter.on('entry', filter_func);
        extracter.on('end', function(err) {
            if (err) return callback(err);
            testbinary(gyp,argv,function(err) {
                if (err) {
                    return callback(err);
                } else {
                    console.log('['+package_json.name+'] Package appears valid');
                    return callback();
                }
            });
        });
        read(tarball).pipe(gunzip).pipe(extracter);
    });
}
