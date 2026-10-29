'use strict'

// Runs a set of tests for a given prefixed/unprefixed animation event (e.g.
// animationstart/webkitAnimationStart).
//
// The eventDetails object must have the following form:
// {
//   isTransition: false, <-- can be omitted, default false
//   unprefixedType: 'animationstart',
//   prefixedType: 'webkitAnimationStart',
//   animationCssStyle: '1ms',  <-- must NOT include animation name or
//                                  transition property
// }
function runAnimationEventTests(eventDetails) {
  const {
    isTransition,
    unprefixedType,
    prefixedType,
    animationCssStyle
  } = eventDetails;

  // Derive the DOM event handler names, e.g. onanimationstart.
  const unprefixedHandler = `on${unprefixedType}`;
  const prefixedHandler = `on${prefixedType.toLowerCase()}`;

  const style = document.createElement('style');
  document.head.appendChild(style);
  if (isTransition) {
    style.sheet.insertRule(
      `.baseStyle { width: 100px; transition: width ${animationCssStyle}; }`);
    style.sheet.insertRule('.transition { width: 200px !important; }');
  } else {
    style.sheet.insertRule('@keyframes anim {}');
  }

  function triggerAnimation(div) {
    if (isTransition) {
      div.classList.add('transition');
    } else {
      div.style.animation = `anim ${animationCssStyle}`;
    }
  }

  test(t => {
    const div = createDiv(t);

    assert_equals(div[unprefixedHandler], null,
        `${unprefixedHandler} should initially be null`);
    assert_equals(div[prefixedHandler], null,
        `${prefixedHandler} should initially be null`);

    // Setting one should not affect the other.
    div[unprefixedHandler] = () => { };

    assert_not_equals(div[unprefixedHandler], null,
        `setting ${unprefixedHandler} should make it non-null`);
    assert_equals(div[prefixedHandler], null,
        `setting ${unprefixedHandler} should not affect ${prefixedHandler}`);

    div[prefixedHandler] = () => { };

    assert_not_equals(div[prefixedHandler], null,
        `setting ${prefixedHandler} should make it non-null`);
    assert_not_equals(div[unprefixedHandler], div[prefixedHandler],
        'the setters should be different');
  }, `${unprefixedHandler} and ${prefixedHandler} are not aliases`);

  // The below tests primarily test the interactions of prefixed animation
  // events in the algorithm for invoking events:
  // https://dom.spec.whatwg.org/#concept-event-listener-invoke

  promise_test(async t => {
    const div = createDiv(t);

    let receivedEventCount = 0;
    addTestScopedEventHandler(t, div, prefixedHandler, () => {
      receivedEventCount++;
    });
    addTestScopedEventListener(t, div, prefixedType, () => {
      receivedEventCount++;
    });

    // The HTML spec[0] specifies that the prefixed event handlers have an
    // 'Event handler event type' of the appropriate prefixed event type. E.g.
    // onwebkitanimationend creates a listener for the event type
    // 'webkitAnimationEnd'.
    //
    // [0]: https://html.spec.whatwg.org/multipage/webappapis.html#event-handlers-on-elements,-document-objects,-and-window-objects
    div.dispatchEvent(new AnimationEvent(prefixedType));
    assert_equals(receivedEventCount, 2,
                'prefixed listener and handler received event');
  }, `dispatchEvent of a ${prefixedType} event does trigger a ` +
      `prefixed event handler or listener`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedEvent = false;
    addTestScopedEventHandler(t, div, unprefixedHandler, () => {
      receivedEvent = true;
    });
    addTestScopedEventListener(t, div, unprefixedType, () => {
      receivedEvent = true;
    });

    div.dispatchEvent(new AnimationEvent(prefixedType));
    assert_false(receivedEvent,
                'prefixed listener or handler received event');
  }, `dispatchEvent of a ${prefixedType} event does not trigger an ` +
    `unprefixed event handler or listener`);


  promise_test(async t => {
    const div = createDiv(t);

    let receivedEvent = false;
    addTestScopedEventHandler(t, div, prefixedHandler, () => {
      receivedEvent = true;
    });
    addTestScopedEventListener(t, div, prefixedType, () => {
      receivedEvent = true;
    });

    // The rewrite rules from
    // https://dom.spec.whatwg.org/#concept-event-listener-invoke step 8 do not
    // apply because isTrusted will be false.
    div.dispatchEvent(new AnimationEvent(unprefixedType));
    assert_false(receivedEvent, 'prefixed listener or handler received event');
  }, `dispatchEvent of an ${unprefixedType} event does not trigger a ` +
      `prefixed event handler or listener`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedEvent = false;
    addTestScopedEventHandler(t, div, prefixedHandler, () => {
      receivedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedType);
    assert_true(receivedEvent, `received ${prefixedHandler} event`);
  }, `${prefixedHandler} event handler should trigger for an animation`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedPrefixedEvent = false;
    addTestScopedEventHandler(t, div, prefixedHandler, () => {
      receivedPrefixedEvent = true;
    });
    let receivedUnprefixedEvent = false;
    addTestScopedEventHandler(t, div, unprefixedHandler, () => {
      receivedUnprefixedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedType);
    assert_true(receivedUnprefixedEvent, `received ${unprefixedHandler} event`);
    assert_false(receivedPrefixedEvent, `received ${prefixedHandler} event`);
  }, `${prefixedHandler} event handler should not trigger if an unprefixed ` +
      `event handler also exists`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedPrefixedEvent = false;
    addTestScopedEventHandler(t, div, prefixedHandler, () => {
      receivedPrefixedEvent = true;
    });
    let receivedUnprefixedEvent = false;
    addTestScopedEventListener(t, div, unprefixedType, () => {
      receivedUnprefixedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedHandler);
    assert_true(receivedUnprefixedEvent, `received ${unprefixedHandler} event`);
    assert_false(receivedPrefixedEvent, `received ${prefixedHandler} event`);
  }, `${prefixedHandler} event handler should not trigger if an unprefixed ` +
      `listener also exists`);

  promise_test(async t => {
    // We use a parent/child relationship to be able to register both prefixed
    // and unprefixed event handlers without the deduplication logic kicking in.
    const parent = createDiv(t);
    const child = createDiv(t);
    parent.appendChild(child);
    // After moving the child, we have to clean style again.
    getComputedStyle(child).transition;
    getComputedStyle(child).width;

    let observedUnprefixedType;
    addTestScopedEventHandler(t, parent, unprefixedHandler, e => {
      observedUnprefixedType = e.type;
    });
    let observedPrefixedType;
    addTestScopedEventHandler(t, child, prefixedHandler, e => {
      observedPrefixedType = e.type;
    });

    triggerAnimation(child);
    await waitForEventThenAnimationFrame(t, unprefixedType);

    assert_equals(observedUnprefixedType, unprefixedType);
    assert_equals(observedPrefixedType, prefixedType);
  }, `event types for prefixed and unprefixed ${unprefixedType} event ` +
    `handlers should be named appropriately`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedEvent = false;
    addTestScopedEventListener(t, div, prefixedType, () => {
      receivedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedHandler);
    assert_true(receivedEvent, `received ${prefixedType} event`);
  }, `${prefixedType} event listener should trigger for an animation`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedPrefixedEvent = false;
    addTestScopedEventListener(t, div, prefixedType, () => {
      receivedPrefixedEvent = true;
    });
    let receivedUnprefixedEvent = false;
    addTestScopedEventListener(t, div, unprefixedType, () => {
      receivedUnprefixedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedHandler);
    assert_true(receivedUnprefixedEvent, `received ${unprefixedType} event`);
    assert_false(receivedPrefixedEvent, `received ${prefixedType} event`);
  }, `${prefixedType} event listener should not trigger if an unprefixed ` +
      `listener also exists`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedPrefixedEvent = false;
    addTestScopedEventListener(t, div, prefixedType, () => {
      receivedPrefixedEvent = true;
    });
    let receivedUnprefixedEvent = false;
    addTestScopedEventHandler(t, div, unprefixedHandler, () => {
      receivedUnprefixedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedHandler);
    assert_true(receivedUnprefixedEvent, `received ${unprefixedType} event`);
    assert_false(receivedPrefixedEvent, `received ${prefixedType} event`);
  }, `${prefixedType} event listener should not trigger if an unprefixed ` +
       `event handler also exists`);

  promise_test(async t => {
    // We use a parent/child relationship to be able to register both prefixed
    // and unprefixed event listeners without the deduplication logic kicking in.
    const parent = createDiv(t);
    const child = createDiv(t);
    parent.appendChild(child);
    // After moving the child, we have to clean style again.
    getComputedStyle(child).transition;
    getComputedStyle(child).width;

    let observedUnprefixedType;
    addTestScopedEventListener(t, parent, unprefixedType, e => {
      observedUnprefixedType = e.type;
    });
    let observedPrefixedType;
    addTestScopedEventListener(t, child, prefixedType, e => {
      observedPrefixedType = e.type;
    });

    triggerAnimation(child);
    await waitForEventThenAnimationFrame(t, unprefixedHandler);

    assert_equals(observedUnprefixedType, unprefixedType);
    assert_equals(observedPrefixedType, prefixedType);
  }, `event types for prefixed and unprefixed ${unprefixedType} event ` +
      `listeners should be named appropriately`);

  promise_test(async t => {
    const div = createDiv(t);

    let receivedEvent = false;
    addTestScopedEventListener(t, div, prefixedType.toLowerCase(), () => {
      receivedEvent = true;
    });
    addTestScopedEventListener(t, div, prefixedType.toUpperCase(), () => {
      receivedEvent = true;
    });

    triggerAnimation(div);
    await waitForEventThenAnimationFrame(t, unprefixedHandler);
    assert_false(receivedEvent, `received ${prefixedType} event`);
  }, `${prefixedType} event listener is case sensitive`);
}

// Below are utility functions.

// Creates a div element, appends it to the document body and removes the
// created element during test cleanup.
function createDiv(test) {
  const element = document.createElement('div');
  element.classList.add('baseStyle');
  document.body.appendChild(element);
  test.add_cleanup(() => {
    element.remove();
  });

  // Flush style before returning. Some browsers only do partial style re-calc,
  // so ask for all important properties to make sure they are applied.
  getComputedStyle(element).transition;
  getComputedStyle(element).width;

  return element;
}

// Adds an event handler for |handlerName| (calling |callback|) to the given
// |target|, that will automatically be cleaned up at the end of the test.
function addTestScopedEventHandler(test, target, handlerName, callback) {
  assert_regexp_match(
      handlerName, /^on/, 'Event handler names must start with "on"');
  assert_equals(target[handlerName], null,
                `${handlerName} must be supported and not previously set`);
  target[handlerName] = callback;
  // We need this cleaned up even if the event handler doesn't run.
  test.add_cleanup(() => {
    if (target[handlerName])
      target[handlerName] = null;
  });
}

// Adds an event listener for |type| (calling |callback|) to the given
// |target|, that will automatically be cleaned up at the end of the test.
function addTestScopedEventListener(test, target, type, callback) {
  target.addEventListener(type, callback);
  // We need this cleaned up even if the event handler doesn't run.
  test.add_cleanup(() => {
    target.removeEventListener(type, callback);
  });
}

// Returns a promise that will resolve once the passed event (|eventName|) has
// triggered and one more animation frame has happened. Automatically chooses
// between an event handler or event listener based on whether |eventName|
// begins with 'on'.
//
// We always listen on window as we don't want to interfere with the test via
// triggering the prefixed event deduplication logic.
function waitForEventThenAnimationFrame(test, eventName) {
  return new Promise((resolve, _) => {
    const eventFunc = eventName.startsWith('on')
        ? addTestScopedEventHandler : addTestScopedEventListener;
    eventFunc(test, window, eventName, () => {
      // rAF once to give the event under test time to come through.
      requestAnimationFrame(resolve);
    });
  });
}
