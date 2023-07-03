'use strict';

const {
  JSONParse,
  ObjectKeys
} = primordials;

const {
  prepareMainThreadExecution,
  markBootstrapComplete
} = require('internal/process/pre_execution');
const { internalModuleReadJSON } = internalBinding('fs');
const { execSync } = require('child_process');
const { getOptionValue } = require('internal/options');
const { log, error } = console;

prepareMainThreadExecution(false, false);

markBootstrapComplete();

const result = internalModuleReadJSON('package.json');
if (result.length === 0) {
  error(`Can't read package.json`);
  process.exit(1);
}

const json = JSONParse(result[0]);
const id = getOptionValue('--run');
const command = json.scripts[id];

if (!command) {
  error(`Missing script: "${id}"`);
  error('Available scripts are:\n');
  for (const script of ObjectKeys(json.scripts)) {
    error(`  ${script}: ${json.scripts[script]}`);
  }
  process.exit(1);
}

log('');
log('>', id);
log('>', command);
log('');

execSync(command, { stdio: 'inherit' });
