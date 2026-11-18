// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The lazy globalThis.Temporal and Date.prototype.toTemporalInstant accessors
// must initialize Temporal in the realm that owns the property, not in the
// realm of the caller.

// First access to another realm's globalThis.Temporal.
{
  const r = Realm.createAllowCrossRealmAccess();
  const t = Realm.global(r).Temporal;
  assertSame(Realm.eval(r, "Temporal"), t);
  assertSame(Realm.eval(r, "Object.prototype"), Object.getPrototypeOf(t));
  assertSame(Realm.eval(r, "new Temporal.Instant(0n)").constructor,
             t.Instant);
  assertTrue(Realm.eval(r,
      "new Date(0).toTemporalInstant() instanceof Temporal.Instant"));
  assertNotSame(Temporal, t);
  assertNotSame(Temporal.Instant, t.Instant);
}

// First access to another realm's Date.prototype.toTemporalInstant.
{
  const r = Realm.createAllowCrossRealmAccess();
  const d = Realm.eval(r, "new Date(0)");
  const fn = d.toTemporalInstant;
  assertSame(Realm.eval(r, "Date.prototype.toTemporalInstant"), fn);
  assertSame(Realm.eval(r, "Function.prototype"), Object.getPrototypeOf(fn));
  assertNotSame(Date.prototype.toTemporalInstant, fn);
  const instant_proto = Realm.eval(r, "Temporal.Instant.prototype");
  assertSame(instant_proto, Object.getPrototypeOf(fn.call(new Date(0))));
  assertSame(instant_proto, Object.getPrototypeOf(
      Realm.eval(r, "new Date(0).toTemporalInstant()")));
  assertTrue(Realm.eval(r,
      "new Date(0).toTemporalInstant() instanceof Temporal.Instant"));
}

// The other realm initializes Temporal itself first, then the main realm
// reads it.
{
  const r = Realm.createAllowCrossRealmAccess();
  const own_temporal = Realm.eval(r, "Temporal");
  const own_fn = Realm.eval(r, "Date.prototype.toTemporalInstant");
  assertSame(own_temporal, Realm.global(r).Temporal);
  assertSame(own_fn, Realm.eval(r, "Date.prototype").toTemporalInstant);
  assertNotSame(Temporal, own_temporal);
  assertNotSame(Date.prototype.toTemporalInstant, own_fn);
}

// Both lazy accessors are first touched from the main realm, in reverse
// order.
{
  const r = Realm.createAllowCrossRealmAccess();
  const fn = Realm.eval(r, "Date.prototype").toTemporalInstant;
  const t = Realm.global(r).Temporal;
  assertSame(Realm.eval(r, "Temporal"), t);
  assertSame(t.Instant.prototype, Object.getPrototypeOf(fn.call(new Date(0))));
  assertTrue(Realm.eval(r,
      "new Date(0).toTemporalInstant() instanceof Temporal.Instant"));
}

// First access through Reflect.get with an unrelated receiver.
{
  const r = Realm.createAllowCrossRealmAccess();
  const t = Reflect.get(Realm.global(r), "Temporal", {});
  assertSame(Realm.eval(r, "Temporal"), t);
  assertNotSame(Temporal, t);
}

// First access through an object inheriting from another realm's
// Date.prototype.
{
  const r = Realm.createAllowCrossRealmAccess();
  const o = Object.create(Realm.eval(r, "Date.prototype"));
  const fn = o.toTemporalInstant;
  assertSame(Realm.eval(r, "Date.prototype.toTemporalInstant"), fn);
  assertNotSame(Date.prototype.toTemporalInstant, fn);
  assertFalse(Object.hasOwn(o, "toTemporalInstant"));
}
