require("../common");
var exec = require('child_process').exec;
success_count = 0;
error_count = 0;

exec("ls /", function (err, stdout, stderr) {
  if (err) {
    error_count++;
    console.log("error!: " + err.code);
    console.log("stdout: " + JSON.stringify(stdout));
    console.log("stderr: " + JSON.stringify(stderr));
    assert.equal(false, err.killed);
  } else {
    success_count++;
    p(stdout);
  }
});


exec("ls /DOES_NOT_EXIST", function (err, stdout, stderr) {
  if (err) {
    error_count++;
    assert.equal("", stdout);
    assert.equal(true, err.code != 0);
    assert.equal(false, err.killed);
    assert.strictEqual(null, err.signal);
    console.log("error code: " + err.code);
    console.log("stdout: " + JSON.stringify(stdout));
    console.log("stderr: " + JSON.stringify(stderr));
  } else {
    success_count++;
    p(stdout);
    assert.equal(true, stdout != "");
  }
});

exec("sleep 10", { timeout: 50 }, function (err, stdout, stderr) {
  assert.ok(err);
  assert.ok(err.killed);
  assert.equal(err.signal, 'SIGKILL');
});

exec('python -c "print 200000*\'C\'"', { maxBuffer: 1000 }, function (err, stdout, stderr) {
  assert.ok(err);
  assert.ok(err.killed);
  assert.equal(err.signal, 'SIGKILL');
});

process.addListener("exit", function () {
  assert.equal(1, success_count);
  assert.equal(1, error_count);
});
