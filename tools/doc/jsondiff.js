// temporary tool to help with development.  Run both the original
// json logic and the new json logic against the same source files
// and compare the results.  Report and stop on the first significant
// difference.

'use strict';

const fs = require('fs');
let depth = 2;
let dump = null;

const Entities = require('html-entities').AllHtmlEntities;
const entities = new Entities();

for (let i = process.argv.length - 1; i >= 2; i--) {
  const arg = process.argv[i];

  if (arg.startsWith('--depth=')) {
    depth = parseInt(arg.replace('--depth=', ''));
    process.argv.splice(i, 1);
  } else if (arg.startsWith('--dump=')) {
    const os = require('os');
    dump = arg.replace('--dump=', '').replace(/^~\//, `${os.homedir}/`);
    process.argv.splice(i, 1);
  }
}

class Diff extends Error {
  constructor(data) {
    super('Differences found');
    this.data = data;
  }
}

let before = null;
process.argv.slice(2).forEach((filename) => {
  try {
    if (filename.match(/\b(all|_toc)\.md$/)) return;

    if (filename.startsWith('--depth=')) {
      depth = parseInt(filename.replace('--depth=', ''));
      return;
    } else if (filename.startsWith('--dump=')) {
      const os = require('os');
      dump = filename.replace('--dump=', '').replace(/^~\//, `${os.homedir}/`);
      return;
    }

    if (filename.endsWith('.json')) {
      if (before) {
        diff(before, JSON.parse(fs.readFileSync(filename, 'utf8')));
        before = null;
      } else {
        before = JSON.parse(fs.readFileSync(filename, 'utf8'));
      }
      return;
    }

    if (process.argv.length > 3) console.log(filename);

    const input = fs.readFileSync(filename, 'utf8');

    require('./json.js')(input, filename, (err, before) => {
      if (err) throw err;
      require('./json2.js')(input, filename, (err, after) => {
        if (err) throw err;
        after = after.contents;
        delete after.type;

        diff(before, after);
      });
    });

  } catch (error) {

    if (!(error instanceof Diff)) throw error;
    const data = error.data;
    console.dir(data, { depth });

    if (data.prop) {
      const before = data.before[data.prop];
      const after = data.after[data.prop];
      if (typeof before === 'string' && typeof after === 'string' &&
          before.includes('\n') && after.includes('\n')) {

        process.env.before = h2t(before, false);
        process.env.after = h2t(after, false);

        const { spawnSync } = require('child_process');

        console.log('');
        spawnSync(
          'diff -u <(echo "$before" ) <(echo "$after")',
          { stdio: 'inherit', shell: '/bin/bash' }
        );
      }
    }

    if (dump) {
      fs.writeFileSync(`${dump}/after.json`,
                       JSON.stringify(data.after, null, 2));

      fs.writeFileSync(`${dump}/before.json`,
                       JSON.stringify(data.before, null, 2));

      if (filename.endsWith('.md')) {
        const unified = require('unified');
        const markdown = require('remark-parse');

        unified()
          .use(markdown)
          .use(function() {
            this.Compiler = (tree) => {
              fs.writeFileSync(`${dump}/input.json`,
                               JSON.stringify(tree, null, 2));
            };
          })
          .processSync(fs.readFileSync(filename, 'utf8'));
      }
    }

    process.exit(1);
  }
});


// HTML to text: remove elements, convert entites to strings
function h2t(string, collapse = true) {
  const stripped = entities.decode(string).replace(/<.*?>/g, '');

  if (collapse) {
    return stripped.replace(/\s*/g, ' ').trim();
  } else {
    return stripped.replace(/\n+/g, '\n').trim();
  }
}

function diff(before, after) {
  // missing params, difference in escaping for textRaw and name
  if (after.name === '<inspector-protocol-method>') return;

  for (const prop in after) {
    if (after[prop] === undefined) continue;

    // TODO: shouldn't events have params?
    if (prop === 'params' && before.type === 'event') continue;

    // TODO: new Console() overloads
    if (prop === 'params' && before.params.length === 1 &&
        after.params.length === 3 && before.params[0].name === 'options' &&
        after.params[0].name === 'stdout') continue;

    // TODO: crypto.constants
    if (before.name === 'constants' && after.name === 'return') continue;

    // TODO: readabileHighWaterMark is missing all sorts of stuff
    if (before.name === 'readableHighWaterMark') continue;
    if (before.name === 'readableLength') continue;
    if (before.name === 'authorized') continue;

    // difference in escaping
    if (after.name === '[Symbol.asyncIterator]') continue;
    if (after.name === '[Symbol.iterator]') continue;

    // difference in ordered list rendering
    if (prop === 'desc' && before.name &&
      before.name.startsWith('working_with_javascript_values'))
      after.desc = after.desc.replace(/^\d\. /mg, '');

    if (before[prop] === undefined) {
      throw new Diff({ prop, before, after });
    } else if (Array.isArray(after[prop])) {
      // TODO: why do methods have an inconsistent number of redundant and
      // incomplete (only names and optional indicator) signatures?
      // examples:
      //   asyncResource.emitDestroy has 0.
      //   asyncResource.asyncId has 1.
      //   buf.readDoubleBE has 2.
      if (prop === 'signatures') {
        // ignore incomplete signatures
        const first = before.signatures.shift();
        before.signatures = before.signatures.filter((sig) => (
          sig.return || sig.params.some((param) => param.type)
        ));
        before.signatures.unshift(first);
      }

      // TODO: debugger/command reference is missing 'Information'
      if (before.name === 'command_reference' && prop === 'modules') continue;

      // TODO: globals require() is missing a signature
      if (before.name === 'require' && prop === 'signatures') continue;

      // TODO: dgram.Socket is missing 'getRecvBufferSize'
      if (before.name === 'dgram.Socket' && prop === 'methods') continue;

      if (before[prop].length !== after[prop].length) {
        throw new Diff({ prop, before, after });
      } else {
        for (const i in after[prop]) {
          if (!before[prop][i]) {
            throw new Diff({ prop, i, before, after, value: after[prop][i] });
          } else {
            diff(before[prop][i], after[prop][i]);
          }
        }
      }
    } else if (prop === 'desc' && h2t(before[prop]) === h2t(after[prop])) {
      // allow
    } else if (prop === 'textRaw' && before[prop].trim() === after[prop]) {
      // allow
    } else if (prop === 'textRaw' &&
      before[prop].replace(/\s/g, '') === after[prop].replace(/\s/g, '')) {
      // allow - TODO fix
    } else if (prop === 'default' &&
      before[prop].replace(/\s/g, '') === after[prop].replace(/\s/g, '')) {
      // allow - TODO fix
    } else if (typeof before[prop] === 'string') {
      if (before[prop] !== after[prop]) {
        throw new Diff({ prop, before, after });
      }
    } else {
      diff(before[prop], after[prop]);
    }
  }

  for (const prop in before) {
    if (before[prop] === undefined) continue;

    // TODO: can numbers have signatures?
    if (prop === 'signatures' && before.type === 'number') continue;

    // TODO: shouldn't events have params?
    if (prop === 'params' && before.type === 'event') continue;

    // TODO: various values obtained from the wrong section
    if (prop === 'desc' && before.name === 'getFips') continue;
    if (prop === 'desc' && before.name === 'getPrivateKey') continue;
    if (prop === 'default' && before.name === 'listening') continue;
    if (before.textRaw === 'domain.create()') continue;
    if (before.name === 'destroyed' && before.type === 'boolean') continue;
    if (prop === 'desc' && before.params && before.params.length === 2 &&
      before.params[0].name === 'filename' &&
      before.params[1].name === 'options')
      continue;
    if (prop === 'desc' && before.name === 'deflate') continue;
    if (prop === 'desc' && before.name === 'deflateRaw') continue;
    if (prop === 'desc' && before.name === 'gunzip') continue;
    if (prop === 'desc' && before.name === 'gzip') continue;
    if (prop === 'desc' && before.name === 'inflate') continue;
    if (prop === 'desc' && before.name === 'inflateRaw') continue;
    if (prop === 'desc' && before.name === 'unzip') continue;

    // TODO: readabileHighWaterMark is missing all sorts of stuff
    if (before.name === 'readableHighWaterMark') continue;
    if (before.name === 'readableLength') continue;
    if (before.name === 'authorized') continue;

    if (prop === 'signatures') {
      // ignore incomplete signatures
      before.signatures = before.signatures.filter((sig) => (
        sig && (sig.return || sig.params.some((param) => param.type))
      ));
      if (before.signatures.length === 0) continue;
    }

    if (after[prop] === undefined) {
      throw new Diff({ prop, before, after, value: before[prop] });
    }
  }
}
