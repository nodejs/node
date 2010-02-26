process.mixin(require("../common"));

var sub = path.join(fixturesDir, 'echo.js');

var gotHelloWorld = false;
var gotEcho = false;

var child = process.createChildProcess(process.argv[0], [sub]);

child.addListener("error", function (data){
  puts("parent stderr: " + data);
});

child.addListener("output", function (data){
  if (data) {
    puts('child said: ' + JSON.stringify(data));
    if (!gotHelloWorld) {
      assert.equal("hello world\r\n", data);
      gotHelloWorld = true;
      child.write('echo me\r\n');
    } else {
      assert.equal("echo me\r\n", data);
      gotEcho = true;
      child.close();
    }
  } else {
    puts('child end');
  }
});


process.addListener('exit', function () {
  assert.ok(gotHelloWorld);
  assert.ok(gotEcho);
});
