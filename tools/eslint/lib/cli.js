/**
 * @fileoverview Main CLI object.
 * @author Nicholas C. Zakas
 */

"use strict";

/*
 * The CLI object should *not* call process.exit() directly. It should only return
 * exit codes. This allows other programs to use the CLI object and still control
 * when the program exits.
 */

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var fs = require("fs"),
    path = require("path"),

    debug = require("debug"),

    options = require("./options"),
    CLIEngine = require("./cli-engine"),
    mkdirp = require("mkdirp"),
    log = require("./logging");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

debug = debug("eslint:cli");

/**
 * Translates the CLI options into the options expected by the CLIEngine.
 * @param {Object} cliOptions The CLI options to translate.
 * @returns {CLIEngineOptions} The options object for the CLIEngine.
 * @private
 */
function translateOptions(cliOptions) {
    return {
        envs: cliOptions.env,
        extensions: cliOptions.ext,
        rules: cliOptions.rule,
        plugins: cliOptions.plugin,
        globals: cliOptions.global,
        ignore: cliOptions.ignore,
        ignorePath: cliOptions.ignorePath,
        ignorePattern: cliOptions.ignorePattern,
        configFile: cliOptions.config,
        rulePaths: cliOptions.rulesdir,
        useEslintrc: cliOptions.eslintrc,
        parser: cliOptions.parser,
        parserOptions: cliOptions.parserOptions,
        cache: cliOptions.cache,
        cacheFile: cliOptions.cacheFile,
        cacheLocation: cliOptions.cacheLocation,
        fix: cliOptions.fix,
        allowInlineConfig: cliOptions.inlineConfig
    };
}

/**
 * Outputs the results of the linting.
 * @param {CLIEngine} engine The CLIEngine to use.
 * @param {LintResult[]} results The results to print.
 * @param {string} format The name of the formatter to use or the path to the formatter.
 * @param {string} outputFile The path for the output file.
 * @returns {boolean} True if the printing succeeds, false if not.
 * @private
 */
function printResults(engine, results, format, outputFile) {
    var formatter,
        output,
        filePath;

    formatter = engine.getFormatter(format);
    if (!formatter) {
        log.error("Could not find formatter '%s'.", format);
        return false;
    }

    output = formatter(results);

    if (output) {
        if (outputFile) {
            filePath = path.resolve(process.cwd(), outputFile);

            if (fs.existsSync(filePath) && fs.statSync(filePath).isDirectory()) {
                log.error("Cannot write to output file path, it is a directory: %s", outputFile);
                return false;
            }

            try {
                mkdirp.sync(path.dirname(filePath));
                fs.writeFileSync(filePath, output);
            } catch (ex) {
                log.error("There was a problem writing the output file:\n%s", ex);
                return false;
            }
        } else {
            log.info(output);
        }
    }

    return true;

}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Encapsulates all CLI behavior for eslint. Makes it easier to test as well as
 * for other Node.js programs to effectively run the CLI.
 */
var cli = {

    /**
     * Executes the CLI based on an array of arguments that is passed in.
     * @param {string|Array|Object} args The arguments to process.
     * @param {string} [text] The text to lint (used for TTY).
     * @returns {int} The exit code for the operation.
     */
    execute: function(args, text) {

        var currentOptions,
            files,
            report,
            engine,
            tooManyWarnings;

        try {
            currentOptions = options.parse(args);
        } catch (error) {
            log.error(error.message);
            return 1;
        }

        files = currentOptions._;

        if (currentOptions.version) { // version from package.json

            log.info("v" + require("../package.json").version);

        } else if (currentOptions.help || (!files.length && !text)) {

            log.info(options.generateHelp());

        } else {

            debug("Running on " + (text ? "text" : "files"));

            // disable --fix for piped-in code until we know how to do it correctly
            if (text && currentOptions.fix) {
                log.error("The --fix option is not available for piped-in code.");
                return 1;
            }

            engine = new CLIEngine(translateOptions(currentOptions));
            if (currentOptions.printConfig) {
                if (files.length !== 1) {
                    log.error("The --print-config option requires a " +
                        "single file as positional argument.");
                    return 1;
                }

                if (text) {
                    log.error("The --print-config option is not available for piped-in code.");
                    return 1;
                }

                var fileConfig = engine.getConfigForFile(files[0]);

                log.info(JSON.stringify(fileConfig, null, "  "));
                return 0;
            }

            report = text ? engine.executeOnText(text, currentOptions.stdinFilename) : engine.executeOnFiles(files);
            if (currentOptions.fix) {
                debug("Fix mode enabled - applying fixes");
                CLIEngine.outputFixes(report);
            }

            if (currentOptions.quiet) {
                debug("Quiet mode enabled - filtering out warnings");
                report.results = CLIEngine.getErrorResults(report.results);
            }

            if (printResults(engine, report.results, currentOptions.format, currentOptions.outputFile)) {
                tooManyWarnings = currentOptions.maxWarnings >= 0 && report.warningCount > currentOptions.maxWarnings;

                if (!report.errorCount && tooManyWarnings) {
                    log.error("ESLint found too many warnings (maximum: %s).", currentOptions.maxWarnings);
                }

                return (report.errorCount || tooManyWarnings) ? 1 : 0;
            } else {
                return 1;
            }

        }

        return 0;
    }
};

module.exports = cli;
