var fs = require('fs');
var path = require('path');

// taken from `ls -1 lib` in node 0.6.11
var core = exports.core = [
    'assert', 'buffer_ieee754', 'buffer', 'child_process', 'cluster', 'console',
    'constants', 'crypto', '_debugger', 'dgram', 'dns', 'events', 'freelist',
    'fs', 'http', 'https', '_linklist', 'module', 'net', 'os', 'path',
    'punycode', 'querystring', 'readline', 'repl', 'stream', 'string_decoder',
    'sys', 'timers', 'tls', 'tty', 'url', 'util', 'vm', 'zlib'
].reduce(function (acc, x) { acc[x] = true; return acc }, {});

exports.isCore = function (x) { return core[x] };

exports.sync = function (x, opts) {
    if (core[x]) return x;
    
    if (!opts) opts = {};
    var isFile = opts.isFile || function (file) {
        return path.existsSync(file) && fs.statSync(file).isFile()
    };
    var readFileSync = opts.readFileSync || fs.readFileSync;
    
    var extensions = opts.extensions || [ '.js' ];
    var y = opts.basedir
        || path.dirname(require.cache[__filename].parent.filename)
    ;

    opts.paths = opts.paths || [];

    if (x.match(/^(?:\.\.?\/|\/|([A-Za-z]:)?\\)/)) {
        var m = loadAsFileSync(path.resolve(y, x))
            || loadAsDirectorySync(path.resolve(y, x));
        if (m) return m;
    }
    
    var n = loadNodeModulesSync(x, y);
    if (n) return n;
    
    throw new Error("Cannot find module '" + x + "'");
    
    function loadAsFileSync (x) {
        if (isFile(x)) {
            return x;
        }
        
        for (var i = 0; i < extensions.length; i++) {
            var file = x + extensions[i];
            if (isFile(file)) {
                return file;
            }
        }
    }
    
    function loadAsDirectorySync (x) {
        var pkgfile = path.join(x, '/package.json');
        if (isFile(pkgfile)) {
            var body = readFileSync(pkgfile, 'utf8');
            try {
                var pkg = JSON.parse(body);
                if (opts.packageFilter) {
                    pkg = opts.packageFilter(pkg);
                }
                
                if (pkg.main) {
                    var m = loadAsFileSync(path.resolve(x, pkg.main));
                    if (m) return m;
                }
            }
            catch (err) {}
        }
        
        return loadAsFileSync(path.join( x, '/index'));
    }
    
    function loadNodeModulesSync (x, start) {
        var dirs = nodeModulesPathsSync(start);
        for (var i = 0; i < dirs.length; i++) {
            var dir = dirs[i];
            var m = loadAsFileSync(path.join( dir, '/', x));
            if (m) return m;
            var n = loadAsDirectorySync(path.join( dir, '/', x ));
            if (n) return n;
        }
    }
    
    function nodeModulesPathsSync (start) {
        var splitRe = process.platform === 'win32' ? /[\/\\]/ : /\/+/;
        var parts = start.split(splitRe);
        
        var dirs = [];
        for (var i = parts.length - 1; i >= 0; i--) {
            if (parts[i] === 'node_modules') continue;
            var dir = path.join(
                path.join.apply(path, parts.slice(0, i + 1)),
                'node_modules'
            );
            if (!parts[0].match(/([A-Za-z]:)/)) {
                dir = '/' + dir;    
            }
            dirs.push(dir);
        }
        return opts.paths.concat(dirs);
    }
};
