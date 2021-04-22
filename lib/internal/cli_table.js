'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  MathCeil,
  MathMax,
  MathMaxApply,
  ObjectPrototypeHasOwnProperty,
  StringPrototypeRepeat,
} = primordials;

const { getStringWidth } = require('internal/util/inspect');

// The use of Unicode characters below is the only non-comment use of non-ASCII
// Unicode characters in Node.js built-in modules. If they are ever removed or
// rewritten with \u escapes, then a test will need to be (re-)added to Node.js
// core to verify that Unicode characters work in built-ins.
// Refs: https://github.com/nodejs/node/issues/10673
const tableChars = {
  /* eslint-disable node-core/non-ascii-character */
  middleMiddle: '─',
  rowMiddle: '┼',
  topRight: '┐',
  topLeft: '┌',
  leftMiddle: '├',
  topMiddle: '┬',
  bottomRight: '┘',
  bottomLeft: '└',
  bottomMiddle: '┴',
  rightMiddle: '┤',
  left: '│ ',
  right: ' │',
  middle: ' │ ',
  /* eslint-enable node-core/non-ascii-character */
};

const renderRow = (row, columnWidths) => {
  let out = tableChars.left;
  for (let i = 0; i < row.length; i++) {
    const cell = row[i];
    const len = getStringWidth(cell);
    const needed = (columnWidths[i] - len) / 2;
    // round(needed) + ceil(needed) will always add up to the amount
    // of spaces we need while also left justifying the output.
    out += StringPrototypeRepeat(' ', needed) + cell +
           StringPrototypeRepeat(' ', MathCeil(needed));
    if (i !== row.length - 1)
      out += tableChars.middle;
  }
  out += tableChars.right;
  return out;
};

const table = (head, columns) => {
  const rows = [];
  const columnWidths = ArrayPrototypeMap(head, (h) => getStringWidth(h));
  const longestColumn = MathMaxApply(ArrayPrototypeMap(columns, (a) =>
    a.length));

  for (let i = 0; i < head.length; i++) {
    const column = columns[i];
    for (let j = 0; j < longestColumn; j++) {
      if (rows[j] === undefined)
        rows[j] = [];
      const value = rows[j][i] =
        ObjectPrototypeHasOwnProperty(column, j) ? column[j] : '';
      const width = columnWidths[i] || 0;
      const counted = getStringWidth(value);
      columnWidths[i] = MathMax(width, counted);
    }
  }

  const divider = ArrayPrototypeMap(columnWidths, (i) =>
    StringPrototypeRepeat(tableChars.middleMiddle, i + 2));

  let result = tableChars.topLeft +
               ArrayPrototypeJoin(divider, tableChars.topMiddle) +
               tableChars.topRight + '\n' +
               renderRow(head, columnWidths) + '\n' +
               tableChars.leftMiddle +
               ArrayPrototypeJoin(divider, tableChars.rowMiddle) +
               tableChars.rightMiddle + '\n';

  for (const row of rows)
    result += `${renderRow(row, columnWidths)}\n`;

  result += tableChars.bottomLeft +
            ArrayPrototypeJoin(divider, tableChars.bottomMiddle) +
            tableChars.bottomRight;

  return result;
};

module.exports = table;
