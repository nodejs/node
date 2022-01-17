setup({ single_test: true });
var interval;
function next() {
  clearInterval(interval);
  done();
}
interval = setInterval(next, Math.pow(2, 32));
setTimeout(assert_unreached, 100);
