async_test(t => {
  const script = document.createElement("script");
  t.add_cleanup(() => script.remove());
  script.src = "resources/script-with-0x00-in-header.py";
  script.onerror = t.step_func_done();
  script.onload = t.unreached_func();
  document.body.append(script);
}, "Expect network error for script with 0x00 in a header");

async_test(t => {
  const frame = document.createElement("iframe");
  t.add_cleanup(() => frame.remove());
  frame.src = "resources/document-with-0x00-in-header.py";
  // If network errors result in load events for frames per
  // https://github.com/whatwg/html/issues/125 and https://github.com/whatwg/html/issues/1230 this
  // should be changed to use the load event instead.
  t.step_timeout(() => {
    assert_equals(frame.contentDocument, null);
    t.done();
  }, 1000);
  document.body.append(frame);
}, "Expect network error for frame navigation to resource with 0x00 in a header");

async_test(t => {
  const img = document.createElement("img");
  t.add_cleanup(() => img.remove());
  img.src = "resources/blue-with-0x00-in-a-header.asis";
  img.onerror = t.step_func_done();
  img.onload = t.unreached_func();
  document.body.append(img);
}, "Expect network error for image with 0x00 in a header");
