var current = process.versions && process.versions.node && process.versions.node.split('.') || [];

function versionIncluded(version) {
    if (version === '*') return true;
    var versionParts = version.split('.');
    for (var i = 0; i < 3; ++i) {
        if ((current[i] || 0) >= (versionParts[i] || 0)) return true;
    }
    return false;
}

var data = require('./core.json');

var core = {};
for (var version in data) {
    if (Object.prototype.hasOwnProperty.call(data, version) && versionIncluded(version)) {
        for (var i = 0; i < data[version].length; ++i) {
            core[data[version][i]] = true;
        }
    }
}
module.exports = core;
