// META: global=worker
// This is a regression test for a crash bug in Chrome: http://crbug.com/239669
function update() {
  onmessage = undefined;
}

test(() => {
  for (var i = 0; i < 8; ++i) {
    update();
  }
}, "Tests that repeatedly setting 'onmessage' within a worker doesn't crash.");
