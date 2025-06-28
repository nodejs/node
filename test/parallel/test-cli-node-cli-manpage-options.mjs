import '../common/index.mjs';
import assert from 'assert';
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

// TODO(dario-piotrowicz): add the missing flags to the node.1 and remove this set
const knownFlagsMissingFromManPage = new Set([
  'build-snapshot',
  'build-snapshot-config',
  'disable-sigusr1',
  'disable-warning',
  'dns-result-order',
  'enable-network-family-autoselection',
  'env-file-if-exists',
  'env-file',
  'experimental-network-inspection',
  'experimental-print-required-tla',
  'experimental-require-module',
  'experimental-sea-config',
  'experimental-worker-inspection',
  'expose-gc',
  'force-node-api-uncaught-exceptions-policy',
  'import',
  'network-family-autoselection-attempt-timeout',
  'no-async-context-frame',
  'no-experimental-detect-module',
  'no-experimental-global-navigator',
  'no-experimental-require-module',
  'no-network-family-autoselection',
  'openssl-legacy-provider',
  'openssl-shared-config',
  'report-dir',
  'report-directory',
  'report-exclude-env',
  'report-exclude-network',
  'run',
  'snapshot-blob',
  'trace-env',
  'trace-env-js-stack',
  'trace-env-native-stack',
  'trace-require-module',
  'use-system-ca',
  'watch-preserve-output',
]);

const optionsEncountered = { dash: 0, dashDash: 0, named: 0 };
let insideOptionsSection = false;

const rl = createInterface({
  input: cliMdContentsStream,
});

const isOptionLineRegex = /^###( `[^`]*`(,)?)*$/;

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
    if (line === '### `-`') {
      if (!manPageContents.includes('\n.It Sy -\n')) {
        throw new Error(`The \`-\` flag is missing in the \`doc/node.1\` file`);
      }
      optionsEncountered.dash++;
      continue;
    }

    if (line === '### `--`') {
      if (!manPageContents.includes('\n.It Fl -\n')) {
        throw new Error(`The \`--\` flag is missing in the \`doc/node.1\` file`);
      }
      optionsEncountered.dashDash++;
      continue;
    }

    const flagNames = extractFlagNames(line);

    optionsEncountered.named += flagNames.length;

    const manLine = `.It ${flagNames
        .map((flag) => `Fl ${flag.length > 1 ? '-' : ''}${flag}`)
        .join(' , ')}`;

    if (
      // Note: we don't check the full line (note the `\n` only at the beginning) because
      //       options can have arguments and we do want to ignore those
      !manPageContents.includes(`\n${manLine}`) &&
        !flagNames.every((flag) => knownFlagsMissingFromManPage.has(flag))) {
      assert.fail(
        `The following flag${
          flagNames.length === 1 ? '' : 's'
        } (present in \`doc/api/cli.md\`) ${flagNames.length === 1 ? 'is' : 'are'} missing in the \`doc/node.1\` file: ${
          flagNames.map((flag) => `"${flag}"`).join(', ')
        }`
      );
    }
  }
}

assert.strictEqual(optionsEncountered.dash, 1);

assert.strictEqual(optionsEncountered.dashDash, 1);

assert(optionsEncountered.named > 0,
       'Unexpectedly not even a single cli flag/option was detected when scanning the `doc/cli.md` file'
);

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
    // Remove the backticks from the flag
    flag = flag.slice(1, -1);

    // Remove the dash or dashes
    flag = flag.replace(/^--?/, '');

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
