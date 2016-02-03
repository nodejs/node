/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli
 * @version 3.2.2
 * @fileoverview CLI Engine.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var chalk = require('chalk');
var chokidar = require('chokidar');
var CLI = require('./cli');
var fileSetPipeline = require('./file-set-pipeline');

/**
 * Run the file set pipeline once.
 *
 * @param {CLI} cli - A CLI instance.
 * @param {function(Error?, boolean)} done - Callback
 *   invoked when done.
 */
function run(cli, done) {
    cli.spinner.stop();

    fileSetPipeline.run(cli, function (err) {
        /*
         * Check if any file has failed.
         */

        var hasFailed = (cli.files || []).some(function (file) {
            return (file.messages || []).some(function (message) {
                return message.fatal === true ||
                    (message.fatal === false && cli.frail);
            });
        });

        done(err, !hasFailed);

        if (!err && cli.watch) {
            cli.spinner.start();
        }
    });
}

/**
 * CLI engine. This is used by `bin/remark`.
 *
 * @example
 *   engine('--use toc . --output --quiet', console.log);
 *
 * @param {Array.<*>|Object} argv - CLI arguments.
 * @param {function(Error?, boolean)} done - Callback
 *   invoked when done.
 */
function engine(argv, done) {
    var cli = new CLI(argv);
    var enabled = chalk.enabled;
    var watcher;

    chalk.enabled = cli.color;

    if (cli.watch) {
        cli.stdout.write(
            chalk.bold('Watching...') + ' (press CTRL+C to exit)\n'
        );
    }

    run(cli, function (err, success) {
        chalk.enabled = enabled;

        done(err, success);

        /*
         * Exit when not-watching, or when an error
         * has occurred.
         */

        if (err || !cli.watch) {
            return;
        }

        /*
         * Trigger warning when files need to be cached.
         */

        if (cli.cache.length) {
            cli.stderr.write(
                chalk.yellow('Warning') + ': remark does not overwrite ' +
                'watched files until exit.\nMessages and other files are ' +
                'not affected.\n'
            );
        }

        /*
         * Watch files source-locations of files in
         * the file-set.
         */

        watcher = chokidar.watch(cli.fileSet.sourcePaths, {
            'ignoreInitial': true
        }).on('all', function (type, filePath) {
            if (type === 'add' || type === 'change') {
                cli.globs = [filePath];

                run(cli, done);
            }
        }).on('error', done);

        process.on('SIGINT', function () {
            cli.cache.writeAll();

            if (watcher) {
                watcher.close();
            }
        });
    });
}

/*
 * Expose.
 */

module.exports = engine;
