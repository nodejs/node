var fs = require('fs');
var path = require('path');
var crypto = require('crypto');

var mkdirp = require('mkdirp');
var detective = require('detective');

module.exports = function (cacheFile) {
    mkdirp.sync(path.dirname(cacheFile), 0700);
    
    var cache = {};
    if (path.existsSync(cacheFile)) {
        var body = fs.readFileSync(cacheFile);
        try {
            cache = JSON.parse(body);
        }
        catch (err) {}
    }
    
    function save (h, res) {
        cache[h] = res;
        fs.writeFileSync(cacheFile, JSON.stringify(cache));
    }
    
    function hash (src) {
        return new crypto.Hash('md5').update(src).digest('hex');
    }
    
    var deputy = function (src) {
        return deputy.find(src).strings;
    };
    
    deputy.find = function (src) {
        var h = hash(src);
        var c = cache[h];
        if (c) return c;
        else {
            c = detective.find(src);
            save(h, c);
            return c;
        }
    };
    
    return deputy;
};
