async_test(t => {
  const frame = document.createElement("iframe"),
        asciiOrigin = location.protocol + "//{{domains[天気の良い日]}}:" + location.port,
        path = new URL("support/WorkerLocation-origin.html", location).pathname;
  frame.src = asciiOrigin + path;
  self.onmessage = t.step_func_done(e => {
    assert_equals(e.data.origin, asciiOrigin);
  });
  document.body.appendChild(frame);
  t.add_cleanup(() => frame.remove());
}, "workerLocation.origin must use ASCII code points");
