'use strict';
const linkManPages = require('./linkManPages');
const linkJsTypeDocs = require('./linkJsTypeDocs');
// handle general body-text replacements
// for example, link man page references to the actual page
module.exports = function parseText(lexed) {
  lexed.forEach(function(tok) {
    if (tok.text && tok.type !== 'code') {
      tok.text = linkManPages(tok.text);
      tok.text = linkJsTypeDocs(tok.text);
    }
  });
};
