/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline
 * @fileoverview Process a file.
 */

'use strict';

/* Dependencies. */
var trough = require('trough');
var read = require('./read');
var configure = require('./configure');
var parse = require('./parse');
var transform = require('./transform');
var queue = require('./queue');
var stringify = require('./stringify');
var copy = require('./copy');
var stdout = require('./stdout');
var fileSystem = require('./file-system');

/* Expose: This pipeline ensures each of the pipes
 * always runs: even if the read pipe fails,
 * queue and write trigger. */
module.exports = trough()
  .use(chunk(trough().use(read).use(configure).use(parse).use(transform)))
  .use(chunk(trough().use(queue)))
  .use(chunk(trough().use(stringify).use(copy).use(stdout).use(fileSystem)));

/**
 * Factory to run a pipe. Wraps a pipe to trigger an
 * error on the `file` in `context`, but still call
 * `next`.
 *
 * @param {Trough} trough - Middelware.
 * @return {function(Object, Function)} - Runner.
 */
function chunk(pipe) {
  return run;

  /** Run the bound bound pipe and handles any errors. */
  function run(context, file, fileSet, done) {
    pipe.run(context, file, fileSet, function (err) {
      var messages = file.messages;
      var pos;

      if (err) {
        pos = messages.indexOf(err);

        if (pos === -1) {
          err = file.message(err);
          pos = messages.length - 1;
        }

        messages[pos].fatal = true;
      }

      done();
    });
  }
}
