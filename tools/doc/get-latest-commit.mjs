#!/usr/bin/env node

const res = await fetch('https://api.github.com/repos/nodejs/api-docs-tooling/commits/main', {
  headers: {
    accept: 'application/vnd.github.VERSION.sha',
  },
});

console.log(await res.text());
