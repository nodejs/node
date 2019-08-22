module.exports = {
    "name" : basename.replace(/^node-/, ''),
    "version" : "0.0.0",
    "description" : (function (cb) {
        var fs = require('fs');
        var value;
        try {
            var src = fs.readFileSync('README.markdown', 'utf8');
            value = src.split('\n').filter(function (line) {
                return /\s+/.test(line)
                    && line.trim() !== basename.replace(/^node-/, '')
                ;
            })[0]
                .trim()
                .replace(/^./, function (c) { return c.toLowerCase() })
                .replace(/\.$/, '')
            ;
        }
        catch (e) {}
        
        return prompt('description', value);
    })(),
    "main" : prompt('entry point', 'index.js'),
    "bin" : function (cb) {
        var path = require('path');
        var fs = require('fs');
        var exists = fs.exists || path.exists;
        exists('bin/cmd.js', function (ex) {
            var bin
            if (ex) {
                var bin = {}
                bin[basename.replace(/^node-/, '')] = 'bin/cmd.js'
            }
            cb(null, bin);
        });
    },
    "directories" : {
        "example" : "example",
        "test" : "test"
    },
    "dependencies" : {},
    "devDependencies" : {
        "tap" : "~0.2.5"
    },
    "scripts" : {
        "test" : "tap test/*.js"
    },
    "repository" : {
        "type" : "git",
        "url" : "git://github.com/substack/" + basename + ".git"
    },
    "homepage" : "https://github.com/substack/" + basename,
    "keywords" : prompt(function (s) { return s.split(/\s+/) }),
    "author" : {
        "name" : "James Halliday",
        "email" : "mail@substack.net",
        "url" : "http://substack.net"
    },
    "license" : "MIT",
    "engine" : { "node" : ">=0.6" }
}
