'use strict';

const common = require('../common');

// The following tests assert that the node.cc PrintHelp() function
// returns the proper set of cli options when invoked

const assert = require('assert');
const { exec } = require('child_process');
let stdOut;


function startPrintHelpTest() {
  exec(`${process.execPath} --help`, common.mustCall((err, stdout, stderr) => {
    assert.ifError(err);
    stdOut = stdout;
    validateNodePrintHelp();
  }));
}

function validateNodePrintHelp() {
  const config = process.config;
  const HAVE_OPENSSL = common.hasCrypto;
  const NODE_FIPS_MODE = common.hasFipsCrypto;
  const NODE_HAVE_I18N_SUPPORT = common.hasIntl;
  const HAVE_INSPECTOR = config.variables.v8_enable_inspector === 1;

  const cliHelpOptions = [
    { compileConstant: HAVE_OPENSSL,
      flags: [ '--openssl-config=file', '--tls-cipher-list=val',
               '--use-bundled-ca', '--use-openssl-ca' ] },
    { compileConstant: NODE_FIPS_MODE,
      flags: [ '--enable-fips', '--force-fips' ] },
    { compileConstant: NODE_HAVE_I18N_SUPPORT,
      flags: [ '--experimental-modules', '--experimental-vm-modules',
               '--icu-data-dir=dir', '--preserve-symlinks',
               'NODE_ICU_DATA', 'NODE_PRESERVE_SYMLINKS' ] },
    { compileConstant: HAVE_INSPECTOR,
      flags: [ '--inspect-brk[=[host:]port]', '--inspect-port=[host:]port',
               '--inspect[=[host:]port]' ] },
  ];

  cliHelpOptions.forEach(testForSubstring);
}

function testForSubstring(options) {
  if (options.compileConstant) {
    options.flags.forEach((flag) => {
      assert.strictEqual(stdOut.indexOf(flag) !== -1, true);
    });
  } else {
    options.flags.forEach((flag) => {
      assert.strictEqual(stdOut.indexOf(flag), -1);
    });
  }
}

startPrintHelpTest();
