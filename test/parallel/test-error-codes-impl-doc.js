'use strict';

// Flags: --expose-internals

// Note: this script reads Node.js sources and docs, and expects to be run on
// the very same Node.js version as the source and doc tree is.

require('../common');
const assert = require('assert');
const errors = require('internal/errors').codes;
const fs = require('fs');
const path = require('path');
const natives = process.binding('natives');

// --report prints only the list of failed checks and does not assert
const report = process.argv[2] === '--report';
const results = [];
function check(ok, type, name, reason) {
  const label = `${type}: ${name} - ${reason}`;
  if (report) {
    if (!ok) results.push(label);
  } else {
    assert.ok(ok, label);
  }
}

const set = {
  all: new Set(),
  js: new Set(), // Error codes found in natives.
  doc: new Set(), // Error codes mentioned in `doc/api/*.md`.
  cpp: new Set(), // Error codes found in `src/node_errors.h`.
  noargs: new Set(), // Subset of `js` which is constructed without arguments.
  // `set.manual` contains errors manually created from js and not defined in
  // internals/errors. That is not scanned now, update this list if required.
  manual: new Set(['ERR_HTTP2_ERROR', 'ERR_UNKNOWN_BUILTIN_MODULE']),
  // `set.examples` contains errors mentioned in documentation purely for
  // demonstration purposes. These errors are not required to be present.
  examples: new Set(['ERR_ERROR_1'])
};

const root = path.join(__dirname, '..', '..');

// File containing error definitions
const jsdata = natives['internal/errors'];
// File containing error documentation
let docdata = fs.readFileSync(path.join(root, 'doc/api/errors.md'), 'utf-8')
docdata = docdata.replace(/## Legacy [\s\S]*/, '');
// File containing cpp-side errors
const cppdata = fs.readFileSync(path.join(root, 'src/node_errors.h'), 'utf-8');
// Directory containing documentation
const docdir = path.join(root, 'doc/api/');

function addSource(source, type) {
  // eslint-disable-next-line node-core/no-unescaped-regexp-dot
  const re = /(.)?(ERR_[A-Z0-9_]+)(..)?/g;
  let match;
  while (match = re.exec(source)) {
    if (match[1] === '_') continue; // does not start with ERR_
    const name = match[2];
    if (type === 'doc' && set.examples.has(name)) continue; // is an example
    set.all.add(name);
    set[type].add(name);
    if (type === 'js' && match[3] === '()') {
      // Is directly called without arguments from js, we will try that
      set.noargs.add(name);
    }
  }
}

// Add all errors from JS natives
for (const file of Object.keys(natives)) addSource(natives[file], 'js');
// Add all errors from src/node_errors.h
addSource(cppdata, 'cpp');
// Add all errors from doc/api/*.md files
for (const file of fs.readdirSync(docdir)) {
  if (!file.endsWith('.md')) continue;
  let data = fs.readFileSync(path.join(docdir, file), 'utf-8');
  if (file === 'errors.md') data = data.replace(/## Legacy [\s\S]*/, '');
  addSource(data, 'doc');
}

// Check that we have all js errors
for (const name of set.js) {
  if (set.manual.has(name)) continue;
  const defined = jsdata.includes(`E('${name}',`);
  check(defined, 'js', name, 'missing JS implementation (source)');
  if (defined) {
    check(errors[name], 'js', name, 'missing JS implementation (runtime)');
  }
}

// Check that we can initialize errors called without args
for (const name of set.noargs) {
  if (!errors[name]) continue; // Already catched that above
  let ok = true;
  try {
    new errors[name]();
  } catch (e) {
    ok = false;
  }
  check(ok, 'init', name, 'failed init without args, but is called with "()"');
}

// Check that we have implementation for all errors mentioned in docs.
// C++ does not need that, and JS is already checked above.
for (const name of set.doc) {
  const ok = set.manual.has(name) ||
             name.startsWith('ERR_NAPI_') || // napi errors are created directly
             jsdata.includes(`E('${name}',`) ||
             cppdata.includes(`V(${name}, `);
  const reason = docdata.includes(`### ${name}\n`) ?
    'documented' : 'mentioned in doc/api/';
  check(ok, 'impl', name, `missing implementation, ${reason}`);
}

// Check that we have documentation for all errors
for (const name of set.all) {
  const ok = docdata.includes(`### ${name}\n`);
  check(ok, 'doc', name, 'missing documentation');
  // Check that documentation references are correctly formatted
  if (ok) {
    const anchor = docdata.includes(`\n\n<a id="${name}"></a>\n### ${name}\n`);
    check(anchor, 'doc', name, 'missing anchor or not properly formatted');
  }
}

// Check that documentation is sorted, formatted, and does not contain dupes
{
  const compare = (a, b) => {
    // HTTP2_ should come after HTTP_
    if (a.startsWith('ERR_HTTP_') && b.startsWith('ERR_HTTP2_')) return -1;
    if (a.startsWith('ERR_HTTP2_') && b.startsWith('ERR_HTTP_')) return 1;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  };
  const re = /\n### (ERR_[A-Z0-9_]+)\n/g;
  let match;
  let last;
  const documented = new Set();
  while (match = re.exec(docdata)) {
    const name = match[1];
    if (documented.has(name)) {
      check(false, 'doc', name, 'duplicate documentation entry');
    } else {
      const sorted = !last || compare(name, last) === 1;
      check(sorted, 'doc', name, 'is out of order');
      documented.add(name);
    }
    last = name;
  }
}

if (report) {
  console.log('Report mode');
  console.log(results.join('\n'));
  console.log(`There were ${results.length} problems found`);
}
