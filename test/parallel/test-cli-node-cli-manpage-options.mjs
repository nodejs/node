import '../common/index.mjs';
import assert from 'node:assert';
import { createReadStream, readFileSync } from 'node:fs';
import { createInterface } from 'node:readline';
import { resolve, join } from 'node:path';

// This test checks that all the CLI flags defined in the public CLI documentation (doc/api/cli.md)
// are also documented in the manpage file (doc/node.1)
// Note: the opposite (that all variables in doc/node.1 are documented in the CLI documentation)
//       is covered in the test-cli-node-options-docs.js file

const rootDir = resolve(import.meta.dirname, '..', '..');

const cliMdPath = join(rootDir, 'doc', 'api', 'cli.md');
const cliMdContentsStream = createReadStream(cliMdPath);

const manPagePath = join(rootDir, 'doc', 'node.1');
const manPageContents = readFileSync(manPagePath, { encoding: 'utf8' });

let insideOptionsSection = false;

const rl = createInterface({
  input: cliMdContentsStream,
});

const isOptionLineRegex = /^###(?: `[^`]*`,?)*$/;

for await (const line of rl) {
  if (line.startsWith('## ')) {
    if (insideOptionsSection) {
      // We were in the options section and we're now exiting it,
      // so there is no need to keep checking the remaining lines,
      // we might as well close the stream and exit the loop
      cliMdContentsStream.close();
      break;
    }

    // We've just entered the options section
    insideOptionsSection = line === '## Options';
    continue;
  }

  if (insideOptionsSection && isOptionLineRegex.test(line)) {
    const flagNames = extractFlagNames(line);
    const flagMatcher = new RegExp(`^\\.It ${flagNames.map((f) => `Fl ${f}.*`).join(', ')}$`, 'm');

    if (!manPageContents.match(flagMatcher)) {
      assert.fail(
        `The following flag${
          flagNames.length === 1 ? '' : 's'
        } (present in \`doc/api/cli.md\`) ${flagNames.length === 1 ? 'is' : 'are'} missing in the \`doc/node.1\` file: ${
          flagNames.map((flag) => `"-${flag}"`).join(', ')
        }`
      );
    }
  }
}

/**
 * Function that given a string containing backtick enclosed cli flags
 * separated by `, ` returns the name of flags present in the string
 * e.g. `extractFlagNames('`-x`, `--print "script"`')` === `['x', 'print']`
 * @param {string} str target string
 * @returns {string[]} the name of the detected flags
 */
function extractFlagNames(str) {
  const match = str.match(/`[^`]*?`/g);
  if (!match) {
    return [];
  }
  return match.map((flag) => {
    // Remove the backticks, and leading dash from the flag
    flag = flag.slice(2, -1);

    // If the flag contains parameters make sure to remove those
    const nameDelimiters = ['=', ' ', '['];
    const nameCutOffIdx = Math.min(...nameDelimiters.map((d) => {
      const idx = flag.indexOf(d);
      if (idx > 0) {
        return idx;
      }
      return flag.length;
    }));
    flag = flag.slice(0, nameCutOffIdx);

    return flag;
  });
}
