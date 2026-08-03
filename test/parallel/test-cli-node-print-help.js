// Flags: --expose-internals
'use strict';

const common = require('../common');

// The following tests assert that the node.cc PrintHelp() function
// returns the proper set of cli options when invoked

const assert = require('assert');
const { exec, spawn } = require('child_process');
const { once } = require('events');
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
               '--enable-fips', '--force-fips' ] },
    { compileConstant: NODE_HAVE_I18N_SUPPORT,
      flags: [ '--icu-data-dir=...', 'NODE_ICU_DATA' ] },
    { compileConstant: HAVE_INSPECTOR,
      flags: [ '--inspect-brk[=[host:]port]', '--inspect-port=[host:]port',
               '--inspect[=[host:]port]' ] },
  ];

  cliHelpOptions.forEach(testForSubstring);

  validateEnvVarSection();
}

// Verify that the "Environment variables:" section lists the expected
// entries. Each entry is printed at the start of its own line, so matching
// against the start of a line avoids false positives from names that also
// appear inside an option's description (e.g. `--icu-data-dir` mentions
// "overrides NODE_ICU_DATA" and `--openssl-config` mentions
// "overrides OPENSSL_CONF"). These checks guard against malformed
// construction of the environment-variable list that would silently drop
// entries from the help output.
function validateEnvVarSection() {
  const { internalBinding } = require('internal/test/binding');
  const { hasNodeOptions } = internalBinding('config');

  const start = stdOut.indexOf('Environment variables:');
  assert.ok(start !== -1, 'Missing "Environment variables:" section');
  const end = stdOut.indexOf('Documentation can be found', start);
  const envSection = stdOut.slice(start, end === -1 ? undefined : end);

  const expectedEnvVars = [
    { name: 'FORCE_COLOR', present: true },
    { name: 'NODE_PATH', present: true },
    { name: 'NODE_ICU_DATA', present: common.hasIntl },
    { name: 'NODE_OPTIONS', present: hasNodeOptions },
    { name: 'OPENSSL_CONF', present: common.hasCrypto },
    { name: 'SSL_CERT_DIR', present: common.hasCrypto },
    { name: 'SSL_CERT_FILE', present: common.hasCrypto },
  ];

  for (const { name, present } of expectedEnvVars) {
    const re = new RegExp(`^${name}\\b`, 'm');
    if (present) {
      assert.match(envSection, re,
                   `Missing environment variable ${name} in:\n${envSection}`);
    } else {
      assert.doesNotMatch(envSection, re,
                          `Unexpected environment variable ${name} in:\n${envSection}`);
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
