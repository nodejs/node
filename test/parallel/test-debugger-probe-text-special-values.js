// This tests debugger probe text output for stable special-cased values.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeText } = require('../common/debugger-probe');
const cwd = fixtures.path('debugger');
const probeArg = 'probe-types.js:17';
const hitText = `${fixtures.fileURL('debugger', 'probe-types.js').href}:17:1`;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', probeArg,
  '--expr', 'stringValue',
  '--probe', probeArg,
  '--expr', 'booleanValue',
  '--probe', probeArg,
  '--expr', 'undefinedValue',
  '--probe', probeArg,
  '--expr', 'nullValue',
  '--probe', probeArg,
  '--expr', 'nanValue',
  '--probe', probeArg,
  '--expr', 'bigintValue',
  '--probe', probeArg,
  '--expr', 'symbolValue',
  '--probe', probeArg,
  '--expr', 'functionValue',
  '--probe', probeArg,
  '--expr', 'objectValue',
  '--probe', probeArg,
  '--expr', 'arrayValue',
  '--probe', probeArg,
  '--expr', 'errorValue',
  'probe-types.js',
], { cwd }, {
  stdout(output) {
    assertProbeText(output, [
      `Hit 1 at ${hitText}`,
      '  stringValue = "hello"',
      `Hit 1 at ${hitText}`,
      '  booleanValue = true',
      `Hit 1 at ${hitText}`,
      '  undefinedValue = undefined',
      `Hit 1 at ${hitText}`,
      '  nullValue = null',
      `Hit 1 at ${hitText}`,
      '  nanValue = NaN',
      `Hit 1 at ${hitText}`,
      '  bigintValue = 1n',
      `Hit 1 at ${hitText}`,
      '  symbolValue = Symbol(tag)',
      `Hit 1 at ${hitText}`,
      '  functionValue = () => 1',
      `Hit 1 at ${hitText}`,
      '  objectValue = {alpha: 1, beta: "two"}',
      `Hit 1 at ${hitText}`,
      '  arrayValue = [1, "two", 3]',
      `Hit 1 at ${hitText}`,
      '  errorValue = Error: boom',
      'Completed',
    ].join('\n'));
  },
  trim: true,
});
