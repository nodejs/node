
var TESTS = [
  'test-spawn-and-execfile',
  'test-fork',
  'test-exec',
];

var execFile = require('child_process').execFile;
var passed = 0, failed = 0;

function next() {
  var test = TESTS.shift();
  if (!test)
    done();

  console.log("Running test: %s", test);
  execFile(process.execPath, [test], { cwd: __dirname }, onExit);
}

function onExit(err, stdout, stderr) {
  if (err) {
    console.log("... failed:\n%s%s\n", stdout, stderr);
    failed++;
  } else {
    console.log("... pass");
    passed++;
  }

  next();
}

function done() {
  console.log("Tests run: %d. Passed: %d. Failed: %d",
              passed + failed,
              passed,
              failed);

  process.exit(+(failed > 0));
}

next();
