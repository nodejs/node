'use strict';
const common = require('../../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

const bindingPath = path.resolve(
  __dirname, 'build', common.buildType, 'binding.node');

if (!fs.existsSync(bindingPath))
  common.skip('binding not built yet');

if (process.argv[2] === 'child') {
  const binding = require(bindingPath);
  binding.installAbortHandler();
  process.abort();
  return;
}

const escapedArgs =
  common.escapePOSIXShell`"${process.execPath}" "${__filename}" child`;
if (!common.isWindows) {
  // Do not create core files, as it can take a lot of disk space on
  // continuous testing and developers' machines.
  escapedArgs[0] = 'ulimit -c 0 && ' + escapedArgs[0];
}

exec(...escapedArgs, common.mustCall((err, stdout, stderr) => {
  assert.ok(
    stderr.includes('CUSTOM_ABORT_HANDLER_RAN'),
    `Expected custom abort handler marker in stderr, got:\n${stderr}`);
  assert.ok(
    !stderr.includes('Native stack trace'),
    `Expected the custom handler to replace the default dump, got:\n${stderr}`);

  // The child aborts. Whether that surfaces as the SIGABRT signal or as exit
  // code 134 depends on shell wrapping: the `ulimit -c 0 && ...` prefix makes
  // /bin/sh wait on (rather than exec-replace itself with) the node grandchild,
  // so sh reports the aborted grandchild as a normal exit with code 134.
  // common.nodeProcessAborted() accepts both forms.
  assert.ok(
    err && common.nodeProcessAborted(err.code, err.signal),
    `Expected the child to abort, got code=${err?.code} signal=${err?.signal}`);
}));
