// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var fs = require('fs');
var marked = require('marked');
var path = require('path');

module.exports = toHTML;

function toHTML(input, filename, template, cb) {
  var lexed = marked.lexer(input);
  fs.readFile(template, 'utf8', function(er, template) {
    if (er) return cb(er);
    render(lexed, filename, template, cb);
  });
}

function render(lexed, filename, template, cb) {
  // get the section
  var section = getSection(lexed);

  filename = path.basename(filename, '.markdown');

  lexed = parseLists(lexed);

  // generate the table of contents.
  // this mutates the lexed contents in-place.
  buildToc(lexed, filename, function(er, toc) {
    if (er) return cb(er);

    template = template.replace(/__FILENAME__/g, filename);
    template = template.replace(/__SECTION__/g, section);
    template = template.replace(/__VERSION__/g, process.env.NODE_DOC_VERSION);
    template = template.replace(/__TOC__/g, toc);

    // content has to be the last thing we do with
    // the lexed tokens, because it's destructive.
    content = marked.parser(lexed);
    template = template.replace(/__CONTENT__/g, content);

    cb(null, template);
  });
}


// just update the list item text in-place.
// lists that come right after a heading are what we're after.
function parseLists(input) {
  var state = null;
  var depth = 0;
  var output = [];
  output.links = input.links;
  input.forEach(function(tok) {
    if (tok.type === 'code' && tok.text.match(/Stability:.*/g)) {
      tok.text = parseAPIHeader(tok.text);
      output.push({ type: 'html', text: tok.text });
      return;
    }
    if (state === null) {
      if (tok.type === 'heading') {
        state = 'AFTERHEADING';
      }
      output.push(tok);
      return;
    }
    if (state === 'AFTERHEADING') {
      if (tok.type === 'list_start') {
        state = 'LIST';
        if (depth === 0) {
          output.push({ type:'html', text: '<div class="signature">' });
        }
        depth++;
        output.push(tok);
        return;
      }
      state = null;
      output.push(tok);
      return;
    }
    if (state === 'LIST') {
      if (tok.type === 'list_start') {
        depth++;
        output.push(tok);
        return;
      }
      if (tok.type === 'list_end') {
        depth--;
        if (depth === 0) {
          state = null;
          output.push({ type:'html', text: '</div>' });
        }
        output.push(tok);
        return;
      }
      if (tok.text) {
        tok.text = parseListItem(tok.text);
      }
    }
    output.push(tok);
  });

  return output;
}


function parseListItem(text) {
  var parts = text.split('`');
  var i;

  for (i = 0; i < parts.length; i += 2) {
    parts[i] = parts[i].replace(/\{([^\}]+)\}/, '<span class="type">$1</span>');
  }

  //XXX maybe put more stuff here?
  return parts.join('`');
}

function parseAPIHeader(text) {
  text = text.replace(/(.*:)\s(\d)([\s\S]*)/,
                      '<pre class="api_stability_$2">$1 $2$3</pre>');
  return text;
}

// section is just the first heading
function getSection(lexed) {
  var section = '';
  for (var i = 0, l = lexed.length; i < l; i++) {
    var tok = lexed[i];
    if (tok.type === 'heading') return tok.text;
  }
  return '';
}


function buildToc(lexed, filename, cb) {
  var indent = 0;
  var toc = [];
  var depth = 0;
  lexed.forEach(function(tok) {
    if (tok.type !== 'heading') return;
    if (tok.depth - depth > 1) {
      return cb(new Error('Inappropriate heading level\n' +
                          JSON.stringify(tok)));
    }

    depth = tok.depth;
    var id = getId(filename + '_' + tok.text.trim());
    toc.push(new Array((depth - 1) * 2 + 1).join(' ') +
             '* <a href="#' + id + '">' +
             tok.text + '</a>');
    tok.text += '<span><a class="mark" href="#' + id + '" ' +
                'id="' + id + '">#</a></span>';
  });

  toc = marked.parse(toc.join('\n'));
  cb(null, toc);
}

var idCounters = {};
function getId(text) {
  text = text.toLowerCase();
  text = text.replace(/[^a-z0-9]+/g, '_');
  text = text.replace(/^_+|_+$/, '');
  text = text.replace(/^([^a-z])/, '_$1');
  if (idCounters.hasOwnProperty(text)) {
    text += '_' + (++idCounters[text]);
  } else {
    idCounters[text] = 0;
  }
  return text;
}

