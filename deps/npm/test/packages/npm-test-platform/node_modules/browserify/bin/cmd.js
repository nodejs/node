#!/usr/bin/env node

var browserify = require('../');
var fs = require('fs');
var resolve = require('resolve');

var argv = require('optimist')
    .usage('Usage: browserify [entry files] {OPTIONS}')
    .wrap(80)
    .option('outfile', {
        alias : 'o',
        desc : 'Write the browserify bundle to this file.\n'
            + 'If unspecified, browserify prints to stdout.'
        ,
    })
    .option('require', {
        alias : 'r',
        desc : 'A module name or file to bundle.require()\n'
            + 'Optionally use a colon separator to set the target.'
        ,
    })
    .option('entry', {
        alias : 'e',
        desc : 'An entry point of your app'
    })
    .option('exports', {
        desc : 'Export these core objects, comma-separated list\n'
            + 'with any of: require, process. If unspecified, the\n'
            + 'export behavior will be inferred.\n'
    })
    .option('ignore', {
        alias : 'i',
        desc : 'Ignore a file'
    })
    .option('alias', {
        alias : 'a',
        desc : 'Register an alias with a colon separator: "to:from"\n'
            + "Example: --alias 'jquery:jquery-browserify'"
        ,
    })
    .option('cache', {
        alias : 'c',
        desc : 'Turn on caching at $HOME/.config/browserling/cache.json '
            + 'or use a file for caching.\n',
        default : true,
    })
    .option('debug', {
        alias : 'd',
        desc : 'Switch on debugging mode with //@ sourceURL=...s.',
        type : 'boolean'
    })
    .option('plugin', {
        alias : 'p',
        desc : 'Use a plugin.\n'
            + 'Example: --plugin aliasify'
        ,
    })
    .option('prelude', {
        default : true,
        type : 'boolean',
        desc : 'Include the code that defines require() in this bundle.'
    })
    .option('watch', {
        alias : 'w',
        desc : 'Watch for changes. The script will stay open and write updates '
            + 'to the output every time any of the bundled files change.\n'
            + 'This option only works in tandem with -o.'
        ,
    })
    .option('verbose', {
        alias : 'v',
        desc : 'Write out how many bytes were written in -o mode. '
            + 'This is especially useful with --watch.'
        ,
    })
    .option('help', {
        alias : 'h',
        desc : 'Show this message'
    })
    .check(function (argv) {
        if (argv.help) throw ''
        if (process.argv.length <= 2) throw 'Specify a parameter.'
    })
    .argv
;

var bundle = browserify({
    watch : argv.watch,
    cache : argv.cache,
    debug : argv.debug,
    exports : argv.exports && argv.exports.split(','),
});

bundle.on('syntaxError', function (err) {
    console.error(err && err.stack || String(err));
    process.exit(1);
});

if (argv.noprelude || argv.prelude === false) {
    bundle.files = [];
    bundle.prepends = [];
}
if (argv.ignore) bundle.ignore(argv.ignore);

([].concat(argv.plugin || [])).forEach(function (plugin) {
    var resolved = resolve.sync(plugin, { basedir : process.cwd() });
    bundle.use(require(resolved));
});

([].concat(argv.alias || [])).forEach(function (alias) {
    if (!alias.match(/:/)) {
        console.error('aliases require a colon separator');
        process.exit();
    }
    bundle.alias.apply(bundle, alias.split(':'));
});

([].concat(argv.require || [])).forEach(function (req) {
    if (req.match(/:/)) {
        var s = req.split(':');
        bundle.require(s[0], { target : s[1] });
        return;
    }
    
    if (!/^[.\/]/.test(req)) {
        try {
            var res = resolve.sync(req, { basedir : process.cwd() });
        }
        catch (e) {
            return bundle.require(req);
        }
        return bundle.require(res, { target : req });
    }
    bundle.require(req);
});

(argv._.concat(argv.entry || [])).forEach(function (entry) {
    bundle.addEntry(entry);
});

if (argv.outfile) {
    function write () {
        var src = bundle.bundle();
        fs.writeFile(argv.outfile, src, function () {
            if (argv.verbose) {
                console.log(Buffer(src).length + ' bytes written');
            }
        });
    }
    
    write();
    if (argv.watch) bundle.on('bundle', write)
}
else {
    var src = bundle.bundle();
    try { Function(src) }
    catch (err) {
        console.error(err.stack);
        process.exit(1);
    }
    console.log(src);
}
