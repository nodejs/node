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

'use strict';

const common = require('./common.js');
const fs = require('fs');
const marked = require('marked');
const path = require('path');
const preprocess = require('./preprocess.js');
const typeParser = require('./type-parser.js');

module.exports = toHTML;

const STABILITY_TEXT_REG_EXP = /(.*:)\s*(\d)([\s\S]*)/;
const DOC_CREATED_REG_EXP = /<!--\s*introduced_in\s*=\s*v([0-9]+)\.([0-9]+)\.([0-9]+)\s*-->/;

// customized heading without id attribute
const renderer = new marked.Renderer();
renderer.heading = function(text, level) {
  return `<h${level}>${text}</h${level}>\n`;
};
marked.setOptions({
  renderer: renderer
});

// TODO(chrisdickinson): never stop vomitting / fix this.
const gtocPath = path.resolve(path.join(
  __dirname,
  '..',
  '..',
  'doc',
  'api',
  '_toc.md'
));
var gtocLoading = null;
var gtocData = null;
var docCreated = null;
var nodeVersion = null;

/**
 * opts: input, filename, template, nodeVersion.
 */
function toHTML(opts, cb) {
  const template = opts.template;

  nodeVersion = opts.nodeVersion || process.version;
  docCreated = opts.input.match(DOC_CREATED_REG_EXP);

  if (gtocData) {
    return onGtocLoaded();
  }

  if (gtocLoading === null) {
    gtocLoading = [onGtocLoaded];
    return loadGtoc(function(err, data) {
      if (err) throw err;
      gtocData = data;
      gtocLoading.forEach(function(xs) {
        xs();
      });
    });
  }

  if (gtocLoading) {
    return gtocLoading.push(onGtocLoaded);
  }

  function onGtocLoaded() {
    const lexed = marked.lexer(opts.input);
    fs.readFile(template, 'utf8', function(er, template) {
      if (er) return cb(er);
      render({
        lexed: lexed,
        filename: opts.filename,
        template: template,
        nodeVersion: nodeVersion,
        analytics: opts.analytics,
      }, cb);
    });
  }
}

function loadGtoc(cb) {
  fs.readFile(gtocPath, 'utf8', function(err, data) {
    if (err) return cb(err);

    preprocess(gtocPath, data, function(err, data) {
      if (err) return cb(err);

      data = marked(data).replace(/<a href="(.*?)"/gm, function(a, m) {
        return `<a class="nav-${toID(m)}" href="${m}"`;
      });
      return cb(null, data);
    });
  });
}

function toID(filename) {
  return filename
    .replace('.html', '')
    .replace(/[^\w-]/g, '-')
    .replace(/-+/g, '-');
}

/**
 * opts: lexed, filename, template, nodeVersion.
 */
function render(opts, cb) {
  var { lexed, filename, template } = opts;
  const nodeVersion = opts.nodeVersion || process.version;

  // get the section
  const section = getSection(lexed);

  filename = path.basename(filename, '.md');

  parseText(lexed);
  lexed = parseLists(lexed);

  // generate the table of contents.
  // this mutates the lexed contents in-place.
  buildToc(lexed, filename, function(er, toc) {
    if (er) return cb(er);

    const id = toID(path.basename(filename));

    template = template.replace(/__ID__/g, id);
    template = template.replace(/__FILENAME__/g, filename);
    template = template.replace(/__SECTION__/g, section || 'Index');
    template = template.replace(/__VERSION__/g, nodeVersion);
    template = template.replace(/__TOC__/g, toc);
    template = template.replace(
      /__GTOC__/g,
      gtocData.replace(`class="nav-${id}`, `class="nav-${id} active`)
    );

    if (opts.analytics) {
      template = template.replace(
        '<!-- __TRACKING__ -->',
        analyticsScript(opts.analytics)
      );
    }

    template = template.replace(/__ALTDOCS__/, altDocs(filename));

    // content has to be the last thing we do with
    // the lexed tokens, because it's destructive.
    const content = marked.parser(lexed);
    template = template.replace(/__CONTENT__/g, content);

    cb(null, template);
  });
}

function analyticsScript(analytics) {
  return `
    <script src="assets/dnt_helper.js"></script>
    <script>
      if (!_dntEnabled()) {
        (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;
        i[r]=i[r]||function(){(i[r].q=i[r].q||[]).push(arguments)},
        i[r].l=1*new Date();a=s.createElement(o),m=s.getElementsByTagName(o)[0];
        a.async=1;a.src=g;m.parentNode.insertBefore(a,m)})(window,document,
        'script','//www.google-analytics.com/analytics.js','ga');
        ga('create', '${analytics}', 'auto');
        ga('send', 'pageview');
      }
    </script>
  `;
}

// replace placeholders in text tokens
function replaceInText(text) {
  return linkJsTypeDocs(linkManPages(text));
}

function altDocs(filename) {
  if (!docCreated) {
    console.error(`Failed to add alternative version links to ${filename}`);
    return '';
  }

  function lte(v) {
    const ns = v.num.split('.');
    if (docCreated[1] > +ns[0])
      return false;
    if (docCreated[1] < +ns[0])
      return true;
    return docCreated[2] <= +ns[1];
  }

  const versions = [
    { num: '9.x' },
    { num: '8.x', lts: true },
    { num: '7.x' },
    { num: '6.x', lts: true },
    { num: '5.x' },
    { num: '4.x', lts: true },
    { num: '0.12.x' },
    { num: '0.10.x' }
  ];

  const host = 'https://nodejs.org';
  const href = (v) => `${host}/docs/latest-v${v.num}/api/${filename}.html`;

  function li(v) {
    let html = `<li><a href="${href(v)}">${v.num}`;

    if (v.lts)
      html += ' <b>LTS</b>';

    return html + '</a></li>';
  }

  const lis = versions.filter(lte).map(li).join('\n');

  if (!lis.length)
    return '';

  return `
    <li class="version-picker">
      <a href="#">View another version <span>&#x25bc;</span></a>
      <ol class="version-picker">${lis}</ol>
    </li>
  `;
}

// handle general body-text replacements
// for example, link man page references to the actual page
function parseText(lexed) {
  lexed.forEach(function(tok) {
    if (tok.type === 'table') {
      if (tok.cells) {
        tok.cells.forEach((row, x) => {
          row.forEach((_, y) => {
            if (tok.cells[x] && tok.cells[x][y]) {
              tok.cells[x][y] = replaceInText(tok.cells[x][y]);
            }
          });
        });
      }

      if (tok.header) {
        tok.header.forEach((_, i) => {
          if (tok.header[i]) {
            tok.header[i] = replaceInText(tok.header[i]);
          }
        });
      }
    } else if (tok.text && tok.type !== 'code') {
      tok.text = replaceInText(tok.text);
    }
  });
}

// just update the list item text in-place.
// lists that come right after a heading are what we're after.
function parseLists(input) {
  var state = null;
  const savedState = [];
  var depth = 0;
  const output = [];
  let headingIndex = -1;
  let heading = null;

  output.links = input.links;
  input.forEach(function(tok, index) {
    if (tok.type === 'blockquote_start') {
      savedState.push(state);
      state = 'MAYBE_STABILITY_BQ';
      return;
    }
    if (tok.type === 'blockquote_end' && state === 'MAYBE_STABILITY_BQ') {
      state = savedState.pop();
      return;
    }
    if ((tok.type === 'paragraph' && state === 'MAYBE_STABILITY_BQ') ||
      tok.type === 'code') {
      if (tok.text.match(/Stability:.*/g)) {
        const stabilityMatch = tok.text.match(STABILITY_TEXT_REG_EXP);
        const stability = Number(stabilityMatch[2]);
        const isStabilityIndex =
          index - 2 === headingIndex || // general
          index - 3 === headingIndex;   // with api_metadata block

        if (heading && isStabilityIndex) {
          heading.stability = stability;
          headingIndex = -1;
          heading = null;
        }
        tok.text = parseAPIHeader(tok.text).replace(/\n/g, ' ');
        output.push({ type: 'html', text: tok.text });
        return;
      } else if (state === 'MAYBE_STABILITY_BQ') {
        output.push({ type: 'blockquote_start' });
        state = savedState.pop();
      }
    }
    if (state === null ||
      (state === 'AFTERHEADING' && tok.type === 'heading')) {
      if (tok.type === 'heading') {
        headingIndex = index;
        heading = tok;
        state = 'AFTERHEADING';
      }
      output.push(tok);
      return;
    }
    if (state === 'AFTERHEADING') {
      if (tok.type === 'list_start') {
        state = 'LIST';
        if (depth === 0) {
          output.push({ type: 'html', text: '<div class="signature">' });
        }
        depth++;
        output.push(tok);
        return;
      }
      if (tok.type === 'html' && common.isYAMLBlock(tok.text)) {
        tok.text = parseYAML(tok.text);
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
        output.push(tok);
        if (depth === 0) {
          state = null;
          output.push({ type: 'html', text: '</div>' });
        }
        return;
      }
    }
    output.push(tok);
  });

  return output;
}

function parseYAML(text) {
  const meta = common.extractAndParseYAML(text);
  const html = ['<div class="api_metadata">'];

  const added = { description: '' };
  const deprecated = { description: '' };

  if (meta.added) {
    added.version = meta.added.join(', ');
    added.description = `<span>Added in: ${added.version}</span>`;
  }

  if (meta.deprecated) {
    deprecated.version = meta.deprecated.join(', ');
    deprecated.description =
        `<span>Deprecated since: ${deprecated.version}</span>`;
  }

  if (meta.changes.length > 0) {
    let changes = meta.changes.slice();
    if (added.description) changes.push(added);
    if (deprecated.description) changes.push(deprecated);

    changes = changes.sort((a, b) => versionSort(a.version, b.version));

    html.push('<details class="changelog"><summary>History</summary>');
    html.push('<table>');
    html.push('<tr><th>Version</th><th>Changes</th></tr>');

    changes.forEach((change) => {
      html.push(`<tr><td>${change.version}</td>`);
      html.push(`<td>${marked(change.description)}</td></tr>`);
    });

    html.push('</table>');
    html.push('</details>');
  } else {
    html.push(`${added.description}${deprecated.description}`);
  }

  html.push('</div>');
  return html.join('\n');
}

// Syscalls which appear in the docs, but which only exist in BSD / OSX
const BSD_ONLY_SYSCALLS = new Set(['lchmod']);

// Handle references to man pages, eg "open(2)" or "lchmod(2)"
// Returns modified text, with such refs replace with HTML links, for example
// '<a href="http://man7.org/linux/man-pages/man2/open.2.html">open(2)</a>'
function linkManPages(text) {
  return text.replace(
    /(^|\s)([a-z.]+)\((\d)([a-z]?)\)/gm,
    (match, beginning, name, number, optionalCharacter) => {
      // name consists of lowercase letters, number is a single digit
      const displayAs = `${name}(${number}${optionalCharacter})`;
      if (BSD_ONLY_SYSCALLS.has(name)) {
        return `${beginning}<a href="https://www.freebsd.org/cgi/man.cgi?query=${name}` +
          `&sektion=${number}">${displayAs}</a>`;
      } else {
        return `${beginning}<a href="http://man7.org/linux/man-pages/man${number}` +
          `/${name}.${number}${optionalCharacter}.html">${displayAs}</a>`;
      }
    });
}

function linkJsTypeDocs(text) {
  const parts = text.split('`');
  var i;
  var typeMatches;

  // Handle types, for example the source Markdown might say
  // "This argument should be a {Number} or {String}"
  for (i = 0; i < parts.length; i += 2) {
    typeMatches = parts[i].match(/\{([^}]+)\}/g);
    if (typeMatches) {
      typeMatches.forEach(function(typeMatch) {
        parts[i] = parts[i].replace(typeMatch, typeParser.toLink(typeMatch));
      });
    }
  }

  //XXX maybe put more stuff here?
  return parts.join('`');
}

function parseAPIHeader(text) {
  const classNames = 'api_stability api_stability_$2';
  const docsUrl = 'documentation.html#documentation_stability_index';

  text = text.replace(
    STABILITY_TEXT_REG_EXP,
    `<div class="${classNames}"><a href="${docsUrl}">$1 $2</a>$3</div>`
  );
  return text;
}

// section is just the first heading
function getSection(lexed) {
  for (var i = 0, l = lexed.length; i < l; i++) {
    var tok = lexed[i];
    if (tok.type === 'heading') return tok.text;
  }
  return '';
}

function getMark(anchor) {
  return `<span><a class="mark" href="#${anchor}" id="${anchor}">#</a></span>`;
}

function buildToc(lexed, filename, cb) {
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
    const apiName = tok.text.trim();
    const id = getId(`${realFilename}_${apiName}`);
    toc.push(new Array((depth - 1) * 2 + 1).join(' ') +
             `* <span class="stability_${tok.stability}">` +
             `<a href="#${id}">${tok.text}</a></span>`);
    tok.text += getMark(id);
    if (realFilename === 'errors' && apiName.startsWith('ERR_')) {
      tok.text += getMark(apiName);
    }
  });

  toc = marked.parse(toc.join('\n'));
  cb(null, toc);
}

const idCounters = {};
function getId(text) {
  text = text.toLowerCase();
  text = text.replace(/[^a-z0-9]+/g, '_');
  text = text.replace(/^_+|_+$/, '');
  text = text.replace(/^([^a-z])/, '_$1');
  if (idCounters.hasOwnProperty(text)) {
    text += `_${++idCounters[text]}`;
  } else {
    idCounters[text] = 0;
  }
  return text;
}

const numberRe = /^(\d*)/;
function versionSort(a, b) {
  a = a.trim();
  b = b.trim();
  let i = 0;  // common prefix length
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  a = a.substr(i);
  b = b.substr(i);
  return +b.match(numberRe)[1] - +a.match(numberRe)[1];
}
