// temporary tool to help with development.  Run both the original
// json logic and the new json logic against the same source files
// and compare the results.  Report and stop on the first significant
// difference.

'use strict';

const fs = require('fs');

process.argv.slice(2).forEach((filename) => {
  if (filename.match(/\b(all|_toc)\.md$/)) return;
  console.log(filename);
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
});

// HTML to text: remove elements, convert entites to strings
function h2t(string) {
  string
    .replace(/<.*?>/g, '')
    .replace(/&#(\d+);/, (match, digits) => (
      String.fromCharCode(parseInt(digits)))
    );
}

function diff(before, after) {
  for (const prop in after) {
    if (after[prop] === undefined) continue;

    // TODO: shouldn't events have params?
    if (prop === 'params' && before.type === 'event') continue;

    // TODO: assert.deepEqual stability
    if (prop === 'stability' && before.name === 'notDeepEqual') continue;
    if (prop === 'stabilityText' && before.name === 'notDeepEqual') continue;
    if (prop === 'stability' && before.name === 'deepEqual') continue;
    if (prop === 'stabilityText' && before.name === 'deepEqual') continue;
    if (prop === 'stability' && before.name === 'equal') continue;
    if (prop === 'stabilityText' && before.name === 'equal') continue;
    if (prop === 'stability' && before.name === 'notEqual') continue;
    if (prop === 'stabilityText' && before.name === 'notEqual') continue;

    if (before[prop] === undefined) {
      console.log({ prop, before, after });
      process.exit(1);
    } else if (Array.isArray(after[prop])) {
      if (before.length !== after.length) {
        console.log({ prop, before, after });
        process.exit(1);
      } else {
        for (const i in after[prop]) {
          if (!before[prop][i]) {
            console.log({ prop, i, before, after, value: after[prop][i] });
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
    } else if (prop === 'textRaw' &&
      before[prop].replace(/\s/g, '') === after[prop].replace(/\s/g, '')) {
      // allow - TODO fix
    } else if (prop === 'default' &&
      before[prop].replace(/\s/g, '') === after[prop].replace(/\s/g, '')) {
      // allow - TODO fix
    } else if (typeof before[prop] === 'string') {
      if (before[prop] !== after[prop]) {
        console.log({ prop, before, after });
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
      console.log({ prop, before, after, value: before[prop] });
      process.exit(1);
    }
  }
}
