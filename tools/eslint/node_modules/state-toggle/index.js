/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module state-toggle
 * @fileoverview Enter/exit a state.
 */

'use strict';

/* eslint-env commonjs */

/* Expose. */
module.exports = factory;

/**
 * Construct a state `toggler`: a function which inverses
 * `property` in context based on its current value.
 * The by `toggler` returned function restores that value.
 *
 * @param {string} key - Property to toggle.
 * @param {boolean} state - Default state.
 * @param {Object?} [ctx] - Context object.
 * @return {Function} - Enter.
 */
function factory(key, state, ctx) {
  /**
   * Enter a state.
   *
   * @return {Function} - Exit state.
   */
  return function () {
    var context = ctx || this;
    var current = context[key];

    context[key] = !state;

    /**
     * Cancel state to its value before entering.
     */
    return function () {
      context[key] = current;
    };
  };
}
