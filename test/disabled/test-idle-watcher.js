common = require("../common");
assert = common.assert

var complete = false;
var idle = new process.IdleWatcher();
idle.callback = function () {
  complete = true;
  idle.stop();
};
idle.setPriority(process.EVMAXPRI);
idle.start();

process.addListener('exit', function () {
  assert.ok(complete);
});
