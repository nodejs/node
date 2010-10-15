// first things first, set the timezone; see tzset(3)
process.env.TZ = 'Europe/Amsterdam';

assert = require('assert');
spawn = require('child_process').spawn;

// time difference between Greenwich and Amsterdam is +2 hours in the summer
date = new Date('Fri, 10 Sep 1982 03:15:00 GMT');
assert.equal(3, date.getUTCHours());
assert.equal(5, date.getHours());

// changes in environment should be visible to child processes
if (process.argv[2] == 'you-are-the-child') {
  // failed assertion results in process exiting with status code 1
  assert.equal(false, 'NODE_PROCESS_ENV_DELETED' in process.env);
  assert.equal(42, process.env.NODE_PROCESS_ENV);
  process.exit(0);
} else {
  process.env.NODE_PROCESS_ENV = 42;
  assert.equal(42, process.env.NODE_PROCESS_ENV);

  process.env.NODE_PROCESS_ENV_DELETED = 42;
  assert.equal(true, 'NODE_PROCESS_ENV_DELETED' in process.env);

  delete process.env.NODE_PROCESS_ENV_DELETED;
  assert.equal(false, 'NODE_PROCESS_ENV_DELETED' in process.env);

  child = spawn(process.argv[0], [process.argv[1], 'you-are-the-child']);
  child.stdout.on('data', function(data) { console.log(data.toString()); });
  child.stderr.on('data', function(data) { console.log(data.toString()); });
  child.on('exit', function(statusCode) {
    if (statusCode != 0) {
      process.exit(statusCode);  // failed assertion in child process
    }
  });
}
