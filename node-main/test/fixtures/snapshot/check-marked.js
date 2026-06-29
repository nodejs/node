'use strict';

let marked;
if (process.env.NODE_TEST_USE_SNAPSHOT === 'true') {
  console.error('NODE_TEST_USE_SNAPSHOT true');
  marked = globalThis.marked;
} else {
  console.error('NODE_TEST_USE_SNAPSHOT false');
  marked = require('./marked');
}

const md = `
# heading

[link][1]

[1]: #heading "heading"
`;

const html = marked(md)
console.log(html);
