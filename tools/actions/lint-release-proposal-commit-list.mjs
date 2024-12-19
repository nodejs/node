#!/usr/bin/env node

// Takes a stream of JSON objects as inputs, validates the CHANGELOG contains a
// line corresponding, then outputs the prURL value.
//
// $ ./lint-release-proposal-commit-list.mjs "path/to/CHANGELOG.md" "deadbeef00" <<'EOF'
// {"prURL":"https://github.com/nodejs/node/pull/56131","smallSha":"d48b5224c0","splitTitle":["doc"," fix module.md headings"]}
// {"prURL":"https://github.com/nodejs/node/pull/56123","smallSha":"f1c2d2f65e","splitTitle":["doc"," update blog release-post link"]}
// EOF

const [,, CHANGELOG_PATH, RELEASE_COMMIT_SHA] = process.argv;

import assert from 'node:assert';
import { readFile } from 'node:fs/promises';
import { createInterface } from 'node:readline';

// Creating the iterator early to avoid missing any data:
const stdinLineByLine = createInterface(process.stdin)[Symbol.asyncIterator]();

const changelog = await readFile(CHANGELOG_PATH, 'utf-8');
const commitListingStart = changelog.indexOf('\n### Commits\n');
const commitListingEnd = changelog.indexOf('\n\n<a', commitListingStart);
const commitList = changelog.slice(commitListingStart, commitListingEnd === -1 ? undefined : commitListingEnd + 1)
  // Checking for semverness is too expansive, it is left as a exercice for human reviewers.
  .replaceAll('**(SEMVER-MINOR)** ', '')
  // Correct Markdown escaping is validated by the linter, getting rid of it here helps.
  .replaceAll('\\', '');

let expectedNumberOfCommitsLeft = commitList.match(/\n\* \[/g).length;
for await (const line of stdinLineByLine) {
  const { smallSha, splitTitle, prURL } = JSON.parse(line);

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

  const expectedCommitTitle = `${`**${splitTitle.shift()}`.replace('**Revert "', '_**Revert**_ "**')}**:${splitTitle.join(':')}`;
  try {
    assert(commitList.lastIndexOf(`/${smallSha})] - ${expectedCommitTitle} (`, lineEnd) > lineStart, `Commit title doesn't match`);
  } catch (e) {
    if (e?.code === 'ERR_ASSERTION') {
      e.operator = 'includes';
      e.expected = expectedCommitTitle;
      e.actual = commitList.slice(lineStart + 1, lineEnd);
    }
    throw e;
  }
  assert.strictEqual(commitList.slice(lineEnd - prURL.length - 2, lineEnd), `(${prURL})`);

  expectedNumberOfCommitsLeft--;
  console.log(prURL);
}
assert.strictEqual(expectedNumberOfCommitsLeft, 0, 'Release commit is not the last commit in the proposal');
