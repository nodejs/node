"use strict";

test(() => {
  const observableArray = document.adoptedStyleSheets;

  let leaked_target = null;
  let leaked_handler = null;

  let target_leaker = (target) => { leaked_target = target; return null; };
  Object.defineProperty(Object.prototype, "getPrototypeOf", {get: function() {
    leaked_handler = this;
    return target_leaker;
  }})
  Object.getPrototypeOf(observableArray);

  assert_equals(leaked_target, null, "The proxy target leaked.");
  assert_equals(leaked_handler, null, "The proxy handler leaked.");
}, "ObservableArray's internals won't leak");
