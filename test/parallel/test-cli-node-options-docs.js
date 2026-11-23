'use strict';
const common = require('../common');
if (process.config.variables.node_without_node_options)
  common.skip('missing NODE_OPTIONS support');

// Test options specified by env variable.

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const rootDir = path.resolve(__dirname, '..', '..');
const cliMd = path.join(rootDir, 'doc', 'api', 'cli.md');
const cliText = fs.readFileSync(cliMd, { encoding: 'utf8' });

const internalApiMd = path.join(rootDir, 'doc', 'contributing', 'internal-api.md');
const internalApiText = fs.readFileSync(internalApiMd, { encoding: 'utf8' });

const nodeOptionsCC = fs.readFileSync(path.resolve(rootDir, 'src', 'node_options.cc'), 'utf8');
const addOptionRE = /AddOption[\s\n\r]*\([\s\n\r]*"([^"]+)"(.*?)\);/gs;

const nodeOptionsText = cliText.match(/<!-- node-options-node start -->(.*)<!-- node-options-others end -->/s)[1];
const v8OptionsText = cliText.match(/<!-- v8-options start -->(.*)<!-- v8-options end -->/s)[1];

const manPage = path.join(rootDir, 'doc', 'node.1');
const manPageText = fs.readFileSync(manPage, { encoding: 'utf8' });

// Documented in /doc/api/deprecations.md
const deprecated = [
  '--debug',
  '--debug-brk',
];


const nodeOptionsH = fs.readFileSync(path.resolve(rootDir, 'src', 'node_options.h'), 'utf8');
const helpSectionList = nodeOptionsH.slice(
  nodeOptionsH.indexOf('#define OPTION_HELP_SECTION_LIST'),
  nodeOptionsH.indexOf('enum class HelpSection'));
const helpSectionTitles = new Map(
  Array.from(helpSectionList.matchAll(/\bV\((k\w+), "([^"]+)"\)/g), (m) => [m[1], m[2]]));
assert.ok(helpSectionTitles.size > 0);
// Option name -> title of the --help section node_options.cc assigns it to.
const optionHelpSections = new Map();

const manPagesOptions = new Set();

for (const [, envVar] of manPageText.matchAll(/\.It Fl (-[a-zA-Z0-9._-]+)/g)) {
  manPagesOptions.add(envVar);
}

for (const [, envVar, config] of nodeOptionsCC.matchAll(addOptionRE)) {
  const hasTrueAsDefaultValue = /,\s*(?:true|HAVE_[A-Z_]+)\s*$/.test(config);
  const isInNodeOption = config.includes('kAllowedInEnvvar') && !config.includes('kDisallowedInEnvvar');
  const isV8Option = config.includes('V8Option{}');
  const isNoOp = config.includes('NoOp{}');
  const helpSection = /HelpSection::(k\w+)/.exec(config)?.[1];
  assert.ok(helpSectionTitles.has(helpSection),
            `${envVar} has unknown help section ${helpSection}`);
  optionHelpSections.set(envVar, helpSectionTitles.get(helpSection));

  if (
    envVar.startsWith('[') ||
    deprecated.includes(envVar) ||
    isNoOp
  ) {
    // assert(!manPagesOptions.has(envVar.slice(1)), `Option ${envVar} should not be documented`)
    manPagesOptions.delete(envVar.slice(1));
    continue;
  }

  // Internal API options are documented in /doc/contributing/internal-api.md
  if (new RegExp(`####.*\`${RegExp.escape(envVar)}[[=\\s\\b\`]`).test(internalApiText)) {
    manPagesOptions.delete(envVar.slice(1));
    continue;
  }

  // CLI options
  if (!isV8Option && !hasTrueAsDefaultValue) {
    if (!new RegExp(`###.*\`${RegExp.escape(envVar)}[[=\\s\\b\`]`).test(cliText)) {
      assert.fail(`Should have option ${envVar} documented`);
    } else {
      manPagesOptions.delete(envVar.slice(1));
    }
  }

  if (!hasTrueAsDefaultValue && new RegExp(`###.*\`--no${RegExp.escape(envVar.slice(1))}[[=\\s\\b\`]`).test(cliText)) {
    assert.fail(`Should not have option --no${envVar.slice(1)} documented`);
  }

  if (!isV8Option && hasTrueAsDefaultValue) {
    if (!new RegExp(`###.*\`--no${RegExp.escape(envVar.slice(1))}[[=\\s\\b\`]`).test(cliText)) {
      assert.fail(`Should have option --no${envVar.slice(1)} documented`);
    } else {
      manPagesOptions.delete(`-no${envVar.slice(1)}`);
    }
  }

  // NODE_OPTIONS
  if (isInNodeOption && !hasTrueAsDefaultValue &&
    !new RegExp(`\`${RegExp.escape(envVar)}\``).test(nodeOptionsText)) {
    assert.fail(`Should have option ${envVar} in NODE_OPTIONS documented`);
  }

  if (isInNodeOption && hasTrueAsDefaultValue && !new RegExp(`\`--no${RegExp.escape(envVar.slice(1))}\``).test(cliText)) {
    assert.fail(`Should have option --no${envVar.slice(1)} in NODE_OPTIONS documented`);
  }

  if (!hasTrueAsDefaultValue && new RegExp(`\`--no${RegExp.escape(envVar.slice(1))}\``).test(cliText)) {
    assert.fail(`Should not have option --no${envVar.slice(1)} in NODE_OPTIONS documented`);
  }

  // V8 options
  if (isV8Option) {
    if (!new RegExp(`###.*\`${RegExp.escape(envVar)}[[=\\s\\b\`]`).test(v8OptionsText)) {
      assert.fail(`Should have option ${envVar} in V8 options documented`);
    } else {
      manPagesOptions.delete(envVar.slice(1));
    }
  }
}

{
  // Entries are sorted within each `##` section, and within each `###` group
  // of entries (such as the option sections under `## Options`).
  const sections = /^(?:## |### (?!`))(.+)$/mg;
  const cliOptionPattern = /^####? (?:`-\w.*`, )?`([^`]+)`/mg;
  let match;
  let previousIndex = 0;
  do {
    const sectionTitle = match?.[1];
    match = sections.exec(cliText);
    const filteredCLIText = cliText.slice(previousIndex, match?.index);
    const options = Array.from(filteredCLIText.matchAll(cliOptionPattern), (match) => match[1]);
    assert.deepStrictEqual(options, options.toSorted(), `doc/api/cli.md ${sectionTitle} subsections are not in alphabetical order`);
    previousIndex = match?.index;
  } while (match);
}

// Options under `## Options` are grouped into `###` sections that match the
// --help sections node_options.cc assigns them to. Each section heading
// carries its label as `help-section` YAML metadata.
{
  const lines = cliText.split('\n');
  const optionsLines = lines.slice(lines.indexOf('## Options'),
                                   lines.indexOf('## Environment variables'));
  const knownTitles = new Set(helpSectionTitles.values());
  let section;
  for (const [index, line] of optionsLines.entries()) {
    const [, depth, heading] = /^(###|####) (.+)$/.exec(line) ?? [];
    if (depth === undefined) continue;
    if (depth === '###') {
      const yaml = optionsLines.slice(index + 1, index + 7).join('\n');
      const label = /^<!-- YAML\nhelp-section: (.+)\n-->/m.exec(yaml)?.[1];
      assert.strictEqual(label, heading,
                         `doc/api/cli.md section "${heading}" needs help-section: ${heading} YAML`);
      assert.ok(knownTitles.has(label),
                `doc/api/cli.md section "${label}" is not a help section in src/node_options.h`);
      section = label;
      continue;
    }
    assert.ok(section, `doc/api/cli.md ${heading} is not inside a section`);
    for (const [, flag] of heading.matchAll(/`(-[-\w.]*)/g)) {
      const expected = optionHelpSections.get(flag) ??
        optionHelpSections.get(flag.replace(/^--no-/, '--'));
      if (expected === undefined) continue;
      assert.strictEqual(section, expected,
                         `doc/api/cli.md documents ${flag} under "${section}", ` +
                         `but src/node_options.cc puts it in "${expected}"`);
      break;
    }
  }
}

// add alias handling
manPagesOptions.delete('-trace-events-enabled');

assert.strictEqual(manPagesOptions.size, 0, `Man page options not documented: ${[...manPagesOptions]}`);
