var t = async_test("Interaction of setTimeout and WebIDL")
function finishTest() {
  assert_equals(log, "ONE TWO ")
  t.done()
}
var log = '';
function logger(s) { log += s + ' '; }

setTimeout({ toString: function () {
  setTimeout("logger('ONE')", 100);
  return "logger('TWO'); t.step(finishTest)";
} }, 100);
