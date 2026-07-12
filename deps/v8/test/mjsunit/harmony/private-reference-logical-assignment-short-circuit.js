// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function doNotCall() {
  throw new Error("The right-hand side should not be evaluated");
}

{
  class C {
    get #readOnlyFalse() {
      return false;
    }

    get #readOnlyTrue() {
      return true;
    }

    get #readOnlyOne() {
      return 1;
    }

    get #readOnlyNull() {
      return null;
    }

    get #readOnlyUndefined() {
      return undefined;
    }

    shortCircuitedAndFalse() {
      return this.#readOnlyFalse &&= doNotCall();
    }

    shortCircuitedOrTrue() {
      return this.#readOnlyTrue ||= doNotCall();
    }

    shortCircuitedNullishOne() {
      return this.#readOnlyOne ??= doNotCall();
    }

    andAssignReadOnly() {
      return this.#readOnlyTrue &&= 1;
    }

    orAssignReadOnly() {
      return this.#readOnlyFalse ||= 0;
    }

    nullishAssignReadOnlyNull() {
      return this.#readOnlyNull ??= 1;
    }

    nullishAssignReadOnlyUndefined() {
      return this.#readOnlyUndefined ??= 1;
    }
  }

  const o = new C();
  assertEquals(
    o.shortCircuitedAndFalse(),
    false,
    "The expression should evaluate to the short-circuited value");
  assertEquals(
    o.shortCircuitedOrTrue(),
    true,
    "The expression should evaluate to the short-circuited value");
  assertEquals(
    o.shortCircuitedNullishOne(),
    1,
    "The expression should evaluate to the short-circuited value");

  assertThrows(
    () => o.andAssignReadOnly(),
    TypeError,
    /'#readOnlyTrue' was defined without a setter/
  );
  assertThrows(
    () => o.orAssignReadOnly(),
    TypeError,
    /'#readOnlyFalse' was defined without a setter/
  );
  assertThrows(
    () => o.nullishAssignReadOnlyNull(),
    TypeError,
    /'#readOnlyNull' was defined without a setter/
  );
  assertThrows(
    () => o.nullishAssignReadOnlyUndefined(),
    TypeError,
    /'#readOnlyUndefined' was defined without a setter/
  );
}

{
  class C {
    #privateMethod() { }

    getPrivateMethod() {
      return this.#privateMethod;
    }

    shortCircuitedNullishPrivateMethod() {
      return this.#privateMethod ??= doNotCall();
    }

    shortCircuitedOrPrivateMethod() {
      return this.#privateMethod ||= doNotCall();
    }

    andAssignReadOnly() {
      return this.#privateMethod &&= 1;
    }
  }

  const o = new C();
  assertEquals(
    o.shortCircuitedNullishPrivateMethod(),
    o.getPrivateMethod(),
    "The expression should evaluate to the short-circuited value");

  assertEquals(
    o.shortCircuitedNullishPrivateMethod(),
    o.getPrivateMethod(),
    "The expression should evaluate to the short-circuited value");

  assertThrows(
    () => o.andAssignReadOnly(),
    TypeError,
    /Private method '#privateMethod' is not writable/
  );
}
