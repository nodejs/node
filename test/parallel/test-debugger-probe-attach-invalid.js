// Tests that probe mode rejects invalid --pid, --host and --port usage.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { assertProbeCliError } = require('../common/debugger-probe');

const probe = ['--probe', 'app.js:1', '--expr', 'x'];

assertProbeCliError([...probe, '--pid', '123', 'app.js'], /--pid and --host cannot be used together with an entry point script/);
assertProbeCliError([...probe, '--host', '127.0.0.1', 'app.js'], /--pid and --host cannot be used together with an entry point script/);

assertProbeCliError([...probe, '--port', '0'], /Cannot attach to port 0/);
assertProbeCliError(probe,
                    /Probe mode requires an entry point script, or --pid, --port or --host to attach to a running process/);
assertProbeCliError([...probe, '--pid', 'abc'], /Invalid pid: abc/);
