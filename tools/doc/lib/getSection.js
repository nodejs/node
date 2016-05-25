'use strict';
// section is just the first heading
module.exports = function getSection(lexed) {
  for (var i = 0, l = lexed.length; i < l; i++) {
    var tok = lexed[i];
    if (tok.type === 'heading') return tok.text;
  }
  return '';
};
