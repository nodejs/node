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

module.exports = doJSON;

// Take the lexed input, and return a JSON-encoded object.
// A module looks like this: https://gist.github.com/1777387.

const common = require('./common.js');
const marked = require('marked');

// Customized heading without id attribute.
const renderer = new marked.Renderer();
renderer.heading = (text, level) => `<h${level}>${text}</h${level}>\n`;
marked.setOptions({ renderer });


function doJSON(input, filename, cb) {
  const root = { source: filename };
  const stack = [root];
  let depth = 0;
  let current = root;
  let state = null;

  const exampleHeading = /^example/i;
  const metaExpr = /<!--([^=]+)=([^-]+)-->\n*/g;
  const stabilityExpr = /^Stability: ([0-5])(?:\s*-\s*)?(.*)$/;

  const lexed = marked.lexer(input);
  lexed.forEach((tok) => {
    const { type } = tok;
    let { text } = tok;

    // <!-- name=module -->
    // This is for cases where the markdown semantic structure is lacking.
    if (type === 'paragraph' || type === 'html') {
      text = text.replace(metaExpr, (_0, key, value) => {
        current[key.trim()] = value.trim();
        return '';
      });
      text = text.trim();
      if (!text) return;
    }

    if (type === 'heading' && !exampleHeading.test(text.trim())) {
      if (tok.depth - depth > 1) {
        return cb(
          new Error(`Inappropriate heading level\n${JSON.stringify(tok)}`));
      }

      // Sometimes we have two headings with a single blob of description.
      // Treat as a clone.
      if (state === 'AFTERHEADING' && depth === tok.depth) {
        const clone = current;
        current = newSection(tok);
        current.clone = clone;
        // Don't keep it around on the stack.
        stack.pop();
      } else {
        // If the level is greater than the current depth,
        // then it's a child, so we should just leave the stack as it is.
        // However, if it's a sibling or higher, then it implies
        // the closure of the other sections that came before.
        // root is always considered the level=0 section,
        // and the lowest heading is 1, so this should always
        // result in having a valid parent node.
        let closingDepth = tok.depth;
        while (closingDepth <= depth) {
          finishSection(stack.pop(), stack[stack.length - 1]);
          closingDepth++;
        }
        current = newSection(tok);
      }

      ({ depth } = tok);
      stack.push(current);
      state = 'AFTERHEADING';
      return;
    }

    // Immediately after a heading, we can expect the following:
    //
    // { type: 'blockquote_start' },
    // { type: 'paragraph', text: 'Stability: ...' },
    // { type: 'blockquote_end' },
    //
    // A list: starting with list_start, ending with list_end,
    // maybe containing other nested lists in each item.
    //
    // A metadata:
    // <!-- YAML
    // added: v1.0.0
    // -->
    //
    // If one of these isn't found, then anything that comes
    // between here and the next heading should be parsed as the desc.
    if (state === 'AFTERHEADING') {
      if (type === 'blockquote_start') {
        state = 'AFTERHEADING_BLOCKQUOTE';
        return;
      } else if (type === 'list_start' && !tok.ordered) {
        state = 'AFTERHEADING_LIST';
        current.list = current.list || [];
        current.list.push(tok);
        current.list.level = 1;
      } else if (type === 'html' && common.isYAMLBlock(tok.text)) {
        current.meta = common.extractAndParseYAML(tok.text);
      } else {
        current.desc = current.desc || [];
        if (!Array.isArray(current.desc)) {
          current.shortDesc = current.desc;
          current.desc = [];
        }
        current.desc.links = lexed.links;
        current.desc.push(tok);
        state = 'DESC';
      }
      return;
    }

    if (state === 'AFTERHEADING_LIST') {
      current.list.push(tok);
      if (type === 'list_start') {
        current.list.level++;
      } else if (type === 'list_end') {
        current.list.level--;
      }
      if (current.list.level === 0) {
        state = 'AFTERHEADING';
        processList(current);
      }
      return;
    }

    if (state === 'AFTERHEADING_BLOCKQUOTE') {
      if (type === 'blockquote_end') {
        state = 'AFTERHEADING';
        return;
      }

      let stability;
      if (type === 'paragraph' && (stability = text.match(stabilityExpr))) {
        current.stability = parseInt(stability[1], 10);
        current.stabilityText = stability[2].trim();
        return;
      }
    }

    current.desc = current.desc || [];
    current.desc.links = lexed.links;
    current.desc.push(tok);
  });

  // Finish any sections left open.
  while (root !== (current = stack.pop())) {
    finishSection(current, stack[stack.length - 1]);
  }

  return cb(null, root);
}


// Go from something like this:
//
// [ { type: "list_item_start" },
//   { type: "text",
//     text: "`options` {Object|string}" },
//   { type: "list_start",
//     ordered: false },
//   { type: "list_item_start" },
//   { type: "text",
//     text: "`encoding` {string|null} **Default:** `'utf8'`" },
//   { type: "list_item_end" },
//   { type: "list_item_start" },
//   { type: "text",
//     text: "`mode` {integer} **Default:** `0o666`" },
//   { type: "list_item_end" },
//   { type: "list_item_start" },
//   { type: "text",
//     text: "`flag` {string} **Default:** `'a'`" },
//   { type: "space" },
//   { type: "list_item_end" },
//   { type: "list_end" },
//   { type: "list_item_end" } ]
//
// to something like:
//
// [ { textRaw: "`options` {Object|string} ",
//     options: [
//       { textRaw: "`encoding` {string|null} **Default:** `'utf8'` ",
//         name: "encoding",
//         type: "string|null",
//         default: "`'utf8'`" },
//       { textRaw: "`mode` {integer} **Default:** `0o666` ",
//         name: "mode",
//         type: "integer",
//         default: "`0o666`" },
//       { textRaw: "`flag` {string} **Default:** `'a'` ",
//         name: "flag",
//         type: "string",
//         default: "`'a'`" } ],
//     name: "options",
//     type: "Object|string",
//     optional: true } ]

function processList(section) {
  const { list } = section;
  const values = [];
  const stack = [];
  let current;

  // For now, *just* build the hierarchical list.
  list.forEach((tok) => {
    const { type } = tok;
    if (type === 'space') return;
    if (type === 'list_item_start' || type === 'loose_item_start') {
      const item = {};
      if (!current) {
        values.push(item);
        current = item;
      } else {
        current.options = current.options || [];
        stack.push(current);
        current.options.push(item);
        current = item;
      }
    } else if (type === 'list_item_end') {
      if (!current) {
        throw new Error('invalid list - end without current item\n' +
                        `${JSON.stringify(tok)}\n` +
                        JSON.stringify(list));
      }
      current = stack.pop();
    } else if (type === 'text') {
      if (!current) {
        throw new Error('invalid list - text without current item\n' +
                        `${JSON.stringify(tok)}\n` +
                        JSON.stringify(list));
      }
      current.textRaw = `${current.textRaw || ''}${tok.text} `;
    }
  });

  // Shove the name in there for properties,
  // since they are always just going to be the value etc.
  if (section.type === 'property' && values[0]) {
    values[0].textRaw = `\`${section.name}\` ${values[0].textRaw}`;
  }

  // Now pull the actual values out of the text bits.
  values.forEach(parseListItem);

  // Now figure out what this list actually means.
  // Depending on the section type, the list could be different things.

  switch (section.type) {
    case 'ctor':
    case 'classMethod':
    case 'method': {
      // Each item is an argument, unless the name is 'return',
      // in which case it's the return value.
      section.signatures = section.signatures || [];
      const sig = {};
      section.signatures.push(sig);
      sig.params = values.filter((value) => {
        if (value.name === 'return') {
          sig.return = value;
          return false;
        }
        return true;
      });
      parseSignature(section.textRaw, sig);
      break;
    }

    case 'property': {
      // There should be only one item, which is the value.
      // Copy the data up to the section.
      const value = values[0] || {};
      delete value.name;
      section.typeof = value.type || section.typeof;
      delete value.type;
      Object.keys(value).forEach((key) => {
        section[key] = value[key];
      });
      break;
    }

    case 'event':
      // Event: each item is an argument.
      section.params = values;
      break;

    default:
      if (section.list.length > 0) {
        section.desc = section.desc || [];
        section.desc.push(...section.list);
      }
  }

  delete section.list;
}


const paramExpr = /\((.+)\);?$/;

// text: "someobject.someMethod(a[, b=100][, c])"
function parseSignature(text, sig) {
  let [, sigParams] = text.match(paramExpr) || [];
  if (!sigParams) return;
  sigParams = sigParams.split(',');
  let optionalLevel = 0;
  const optionalCharDict = { '[': 1, ' ': 0, ']': -1 };
  sigParams.forEach((sigParam, i) => {
    sigParam = sigParam.trim();
    if (!sigParam) {
      throw new Error(`Empty parameter slot: ${text}`);
    }
    let listParam = sig.params[i];
    let optional = false;
    let defaultValue;

    // For grouped optional params such as someMethod(a[, b[, c]]).
    let pos;
    for (pos = 0; pos < sigParam.length; pos++) {
      const levelChange = optionalCharDict[sigParam[pos]];
      if (levelChange === undefined) break;
      optionalLevel += levelChange;
    }
    sigParam = sigParam.substring(pos);
    optional = (optionalLevel > 0);
    for (pos = sigParam.length - 1; pos >= 0; pos--) {
      const levelChange = optionalCharDict[sigParam[pos]];
      if (levelChange === undefined) break;
      optionalLevel += levelChange;
    }
    sigParam = sigParam.substring(0, pos + 1);

    const eq = sigParam.indexOf('=');
    if (eq !== -1) {
      defaultValue = sigParam.substr(eq + 1);
      sigParam = sigParam.substr(0, eq);
    }
    if (!listParam) {
      listParam = sig.params[i] = { name: sigParam };
    }
    // At this point, the name should match.
    if (sigParam !== listParam.name) {
      throw new Error(
        `Warning: invalid param "${sigParam}"\n` +
        ` > ${JSON.stringify(listParam)}\n` +
        ` > ${text}`
      );
    }
    if (optional) listParam.optional = true;
    if (defaultValue !== undefined) listParam.default = defaultValue.trim();
  });
}


const returnExpr = /^returns?\s*:?\s*/i;
const nameExpr = /^['`"]?([^'`": {]+)['`"]?\s*:?\s*/;
const typeExpr = /^\{([^}]+)\}\s*/;
const leadingHyphen = /^-\s*/;
const defaultExpr = /\s*\*\*Default:\*\*\s*([^]+)$/i;

function parseListItem(item) {
  if (item.options) item.options.forEach(parseListItem);
  if (!item.textRaw) {
    throw new Error(`Empty list item: ${JSON.stringify(item)}`);
  }

  // The goal here is to find the name, type, default, and optional.
  // Anything left over is 'desc'.
  let text = item.textRaw.trim();

  if (returnExpr.test(text)) {
    item.name = 'return';
    text = text.replace(returnExpr, '');
  } else {
    const [, name] = text.match(nameExpr) || [];
    if (name) {
      item.name = name;
      text = text.replace(nameExpr, '');
    }
  }

  const [, type] = text.match(typeExpr) || [];
  if (type) {
    item.type = type;
    text = text.replace(typeExpr, '');
  }

  text = text.replace(leadingHyphen, '');

  const [, defaultValue] = text.match(defaultExpr) || [];
  if (defaultValue) {
    item.default = defaultValue.replace(/\.$/, '');
    text = text.replace(defaultExpr, '');
  }

  if (text) item.desc = text;
}


function finishSection(section, parent) {
  if (!section || !parent) {
    throw new Error('Invalid finishSection call\n' +
                    `${JSON.stringify(section)}\n` +
                    JSON.stringify(parent));
  }

  if (!section.type) {
    section.type = 'module';
    if (parent.type === 'misc') {
      section.type = 'misc';
    }
    section.displayName = section.name;
    section.name = section.name.toLowerCase()
      .trim().replace(/\s+/g, '_');
  }

  if (section.desc && Array.isArray(section.desc)) {
    section.desc.links = section.desc.links || [];
    section.desc = marked.parser(section.desc);
  }

  if (!section.list) section.list = [];
  processList(section);

  // Classes sometimes have various 'ctor' children
  // which are actually just descriptions of a constructor class signature.
  // Merge them into the parent.
  if (section.type === 'class' && section.ctors) {
    section.signatures = section.signatures || [];
    const sigs = section.signatures;
    section.ctors.forEach((ctor) => {
      ctor.signatures = ctor.signatures || [{}];
      ctor.signatures.forEach((sig) => {
        sig.desc = ctor.desc;
      });
      sigs.push(...ctor.signatures);
    });
    delete section.ctors;
  }

  // Properties are a bit special.
  // Their "type" is the type of object, not "property".
  if (section.properties) {
    section.properties.forEach((prop) => {
      if (prop.typeof) {
        prop.type = prop.typeof;
        delete prop.typeof;
      } else {
        delete prop.type;
      }
    });
  }

  // Handle clones.
  if (section.clone) {
    const { clone } = section;
    delete section.clone;
    delete clone.clone;
    deepCopy(section, clone);
    finishSection(clone, parent);
  }

  let plur;
  if (section.type.slice(-1) === 's') {
    plur = `${section.type}es`;
  } else if (section.type.slice(-1) === 'y') {
    plur = section.type.replace(/y$/, 'ies');
  } else {
    plur = `${section.type}s`;
  }

  // If the parent's type is 'misc', then it's just a random
  // collection of stuff, like the "globals" section.
  // Make the children top-level items.
  if (section.type === 'misc') {
    Object.keys(section).forEach((key) => {
      switch (key) {
        case 'textRaw':
        case 'name':
        case 'type':
        case 'desc':
        case 'miscs':
          return;
        default:
          if (parent.type === 'misc') {
            return;
          }
          if (parent[key] && Array.isArray(parent[key])) {
            parent[key] = parent[key].concat(section[key]);
          } else if (!parent[key]) {
            parent[key] = section[key];
          }
      }
    });
  }

  parent[plur] = parent[plur] || [];
  parent[plur].push(section);
}


// Not a general purpose deep copy.
// But sufficient for these basic things.
function deepCopy(src, dest) {
  Object.keys(src)
    .filter((key) => !dest.hasOwnProperty(key))
    .forEach((key) => { dest[key] = cloneValue(src[key]); });
}

function cloneValue(src) {
  if (!src) return src;
  if (Array.isArray(src)) {
    const clone = new Array(src.length);
    src.forEach((value, i) => {
      clone[i] = cloneValue(value);
    });
    return clone;
  }
  if (typeof src === 'object') {
    const clone = {};
    Object.keys(src).forEach((key) => {
      clone[key] = cloneValue(src[key]);
    });
    return clone;
  }
  return src;
}


// This section parse out the contents of an H# tag.

// To reduse escape slashes in RegExp string components.
const r = String.raw;

const eventPrefix = '^Event: +';
const classPrefix = '^[Cc]lass: +';
const ctorPrefix = '^(?:[Cc]onstructor: +)?new +';
const classMethodPrefix = '^Class Method: +';
const maybeClassPropertyPrefix = '(?:Class Property: +)?';

const maybeQuote = `['"]?`;
const notQuotes = `[^'"]+`;

// To include constructs like `readable\[Symbol.asyncIterator\]()`
// or `readable.\_read(size)` (with Markdown escapes).
const simpleId = r`(?:(?:\\?_)+|\b)\w+\b`;
const computedId = r`\\?\[[\w\.]+\\?\]`;
const id = `(?:${simpleId}|${computedId})`;
const classId = r`[A-Z]\w+`;

const ancestors = r`(?:${id}\.?)+`;
const maybeAncestors = r`(?:${id}\.?)*`;

const callWithParams = r`\([^)]*\)`;

const noCallOrProp = '(?![.[(])';

const maybeExtends = `(?: +extends +${maybeAncestors}${classId})?`;

const headingExpressions = [
  { type: 'event', re: RegExp(
    `${eventPrefix}${maybeQuote}(${notQuotes})${maybeQuote}$`, 'i') },

  { type: 'class', re: RegExp(
    `${classPrefix}(${maybeAncestors}${classId})${maybeExtends}$`, '') },

  { type: 'ctor', re: RegExp(
    `${ctorPrefix}(${maybeAncestors}${classId})${callWithParams}$`, '') },

  { type: 'classMethod', re: RegExp(
    `${classMethodPrefix}${maybeAncestors}(${id})${callWithParams}$`, 'i') },

  { type: 'method', re: RegExp(
    `^${maybeAncestors}(${id})${callWithParams}$`, 'i') },

  { type: 'property', re: RegExp(
    `^${maybeClassPropertyPrefix}${ancestors}(${id})${noCallOrProp}$`, 'i') },
];

function newSection({ text }) {
  // Infer the type from the text.
  for (const { type, re } of headingExpressions) {
    const [, name] = text.match(re) || [];
    if (name) {
      return { textRaw: text, type, name };
    }
  }
  return { textRaw: text, name: text };
}
