// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls --expose-fast-api

// d8.test.FastCAPI and d8.test.LeafInterfaceType are instantiated lazily in
// every realm, but their templates are shared by all realms of an isolate.
// Thus is_fast_c_api_object() recognizes the FastCAPI objects of all realms,
// independently of the order in which the realms accessed the constructor.

// The constructors are not exposed in all configurations.
if (d8.test.FastCAPI !== undefined) {
  const main_obj = new d8.test.FastCAPI();
  const realm = Realm.create();
  const realm_obj = Realm.eval(realm, 'new d8.test.FastCAPI()');

  // Every realm has its own constructor function, created in that realm.
  const realm_ctor = Realm.eval(realm, 'd8.test.FastCAPI');
  assertNotSame(d8.test.FastCAPI, realm_ctor);
  assertSame(
      Realm.eval(realm, 'Function.prototype'),
      Object.getPrototypeOf(realm_ctor));
  // Later accesses return the same function.
  assertSame(realm_ctor, Realm.eval(realm, 'd8.test.FastCAPI'));

  // The constructor is created in the realm of the d8.test object, even if
  // the first access comes from another realm.
  const other_realm = Realm.create();
  const other_test = Realm.eval(other_realm, 'd8.test');
  const other_ctor = other_test.FastCAPI;
  assertSame(
      Realm.eval(other_realm, 'Function.prototype'),
      Object.getPrototypeOf(other_ctor));
  assertSame(other_ctor, Realm.eval(other_realm, 'd8.test.FastCAPI'));
  assertSame(
      Realm.eval(other_realm, 'Function.prototype'),
      Object.getPrototypeOf(other_test.LeafInterfaceType));

  assertTrue(main_obj.is_fast_c_api_object(main_obj));
  assertTrue(main_obj.is_fast_c_api_object(realm_obj));
  Realm.shared = {main_obj, realm_obj};
  assertTrue(Realm.eval(
      realm,
      'Realm.shared.realm_obj.is_fast_c_api_object(Realm.shared.main_obj)'));

  // Accessing the constructor in another realm does not change the result.
  const realm2 = Realm.create();
  const realm2_obj = Realm.eval(realm2, 'new d8.test.FastCAPI()');
  assertTrue(main_obj.is_fast_c_api_object(main_obj));
  assertTrue(main_obj.is_fast_c_api_object(realm_obj));
  assertTrue(main_obj.is_fast_c_api_object(realm2_obj));

  // Objects of an unrelated API type are not FastCAPI objects in any realm.
  assertFalse(main_obj.is_fast_c_api_object(new d8.test.LeafInterfaceType()));
  assertFalse(main_obj.is_fast_c_api_object(
      Realm.eval(realm, 'new d8.test.LeafInterfaceType()')));
  assertFalse(main_obj.is_fast_c_api_object({}));
}
