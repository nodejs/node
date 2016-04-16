// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

'use strict';

function assertWeakClassWeakInstances(x) {
  assertFalse(%IsStrong(x));
  assertFalse(%IsStrong(x.prototype));
  assertFalse(%IsStrong(new x));
}

function assertWeakClassStrongInstances(x) {
  assertFalse(%IsStrong(x));
  assertFalse(%IsStrong(x.prototype));
  assertTrue(%IsStrong(new x));
}

function assertStrongClassWeakInstances(x) {
  assertTrue(%IsStrong(x));
  assertTrue(%IsStrong(x.prototype));
  assertFalse(%IsStrong(new x));
}

function assertStrongClassStrongInstances(x) {
  assertTrue(%IsStrong(x));
  assertTrue(%IsStrong(x.prototype));
  assertTrue(%IsStrong(new x));
}

function getWeakClass() {
  return (class {});
}

function getWeakClassExtends(x) {
  return (class extends x {});
}

function getStrongClass() {
  "use strong";
  return (class {});
}

function getStrongClassExtends(x) {
  "use strong";
  return (class extends x {});
}

(function SimpleWeakClassLiterals() {
  class C {};
  class D extends C {};
  class E extends Object {};

  assertWeakClassWeakInstances(C);
  assertWeakClassWeakInstances(D);
  assertWeakClassWeakInstances(E);

  assertWeakClassWeakInstances(class {});
  assertWeakClassWeakInstances(class extends Object {});
  assertWeakClassWeakInstances(class extends C {});
  assertWeakClassWeakInstances(class extends class {} {});
})();

(function SimpleStrongClassLiterals() {
  'use strong';
  class C {};
  class D extends C {};

  assertStrongClassStrongInstances(C);
  assertStrongClassStrongInstances(D);

  assertStrongClassStrongInstances(class {});
  assertStrongClassStrongInstances(class extends C {});
  assertStrongClassStrongInstances(class extends class {} {});
})();

(function MixedWeakClassLiterals() {
  class C extends getStrongClass() {};
  class D extends getStrongClassExtends((class {})) {};
  class E extends getStrongClassExtends(C) {};

  assertWeakClassStrongInstances(C);
  assertWeakClassStrongInstances(class extends getStrongClass() {});

  assertWeakClassWeakInstances(D);
  assertWeakClassWeakInstances(
    class extends getStrongClassExtends((class {})) {});

  assertWeakClassStrongInstances(E);
  assertWeakClassStrongInstances(
    class extends getStrongClassExtends(class extends getStrongClass() {}) {});
})();

(function MixedStrongClassLiterals() {
  'use strong';
  class C extends getWeakClass() {};
  class D extends getWeakClassExtends((class {})) {};
  class E extends getWeakClassExtends(C) {};
  class F extends Object {};

  assertStrongClassWeakInstances(C);
  assertStrongClassWeakInstances(class extends getWeakClass() {});

  assertStrongClassStrongInstances(D);
  assertStrongClassStrongInstances(
    class extends getWeakClassExtends((class {})) {});

  assertStrongClassWeakInstances(E);
  assertStrongClassWeakInstances(
    class extends getWeakClassExtends(class extends getWeakClass() {}) {});

  assertStrongClassWeakInstances(F);
  assertStrongClassWeakInstances(class extends Object {});
})();

(function WeakMonkeyPatchedClassLiterals() {
  class C {};
  assertWeakClassWeakInstances(C);
  C.__proto__ = getStrongClass();
  // C's default constructor doesn't call super.
  assertWeakClassWeakInstances(C);

  class D extends Object {};
  assertWeakClassWeakInstances(D);
  D.__proto__ = getStrongClass();
  // D is a derived class, so its default constructor calls super.
  assertWeakClassStrongInstances(D);

  class E extends (class {}) {};
  E.__proto__ = C;
  assertWeakClassWeakInstances(E);

  class F extends (class {}) {};
  F.__proto__ = D;
  assertWeakClassStrongInstances(F);

  class G extends getStrongClass() {};
  G.__proto__ = getWeakClass();
  assertWeakClassWeakInstances(G);
})();

(function StrongMonkeyPatchedClassLiterals() {
  let C = getStrongClassExtends(getWeakClassExtends(getStrongClass()));
  let D = getStrongClassExtends(getWeakClassExtends(getWeakClass()));

  assertStrongClassStrongInstances(C);
  C.__proto__.__proto__ = getWeakClass();
  assertStrongClassWeakInstances(C);
  C.__proto__.__proto__ = getStrongClass();
  assertStrongClassStrongInstances(C);

  assertStrongClassWeakInstances(D);
  D.__proto__.__proto__ = getStrongClass();
  assertStrongClassStrongInstances(D);
  D.__proto__.__proto__ = getWeakClass();
  assertStrongClassWeakInstances(D);
})();
