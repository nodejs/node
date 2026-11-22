// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All realms of an isolate share the templates of the d8 API. The objects and
// functions are still created per realm, but API objects created in one realm
// pass the receiver checks of the API functions of another realm.

function getNodeTypeGetter(obj) {
  for (let p = Object.getPrototypeOf(obj); p !== null;
       p = Object.getPrototypeOf(p)) {
    const desc = Object.getOwnPropertyDescriptor(p, 'nodeType');
    if (desc !== undefined) return desc.get;
  }
  return undefined;
}

const realm = Realm.create();
const div = new d8.dom.Div();
const realm_div = Realm.eval(realm, 'new d8.dom.Div()');

// Constructors, prototypes and API functions are per realm.
assertNotSame(d8.dom.Div, Realm.eval(realm, 'd8.dom.Div'));
assertNotSame(Object.getPrototypeOf(div), Object.getPrototypeOf(realm_div));
const getter = getNodeTypeGetter(div);
const realm_getter = getNodeTypeGetter(realm_div);
assertEquals('function', typeof getter);
assertEquals('function', typeof realm_getter);
assertNotSame(getter, realm_getter);

// The receiver checks accept the objects of both realms.
assertEquals(div.nodeType, getter.call(realm_div));
assertEquals(realm_div.nodeType, realm_getter.call(div));

// Non-API objects are still rejected (with the TypeError of the getter's
// realm).
assertThrows(() => getter.call({}), TypeError);
assertThrows(() => realm_getter.call({}), Realm.eval(realm, 'TypeError'));

// Every realm still gets fresh objects: changes in one realm are not visible
// in another one.
d8.test.foo = 42;
assertEquals(undefined, Realm.eval(realm, 'd8.test.foo'));
assertEquals(undefined, Realm.eval(Realm.create(), 'd8.test.foo'));
