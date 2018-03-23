'use strict';

module.exports = processIncludes;

const path = require('path');
const fs = require('fs');

const includeExpr = /^@include\s+([\w-]+)(?:\.md)?$/gmi;
const commentExpr = /^@\/\/.*$/gmi;

function processIncludes(inputFile, input, cb) {
  const includes = input.match(includeExpr);
  if (includes === null)
    return cb(null, input.replace(commentExpr, ''));

  let errState = null;
  let incCount = includes.length;

  includes.forEach((include) => {
    const fname = include.replace(includeExpr, '$1.md');
    const fullFname = path.resolve(path.dirname(inputFile), fname);

    fs.readFile(fullFname, 'utf8', function(er, inc) {
      if (errState) return;
      if (er) return cb(errState = er);
      incCount--;

      // Add comments to let the HTML generator know
      // how the anchors for headings should look like.
      inc = `<!-- [start-include:${fname}] -->\n` +
            `${inc}\n<!-- [end-include:${fname}] -->\n`;
      input = input.split(`${include}\n`).join(`${inc}\n`);

      if (incCount === 0)
        return cb(null, input.replace(commentExpr, ''));
    });
  });
}
