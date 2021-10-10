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
