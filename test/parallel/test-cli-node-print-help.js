// Flags: --expose-internals
'use strict';

const common = require('../common');

// The following tests assert that the node.cc PrintHelp() function
// returns the proper set of cli options when invoked

const assert = require('assert');
const { exec, spawn } = require('child_process');
const { once } = require('events');
let stdOut;

// The execPath might contain chars that should be escaped in a shell context.
// On non-Windows, we can pass the path via the env; `"` is not a valid char on
// Windows, so we can simply pass the path.
const execNode = (args, callback) => exec(
  `"${common.isWindows ? process.execPath : '$NODE'}" ${args}`,
  common.isWindows ? undefined : { env: { ...process.env, NODE: process.execPath } },
  callback,
);


function startPrintHelpTest() {
  execNode('--help', common.mustSucceed((stdout, stderr) => {
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
               '--use-bundled-ca', '--use-openssl-ca',
               '--enable-fips', '--force-fips' ] },
    { compileConstant: NODE_HAVE_I18N_SUPPORT,
      flags: [ '--icu-data-dir=...', 'NODE_ICU_DATA' ] },
    { compileConstant: HAVE_INSPECTOR,
      flags: [ '--inspect-brk[=[host:]port]', '--inspect-port=[host:]port',
               '--inspect[=[host:]port]' ] },
  ];

  cliHelpOptions.forEach(testForSubstring);
}

function testForSubstring(options) {
  if (options.compileConstant) {
    options.flags.forEach((flag) => {
      assert.strictEqual(stdOut.indexOf(flag) !== -1, true,
                         `Missing flag ${flag} in ${stdOut}`);
    });
  } else {
    options.flags.forEach((flag) => {
      assert.strictEqual(stdOut.indexOf(flag), -1,
                         `Unexpected flag ${flag} in ${stdOut}`);
    });
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
