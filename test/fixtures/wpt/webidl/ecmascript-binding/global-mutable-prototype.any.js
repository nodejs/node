// META: global=shadowrealm
// META: title=Mutability of the global prototype chain

const objects = [];
setup(() => {
  for (let object = self; object; object = Object.getPrototypeOf(object)) {
    objects.push(object);
  }
});

test(() => {
  for (const object of objects) {
    const proto = Object.getPrototypeOf(object);
    const plainObject = {};
    Object.setPrototypeOf(object, plainObject);
    assert_equals(Object.getPrototypeOf(object), plainObject);
    Object.setPrototypeOf(object, proto);
  }
}, "Setting to a different prototype");

test(() => {
  for (const object of objects) {
    const expected = Object.getPrototypeOf(object);
    Object.setPrototypeOf(object, expected);
    assert_equals(Object.getPrototypeOf(object), expected);
  }
}, "Setting to the same prototype");
