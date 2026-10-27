#!/usr/bin/env node

// Takes a stream of JSON objects as inputs, validates the CHANGELOG contains a
// line corresponding, then outputs the prURL value.
//
// Example:
// $ git log upstream/vXX.x...upstream/vX.X.X-proposal \
//     --reverse --format='{"prURL":"%(trailers:key=PR-URL,valueonly,separator=)","title":"%s","smallSha":"%h"}' \
//     --abbrev=10 \
//   | sed 's/,"title":"Revert "\([^"]\+\)""/,"title":"Revert \\"\1\\""/g' \
//   | ./lint-release-proposal-commit-list.mjs "path/to/CHANGELOG.md" "$(git rev-parse upstream/vX.X.X-proposal)"

const [,, CHANGELOG_PATH, RELEASE_COMMIT_SHA] = process.argv;

import assert from 'node:assert';
import { readFile } from 'node:fs/promises';
import { createInterface } from 'node:readline';

// Creating the iterator early to avoid missing any data:
const stdinLineByLine = createInterface(process.stdin)[Symbol.asyncIterator]();

const changelog = await readFile(CHANGELOG_PATH, 'utf-8');
let commitListingStart = changelog.indexOf('\n### Commits\n');
let commitList;
// Whether every commit listed in the changelog must be in the proposal.
let listMustMatchProposal = true;
if (commitListingStart === -1) {
  // We're preparing the first release of a new release line: the changelog
  // lists every commit since the previous release line, most of them already on
  // the release branch, so the proposal only contains a subset of the list.
  commitListingStart = changelog.indexOf('\n### Semver-Major Commits\n');
  assert.notStrictEqual(commitListingStart, -1, 'Could not find the commit list');
  const releaseEnd = changelog.indexOf('\n\n<a', commitListingStart);
  commitList = changelog.slice(commitListingStart, releaseEnd === -1 ? changelog.length : releaseEnd + 1);
  listMustMatchProposal = false;
} else {
  // We can't assume the Commits section is the one for this release in case of
  // a release to transition to LTS (i.e. with no commits).
  const releaseStart = /\n<a id="(\d+(?:\.0\.0-alpha\.\d+)?\.\d+\.\d+)"><\/a>\n\n## \d+-\d+-\d+, Version \1/.exec(changelog);
  assert.ok(releaseStart, 'Could not determine the start of the release section');
  const releaseEnd = changelog.indexOf('\n\n<a', releaseStart.index);
  assert.notStrictEqual(releaseEnd, -1, 'Could not determine the end of the release section');
  commitList = changelog.slice(commitListingStart, releaseEnd + 1);
}

// Normalize for consistent comparison
commitList = commitList
  .replaceAll('**(SEMVER-MAJOR)** ', '')
  .replaceAll('**(SEMVER-MINOR)** ', '')
  .replaceAll(/(?<= - )\*\*\(CVE-\d{4}-\d+\)\*\* (?=\*\*)/g, '')
  .replaceAll('\\', '');

let expectedNumberOfCommitsLeft = commitList.match(/\n\* \[/g)?.length ?? 0;

for await (const line of stdinLineByLine) {
  const { smallSha, title, prURL } = JSON.parse(line);

  if (smallSha === RELEASE_COMMIT_SHA.slice(0, 10)) {
    if (listMustMatchProposal) {
      assert.strictEqual(
        expectedNumberOfCommitsLeft, 0,
        'Some commits are listed without being included in the proposal, or are listed more than once',
      );
    }
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
if (listMustMatchProposal) {
  assert.strictEqual(expectedNumberOfCommitsLeft, 0, 'Release commit is not the last commit in the proposal');
}
