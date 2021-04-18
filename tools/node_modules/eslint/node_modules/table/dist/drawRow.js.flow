import drawHorizontalContent from './drawHorizontalContent';

/**
 * @typedef {object} drawRow~border
 * @property {string} bodyLeft
 * @property {string} bodyRight
 * @property {string} bodyJoin
 */

/**
 * @param {string[]} columns
 * @param {object} config
 * @param {border} config.border
 * @param {Function} config.drawVerticalLine
 * @returns {string}
 */
export default (columns, {
  border,
  drawVerticalLine,
}) => {
  return drawHorizontalContent(columns, {
    drawVerticalLine,
    separator: {
      join: border.bodyJoin,
      left: border.bodyLeft,
      right: border.bodyRight,
    },
  });
};
