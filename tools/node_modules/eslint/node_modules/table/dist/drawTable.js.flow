import {
  drawBorderTop, drawBorderJoin, drawBorderBottom,
} from './drawBorder';
import drawRow from './drawRow';

/**
 * @param {string[][]} rows
 * @param {Array} columnSizeIndex
 * @param {Array} rowSpanIndex
 * @param {table~config} config
 * @returns {string}
 */
export default (
  rows,
  columnSizeIndex,
  rowSpanIndex,
  config,
) => {
  const {
    drawHorizontalLine,
    singleLine,
  } = config;

  let output;
  let realRowIndex;
  let rowHeight;

  const rowCount = rows.length;

  realRowIndex = 0;

  output = '';

  if (drawHorizontalLine(realRowIndex, rowCount)) {
    output += drawBorderTop(columnSizeIndex, config);
  }

  rows.forEach((row, index0) => {
    output += drawRow(row, config);

    if (!rowHeight) {
      rowHeight = rowSpanIndex[realRowIndex];

      realRowIndex++;
    }

    rowHeight--;

    if (
      !singleLine &&
      rowHeight === 0 &&
      index0 !== rowCount - 1 &&
      drawHorizontalLine(realRowIndex, rowCount)
    ) {
      output += drawBorderJoin(columnSizeIndex, config);
    }
  });

  if (drawHorizontalLine(realRowIndex, rowCount)) {
    output += drawBorderBottom(columnSizeIndex, config);
  }

  return output;
};
