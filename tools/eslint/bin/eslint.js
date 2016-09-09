#!/usr/bin/env node

/**
 * @fileoverview Main CLI that is run via the eslint command.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var useStdIn = (process.argv.indexOf("--stdin") > -1),
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
            process.exitCode = cli.execute(process.argv, text);
        } catch (ex) {
            console.error(ex.message);
            console.error(ex.stack);
            process.exitCode = 1;
        }
    }));
} else if (init) {
    var configInit = require("../lib/config/config-initializer");
    configInit.initializeConfig(function(err) {
        if (err) {
            process.exitCode = 1;
            console.error(err.message);
            console.error(err.stack);
        } else {
            process.exitCode = 0;
        }
    });
} else {
    process.exitCode = cli.execute(process.argv);
}
