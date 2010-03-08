require("../common");

var WINDOW = 200; // why is does this need to be so big?

var interval_count = 0;
var setTimeout_called = false;

assert.equal(true, setTimeout instanceof Function);
var starttime = new Date;
setTimeout(function () {
  var endtime = new Date;

  var diff = endtime - starttime;
  assert.ok(diff > 0);
  puts("diff: " + diff);

  assert.equal(true, 1000 - WINDOW < diff && diff < 1000 + WINDOW);
  setTimeout_called = true;
}, 1000);

// this timer shouldn't execute
var id = setTimeout(function () { assert.equal(true, false); }, 500);
clearTimeout(id);

setInterval(function () {
  interval_count += 1;
  var endtime = new Date;

  var diff = endtime - starttime;
  assert.ok(diff > 0);
  puts("diff: " + diff);

  var t = interval_count * 1000;

  assert.equal(true, t - WINDOW < diff && diff < t + WINDOW);

  assert.equal(true, interval_count <= 3);
  if (interval_count == 3)
    clearInterval(this);
}, 1000);


// Single param:
setTimeout(function(param){
  assert.equal("test param", param);
}, 1000, "test param");

var interval_count2 = 0;
setInterval(function(param){
  ++interval_count2;
  assert.equal("test param", param);

  if(interval_count2 == 3)
    clearInterval(this);
}, 1000, "test param");


// Multiple param
setTimeout(function(param1, param2){
  assert.equal("param1", param1);
  assert.equal("param2", param2);
}, 1000, "param1", "param2");

var interval_count3 = 0;
setInterval(function(param1, param2){
  ++interval_count3;
  assert.equal("param1", param1);
  assert.equal("param2", param2);

  if(interval_count3 == 3)
    clearInterval(this);
}, 1000, "param1", "param2");

process.addListener("exit", function () {
  assert.equal(true, setTimeout_called);
  assert.equal(3, interval_count);
});
