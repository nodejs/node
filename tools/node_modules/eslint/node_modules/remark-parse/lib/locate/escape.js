/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:locate:escape
 * @fileoverview Locate an escape.
 */

'use strict';

module.exports = locate;

function locate(value, fromIndex) {
  return value.indexOf('\\', fromIndex);
}
