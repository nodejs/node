// This tests --build-sea with the "executable" field pointing to a copied Node.js binary.

'use strict';

require('../common');
const { buildSEA, skipIfBuildSEAIsNotSupported } = require('../common/sea');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const fs = require('fs');

skipIfBuildSEAIsNotSupported();

tmpdir.refresh();

// Copy fixture files to working directory.
const fixtureDir = fixtures.path('sea', 'executable-field');
fs.cpSync(fixtureDir, tmpdir.path, { recursive: true });

// Copy the Node.js executable to the working directory.
const executableName = process.platform === 'win32' ? 'copy.exe' : 'copy';
fs.copyFileSync(process.execPath, tmpdir.resolve(executableName));

// Update the config to use the correct executable name and output extension.
const configPath = tmpdir.resolve('sea-config.json');
const config = JSON.parse(fs.readFileSync(configPath, 'utf-8'));
config.executable = executableName;
if (process.platform === 'win32') {
  if (!config.output.endsWith('.exe')) {
    config.output += '.exe';
  }
}
fs.writeFileSync(configPath, JSON.stringify(config, null, 2));

// Build the SEA.
const outputFile = buildSEA(tmpdir.path, { workingDir: tmpdir.path });

spawnSyncAndAssert(
  outputFile,
  [],
  {
    cwd: tmpdir.path,
  },
  {
    stdout: 'Hello from SEA with executable field!\n',
  },
);
