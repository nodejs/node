self.testSettingImmutablePrototypeToNewValueOnly =
  (prefix, target, newValue, newValueString, { isSameOriginDomain }) => {
  test(() => {
    assert_throws(new TypeError, () => {
      Object.setPrototypeOf(target, newValue);
    });
  }, `${prefix}: setting the prototype to ${newValueString} via Object.setPrototypeOf should throw a TypeError`);

  let dunderProtoError = "SecurityError";
  let dunderProtoErrorName = "\"SecurityError\" DOMException";
  if (isSameOriginDomain) {
    dunderProtoError = new TypeError();
    dunderProtoErrorName = "TypeError";
  }

  test(() => {
    assert_throws(dunderProtoError, function() {
      target.__proto__ = newValue;
    });
  }, `${prefix}: setting the prototype to ${newValueString} via __proto__ should throw a ${dunderProtoErrorName}`);

  test(() => {
    assert_false(Reflect.setPrototypeOf(target, newValue));
  }, `${prefix}: setting the prototype to ${newValueString} via Reflect.setPrototypeOf should return false`);
};

self.testSettingImmutablePrototype =
  (prefix, target, originalValue, { isSameOriginDomain }, newValue = {}, newValueString = "an empty object") => {
  testSettingImmutablePrototypeToNewValueOnly(prefix, target, newValue, newValueString, { isSameOriginDomain });

  const originalValueString = originalValue === null ? "null" : "its original value";

  test(() => {
    assert_equals(Object.getPrototypeOf(target), originalValue);
  }, `${prefix}: the prototype must still be ${originalValueString}`);

  test(() => {
    Object.setPrototypeOf(target, originalValue);
  }, `${prefix}: setting the prototype to ${originalValueString} via Object.setPrototypeOf should not throw`);

  if (isSameOriginDomain) {
    test(() => {
      target.__proto__ = originalValue;
    }, `${prefix}: setting the prototype to ${originalValueString} via __proto__ should not throw`);
  } else {
    test(() => {
      assert_throws("SecurityError", function() {
        target.__proto__ = newValue;
      });
    }, `${prefix}: setting the prototype to ${originalValueString} via __proto__ should throw a "SecurityError" since ` +
       `it ends up in CrossOriginGetOwnProperty`);
  }

  test(() => {
    assert_true(Reflect.setPrototypeOf(target, originalValue));
  }, `${prefix}: setting the prototype to ${originalValueString} via Reflect.setPrototypeOf should return true`);
};
