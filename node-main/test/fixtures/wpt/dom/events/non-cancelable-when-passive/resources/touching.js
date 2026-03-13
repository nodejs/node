function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // rAF twice.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

function injectInput(touchDiv) {
  return new test_driver.Actions()
    .addPointer("touch_pointer", "touch")
    .pointerMove(0, 0, {origin: touchDiv})
    .pointerDown()
    .pointerMove(30, 30)
    .pointerUp()
    .send();
}

function runTest({target, eventName, passive, expectCancelable}) {
  let touchDiv = document.getElementById("touchDiv");
  let cancelable = null;
  let arrived = false;
  target.addEventListener(eventName, function (event) {
    cancelable = event.cancelable;
    arrived = true;
  }, {passive});
  promise_test(async () => {
    await waitForCompositorCommit();
    await injectInput(touchDiv);
    await waitFor(() => arrived);
    assert_equals(cancelable, expectCancelable);
  });
}
