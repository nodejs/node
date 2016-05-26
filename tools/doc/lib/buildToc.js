'use strict';
const marked = require('marked');
const getId = require('./getId.js');

module.exports = function buildToc(lexed, filename, cb) {
  var toc = [];
  var depth = 0;

  const startIncludeRefRE = /^\s*<!-- \[start-include:(.+)\] -->\s*$/;
  const endIncludeRefRE = /^\s*<!-- \[end-include:(.+)\] -->\s*$/;
  const realFilenames = [filename];

  lexed.forEach(function(tok) {
    // Keep track of the current filename along @include directives.
    if (tok.type === 'html') {
      let match;
      if ((match = tok.text.match(startIncludeRefRE)) !== null)
        realFilenames.unshift(match[1]);
      else if (tok.text.match(endIncludeRefRE))
        realFilenames.shift();
    }

    if (tok.type !== 'heading') return;
    if (tok.depth - depth > 1) {
      return cb(new Error('Inappropriate heading level\n' +
                          JSON.stringify(tok)));
    }

    depth = tok.depth;
    const realFilename = path.basename(realFilenames[0], '.md');
    const id = getId(realFilename + '_' + tok.text.trim());
    toc.push(new Array((depth - 1) * 2 + 1).join(' ') +
             '* <a href="#' + id + '">' +
             tok.text + '</a>');
    tok.text += '<span><a class="mark" href="#' + id + '" ' +
                'id="' + id + '">#</a></span>';
  });

  toc = marked.parse(toc.join('\n'));
  cb(null, toc);
};
