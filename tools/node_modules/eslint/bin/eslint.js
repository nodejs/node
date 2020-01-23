#!/usr/bin/env node

/**
 * @fileoverview Main CLI that is run via the eslint command.
 * @author Nicholas C. Zakas
 */

/* eslint no-console:off */

"use strict";

// to use V8's code cache to speed up instantiation time
require("v8-compile-cache");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const useStdIn = process.argv.includes("--stdin"),
    init = process.argv.includes("--init"),
    debug = process.argv.includes("--debug");

// must do this initialization *before* other requires in order to work
if (debug) {
    require("debug").enable("eslint:*,-eslint:code-path");
}

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

// now we can safely include the other modules that use debug
const path = require("path"),
    fs = require("fs"),
    cli = require("../lib/cli");

//------------------------------------------------------------------------------
// Execution
//------------------------------------------------------------------------------

process.once("uncaughtException", err => {

    // lazy load
    const lodash = require("lodash");

    if (typeof err.messageTemplate === "string" && err.messageTemplate.length > 0) {
        const template = lodash.template(fs.readFileSync(path.resolve(__dirname, `../messages/${err.messageTemplate}.txt`), "utf-8"));
        const pkg = require("../package.json");

        console.error("\nOops! Something went wrong! :(");
        console.error(`\nESLint: ${pkg.version}.\n\n${template(err.messageData || {})}`);
    } else {
        console.error(err.stack);
    }

    process.exitCode = 2;
});

if (useStdIn) {

    /*
     * Note: See
     * - https://github.com/nodejs/node/blob/master/doc/api/process.md#processstdin
     * - https://github.com/nodejs/node/blob/master/doc/api/process.md#a-note-on-process-io
     * - https://lists.gnu.org/archive/html/bug-gnu-emacs/2016-01/msg00419.html
     * - https://github.com/nodejs/node/issues/7439 (historical)
     *
     * On Windows using `fs.readFileSync(STDIN_FILE_DESCRIPTOR, "utf8")` seems
     * to read 4096 bytes before blocking and never drains to read further data.
     *
     * The investigation on the Emacs thread indicates:
     *
     * > Emacs on MS-Windows uses pipes to communicate with subprocesses; a
     * > pipe on Windows has a 4K buffer. So as soon as Emacs writes more than
     * > 4096 bytes to the pipe, the pipe becomes full, and Emacs then waits for
     * > the subprocess to read its end of the pipe, at which time Emacs will
     * > write the rest of the stuff.
     *
     * Using the nodejs code example for reading from stdin.
     */
    let contents = "",
        chunk = "";

    process.stdin.setEncoding("utf8");
    process.stdin.on("readable", () => {

        // Use a loop to make sure we read all available data.
        while ((chunk = process.stdin.read()) !== null) {
            contents += chunk;
        }
    });

    process.stdin.on("end", () => {
        process.exitCode = cli.execute(process.argv, contents, "utf8");
    });
} else if (init) {
    const configInit = require("../lib/init/config-initializer");

    configInit.initializeConfig().then(() => {
        process.exitCode = 0;
    }).catch(err => {
        process.exitCode = 1;
        console.error(err.message);
        console.error(err.stack);
    });
} else {
    process.exitCode = cli.execute(process.argv);
}
