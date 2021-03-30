import truncate from 'lodash.truncate';

/**
 * @todo Make it work with ASCII content.
 * @param {table~row[]} rows
 * @param {object} config
 * @returns {table~row[]}
 */
export default (rows, config) => {
  return rows.map((cells) => {
    return cells.map((content, index) => {
      return truncate(content, {
        length: config.columns[index].truncate,
      });
    });
  });
};
