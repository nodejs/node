'use strict';
module.exports = function toID(filename) {
  return filename
    .replace('.html', '')
    .replace(/[^\w\-]/g, '-')
    .replace(/-+/g, '-');
};
