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
const typeParser = require('./type-parser.js');

module.exports = toHTML;

// Make `marked` to not automatically insert id attributes in headings.
const renderer = new marked.Renderer();
renderer.heading = (text, level) => `<h${level}>${text}</h${level}>\n`;
marked.setOptions({ renderer });

const docPath = path.resolve(__dirname, '..', '..', 'doc');

const gtocPath = path.join(docPath, 'api', '_toc.md');
const gtocMD = fs.readFileSync(gtocPath, 'utf8').replace(/^@\/\/.*$/gm, '');
const gtocHTML = marked(gtocMD).replace(
  /<a href="(.*?)"/g,
  (all, href) => `<a class="nav-${href.replace('.html', '')
                                      .replace(/\W+/g, '-')}" href="${href}"`
);

const templatePath = path.join(docPath, 'template.html');
const template = fs.readFileSync(templatePath, 'utf8');

function toHTML({ input, filename, nodeVersion, analytics }, cb) {
  filename = path.basename(filename, '.md');

  const lexed = marked.lexer(input);

  const firstHeading = lexed.find(({ type }) => type === 'heading');
  const section = firstHeading ? firstHeading.text : 'Index';

  preprocessText(lexed);
  preprocessElements(lexed, filename);

  // Generate the table of contents. This mutates the lexed contents in-place.
  const toc = buildToc(lexed, filename);

  const id = filename.replace(/\W+/g, '-');

  let HTML = template.replace('__ID__', id)
                     .replace(/__FILENAME__/g, filename)
                     .replace('__SECTION__', section)
                     .replace(/__VERSION__/g, nodeVersion)
                     .replace('__TOC__', toc)
                     .replace('__GTOC__', gtocHTML.replace(
                       `class="nav-${id}`, `class="nav-${id} active`));

  if (analytics) {
    HTML = HTML.replace('<!-- __TRACKING__ -->', `
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
    </script>`);
  }

  const docCreated = input.match(
    /<!--\s*introduced_in\s*=\s*v([0-9]+)\.([0-9]+)\.[0-9]+\s*-->/);
  if (docCreated) {
    HTML = HTML.replace('__ALTDOCS__', altDocs(filename, docCreated));
  } else {
    console.error(`Failed to add alternative version links to ${filename}`);
    HTML = HTML.replace('__ALTDOCS__', '');
  }

  // Content insertion has to be the last thing we do with the lexed tokens,
  // because it's destructive.
  HTML = HTML.replace('__CONTENT__', marked.parser(lexed));

  cb(null, HTML);
}

// Handle general body-text replacements.
// For example, link man page references to the actual page.
function preprocessText(lexed) {
  lexed.forEach((token) => {
    if (token.type === 'table') {
      if (token.header) {
        token.header = token.header.map(replaceInText);
      }

      if (token.cells) {
        token.cells.forEach((row, i) => {
          token.cells[i] = row.map(replaceInText);
        });
      }
    } else if (token.text && token.type !== 'code') {
      token.text = replaceInText(token.text);
    }
  });
}

// Replace placeholders in text tokens.
function replaceInText(text) {
  if (text === '') return text;
  return linkJsTypeDocs(linkManPages(text));
}

// Syscalls which appear in the docs, but which only exist in BSD / macOS.
const BSD_ONLY_SYSCALLS = new Set(['lchmod']);
const MAN_PAGE = /(^|\s)([a-z.]+)\((\d)([a-z]?)\)/gm;

// Handle references to man pages, eg "open(2)" or "lchmod(2)".
// Returns modified text, with such refs replaced with HTML links, for example
// '<a href="http://man7.org/linux/man-pages/man2/open.2.html">open(2)</a>'.
function linkManPages(text) {
  return text.replace(
    MAN_PAGE, (match, beginning, name, number, optionalCharacter) => {
      // Name consists of lowercase letters,
      // number is a single digit with an optional lowercase letter.
      const displayAs = `<code>${name}(${number}${optionalCharacter})</code>`;

      if (BSD_ONLY_SYSCALLS.has(name)) {
        return `${beginning}<a href="https://www.freebsd.org/cgi/man.cgi` +
          `?query=${name}&sektion=${number}">${displayAs}</a>`;
      }
      return `${beginning}<a href="http://man7.org/linux/man-pages/man${number}` +
        `/${name}.${number}${optionalCharacter}.html">${displayAs}</a>`;
    });
}

const TYPE_SIGNATURE = /\{[^}]+\}/g;
function linkJsTypeDocs(text) {
  const parts = text.split('`');

  // Handle types, for example the source Markdown might say
  // "This argument should be a {number} or {string}".
  for (let i = 0; i < parts.length; i += 2) {
    const typeMatches = parts[i].match(TYPE_SIGNATURE);
    if (typeMatches) {
      typeMatches.forEach((typeMatch) => {
        parts[i] = parts[i].replace(typeMatch, typeParser.toLink(typeMatch));
      });
    }
  }

  return parts.join('`');
}

// Preprocess stability blockquotes and YAML blocks.
function preprocessElements(lexed, filename) {
  const STABILITY_RE = /(.*:)\s*(\d)([\s\S]*)/;
  let state = null;
  let headingIndex = -1;
  let heading = null;

  lexed.forEach((token, index) => {
    if (token.type === 'heading') {
      headingIndex = index;
      heading = token;
    }
    if (token.type === 'html' && common.isYAMLBlock(token.text)) {
      token.text = parseYAML(token.text);
    }
    if (token.type === 'blockquote_start') {
      state = 'MAYBE_STABILITY_BQ';
      lexed[index] = { type: 'space' };
    }
    if (token.type === 'blockquote_end' && state === 'MAYBE_STABILITY_BQ') {
      state = null;
      lexed[index] = { type: 'space' };
    }
    if (token.type === 'paragraph' && state === 'MAYBE_STABILITY_BQ') {
      if (token.text.includes('Stability:')) {
        const [, prefix, number, explication] = token.text.match(STABILITY_RE);
        const isStabilityIndex =
          index - 2 === headingIndex || // General.
          index - 3 === headingIndex;   // With api_metadata block.

        if (heading && isStabilityIndex) {
          heading.stability = number;
          headingIndex = -1;
          heading = null;
        }

        // Do not link to the section we are already in.
        const noLinking = filename === 'documentation' &&
          heading !== null && heading.text === 'Stability Index';
        token.text = `<div class="api_stability api_stability_${number}">` +
          (noLinking ? '' :
            '<a href="documentation.html#documentation_stability_index">') +
          `${prefix} ${number}${noLinking ? '' : '</a>'}${explication}</div>`
          .replace(/\n/g, ' ');

        lexed[index] = { type: 'html', text: token.text };
      } else if (state === 'MAYBE_STABILITY_BQ') {
        state = null;
        lexed[index - 1] = { type: 'blockquote_start' };
      }
    }
  });
}

function parseYAML(text) {
  const meta = common.extractAndParseYAML(text);
  let html = '<div class="api_metadata">\n';

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
    if (added.description) meta.changes.push(added);
    if (deprecated.description) meta.changes.push(deprecated);

    meta.changes.sort((a, b) => versionSort(a.version, b.version));

    html += '<details class="changelog"><summary>History</summary>\n' +
            '<table>\n<tr><th>Version</th><th>Changes</th></tr>\n';

    meta.changes.forEach((change) => {
      html += `<tr><td>${change.version}</td>\n` +
                  `<td>${marked(change.description)}</td></tr>\n`;
    });

    html += '</table>\n</details>\n';
  } else {
    html += `${added.description}${deprecated.description}\n`;
  }

  html += '</div>';
  return html;
}

const numberRe = /^\d*/;
function versionSort(a, b) {
  a = a.trim();
  b = b.trim();
  let i = 0; // Common prefix length.
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  a = a.substr(i);
  b = b.substr(i);
  return +b.match(numberRe)[0] - +a.match(numberRe)[0];
}

function buildToc(lexed, filename) {
  const startIncludeRefRE = /^\s*<!-- \[start-include:(.+)\] -->\s*$/;
  const endIncludeRefRE = /^\s*<!-- \[end-include:.+\] -->\s*$/;
  const realFilenames = [filename];
  const idCounters = Object.create(null);
  let toc = '';
  let depth = 0;

  lexed.forEach((token) => {
    // Keep track of the current filename along comment wrappers of inclusions.
    if (token.type === 'html') {
      const [, includedFileName] = token.text.match(startIncludeRefRE) || [];
      if (includedFileName !== undefined)
        realFilenames.unshift(includedFileName);
      else if (endIncludeRefRE.test(token.text))
        realFilenames.shift();
    }

    if (token.type !== 'heading') return;

    if (token.depth - depth > 1) {
      throw new Error(`Inappropriate heading level:\n${JSON.stringify(token)}`);
    }

    depth = token.depth;
    const realFilename = path.basename(realFilenames[0], '.md');
    const headingText = token.text.trim();
    const id = getId(`${realFilename}_${headingText}`, idCounters);

    const hasStability = token.stability !== undefined;
    toc += ' '.repeat((depth - 1) * 2) +
      (hasStability ? `* <span class="stability_${token.stability}">` : '* ') +
      `<a href="#${id}">${token.text}</a>${hasStability ? '</span>' : ''}\n`;

    token.text += `<span><a class="mark" href="#${id}" id="${id}">#</a></span>`;
    if (realFilename === 'errors' && headingText.startsWith('ERR_')) {
      token.text += `<span><a class="mark" href="#${headingText}" ` +
                                             `id="${headingText}">#</a></span>`;
    }
  });

  return marked(toc);
}

const notAlphaNumerics = /[^a-z0-9]+/g;
const edgeUnderscores = /^_+|_+$/g;
const notAlphaStart = /^[^a-z]/;
function getId(text, idCounters) {
  text = text.toLowerCase()
             .replace(notAlphaNumerics, '_')
             .replace(edgeUnderscores, '')
             .replace(notAlphaStart, '_$&');
  if (idCounters[text] !== undefined) {
    return `${text}_${++idCounters[text]}`;
  }
  idCounters[text] = 0;
  return text;
}

function altDocs(filename, docCreated) {
  const [, docCreatedMajor, docCreatedMinor] = docCreated.map(Number);
  const host = 'https://nodejs.org';
  const versions = [
    { num: '10.x' },
    { num: '9.x' },
    { num: '8.x', lts: true },
    { num: '7.x' },
    { num: '6.x', lts: true },
    { num: '5.x' },
    { num: '4.x' },
    { num: '0.12.x' },
    { num: '0.10.x' }
  ];

  const getHref = (versionNum) =>
    `${host}/docs/latest-v${versionNum}/api/${filename}.html`;

  const wrapInListItem = (version) =>
    `<li><a href="${getHref(version.num)}">${version.num}` +
    `${version.lts ? ' <b>LTS</b>' : ''}</a></li>`;

  function isDocInVersion(version) {
    const [versionMajor, versionMinor] = version.num.split('.').map(Number);
    if (docCreatedMajor > versionMajor) return false;
    if (docCreatedMajor < versionMajor) return true;
    return docCreatedMinor <= versionMinor;
  }

  const list = versions.filter(isDocInVersion).map(wrapInListItem).join('\n');

  return list ? `
    <li class="version-picker">
      <a href="#">View another version <span>&#x25bc;</span></a>
      <ol class="version-picker">${list}</ol>
    </li>
  ` : '';
}
