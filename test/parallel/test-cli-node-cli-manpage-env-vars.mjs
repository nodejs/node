import '../common/index.mjs';
import assert from 'node:assert';
import { createReadStream } from 'node:fs';
import { createInterface } from 'node:readline';
import { resolve, join } from 'node:path';

// This test checks that all the environment variables defined in the public CLI documentation (doc/api/cli.md)
// are also documented in the manpage file (doc/node.1) and vice-versa (that all the environment variables
// in the manpage are present in the CLI documentation)

const rootDir = resolve(import.meta.dirname, '..', '..');

const cliMdEnvVarNames = await collectCliMdEnvVarNames();
const manpageEnvVarNames = await collectManPageEnvVarNames();

assert(cliMdEnvVarNames.size > 0,
       'Unexpectedly not even a single env variable was detected when scanning the `doc/api/cli.md` file'
);

assert(manpageEnvVarNames.size > 0,
       'Unexpectedly not even a single env variable was detected when scanning the `doc/node.1` file'
);

for (const envVarName of cliMdEnvVarNames) {
  if (!manpageEnvVarNames.has(envVarName) && !knownEnvVariablesMissingFromManPage.has(envVarName)) {
    assert.fail(`The "${envVarName}" environment variable (present in \`doc/api/cli.md\`) is missing from the \`doc/node.1\` file`);
  }
  manpageEnvVarNames.delete(envVarName);
}

if (manpageEnvVarNames.size > 0) {
  assert.fail(`The following env variables are present in the \`doc/node.1\` file but not in the \`doc/api/cli.md\` file: ${
    [...manpageEnvVarNames].map((name) => `"${name}"`).join(', ')
  }`);
}

async function collectManPageEnvVarNames() {
  const manPagePath = join(rootDir, 'doc', 'node.1');
  const fileStream = createReadStream(manPagePath);

  const rl = createInterface({
    input: fileStream,
  });

  const envVarNames = new Set();

  for await (const line of rl) {
    const match = line.match(/^\.It Ev (?<envName>[^ ]*)/);
    if (match) {
      envVarNames.add(match.groups.envName);
    }
  }

  return envVarNames;
}

async function collectCliMdEnvVarNames() {
  const cliMdPath = join(rootDir, 'doc', 'api', 'cli.md');
  const fileStream = createReadStream(cliMdPath);

  let insideEnvVariablesSection = false;

  const rl = createInterface({
    input: fileStream,
  });

  const envVariableRE = /^### `(?<varName>[^`]*?)(?:=[^`]+)?`$/;

  const envVarNames = new Set();

  for await (const line of rl) {
    if (line.startsWith('## ')) {
      if (insideEnvVariablesSection) {
        // We were in the environment variables section and we're now exiting it,
        // so there is no need to keep checking the remaining lines,
        // we might as well close the stream and return
        fileStream.close();
        return envVarNames;
      }

      // We've just entered the options section
      insideEnvVariablesSection = line === '## Environment variables';
      continue;
    }

    if (insideEnvVariablesSection) {
      const match = line.match(envVariableRE);
      if (match) {
        const { varName } = match.groups;
        envVarNames.add(varName);
      }
    }
  }

  return envVarNames;
}
