#!/usr/bin/env node

/*!
 * Module dependencies.
 */

var qrcode = require('../lib/main'),
    path = require('path'),
    fs = require('fs');

/*!
 * Parse the process name
 */

var name = process.argv[1].replace(/^.*[\\\/]/, '').replace('.js', '');

/*!
 * Parse the input
 */

if (process.stdin.isTTY) {
    // called with input as argument, e.g.:
    // ./qrcode-terminal.js "INPUT"

    var input = process.argv[2];
    handleInput(input);
} else {
    // called with piped input, e.g.:
    // echo "INPUT" | ./qrcode-terminal.js

    var readline = require('readline');

    var interface = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        terminal: false
    });

    interface.on('line', function(line) {
        handleInput(line);
    });
}

/*!
 * Process the input
 */

function handleInput(input) {

    /*!
     * Display help
     */

    if (!input || input === '-h' || input === '--help') {
        help();
        process.exit();
    }

    /*!
     * Display version
     */

    if (input === '-v' || input === '--version') {
        version();
        process.exit();
    }

    /*!
     * Render the QR Code
     */

    qrcode.generate(input);
}

/*!
 * Helper functions
 */

function help() {
    console.log([
        '',
        'Usage: ' + name + ' <message>',
        '',
        'Options:',
        '  -h, --help           output usage information',
        '  -v, --version        output version number',
        '',
        'Examples:',
        '',
        '  $ ' + name + ' hello',
        '  $ ' + name + ' "hello world"',
        ''
    ].join('\n'));
}

function version() {
    var packagePath = path.join(__dirname, '..', 'package.json'),
        packageJSON = JSON.parse(fs.readFileSync(packagePath), 'utf8');

    console.log(packageJSON.version);
}
