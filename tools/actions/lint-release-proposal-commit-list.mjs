#!/usr/bin/env node

const [,, CHANGELOG_PATH, RELEASE_COMMIT_SHA] = process.argv;

import assert from 'node:assert';
import { readFile } from 'node:fs/promises';
import { createInterface } from 'node:readline';

// Creating the iterator early to avoid missing any data
const stdinLineByLine = createInterface(process.stdin)[Symbol.asyncIterator]();

const changelog = await readFile(CHANGELOG_PATH, 'utf-8');
const startCommitListing = changelog.indexOf('\n### Commits\n');
const commitList = changelog.slice(startCommitListing, changelog.indexOf('\n\n<a', startCommitListing))
  // Checking for semverness is too expansive, it is left as a exercice for human reviewers.
  .replaceAll('**(SEMVER-MINOR)** ', '')
  // Correct Markdown escaping is validated by the linter, getting rid of it here helps.
  .replaceAll('\\', '');

let expectedNumberOfCommitsLeft = commitList.match(/\n\* \[/g).length;
for await (const line of stdinLineByLine) {
  if (line.includes(RELEASE_COMMIT_SHA.slice(0, 10))) {
    assert.strictEqual(
      expectedNumberOfCommitsLeft, 0,
      'Some commits are listed without being included in the proposal, or are listed more than once',
    );
    continue;
  }

  assert(commitList.includes('\n' + line), `Missing "${line}" in commit list`);
  expectedNumberOfCommitsLeft--;
}
assert.strictEqual(expectedNumberOfCommitsLeft, 0, 'Release commit is not the last commit in the proposal');
