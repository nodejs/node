#!/usr/bin/env node

const res = await fetch('https://api.github.com/repos/nodejs/doc-kit/commits/main', {
  headers: {
    accept: 'application/vnd.github.VERSION.sha',
  },
});

console.log(await res.text());
