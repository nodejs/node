#!/usr/bin/env node

// Takes a stream of JSON objects as inputs, validates the CHANGELOG contains a
// line corresponding, then outputs the prURL value.
//
// Example:
// $ git log upstream/vXX.x...upstream/vX.X.X-proposal \
//     --reverse --format='{"prURL":"%(trailers:key=PR-URL,valueonly,separator=)","title":"%s","smallSha":"%h"}' \
//   | sed 's/,"title":"Revert "\([^"]\+\)""/,"title":"Revert \\"\1\\""/g' \
//   | ./lint-release-proposal-commit-list.mjs "path/to/CHANGELOG.md" "$(git rev-parse upstream/vX.X.X-proposal)"

const [,, CHANGELOG_PATH, RELEASE_COMMIT_SHA] = process.argv;

import assert from 'node:assert';
import { readFile } from 'node:fs/promises';
import { createInterface } from 'node:readline';

// Creating the iterator early to avoid missing any data:
const stdinLineByLine = createInterface(process.stdin)[Symbol.asyncIterator]();

const changelog = await readFile(CHANGELOG_PATH, 'utf-8');
const commitListingStart = changelog.indexOf('\n### Commits\n');
let commitList;
if (commitListingStart === -1) {
  // We're preparing a semver-major release.
  commitList = changelog.replace(/(^.+\n### Semver-Major|\n### Semver-(Minor|Patch)) Commits\n/gs, '')
    .replaceAll('**(SEMVER-MAJOR)** ', '');
} else {
  const commitListingEnd = changelog.indexOf('\n\n<a', commitListingStart);
  assert.notStrictEqual(commitListingEnd, -1);
  commitList = changelog.slice(commitListingStart, commitListingEnd + 1);
}

// Normalize for consistent comparison
commitList = commitList
  .replaceAll('**(SEMVER-MINOR)** ', '')
  .replaceAll('\\', '');

let expectedNumberOfCommitsLeft = commitList.match(/\n\* \[/g)?.length ?? 0;

for await (const line of stdinLineByLine) {
  const { smallSha, title, prURL } = JSON.parse(line);

  if (smallSha === RELEASE_COMMIT_SHA.slice(0, 10)) {
    assert.strictEqual(
      expectedNumberOfCommitsLeft, 0,
      'Some commits are listed without being included in the proposal, or are listed more than once',
    );
    continue;
  }

  const lineStart = commitList.indexOf(`\n* [[\`${smallSha}\`]`);
  assert.notStrictEqual(lineStart, -1, `Cannot find ${smallSha} on the list`);
  const lineEnd = commitList.indexOf('\n', lineStart + 1);

  try {
    const colonIndex = title.indexOf(':');
    const expectedCommitTitle = `${`**${title.slice(0, colonIndex)}`.replace('**Revert "', '_**Revert**_ "**')}**${title.slice(colonIndex)}`;
    try {
      assert(commitList.lastIndexOf(`/${smallSha})] - ${expectedCommitTitle} (`, lineEnd) > lineStart, `Changelog entry doesn't match for ${smallSha}`);
    } catch (e) {
      if (e?.code === 'ERR_ASSERTION') {
        e.operator = 'includes';
        e.expected = expectedCommitTitle;
        e.actual = commitList.slice(lineStart + 1, lineEnd);
      }
      throw e;
    }
    assert.strictEqual(commitList.slice(lineEnd - prURL.length - 2, lineEnd), `(${prURL})`, `when checking ${smallSha} ${title}`);

  } catch (e) {
    if (e?.code !== 'ERR_ASSERTION') {
      throw e;
    }
    let line = 1;
    for (let i = 0; i < lineStart + commitListingStart; i = changelog.indexOf('\n', i + 1)) {
      line++;
    }
    console.error(`::error file=${CHANGELOG_PATH},line=${line},title=Release proposal linter::${e.message}`);
    console.error(e);
    process.exitCode ||= 1;
  }
  expectedNumberOfCommitsLeft--;
  console.log(prURL);
}
assert.strictEqual(expectedNumberOfCommitsLeft, 0, 'Release commit is not the last commit in the proposal');
