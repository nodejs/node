/**
 * @param {table~row[]} rows
 * @param {object} config
 * @returns {table~row[]}
 */
export default (rows, config) => {
  return rows.map((cells) => {
    return cells.map((value, index1) => {
      const column = config.columns[index1];

      return ' '.repeat(column.paddingLeft) + value + ' '.repeat(column.paddingRight);
    });
  });
};
