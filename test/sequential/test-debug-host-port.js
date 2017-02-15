'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

let run = () => {};
function test(args, needle) {
  const next = run;
  run = () => {
    const options = {encoding: 'utf8'};
    const proc = spawn(process.execPath, args.concat(['-e', '0']), options);
    let stderr = '';
    proc.stderr.setEncoding('utf8');
    proc.stderr.on('data', (data) => {
      stderr += data;
      if (stderr.includes(needle)) proc.kill();
    });
    proc.on('exit', common.mustCall(() => {
      assert(stderr.includes(needle));
      next();
    }));
  };
}

test(['--debug-brk'], 'Debugger listening on 127.0.0.1:5858');
test(['--debug-brk=1234'], 'Debugger listening on 127.0.0.1:1234');
test(['--debug-brk=0.0.0.0'], 'Debugger listening on 0.0.0.0:5858');
test(['--debug-brk=0.0.0.0:1234'], 'Debugger listening on 0.0.0.0:1234');
test(['--debug-brk=localhost'], 'Debugger listening on 127.0.0.1:5858');
test(['--debug-brk=localhost:1234'], 'Debugger listening on 127.0.0.1:1234');

if (common.hasIPv6) {
  test(['--debug-brk=::'], 'Debug port must be in range 1024 to 65535');
  test(['--debug-brk=::0'], 'Debug port must be in range 1024 to 65535');
  test(['--debug-brk=::1'], 'Debug port must be in range 1024 to 65535');
  test(['--debug-brk=[::]'], 'Debugger listening on [::]:5858');
  test(['--debug-brk=[::0]'], 'Debugger listening on [::]:5858');
  test(['--debug-brk=[::]:1234'], 'Debugger listening on [::]:1234');
  test(['--debug-brk=[::0]:1234'], 'Debugger listening on [::]:1234');
  test(['--debug-brk=[::ffff:127.0.0.1]:1234'],
       'Debugger listening on [::ffff:127.0.0.1]:1234');
}

run();  // Runs tests in reverse order.
