'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e1, 1e2],
  size: [1e3, 1e4],
}, {
  flags: ['--expose-internals'],
});

const bechText = `
a ={
a: 1,
b: 2,
}

\`const a ={
  a: 1,
  b: 2,
  c: 3,
  d: [1, 2, 3],
}\`

\`const b = [
  1,
  2,
  3,
  4,
]\`

const d = [
  {
    a: 1,
    b: 2,
  },
  {
    a: 3,
    b: 4,
  }
]

var e = [
  {
    a: 1,
    b: 2,
  },
  {
    a: 3,
    b: 4,
    c: [
      {
        a: 1,
        b: 2,
      },
      {
        a: 3,
        b: 4,
      }
    ]
  }
]

a = \`
I am a multiline string
I can be as long as I want\`


b = \`
111111111111
222222222222\`


c = \`
111111=11111
222222222222\`

function sum(a, b) {
    return a + b
}

console.log(
  'something'
)
`;

async function main({ n, size }) {
  const { parseHistoryFromFile } = require('internal/repl/history-utils');

  bench.start();

  for (let i = 0; i < n; i++) {
    parseHistoryFromFile(bechText.repeat(size));
  }

  bench.end(n);
}
