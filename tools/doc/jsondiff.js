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

for (let i = process.argv.length-1; i >= 2; i--) {
  const arg = process.argv[i];

  if (arg.startsWith('--depth=')) {
    depth = parseInt(arg.replace('--depth=', ''))
    process.argv.splice(i, 1);
  } else if (arg.startsWith('--dump=')) {
    const os = require('os');
    dump = arg.replace('--dump=', '').replace(/^~\//, `${os.homedir}/`);
    process.argv.splice(i, 1);
  }
};

process.argv.slice(2).forEach((filename) => {
  let contents = {};

  try {
    if (filename.match(/\b(all|_toc)\.md$/)) return;

    if (filename.endsWith('.json')) {
      if (contents.before) {
	contents.after = JSON.parse(fs.readFileSync(filename, 'utf8'));
	diff(contents.before, contents.after);
	contents.before = null;
      } else {
	contents.before = JSON.parse(fs.readFileSync(filename, 'utf8'));
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

        contents = { before, after };
	diff(before, after);
      });
    });

  } catch (error) {

    if (!error.before) throw error;
    console.dir(error, {depth: depth});

    if (error.prop) {
      const before = error.before[error.prop];
      const after = error.after[error.prop];
      if (typeof before === 'string' && typeof after === 'string' &&
          before.includes('\n') && after.includes('\n')) {

        process.env.before = h2t(before);
        process.env.after = h2t(after);

        const { spawnSync } = require('child_process');

        console.log('');
        spawnSync(
          'diff -u <(echo "$before" ) <(echo "$after")',
          {stdio: 'inherit', shell: '/bin/bash'}
        );
      }
    }

    if (dump) {
      fs.writeFileSync(`${dump}/after.json`,
                       JSON.stringify(contents.after, null, 2));

      fs.writeFileSync(`${dump}/before.json`,
                       JSON.stringify(contents.before, null, 2));

      if (filename.endsWith('.md')) {
        const unified = require('unified');
        const markdown = require('remark-parse');

        unified()
          .use(markdown)
          .use(function() {
            this.Compiler = (tree) => {
              fs.writeFileSync(`${dump}/input.json`,
                               JSON.stringify(tree, null, 2));
            }
          })
          .processSync(fs.readFileSync(filename, 'utf8'));
      }
    }

    process.exit(1);
  }
});


function h2t(string) {
  return entities.decode(string)
    .replace(/<.*?>/g, '')
    .replace(/\n+/g, '\n')
    .trim();
}
h2t.x = 'hi'

function diff(before, after) {
  for (const prop in after) {
    if (prop === 'desc') continue;
    if (prop === 'textRaw') continue;
    if (prop === 'shortDesc') continue;

    if (after[prop] === undefined) continue;

    // TODO: shouldn't events have params?
    if (prop === 'params' && before.type === 'event') continue;

    if (before[prop] === undefined) {
      throw { prop, before, after };
    } else if (Array.isArray(after[prop])) {
      // TODO: why do methods have an inconsistent number of redundant and
      // incomplete (only names and optional indicator) signatures?
      // examples:
      //   asyncResource.emitDestroy has 0.
      //   asyncResource.asyncId has 1.
      //   buf.readDoubleBE has 2.
      if (prop === 'signatures') {
        // ignore incomplete signatures
        const first = before.signatures.shift()
        before.signatures = before.signatures.filter((sig) => (
          sig.return || sig.params.some((param) => param.type)
        ));
        before.signatures.unshift(first);
      }

      if (before[prop].length !== after[prop].length) {
        throw { prop, before, after };
        process.exit(1);
      } else {
        for (const i in after[prop]) {
          if (!before[prop][i]) {
            throw { prop, i, before, after, value: after[prop][i] };
            process.exit(1);
          } else {
            diff(before[prop][i], after[prop][i]);
          }
        }
      }
    } else if (prop === 'desc' && h2t(before[prop]) === h2t(after[prop])) {
      // allow
    } else if (prop === 'textRaw' && before[prop].trim() === after[prop]) {
      // allow
    } else if (prop === 'default' &&
      before[prop].replace(/\s/g, '') === after[prop].replace(/\s/g, '')) {
      // allow - TODO fix
    } else if (prop === 'desc') {
      // allow - TODO fix
    } else if (typeof before[prop] === 'string') {
      if (before[prop] !== after[prop]) {
        throw { prop, before, after };
        process.exit(1);
      }
    } else {
      diff(before[prop], after[prop]);
    }
  }

  for (const prop in before) {
    if (before[prop] === undefined) continue;

    // TODO: can numbers have signatures?
    if (prop === 'signatures' && before.type === 'number') continue;

    if (after[prop] === undefined) {
      throw { prop, before, after, value: before[prop] };
      process.exit(1);
    }
  }
}
