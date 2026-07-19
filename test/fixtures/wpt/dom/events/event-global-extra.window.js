const otherWindow = document.body.appendChild(document.createElement("iframe")).contentWindow;

["EventTarget", "XMLHttpRequest"].forEach(constructorName => {
  async_test(t => {
    const eventTarget = new otherWindow[constructorName]();
    eventTarget.addEventListener("hi", t.step_func_done(e => {
      assert_equals(otherWindow.event, undefined);
      assert_equals(e, window.event);
    }));
    eventTarget.dispatchEvent(new Event("hi"));
  }, "window.event for constructors from another global: " + constructorName);
});

// XXX: It would be good to test a subclass of EventTarget once we sort out
// https://github.com/heycam/webidl/issues/540

async_test(t => {
  const element = document.body.appendChild(otherWindow.document.createElement("meh"));
  element.addEventListener("yo", t.step_func_done(e => {
    assert_equals(e, window.event);
  }));
  element.dispatchEvent(new Event("yo"));
}, "window.event and element from another document");

async_test(t => {
  const doc = otherWindow.document,
        element = doc.body.appendChild(doc.createElement("meh")),
        child = element.appendChild(doc.createElement("bleh"));
  element.addEventListener("yoyo", t.step_func(e => {
    document.body.appendChild(element);
    assert_equals(element.ownerDocument, document);
    assert_equals(window.event, e);
    assert_equals(otherWindow.event, undefined);
  }), true);
  element.addEventListener("yoyo", t.step_func(e => {
    assert_equals(element.ownerDocument, document);
    assert_equals(window.event, e);
    assert_equals(otherWindow.event, undefined);
  }), true);
  child.addEventListener("yoyo", t.step_func_done(e => {
    assert_equals(child.ownerDocument, document);
    assert_equals(window.event, e);
    assert_equals(otherWindow.event, undefined);
  }));
  child.dispatchEvent(new Event("yoyo"));
}, "window.event and moving an element post-dispatch");

test(t => {
  const host = document.createElement("div"),
        shadow = host.attachShadow({ mode: "open" }),
        child = shadow.appendChild(document.createElement("trala")),
        furtherChild = child.appendChild(document.createElement("waddup"));
  let counter = 0;
  host.addEventListener("hi", t.step_func(e => {
    assert_equals(window.event, e);
    assert_equals(counter++, 3);
  }));
  child.addEventListener("hi", t.step_func(e => {
    assert_equals(window.event, undefined);
    assert_equals(counter++, 2);
  }));
  furtherChild.addEventListener("hi", t.step_func(e => {
    host.appendChild(child);
    assert_equals(window.event, undefined);
    assert_equals(counter++, 0);
  }));
  furtherChild.addEventListener("hi", t.step_func(e => {
    assert_equals(window.event, undefined);
    assert_equals(counter++, 1);
  }));
  furtherChild.dispatchEvent(new Event("hi", { composed: true, bubbles: true }));
  assert_equals(counter, 4);
}, "window.event should not be affected by nodes moving post-dispatch");

async_test(t => {
  const frame = document.body.appendChild(document.createElement("iframe"));
  frame.src = "resources/event-global-extra-frame.html";
  frame.onload = t.step_func_done((load_event) => {
    const event = new Event("hi");
    document.addEventListener("hi", frame.contentWindow.listener); // listener intentionally not wrapped in t.step_func
    document.addEventListener("hi", t.step_func(e => {
      assert_equals(event, e);
      assert_equals(window.event, e);
    }));
    document.dispatchEvent(event);
    assert_equals(frameState.event, event);
    assert_equals(frameState.windowEvent, event);
    assert_equals(frameState.parentEvent, load_event);
  });
}, "Listener from a different global");
