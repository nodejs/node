require("../common");
var exec = require('child_process').exec;
success_count = 0;
error_count = 0;

exec("ls /", function (err, stdout, stderr) {
  if (err) {
    error_count++;
    puts("error!: " + err.code);
    puts("stdout: " + JSON.stringify(stdout));
    puts("stderr: " + JSON.stringify(stderr));
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
    puts("error code: " + err.code);
    puts("stdout: " + JSON.stringify(stdout));
    puts("stderr: " + JSON.stringify(stderr));
  } else {
    success_count++;
    p(stdout);
    assert.equal(true, stdout != "");
  }
});


process.addListener("exit", function () {
  assert.equal(1, success_count);
  assert.equal(1, error_count);
});
