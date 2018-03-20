'use strict';

module.exports = preprocess;

const path = require('path');
const fs = require('fs');

const includeExpr = /^@include\s+[\w-]+\.?[a-zA-Z]*$/gmi;
const includeData = {};

function preprocess(inputFile, input, cb) {
  input = stripComments(input);
  processIncludes(inputFile, input, cb);
}

function stripComments(input) {
  return input.replace(/^@\/\/.*$/gmi, '');
}

function processIncludes(inputFile, input, cb) {
  const includes = input.match(includeExpr);
  if (includes === null) return cb(null, input);
  let errState = null;
  let incCount = includes.length;

  includes.forEach((include) => {
    let fname = include.replace(/^@include\s+/, '');
    if (!/\.md$/.test(fname)) fname = `${fname}.md`;

    if (includeData.hasOwnProperty(fname)) {
      input = input.split(include).join(includeData[fname]);
      incCount--;
      if (incCount === 0) {
        return cb(null, input);
      }
    }

    const fullFname = path.resolve(path.dirname(inputFile), fname);
    fs.readFile(fullFname, 'utf8', function(er, inc) {
      if (errState) return;
      if (er) return cb(errState = er);
      preprocess(inputFile, inc, function(er, inc) {
        if (errState) return;
        if (er) return cb(errState = er);
        incCount--;

        // Add comments to let the HTML generator know how the anchors for
        // headings should look like.
        includeData[fname] = `<!-- [start-include:${fname}] -->\n` +
                             `${inc}\n<!-- [end-include:${fname}] -->\n`;
        input = input.split(`${include}\n`).join(`${includeData[fname]}\n`);
        if (incCount === 0) {
          return cb(null, input);
        }
      });
    });
  });
}
