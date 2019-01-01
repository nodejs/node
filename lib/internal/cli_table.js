'use strict';

const { Buffer } = require('buffer');
const { removeColors } = require('internal/util');
const HasOwnProperty = Function.call.bind(Object.prototype.hasOwnProperty);

// The use of Unicode characters below is the only non-comment use of non-ASCII
// Unicode characters in Node.js built-in modules. If they are ever removed or
// rewritten with \u escapes, then a test will need to be (re-)added to Node.js
// core to verify that Unicode characters work in built-ins. Otherwise,
// consumers using Unicode in _third_party_main.js will run into problems.
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

const countSymbols = (string) => {
  const normalized = removeColors(string).normalize('NFC');
  return Buffer.from(normalized, 'UCS-2').byteLength / 2;
};

const renderRow = (row, columnWidths) => {
  let out = tableChars.left;
  for (var i = 0; i < row.length; i++) {
    const cell = row[i];
    const len = countSymbols(cell);
    const needed = (columnWidths[i] - len) / 2;
    // round(needed) + ceil(needed) will always add up to the amount
    // of spaces we need while also left justifying the output.
    out += `${' '.repeat(needed)}${cell}${' '.repeat(Math.ceil(needed))}`;
    if (i !== row.length - 1)
      out += tableChars.middle;
  }
  out += tableChars.right;
  return out;
};

const table = (head, columns) => {
  const rows = [];
  const columnWidths = head.map((h) => countSymbols(h));
  const longestColumn = columns.reduce((n, a) => Math.max(n, a.length), 0);

  for (var i = 0; i < head.length; i++) {
    const column = columns[i];
    for (var j = 0; j < longestColumn; j++) {
      if (rows[j] === undefined)
        rows[j] = [];
      const value = rows[j][i] = HasOwnProperty(column, j) ? column[j] : '';
      const width = columnWidths[i] || 0;
      const counted = countSymbols(value);
      columnWidths[i] = Math.max(width, counted);
    }
  }

  const divider = columnWidths.map((i) =>
    tableChars.middleMiddle.repeat(i + 2));

  let result = `${tableChars.topLeft}${divider.join(tableChars.topMiddle)}` +
               `${tableChars.topRight}\n${renderRow(head, columnWidths)}\n` +
               `${tableChars.leftMiddle}${divider.join(tableChars.rowMiddle)}` +
               `${tableChars.rightMiddle}\n`;

  for (const row of rows)
    result += `${renderRow(row, columnWidths)}\n`;

  result += `${tableChars.bottomLeft}${divider.join(tableChars.bottomMiddle)}` +
            tableChars.bottomRight;

  return result;
};

module.exports = table;
