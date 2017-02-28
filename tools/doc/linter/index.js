'use strict';

const { readFileSync, readdirSync } = require('fs');
const Files = readdirSync('doc/api').filter((f) => f.endsWith('.md')).map(
  (file) => [`doc/api/${file}`, readFileSync(`doc/api/${file}`).toString()]
);
const errors = [];
const LinterRules = [
  require('./linksOrderLinter')
];

Files.forEach(function([fileName, fileContents]) {
  LinterRules.forEach(function(rule) {
    rule(fileName, fileContents, errors);
  });
});

errors.forEach((error, idx) => console.error(`\t${idx + 1}. ${error}`));

process.exit(errors.length ? 1 : 0);
