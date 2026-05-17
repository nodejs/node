// This tests debugger probe text output for stable special-cased values.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const {
  assertProbeText,
  probeTypesScript,
} = require('../common/debugger-probe');

const location = `${probeTypesScript}:17`;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', location,
  '--expr', 'stringValue',
  '--probe', location,
  '--expr', 'booleanValue',
  '--probe', location,
  '--expr', 'undefinedValue',
  '--probe', location,
  '--expr', 'nullValue',
  '--probe', location,
  '--expr', 'nanValue',
  '--probe', location,
  '--expr', 'bigintValue',
  '--probe', location,
  '--expr', 'symbolValue',
  '--probe', location,
  '--expr', 'functionValue',
  '--probe', location,
  '--expr', 'objectValue',
  '--probe', location,
  '--expr', 'arrayValue',
  '--probe', location,
  '--expr', 'errorValue',
  probeTypesScript,
], {
  stdout(output) {
    assertProbeText(output, [
      `Hit 1 at ${location}`,
      '  stringValue = "hello"',
      `Hit 1 at ${location}`,
      '  booleanValue = true',
      `Hit 1 at ${location}`,
      '  undefinedValue = undefined',
      `Hit 1 at ${location}`,
      '  nullValue = null',
      `Hit 1 at ${location}`,
      '  nanValue = NaN',
      `Hit 1 at ${location}`,
      '  bigintValue = 1n',
      `Hit 1 at ${location}`,
      '  symbolValue = Symbol(tag)',
      `Hit 1 at ${location}`,
      '  functionValue = () => 1',
      `Hit 1 at ${location}`,
      '  objectValue = {alpha: 1, beta: "two"}',
      `Hit 1 at ${location}`,
      '  arrayValue = [1, "two", 3]',
      `Hit 1 at ${location}`,
      '  errorValue = Error: boom',
      'Completed',
    ].join('\n'));
  },
  trim: true,
});
