'use strict';

const common = require('../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const rootDir = path.resolve(__dirname, '..', '..');
const cliMd = path.join(rootDir, 'doc', 'api', 'cli.md');
const cliText = fs.readFileSync(cliMd, { encoding: 'utf8' });

const parseSection = (text, startMarker, endMarker) => {
  const regExp = new RegExp(`${startMarker}\r?\n([^]*)\r?\n${endMarker}`);
  const match = text.match(regExp);
  assert(match,
         `Unable to locate text between '${startMarker}' and '${endMarker}'.`);
  return match[1].split(/\r?\n/);
};

const nodeOptionsLines = parseSection(cliText,
                                      '<!-- node-options-node start -->',
                                      '<!-- node-options-node end -->');
const v8OptionsLines = parseSection(cliText,
                                    '<!-- node-options-v8 start -->',
                                    '<!-- node-options-v8 end -->');
// Check the options are documented in alphabetical order.
assert.deepStrictEqual(nodeOptionsLines, [...nodeOptionsLines].sort());
assert.deepStrictEqual(v8OptionsLines, [...v8OptionsLines].sort());

const documented = new Set();
for (const line of [...nodeOptionsLines, ...v8OptionsLines]) {
  for (const match of line.matchAll(/`(-[^`]+)`/g)) {
    const option = match[1];
    assert(!documented.has(option),
           `Option '${option}' was documented more than once as an ` +
           `allowed option for NODE_OPTIONS in ${cliMd}.`);
    documented.add(option);
  }
}

// Filter out options that are conditionally present.
const conditionalOpts = [
  { include: common.hasCrypto,
    filter: (opt) => {
      return ['--openssl-config', '--tls-cipher-list', '--use-bundled-ca',
              '--use-openssl-ca' ].includes(opt);
    } },
  {
    // We are using openssl_is_fips from the configuration because it could be
    // the case that OpenSSL is FIPS compatible but fips has not been enabled
    // (starting node with --enable-fips). If we use common.hasFipsCrypto
    // that would only tells us if fips has been enabled, but in this case we
    // want to check options which will be available regardless of whether fips
    // is enabled at runtime or not.
    include: process.config.variables.openssl_is_fips,
    filter: (opt) => opt.includes('-fips') },
  { include: common.hasIntl,
    filter: (opt) => opt === '--icu-data-dir' },
  { include: process.features.inspector,
    filter: (opt) => opt.startsWith('--inspect') || opt === '--debug-port' },
];
documented.forEach((opt) => {
  conditionalOpts.forEach(({ include, filter }) => {
    if (!include && filter(opt)) {
      documented.delete(opt);
    }
  });
});

const difference = (setA, setB) => {
  return new Set([...setA].filter((x) => !setB.has(x)));
};

const overdocumented = difference(documented,
                                  process.allowedNodeEnvironmentFlags);
assert.strictEqual(overdocumented.size, 0,
                   'The following options are documented as allowed in ' +
                   `NODE_OPTIONS in ${cliMd}: ` +
                   `${[...overdocumented].join(' ')} ` +
                   'but are not in process.allowedNodeEnvironmentFlags');
const undocumented = difference(process.allowedNodeEnvironmentFlags,
                                documented);
// Remove intentionally undocumented options.
assert(undocumented.delete('--debug-arraybuffer-allocations'));
assert(undocumented.delete('--es-module-specifier-resolution'));
assert(undocumented.delete('--experimental-report'));
assert(undocumented.delete('--experimental-worker'));
assert(undocumented.delete('--no-node-snapshot'));
assert(undocumented.delete('--loader'));

assert.strictEqual(undocumented.size, 0,
                   'The following options are not documented as allowed in ' +
                   `NODE_OPTIONS in ${cliMd}: ${[...undocumented].join(' ')}`);
