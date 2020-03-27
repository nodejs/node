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
const unified = require('unified');
const find = require('unist-util-find');
const visit = require('unist-util-visit');
const markdown = require('remark-parse');
const remark2rehype = require('remark-rehype');
const raw = require('rehype-raw');
const htmlStringify = require('rehype-stringify');
const path = require('path');
const typeParser = require('./type-parser.js');

module.exports = {
  toHTML, firstHeader, preprocessText, preprocessElements, buildToc
};

const docPath = path.resolve(__dirname, '..', '..', 'doc');

// Add class attributes to index navigation links.
function navClasses() {
  return (tree) => {
    visit(tree, { type: 'element', tagName: 'a' }, (node) => {
      node.properties.class = 'nav-' +
        node.properties.href.replace('.html', '').replace(/\W+/g, '-');
    });
  };
}

const gtocPath = path.join(docPath, 'api', 'index.md');
const gtocMD = fs.readFileSync(gtocPath, 'utf8').replace(/^<!--.*?-->/gms, '');
const gtocHTML = unified()
  .use(markdown)
  .use(remark2rehype, { allowDangerousHTML: true })
  .use(raw)
  .use(navClasses)
  .use(htmlStringify)
  .processSync(gtocMD).toString();

const templatePath = path.join(docPath, 'template.html');
const template = fs.readFileSync(templatePath, 'utf8');

function toHTML({ input, content, filename, nodeVersion, versions }) {
  filename = path.basename(filename, '.md');

  const id = filename.replace(/\W+/g, '-');

  let HTML = template.replace('__ID__', id)
                     .replace(/__FILENAME__/g, filename)
                     .replace('__SECTION__', content.section)
                     .replace(/__VERSION__/g, nodeVersion)
                     .replace('__TOC__', content.toc)
                     .replace('__GTOC__', gtocHTML.replace(
                       `class="nav-${id}`, `class="nav-${id} active`))
                     .replace('__EDIT_ON_GITHUB__', editOnGitHub(filename))
                     .replace('__CONTENT__', content.toString());

  const docCreated = input.match(
    /<!--\s*introduced_in\s*=\s*v([0-9]+)\.([0-9]+)\.[0-9]+\s*-->/);
  if (docCreated) {
    HTML = HTML.replace('__ALTDOCS__', altDocs(filename, docCreated, versions));
  } else {
    console.error(`Failed to add alternative version links to ${filename}`);
    HTML = HTML.replace('__ALTDOCS__', '');
  }

  return HTML;
}

// Set the section name based on the first header.  Default to 'Index'.
function firstHeader() {
  return (tree, file) => {
    file.section = 'Index';

    const heading = find(tree, { type: 'heading' });
    if (heading) {
      const text = find(heading, { type: 'text' });
      if (text) file.section = text.value;
    }
  };
}

// Handle general body-text replacements.
// For example, link man page references to the actual page.
function preprocessText() {
  return (tree) => {
    visit(tree, null, (node) => {
      if (node.type === 'text' && node.value) {
        const value = linkJsTypeDocs(linkManPages(node.value));
        if (value !== node.value) {
          node.type = 'html';
          node.value = value;
        }
      }
    });
  };
}

// Syscalls which appear in the docs, but which only exist in BSD / macOS.
const BSD_ONLY_SYSCALLS = new Set(['lchmod']);
const LINUX_DIE_ONLY_SYSCALLS = new Set(['uname']);
const HAXX_ONLY_SYSCALLS = new Set(['curl']);
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
      if (LINUX_DIE_ONLY_SYSCALLS.has(name)) {
        return `${beginning}<a href="https://linux.die.net/man/` +
                `${number}/${name}">${displayAs}</a>`;
      }
      if (HAXX_ONLY_SYSCALLS.has(name)) {
        return `${beginning}<a href="https://${name}.haxx.se/docs/manpage.html">${displayAs}</a>`;
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

// Preprocess headers, stability blockquotes, and YAML blocks.
function preprocessElements({ filename }) {
  return (tree) => {
    const STABILITY_RE = /(.*:)\s*(\d)([\s\S]*)/;
    let headingIndex = -1;
    let heading = null;

    visit(tree, null, (node, index) => {
      if (node.type === 'heading') {
        headingIndex = index;
        heading = node;
      } else if (node.type === 'html' && common.isYAMLBlock(node.value)) {
        node.value = parseYAML(node.value);

      } else if (node.type === 'blockquote') {
        const paragraph = node.children[0].type === 'paragraph' &&
          node.children[0];
        const text = paragraph && paragraph.children[0].type === 'text' &&
          paragraph.children[0];
        if (text && text.value.includes('Stability:')) {
          const [, prefix, number, explication] =
            text.value.match(STABILITY_RE);

          const isStabilityIndex =
            index - 2 === headingIndex || // General.
            index - 3 === headingIndex;   // With api_metadata block.

          if (heading && isStabilityIndex) {
            heading.stability = number;
            headingIndex = -1;
            heading = null;
          }

          // Do not link to the section we are already in.
          const noLinking = filename.includes('documentation') &&
            heading !== null && heading.children[0].value === 'Stability Index';

          // Collapse blockquote and paragraph into a single node
          node.type = 'paragraph';
          node.children.shift();
          node.children.unshift(...paragraph.children);

          // Insert div with prefix and number
          node.children.unshift({
            type: 'html',
            value: `<div class="api_stability api_stability_${number}">` +
              (noLinking ? '' :
                '<a href="documentation.html#documentation_stability_index">') +
              `${prefix} ${number}${noLinking ? '' : '</a>'}`
                .replace(/\n/g, ' ')
          });

          // Remove prefix and number from text
          text.value = explication;

          // close div
          node.children.push({ type: 'html', value: '</div>' });
        }
      }
    });
  };
}

function parseYAML(text) {
  const meta = common.extractAndParseYAML(text);
  let result = '<div class="api_metadata">\n';

  const added = { description: '' };
  const deprecated = { description: '' };
  const removed = { description: '' };

  if (meta.added) {
    added.version = meta.added.join(', ');
    added.description = `<span>Added in: ${added.version}</span>`;
  }

  if (meta.deprecated) {
    deprecated.version = meta.deprecated.join(', ');
    deprecated.description =
        `<span>Deprecated since: ${deprecated.version}</span>`;
  }

  if (meta.removed) {
    removed.version = meta.removed.join(', ');
    removed.description = `<span>Removed in: ${removed.version}</span>`;
  }

  if (meta.changes.length > 0) {
    if (added.description) meta.changes.push(added);
    if (deprecated.description) meta.changes.push(deprecated);
    if (removed.description) meta.changes.push(removed);

    meta.changes.sort((a, b) => versionSort(a.version, b.version));

    result += '<details class="changelog"><summary>History</summary>\n' +
            '<table>\n<tr><th>Version</th><th>Changes</th></tr>\n';

    meta.changes.forEach((change) => {
      const description = unified()
        .use(markdown)
        .use(remark2rehype, { allowDangerousHTML: true })
        .use(raw)
        .use(htmlStringify)
        .processSync(change.description).toString();

      const version = common.arrify(change.version).join(', ');

      result += `<tr><td>${version}</td>\n` +
                  `<td>${description}</td></tr>\n`;
    });

    result += '</table>\n</details>\n';
  } else {
    result += `${added.description}${deprecated.description}` +
              `${removed.description}\n`;
  }

  if (meta.napiVersion) {
    result += `<span>N-API version: ${meta.napiVersion.join(', ')}</span>\n`;
  }

  result += '</div>';
  return result;
}

function minVersion(a) {
  return common.arrify(a).reduce((min, e) => {
    return !min || versionSort(min, e) < 0 ? e : min;
  });
}

const numberRe = /^\d*/;
function versionSort(a, b) {
  a = minVersion(a).trim();
  b = minVersion(b).trim();
  let i = 0; // Common prefix length.
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  a = a.substr(i);
  b = b.substr(i);
  return +b.match(numberRe)[0] - +a.match(numberRe)[0];
}

function buildToc({ filename, apilinks }) {
  return (tree, file) => {
    const startIncludeRefRE = /^\s*<!-- \[start-include:(.+)\] -->\s*$/;
    const endIncludeRefRE = /^\s*<!-- \[end-include:.+\] -->\s*$/;
    const realFilenames = [filename];
    const idCounters = Object.create(null);
    let toc = '';
    let depth = 0;

    visit(tree, null, (node) => {
      // Keep track of the current filename for comment wrappers of inclusions.
      if (node.type === 'html') {
        const [, includedFileName] = node.value.match(startIncludeRefRE) || [];
        if (includedFileName !== undefined)
          realFilenames.unshift(includedFileName);
        else if (endIncludeRefRE.test(node.value))
          realFilenames.shift();
      }

      if (node.type !== 'heading') return;

      if (node.depth - depth > 1) {
        throw new Error(
          `Inappropriate heading level:\n${JSON.stringify(node)}`
        );
      }

      depth = node.depth;
      const realFilename = path.basename(realFilenames[0], '.md');
      const headingText = file.contents.slice(
        node.children[0].position.start.offset,
        node.position.end.offset).trim();
      const id = getId(`${realFilename}_${headingText}`, idCounters);

      const hasStability = node.stability !== undefined;
      toc += ' '.repeat((depth - 1) * 2) +
        (hasStability ? `* <span class="stability_${node.stability}">` : '* ') +
        `<a href="#${id}">${headingText}</a>${hasStability ? '</span>' : ''}\n`;

      let anchor =
         `<span><a class="mark" href="#${id}" id="${id}">#</a></span>`;

      if (realFilename === 'errors' && headingText.startsWith('ERR_')) {
        anchor += `<span><a class="mark" href="#${headingText}" ` +
                  `id="${headingText}">#</a></span>`;
      }

      const api = headingText.replace(/^.*:\s+/, '').replace(/\(.*/, '');
      if (apilinks[api]) {
        anchor = `<a class="srclink" href=${apilinks[api]}>[src]</a>${anchor}`;
      }

      node.children.push({ type: 'html', value: anchor });
    });

    file.toc = unified()
      .use(markdown)
      .use(remark2rehype, { allowDangerousHTML: true })
      .use(raw)
      .use(htmlStringify)
      .processSync(toc).toString();
  };
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

function altDocs(filename, docCreated, versions) {
  const [, docCreatedMajor, docCreatedMinor] = docCreated.map(Number);
  const host = 'https://nodejs.org';

  const getHref = (versionNum) =>
    `${host}/docs/latest-v${versionNum}/api/${filename}.html`;

  const wrapInListItem = (version) =>
    `<li><a href="${getHref(version.num)}">${version.num}` +
    `${version.lts ? ' <b>LTS</b>' : ''}</a></li>`;

  function isDocInVersion(version) {
    const [versionMajor, versionMinor] = version.num.split('.').map(Number);
    if (docCreatedMajor > versionMajor) return false;
    if (docCreatedMajor < versionMajor) return true;
    if (Number.isNaN(versionMinor)) return true;
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

// eslint-disable-next-line max-len
const githubLogo = '<span class="github_icon"><svg height="16" width="16" viewBox="0 0 16.1 16.1" fill="currentColor"><path d="M8 0a8 8 0 0 0-2.5 15.6c.4 0 .5-.2.5-.4v-1.5c-2 .4-2.5-.5-2.7-1 0-.1-.5-.9-.8-1-.3-.2-.7-.6 0-.6.6 0 1 .6 1.2.8.7 1.2 1.9 1 2.4.7 0-.5.2-.9.5-1-1.8-.3-3.7-1-3.7-4 0-.9.3-1.6.8-2.2 0-.2-.3-1 .1-2 0 0 .7-.3 2.2.7a7.4 7.4 0 0 1 4 0c1.5-1 2.2-.8 2.2-.8.5 1.1.2 2 .1 2.1.5.6.8 1.3.8 2.2 0 3-1.9 3.7-3.6 4 .3.2.5.7.5 1.4v2.2c0 .2.1.5.5.4A8 8 0 0 0 16 8a8 8 0 0 0-8-8z"/></svg></span>';
function editOnGitHub(filename) {
  return `<li class="edit_on_github"><a href="https://github.com/nodejs/node/edit/master/doc/api/${filename}.md">${githubLogo}Edit on GitHub</a></li>`;
}
