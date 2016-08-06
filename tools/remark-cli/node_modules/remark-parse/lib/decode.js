/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:decode
 * @fileoverview Decode entities.
 */

'use strict';

/* Dependencies. */
var entities = require('parse-entities');

/* Expose. */
module.exports = factory;

/**
 * Factory to create an entity decoder.
 *
 * @param {Object} ctx - Context to attach to.
 * @return {Function} - See `decode`.
 */
function factory(ctx) {
  decoder.raw = decodeRaw;

  return decoder;

  /**
   * Normalize `position` to add an `indent`.
   *
   * @param {Position} position - Reference
   * @return {Position} - Augmented with `indent`.
   */
  function normalize(position) {
    var offsets = ctx.offset;
    var line = position.line;
    var result = [];

    while (++line) {
      if (!(line in offsets)) {
        break;
      }

      result.push((offsets[line] || 0) + 1);
    }

    return {
      start: position,
      indent: result
    };
  }

  /**
   * Handle a warning.
   *
   * @this {VFile} - Virtual file.
   * @param {string} reason - Reason for warning.
   * @param {Position} position - Place of warning.
   * @param {number} code - Code for warning.
   */
  function handleWarning(reason, position, code) {
    if (code === 3) {
      return;
    }

    ctx.file.message(reason, position);
  }

  /**
   * Decode `value` (at `position`) into text-nodes.
   *
   * @param {string} value - Value to parse.
   * @param {Position} position - Position to start parsing at.
   * @param {Function} handler - Node handler.
   */
  function decoder(value, position, handler) {
    entities(value, {
      position: normalize(position),
      warning: handleWarning,
      text: handler,
      reference: handler,
      textContext: ctx,
      referenceContext: ctx
    });
  }

  /**
   * Decode `value` (at `position`) into a string.
   *
   * @param {string} value - Value to parse.
   * @param {Position} position - Position to start
   *   parsing at.
   * @return {string} - Plain-text.
   */
  function decodeRaw(value, position) {
    return entities(value, {
      position: normalize(position),
      warning: handleWarning
    });
  }
}
