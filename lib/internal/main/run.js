'use strict';
/* eslint-disable node-core/prefer-primordials */

// There is no need to add primordials to this file.
// `run.js` is a script only executed when `node --run <script>` is called.
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { getPackageJSONScripts } = internalBinding('modules');
const { execSync } = require('child_process');
const { resolve, delimiter } = require('path');
const { escapeShell } = require('internal/shell');
const { getOptionValue } = require('internal/options');
const { emitExperimentalWarning } = require('internal/util');

prepareMainThreadExecution(false, false);
markBootstrapComplete();
emitExperimentalWarning('Task runner');

// TODO(@anonrig): Search for all package.json's until root folder.
const json_string = getPackageJSONScripts();

// Check if package.json exists and is parseable
if (json_string === undefined) {
  process.exitCode = 1;
  return;
}
const scripts = JSON.parse(json_string);
// Remove the first argument, which are the node binary.
const args = process.argv.slice(1);
const id = getOptionValue('--run');
let command = scripts[id];

if (!command) {
  const { error } = require('internal/console/global');

  error(`Missing script: "${id}"\n`);

  const keys = Object.keys(scripts);
  if (keys.length === 0) {
    error('There are no scripts available in package.json');
  } else {
    error('Available scripts are:');
    for (const script of keys) {
      error(`  ${script}: ${scripts[script]}`);
    }
  }
  process.exit(1);
  return;
}

const env = process.env;
const cwd = process.cwd();
const binPath = resolve(cwd, 'node_modules/.bin');

// Filter all environment variables that contain the word "path"
const keys = Object.keys(env).filter((key) => /^path$/i.test(key));
const PATH = keys.map((key) => env[key]);

// Append only the current folder bin path to the PATH variable.
// TODO(@anonrig): Prepend the bin path of all parent folders.
const paths = [binPath, PATH].join(delimiter);
for (const key of keys) {
  env[key] = paths;
}

// If there are any remaining arguments left, append them to the command.
// This is useful if you want to pass arguments to the script, such as
// `node --run linter -- --help` which runs `biome --check . --help`
if (args.length > 0) {
  command += ' ' + escapeShell(args.map((arg) => arg.trim()).join(' '));
}
execSync(command, { stdio: 'inherit', env, shell: true });
