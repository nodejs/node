'use strict';

const console = require('console');
const util = require('util');
const { TapParser } = require('./tap_parser');

function TAP(input) {
  const parser = new TapParser(input);
  const ast = parser.parse();
  console.log(`------------------------------`);
  console.log(util.inspect(ast, { depth: 6, colors: true }));
}


// should parse version and plan with no body
TAP(`
TAP version 14
1..4
`);

// should parse version and plan with no body, with a reason (simple)
TAP(`
TAP version 14
1..4 # reason for plan ok
`);

// should parse version and plan with no body, with a reason (spacial characterss)
TAP(`
TAP version 14
1..4 # reason for plan "\\ !"\\#$%&'()*+,\\-./:;<=>?@[]^_\`{|}~"
`);

// should parse version and plan and body
TAP(`
TAP version 14
1..4 # reason for plan ok not
ok 1 this is a test
`);
