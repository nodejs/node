#!/usr/bin/env node
'use strict';
var fs = require('fs');
var stdin = require('get-stdin');
var argv = require('minimist')(process.argv.slice(2));
var pkg = require('./package.json');
var detectIndent = require('./');
var input = argv._[0];

function help() {
	console.log([
		'',
		'  ' + pkg.description,
		'',
		'  Usage',
		'    detect-indent <file>',
		'    echo <string> | detect-indent',
		'',
		'  Example',
		'    echo \'  foo\\n  bar\' | detect-indent | wc --chars',
		'    2'
	].join('\n'));
}

function init(data) {
	var indent = detectIndent(data).indent;

	if (indent !== null) {
		process.stdout.write(indent);
	} else {
		console.error('Indentation could not be detected');
		process.exit(2);
	}
}

if (argv.help) {
	help();
	return;
}

if (argv.version) {
	console.log(pkg.version);
	return;
}

if (process.stdin.isTTY) {
	if (!input) {
		help();
		return;
	}

	init(fs.readFileSync(input, 'utf8'));
} else {
	stdin(init);
}
