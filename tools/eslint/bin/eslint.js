#!/usr/bin/env node

/**
 * @fileoverview Main CLI that is run via the eslint command.
 * @author Nicholas C. Zakas
 * @copyright 2013 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var exitCode = 0,
    useStdIn = (process.argv.indexOf("--stdin") > -1),
    init = (process.argv.indexOf("--init") > -1),
    debug = (process.argv.indexOf("--debug") > -1);

// must do this initialization *before* other requires in order to work
if (debug) {
    require("debug").enable("eslint:*,-eslint:code-path");
}

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

// now we can safely include the other modules that use debug
var concat = require("concat-stream"),
    cli = require("../lib/cli"),
    path = require("path"),
    fs = require("fs");

//------------------------------------------------------------------------------
// Execution
//------------------------------------------------------------------------------

process.on("uncaughtException", function(err){
    // lazy load
    var lodash = require("lodash");

    if (typeof err.messageTemplate === "string" && err.messageTemplate.length > 0) {
        var template = lodash.template(fs.readFileSync(path.resolve(__dirname, "../messages/" + err.messageTemplate + ".txt"), "utf-8"));

        console.log("\nOops! Something went wrong! :(");
        console.log("\n" + template(err.messageData || {}));
    } else {
        console.log(err.message);
        console.log(err.stack);
    }

    process.exit(1);
});

if (useStdIn) {
    process.stdin.pipe(concat({ encoding: "string" }, function(text) {
        try {
            exitCode = cli.execute(process.argv, text);
        } catch (ex) {
            console.error(ex.message);
            console.error(ex.stack);
            exitCode = 1;
        }
    }));
} else if (init) {
    var configInit = require("../lib/config/config-initializer");
    configInit.initializeConfig(function(err) {
        if (err) {
            exitCode = 1;
            console.error(err.message);
            console.error(err.stack);
        } else {
            exitCode = 0;
        }
    });
} else {
    exitCode = cli.execute(process.argv);
}

// https://github.com/eslint/eslint/issues/4691
// In Node.js >= 0.12, you can use a cleaner way
if ("exitCode" in process) {
    process.exitCode = exitCode;
} else {
    /*
     * Wait for the stdout buffer to drain.
     * See https://github.com/eslint/eslint/issues/317
     */
    process.on("exit", function() {
        process.exit(exitCode);
    });
}
