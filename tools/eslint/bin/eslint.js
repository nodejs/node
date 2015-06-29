#!/usr/bin/env node
var concat = require("concat-stream"),
    configInit = require("../lib/config-initializer"),
    cli = require("../lib/cli");

var exitCode = 0,
    useStdIn = (process.argv.indexOf("--stdin") > -1),
    init = (process.argv.indexOf("--init") > -1);

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
    configInit.initializeConfig(function(err) {
        if (err) {
            exitCode = 1;
            console.error(err.message);
            console.error(err.stack);
        } else {
            console.log("Successfully created .eslintrc file in " + process.cwd());
            exitCode = 0;
        }
    });
} else {
    exitCode = cli.execute(process.argv);
}

/*
 * Wait for the stdout buffer to drain.
 * See https://github.com/eslint/eslint/issues/317
 */
process.on("exit", function() {
    process.exit(exitCode);
});
