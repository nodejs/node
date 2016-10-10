#!/usr/bin/env node
'use strict';
var pkg = require('./package.json');
var repeating = require('./');
var argv = process.argv.slice(2);

function help() {
	console.log([
		'',
		'  ' + pkg.description,
		'',
		'  Usage',
		'    $ repeating <string> <count>',
		'',
		'  Example',
		'    $ repeating \'unicorn \' 2',
		'    unicorn unicorn'
	].join('\n'));
}

if (process.argv.indexOf('--help') !== -1) {
	help();
	return;
}

if (process.argv.indexOf('--version') !== -1) {
	console.log(pkg.version);
	return;
}

if (!argv[1]) {
	console.error('You have to define how many times to repeat the string.');
	process.exit(1);
}

console.log(repeating(argv[0], Number(argv[1])));
