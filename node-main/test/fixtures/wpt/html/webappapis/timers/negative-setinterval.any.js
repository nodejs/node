setup({ single_test: true });
var i = 0;
var interval;
function next() {
  i++;
  if (i === 20) {
    clearInterval(interval);
    done();
  }
}
setTimeout(assert_unreached, 1000);
interval = setInterval(next, -100);
