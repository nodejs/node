"use strict";

module.exports = exports = reveal;

exports.usage = 'Reveals data on the versioned binary';

var fs = require('fs');
var versioning = require('./util/versioning.js');

function unix_paths(key, val) {
    return val && val.replace ? val.replace(/\\/g, '/') : val;
}

function reveal(gyp, argv, callback) {
    var package_json = JSON.parse(fs.readFileSync('./package.json'));
    var opts = versioning.evaluate(package_json, gyp.opts);
    var hit = false;
    // if a second arg is passed look to see
    // if it is a known option
    //console.log(JSON.stringify(gyp.opts,null,1))
    var remain = gyp.opts.argv.remain.pop();
    if (remain && opts.hasOwnProperty(remain)) {
        console.log(opts[remain].replace(/\\/g, '/'));
        hit = true;
    }
    // otherwise return all options as json
    if (!hit) {
        console.log(JSON.stringify(opts,unix_paths,2));
    }
    return callback();
}
