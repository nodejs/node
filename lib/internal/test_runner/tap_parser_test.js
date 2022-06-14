'use strict';

const util = require('util');
const { TapParser } = require('./tap_parser');

function TAP(input) {
  const parser = new TapParser(input);
  const ast = parser.parse();

  console.log(`------------------------------`);
  console.log(util.inspect(ast, { depth: 6, colors: true }));
  return parser;
}

//-------------TESTS-------------//

// TAP(`
// TAP version 14
// 1..4
// `);


// TAP(`


// TAP version 14


// 1..4


// `);

// TAP(`TAP version 14
// 1..4 # this is a description
// `);

// TAP(`
// TAP version 14
// 1..4 # reason for plan "\\ !"\\#$%&'()*+,\\-./:;<=>?@[]^_\`{|}~"
// `);

// TAP(`
// TAP version 14
// 1..4 # reason for plan
// ok
// `);

// TAP(`
// TAP version 14
// 1..4 # reason for plan ok not ok not
// ok
// `);

// TAP(`
// TAP version 14
// 1..4 # reason for plan ok not
// ok 1 this is a test
// `);

// TAP(`
// TAP version 14
// 1..4 # description for test plan ok TAP version 14
// ok 1 this is an ok test
// ok 2 - this is  a test ok
// not ok - 3 this is a not ok test
// ok 4 this is a test`);

// TAP(`
// TAP version 14
// 1..4
// ok 1 - this is an ok test
// ok 2 this is  a test ok
// not ok 3 this is a not ok test
// ok 4 this is a test`);

TAP(`
TAP version 14
1..4
ok 1 - hello \\# \\\\ world # TODO escape \\# characters with \\\\
ok foo bar
`);
