'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { test } = require('node:test');

// Skip test if NODE_OPTIONS support is missing
if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const rootDir = path.resolve(__dirname, '..', '..');
const cliFilePath = path.join(rootDir, 'doc', 'api', 'cli.md');
const internalApiFilePath = path.join(rootDir, 'doc', 'contributing', 'internal-api.md');
const manPagePath = path.join(rootDir, 'doc', 'node.1');
const nodeOptionsCCPath = path.join(rootDir, 'src', 'node_options.cc');

// Read files
const cliText = fs.readFileSync(cliFilePath, 'utf8');
const internalApiText = fs.readFileSync(internalApiFilePath, 'utf8');
const manPageText = fs.readFileSync(manPagePath, 'utf8');
const nodeOptionsCC = fs.readFileSync(nodeOptionsCCPath, 'utf8');

// Regular Expressions
const addOptionRE = /AddOption[\s\n\r]*\([\s\n\r]*"([^"]+)"(.*?)\);/gs;
const nodeOptionsText = cliText.match(/<!-- node-options-node start -->(.*)<!-- node-options-others end -->/s)[1];
const v8OptionsText = cliText.match(/<!-- v8-options start -->(.*)<!-- v8-options end -->/s)[1];

// Deprecated options
const deprecatedOptions = new Set(['--debug', '--debug-brk']);

// Helper Functions
const isOptionDocumentedInCli = (envVar) => new RegExp(`###.*\`${envVar}[[=\\s\\b\`]`).test(cliText);
const isOptionDocumentedInInternalApi = (envVar) => new RegExp(`####.*\`${envVar}[[=\\s\\b\`]`).test(internalApiText);
const isOptionDocumentedInV8 = (envVar) => new RegExp(`###.*\`${envVar}[[=\\s\\b\`]`).test(v8OptionsText);
const isOptionInNodeOptions = (envVar) => new RegExp(`\`${envVar}\``).test(nodeOptionsText);

const validateOption = (envVar, config) => {
  let hasTrueAsDefault = false;
  let isInNodeOptions = false;
  let isV8Option = false;
  const isNoOp = config.includes('NoOp{}');

  // Check option categories
  if (config.includes('kAllowedInEnvvar')) isInNodeOptions = true;
  if (config.includes('V8Option{}')) isV8Option = true;
  if (/^\s*true\s*$/.test(config.split(',').pop())) hasTrueAsDefault = true;

  const manpageEntry = hasTrueAsDefault ? `-no${envVar.slice(1)}` : envVar.slice(1);
  if (envVar.startsWith('[') || deprecatedOptions.has(envVar) || isNoOp) {
    return;
  }

  if (isOptionDocumentedInInternalApi(envVar)) {
    return;
  }

  if (!isV8Option && !hasTrueAsDefault && !isOptionDocumentedInCli(envVar)) {
    assert.fail(`Should have option ${envVar} documented`);
  }

  if (!hasTrueAsDefault && isOptionDocumentedInCli(`--no${envVar.slice(1)}`)) {
    assert.fail(`Should not have option --no${envVar.slice(1)} documented`);
  }

  if (!isV8Option && hasTrueAsDefault && !isOptionDocumentedInCli(`--no${envVar.slice(1)}`)) {
    assert.fail(`Should have option --no${envVar.slice(1)} documented`);
  }

  if (isInNodeOptions && !hasTrueAsDefault && !isOptionInNodeOptions(envVar)) {
    assert.fail(`Should have option ${envVar} in NODE_OPTIONS documented`);
  }

  if (isInNodeOptions && hasTrueAsDefault && !isOptionInNodeOptions(`--no${envVar.slice(1)}`)) {
    assert.fail(`Should have option --no${envVar.slice(1)} in NODE_OPTIONS documented`);
  }

  if (isV8Option && !isOptionDocumentedInV8(envVar)) {
    assert.fail(`Should have option ${envVar} in V8 options documented`);
  }

  assert(manPageText.includes(manpageEntry), `Should have option ${envVar} in node.1`);
};

// Parse node options from source file
for (const [, envVar, config] of nodeOptionsCC.matchAll(addOptionRE)) {
  test(envVar, () => validateOption(envVar, config));
}
