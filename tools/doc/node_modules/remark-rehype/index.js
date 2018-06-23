'use strict';

var mdast2hast = require('mdast-util-to-hast');

module.exports = remark2rehype;

/* Attacher.
 * If a destination is given, runs the destination with
 * the new HAST tree (bridge-mode).
 * Without destination, returns the HAST tree: further
 * plug-ins run on that tree (mutate-mode). */
function remark2rehype(destination, options) {
  if (destination && !destination.process) {
    options = destination;
    destination = null;
  }

  return destination ? bridge(destination, options) : mutate(options);
}

/* Bridge-mode.  Runs the destination with the new HAST
 * tree. */
function bridge(destination, options) {
  return transformer;
  function transformer(node, file, next) {
    destination.run(mdast2hast(node, options), file, done);
    function done(err) {
      next(err);
    }
  }
}

/* Mutate-mode.  Further transformers run on the HAST tree. */
function mutate(options) {
  return transformer;
  function transformer(node) {
    return mdast2hast(node, options);
  }
}
