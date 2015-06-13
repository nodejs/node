#!/usr/bin/env node
var concat = require("concat-stream"),
    cli = require("../lib/cli");

var exitCode = 0,
    useStdIn = (process.argv.indexOf("--stdin") > -1);

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
