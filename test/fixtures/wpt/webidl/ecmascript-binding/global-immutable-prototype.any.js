// META: global=window,worker
// META: title=Immutability of the global prototype chain

const objects = [];
setup(() => {
  for (let object = self; object; object = Object.getPrototypeOf(object)) {
    objects.push(object);
  }
});

test(() => {
  for (const object of objects) {
    assert_throws_js(TypeError, () => {
      Object.setPrototypeOf(object, {});
    });
  }
}, "Setting to a different prototype");

test(() => {
  for (const object of objects) {
    const expected = Object.getPrototypeOf(object);
    Object.setPrototypeOf(object, expected);
    assert_equals(Object.getPrototypeOf(object), expected);
  }
}, "Setting to the same prototype");
