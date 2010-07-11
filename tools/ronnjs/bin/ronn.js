#!/usr/bin/nodejs

var opts = require(__dirname + '/../lib/ext/opts');
var ronn = require(__dirname + '/../lib/ronn');

var options = [
	{ short       : 'V'
	, description : 'Show version and exit'
	, callback    : function () { sys.puts('0.1'); process.exit(1); }
	},
	{ short       : 'b'
	, long        : 'build'
	, description : 'Output to files with appropriate extension'
	},
	{ short       : 'm'
	, long        : 'man'
	, description : 'Convert to roff and open with man'
	},
	{ short       : 'r'
	, long        : 'roff'
	, description : 'Convert to roff format'
	},
	{ short       : '5'
	, long        : 'html'
	, description : 'Convert to html format'
	},
	{ short       : 'f'
	, long        : 'fragment'
	, description : 'Convert to html fragment format'
	},
	{ long        : 'manual'
	, description : 'Set "manual" attribute'
	, value       : true
	},
	{ long        : 'version'
	, description : 'Set "version" attribute'
	, value       : true
	},
	{ long        : 'date'
	, description : 'Set "date" attribute'
	, value       : true
	}
];
var arguments = [
	{ name : 'file'
	, required : true
	, description: 'A ronn file'
	}
];
opts.parse(options, arguments, true);


var sys = require('sys');
var fs = require('fs');
var path = require('path');

var fPath = opts.arg('file');
var fBase = path.join(path.dirname(fPath), path.basename(fPath, path.extname(fPath)));

var fTxt = fs.readFileSync(fPath, 'utf8');
var ronn = new ronn.Ronn(fTxt, opts.get("version"), opts.get("manual"), opts.get("date"));

if (opts.get("man") && !opts.get("build")) {
	var spawn = require('child_process').spawn;
	var man = spawn('man', ['--warnings',  '-E UTF-8',  '-l',  '-'], {"LANG":"C"});
	man.stdout.addListener('data', function (data) {
		sys.puts(data);
	});
	man.stderr.addListener('data', function (data) {
		sys.puts(data);
	});
	man.addListener('exit', function() {
		process.exit(0);
	});
	man.stdin.write(ronn.roff(), 'utf8');
	man.stdin.end();
} else {
	var fRoff = null;
	var fHtml = null;
	var fFrag = null;
	if (!opts.get("html") && !opts.get("fragment")) fRoff = ronn.roff();
	else {
		if (opts.get("roff")) fRoff = ronn.roff();
		if (opts.get("html")) fHtml = ronn.html();
		if (opts.get("fragment")) {
			if (opts.get("html")) {
				sys.debug("Can't use both --fragment and --html");
				process.exit(-1);
			}
			fFrag = ronn.fragment();
		}
	}
	if (opts.get("build")) {
		if (fRoff) fs.writeFileSync(fBase + ".roff", fRoff, 'utf8');
		if (fHtml) fs.writeFileSync(fBase + ".html", fHtml, 'utf8');
		if (fFrag) fs.writeFileSync(fBase + ".fragment", fFrag, 'utf8');
	} else {
		if (fRoff) sys.puts(fRoff);
		if (fHtml) sys.puts(fHtml);
		if (fFrag) sys.puts(fFrag);
	}
}
