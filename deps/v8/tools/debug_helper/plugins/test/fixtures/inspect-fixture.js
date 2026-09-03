// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Greeter {
  constructor(name) {
    this.name = name;
    this.count = 0;
  }

  greet() {
    this.count += 1;
    return this.greetInner('!', 3, true);
  }

  greetInner(suffix, repeat, fatal) {
    const message = this.name + (suffix || '').repeat(repeat || 0);
    if (fatal) {
      throw new Error('v8 inspect test: ' + message);
    }
    return message;
  }
}

const greeter = new Greeter('world');
greeter.greet();
