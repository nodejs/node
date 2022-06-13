
const util = require('util');


const { TapParser } = require('./tap_parser');

const input = `
TAP version 14
1..4 # reason for plan
ok 112
ok 112
ok 112
ok 1 - description 1
not ok 4 - description 4
ok 5 - description 5 # directive 5
`;
const parser = new TapParser(input);
const ast = parser.parse();
console.log(util.inspect(ast, { depth: 6, colors: true }));
