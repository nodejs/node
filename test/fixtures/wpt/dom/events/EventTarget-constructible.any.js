"use strict";

test(() => {
  const target = new EventTarget();
  const event = new Event("foo", { bubbles: true, cancelable: false });
  let callCount = 0;

  function listener(e) {
    assert_equals(e, event);
    ++callCount;
  }

  target.addEventListener("foo", listener);

  target.dispatchEvent(event);
  assert_equals(callCount, 1);

  target.dispatchEvent(event);
  assert_equals(callCount, 2);

  target.removeEventListener("foo", listener);
  target.dispatchEvent(event);
  assert_equals(callCount, 2);
}, "A constructed EventTarget can be used as expected");

test(() => {
  class NicerEventTarget extends EventTarget {
    on(...args) {
      this.addEventListener(...args);
    }

    off(...args) {
      this.removeEventListener(...args);
    }

    dispatch(type, detail) {
      this.dispatchEvent(new CustomEvent(type, { detail }));
    }
  }

  const target = new NicerEventTarget();
  const event = new Event("foo", { bubbles: true, cancelable: false });
  const detail = "some data";
  let callCount = 0;

  function listener(e) {
    assert_equals(e.detail, detail);
    ++callCount;
  }

  target.on("foo", listener);

  target.dispatch("foo", detail);
  assert_equals(callCount, 1);

  target.dispatch("foo", detail);
  assert_equals(callCount, 2);

  target.off("foo", listener);
  target.dispatch("foo", detail);
  assert_equals(callCount, 2);
}, "EventTarget can be subclassed");
