// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"use strict";

let i = 0;
function EvaluateSinglePublicFieldClass() {
  return class SinglePublicFieldClass {
    x = i;

    check() {
      return this.x === i;
    }
  };
}

function EvaluateMultiPublicFieldClass() {
  return class MultiPublicFieldClass {
    x = i;
    y = i+1;
    z = i+2;
    q = i+3;
    r = i+4;
    a = i+5;

    check() {
      return this.x + 1 === this.y && this.y + 1 === this.z &&
            this.z + 1 === this.q && this.q + 1 === this.r &&
            this.r + 1 === this.a;
    }
  };
}

function EvaluateSinglePrivateFieldClass() {
  return class SinglePrivateFieldClass {
    #x = i;

    check() {
      return this.#x === i;
    }
  }
}

function EvaluateMultiPrivateFieldClass() {
  return class MultiPrivateFieldClass {
    #x = i;
    #y = i+1;
    #z = i+2;
    #q = i+3;
    #r = i+4;
    #a = i+5;

    check() {
      return this.#x + 1 === this.#y && this.#y + 1 === this.#z &&
            this.#z + 1 === this.#q && this.#q + 1 === this.#r &&
            this.#r + 1 === this.#a;
    }
  };
}

function EvaluateSinglePrivateMethodClass() {
  return class PrivateSingleMethodClass {
    #x() { return 0; }

    check() {
      return this.#x() === 0;
    }
  };
}

function EvaluateMultiPrivateMethodClass() {
  return class PrivateMultiMethodClass {
    #x() { return 0; }
    #y() { return 1; }
    #z() { return 2; }
    #q() { return 3; }
    #r() { return 4; }
    #a() { return 5; }

    check() {
      return this.#x() + 1 === this.#y() && this.#y() + 1 === this.#z() &&
            this.#z() + 1 === this.#q() && this.#q() + 1 === this.#r() &&
            this.#r() + 1 === this.#a();
    }
  };
}

function key(i) {
  return 'abcdefghijklmnopqrstuvwxyz'[i];
}

function EvaluateSingleComputedFieldClass() {
  return class SingleComputedFieldClass {
    [key(0)] = i;

    check() {
      return this[key(0)] === i;
    }
  }
}

function EvaluateMultiComputedFieldClass() {
  return class MultiComputedFieldClass {
    [key(0)] = i;
    [key(1)] = i+1;
    [key(2)] = i+2;
    [key(3)] = i+3;
    [key(4)] = i+4;
    [key(5)] = i+5;

    check() {
      return this[key(0)] + 1 === this[key(1)] && this[key(1)] + 1 === this[key(2)] &&
            this[key(2)] + 1 === this[key(3)] && this[key(3)] + 1 === this[key(4)] &&
            this[key(4)] + 1 === this[key(5)];
    }
  };
}
