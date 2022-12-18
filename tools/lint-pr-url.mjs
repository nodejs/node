#!/usr/bin/env node

// Usage:
// git diff upstream/main...HEAD -G"pr-url:" -- "*.md" | \
// ./tools/lint-pr-url.mjs <expected-pr-url>

import process from 'node:process';
import readline from 'node:readline';

const [, , expectedPrUrl] = process.argv;

const fileDelimiter = /^\+\+\+ b\/(.+\.md)$/;
const changeDelimiter = /^@@ -\d+,\d+ \+(\d+),\d+ @@/;
const prUrlDefinition = /^\+\s+pr-url: (.+)$/;

const validatePrUrl = (url) => url == null || url === expectedPrUrl;

let currentFile;
let currentLine;

const diff = readline.createInterface({ input: process.stdin });
for await (const line of diff) {
  if (fileDelimiter.test(line)) {
    currentFile = line.match(fileDelimiter)[1];
    console.log(`Parsing changes in ${currentFile}.`);
  } else if (changeDelimiter.test(line)) {
    currentLine = Number(line.match(changeDelimiter)[1]);
  } else if (!validatePrUrl(line.match(prUrlDefinition)?.[1])) {
    console.warn(
      `::warning file=${currentFile},line=${currentLine++},col=${line.length}` +
      '::pr-url doesn\'t match the URL of the current PR.',
    );
  } else if (line[0] !== '-') {
    // Increment line counter if line is not being deleted.
    currentLine++;
  }
}
