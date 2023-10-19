// META: title=AddEventListenerOptions.passive

test(function() {
  var supportsPassive = false;
  var query_options = {
    get passive() {
      supportsPassive = true;
      return false;
    },
    get dummy() {
      assert_unreached("dummy value getter invoked");
      return false;
    }
  };

  const et = new EventTarget();
  et.addEventListener('test_event', null, query_options);
  assert_true(supportsPassive, "addEventListener doesn't support the passive option");

  supportsPassive = false;
  et.removeEventListener('test_event', null, query_options);
  assert_false(supportsPassive, "removeEventListener supports the passive option when it should not");
}, "Supports passive option on addEventListener only");

function testPassiveValue(optionsValue, expectedDefaultPrevented, existingEventTarget) {
  var defaultPrevented = undefined;
  var handler = function handler(e) {
    assert_false(e.defaultPrevented, "Event prematurely marked defaultPrevented");
    e.preventDefault();
    defaultPrevented = e.defaultPrevented;
  }
  const et = existingEventTarget || new EventTarget();
  et.addEventListener('test', handler, optionsValue);
  var uncanceled = et.dispatchEvent(new Event('test', {bubbles: true, cancelable: true}));

  assert_equals(defaultPrevented, expectedDefaultPrevented, "Incorrect defaultPrevented for options: " + JSON.stringify(optionsValue));
  assert_equals(uncanceled, !expectedDefaultPrevented, "Incorrect return value from dispatchEvent");

  et.removeEventListener('test', handler, optionsValue);
}

test(function() {
  testPassiveValue(undefined, true);
  testPassiveValue({}, true);
  testPassiveValue({passive: false}, true);
  testPassiveValue({passive: true}, false);
  testPassiveValue({passive: 0}, true);
  testPassiveValue({passive: 1}, false);
}, "preventDefault should be ignored if-and-only-if the passive option is true");

function testPassiveValueOnReturnValue(test, optionsValue, expectedDefaultPrevented) {
  var defaultPrevented = undefined;
  var handler = test.step_func(e => {
    assert_false(e.defaultPrevented, "Event prematurely marked defaultPrevented");
    e.returnValue = false;
    defaultPrevented = e.defaultPrevented;
  });
  const et = new EventTarget();
  et.addEventListener('test', handler, optionsValue);
  var uncanceled = et.dispatchEvent(new Event('test', {bubbles: true, cancelable: true}));

  assert_equals(defaultPrevented, expectedDefaultPrevented, "Incorrect defaultPrevented for options: " + JSON.stringify(optionsValue));
  assert_equals(uncanceled, !expectedDefaultPrevented, "Incorrect return value from dispatchEvent");

  et.removeEventListener('test', handler, optionsValue);
}

async_test(t => {
  testPassiveValueOnReturnValue(t, undefined, true);
  testPassiveValueOnReturnValue(t, {}, true);
  testPassiveValueOnReturnValue(t, {passive: false}, true);
  testPassiveValueOnReturnValue(t, {passive: true}, false);
  testPassiveValueOnReturnValue(t, {passive: 0}, true);
  testPassiveValueOnReturnValue(t, {passive: 1}, false);
  t.done();
}, "returnValue should be ignored if-and-only-if the passive option is true");

function testPassiveWithOtherHandlers(optionsValue, expectedDefaultPrevented) {
  var handlerInvoked1 = false;
  var dummyHandler1 = function() {
    handlerInvoked1 = true;
  };
  var handlerInvoked2 = false;
  var dummyHandler2 = function() {
    handlerInvoked2 = true;
  };

  const et = new EventTarget();
  et.addEventListener('test', dummyHandler1, {passive:true});
  et.addEventListener('test', dummyHandler2);

  testPassiveValue(optionsValue, expectedDefaultPrevented, et);

  assert_true(handlerInvoked1, "Extra passive handler not invoked");
  assert_true(handlerInvoked2, "Extra non-passive handler not invoked");

  et.removeEventListener('test', dummyHandler1);
  et.removeEventListener('test', dummyHandler2);
}

test(function() {
  testPassiveWithOtherHandlers({}, true);
  testPassiveWithOtherHandlers({passive: false}, true);
  testPassiveWithOtherHandlers({passive: true}, false);
}, "passive behavior of one listener should be unaffected by the presence of other listeners");

function testOptionEquivalence(optionValue1, optionValue2, expectedEquality) {
  var invocationCount = 0;
  var handler = function handler(e) {
    invocationCount++;
  }
  const et = new EventTarget();
  et.addEventListener('test', handler, optionValue1);
  et.addEventListener('test', handler, optionValue2);
  et.dispatchEvent(new Event('test', {bubbles: true}));
  assert_equals(invocationCount, expectedEquality ? 1 : 2, "equivalence of options " +
    JSON.stringify(optionValue1) + " and " + JSON.stringify(optionValue2));
  et.removeEventListener('test', handler, optionValue1);
  et.removeEventListener('test', handler, optionValue2);
}

test(function() {
  // Sanity check options that should be treated as distinct handlers
  testOptionEquivalence({capture:true}, {capture:false, passive:false}, false);
  testOptionEquivalence({capture:true}, {passive:true}, false);

  // Option values that should be treated as equivalent
  testOptionEquivalence({}, {passive:false}, true);
  testOptionEquivalence({passive:true}, {passive:false}, true);
  testOptionEquivalence(undefined, {passive:true}, true);
  testOptionEquivalence({capture: true, passive: false}, {capture: true, passive: true}, true);

}, "Equivalence of option values");

