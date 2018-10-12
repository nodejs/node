import _ from 'lodash';
import stringWidth from 'string-width';
import wrapWord from './wrapWord';

/**
 * @param {string} value
 * @param {number} columnWidth
 * @param {boolean} useWrapWord
 * @returns {number}
 */
export default (value, columnWidth, useWrapWord = false) => {
  if (!_.isString(value)) {
    throw new TypeError('Value must be a string.');
  }

  if (!Number.isInteger(columnWidth)) {
    throw new TypeError('Column width must be an integer.');
  }

  if (columnWidth < 1) {
    throw new Error('Column width must be greater than 0.');
  }

  if (useWrapWord) {
    return wrapWord(value, columnWidth).length;
  }

  return Math.ceil(stringWidth(value) / columnWidth);
};
