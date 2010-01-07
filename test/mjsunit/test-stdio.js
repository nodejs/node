process.mixin(require("./common"));

var sub = path.join(fixturesDir, 'echo.js');

var result = false;
 
var child = process.createChildProcess(path.join(libDir, "../bin/node"), [sub]);
child.addListener("error", function (data){
  puts("parent stderr: " + data);
});
child.addListener("output", function (data){
  if (data && data[0] == 't') {
    result = true;
  }
});
setTimeout(function () {
  child.write('t\r\n');
}, 100);
setTimeout(function (){
  assert.ok(result);
}, 500)
