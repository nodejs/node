// Flags: --expose-internals
'use strict';

const common = require('../common');

// The following tests assert that the node.cc PrintHelp() function
// returns the proper set of cli options when invoked

const assert = require('assert');
const { exec, spawn } = require('child_process');
const { once } = require('events');
const { stripVTControlCharacters } = require('util');
const { internalBinding } = require('internal/test/binding');
let stdOut;

function startPrintHelpTest() {
  exec(...common.escapePOSIXShell`"${process.execPath}" --help`, common.mustSucceed((stdout, stderr) => {
    stdOut = stdout;
    validateNodePrintHelp();
  }));
}

function validateNodePrintHelp() {
  const HAVE_OPENSSL = common.hasCrypto;
  const NODE_HAVE_I18N_SUPPORT = common.hasIntl;
  const HAVE_INSPECTOR = process.features.inspector;

  const cliHelpOptions = [
    { compileConstant: HAVE_OPENSSL,
      flags: [ '--openssl-config=...', '--tls-cipher-list=...',
               '--use-bundled-ca', '--use-openssl-ca', '--use-system-ca',
               '--enable-fips', '--enable-fips-indicator-events',
               '--force-fips' ] },
    { compileConstant: NODE_HAVE_I18N_SUPPORT,
      flags: [ '--icu-data-dir=...', 'NODE_ICU_DATA' ] },
    { compileConstant: HAVE_INSPECTOR,
      flags: [ '--inspect-brk[=[host:]port]', '--inspect-port=[host:]port',
               '--inspect[=[host:]port]' ] },
  ];

  cliHelpOptions.forEach(testForSubstring);

  // The usage section lists every top-level run mode.
  const usage = stripVTControlCharacters(stdOut.slice(0, stdOut.indexOf('\n\n')));
  const usageLines = usage.split('\n').map((line) => line.replace(/^(?:Usage:)?\s+/, ''));
  assert.deepStrictEqual(usageLines, [
    'node [options] [ script.js ] [arguments]',
    'node [options] -e "script" [arguments]',
    'node [options] -p "script" [arguments]',
    'node inspect [options] [ script.js | host:port ] [arguments]',
    'node --run [ script ] [-- arguments]',
    'node --watch [options] script.js [arguments]',
    'node --test [options] [ patterns... ]',
    'node --bench --experimental-bench [options] patterns...',
  ]);

  // Environment variables are listed at the start of a line in their own
  // section. Matching on line starts avoids false positives from option help
  // text that mentions them (e.g. --icu-data-dir mentions NODE_ICU_DATA).
  const envSection = stripVTControlCharacters(
    stdOut.slice(stdOut.indexOf('Environment variables:')));
  const { hasNodeOptions } = internalBinding('config');
  const envVars = [
    { compileConstant: HAVE_OPENSSL,
      names: [ 'OPENSSL_CONF', 'SSL_CERT_DIR', 'SSL_CERT_FILE' ] },
    { compileConstant: NODE_HAVE_I18N_SUPPORT, names: [ 'NODE_ICU_DATA' ] },
    { compileConstant: hasNodeOptions, names: [ 'NODE_OPTIONS' ] },
  ];
  for (const { compileConstant, names } of envVars) {
    for (const name of names) {
      const listed = new RegExp(`^${name}\\s`, 'm').test(envSection);
      assert.strictEqual(listed, Boolean(compileConstant),
                         `${name} listed: ${listed} in ${envSection}`);
    }
  }
}

function testForSubstring(options) {
  if (options.compileConstant) {
    for (const flag of options.flags) {
      assert.notStrictEqual(stdOut.indexOf(flag), -1,
                            `Missing flag ${flag} in ${stdOut}`);
    }
  } else {
    for (const flag of options.flags) {
      assert.strictEqual(stdOut.indexOf(flag), -1,
                         `Unexpected flag ${flag} in ${stdOut}`);
    }
  }
}

startPrintHelpTest();

// Test closed stdout for `node --help`. Like `node --help | head -n5`.
(async () => {
  const cp = spawn(process.execPath, ['--help'], {
    stdio: ['inherit', 'pipe', 'inherit'],
  });
  cp.stdout.destroy();
  const [exitCode] = await once(cp, 'exit');
  assert.strictEqual(exitCode, 0);
})().then(common.mustCall());
