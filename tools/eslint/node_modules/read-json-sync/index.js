/*!
 * read-json-sync | MIT (c) Shinnosuke Watanabe
 * https://github.com/shinnn/read-json-sync
*/
'use strict';

var fs = require('graceful-fs');

module.exports = function readJsonSync(filePath, options) {
  return JSON.parse(String(fs.readFileSync(filePath, options)).replace(/^\ufeff/g, ''));
};
