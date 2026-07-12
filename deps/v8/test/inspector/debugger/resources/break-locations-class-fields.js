// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let x = class {}

x = class {
  x = 1;
  y = 2;
}

x = class {
  x = foo();
  y = 2;
  z = bar();
}

x = class {
  x = foo();
  y = 2;
  z = bar();
  constructor() {
    this.x;
  }
}

x = class {
  x = foo();
  y = 2;
  constructor() {
    this.x;
  }
  z = bar();
}

x = class {
  x = foo();
  y = 2;
  constructor() {
    this.x;
  }
  z = bar();
}

x = class {
  x = 1;
  foo() {}
  y = 2;
}

x = class {
  x = (function() {
    foo();
  })();
  y = (() => {
    bar();
  })();
}

x = class {
  x = function() {
    foo();
  };
}

x = class {
  x = async function() {
    await foo();
  };
}

x = class {
  x = () => {
    foo();
  };
  y = () => bar();
}

x = class {
  x = async () => {
    await foo();
  };
  y = async () => await bar();
}

x = class {
  [x] = 1;
  [foo()] = 2;
}

x = class {
  [x] = [...this];
}

x = class {
  x;
  [foo()];
}

x = class {
  x = function*() {
    yield 1;
  };
}

x = class {
  static x = 1;
  static y = 2;
}

x = class {
  static x = foo();
  static y = 2;
  static z = bar();
}

x = class {
  static x = foo();
  static y = 2;
  static z = bar();
  constructor() {
    this.x;
  }
}

x = class {
  static x = foo();
  static y = 2;
  constructor() {
    this.x;
  }
  static z = bar();
}

x = class {
  static x = 1;
  static foo() {}
  bar() {}
  static y = 2;
}

x = class {
  static x = (function() {
    foo();
  })();
  static y = (() => {
    bar();
  })();
}

x = class {
  static x = function() {
    foo();
  };
}

x = class {
  static x = async function() {
    await foo();
  };
}

x = class {
  static x = () => {
    foo();
  };
  static y = () => bar();
}

x = class {
  static x = async () => {
    await foo();
  };
  static y = async () => await bar();
}

x = class {
  static [x] = 1;
  static [foo()] = 2;
}

x = class {
  static [x] = [...this];
}

x = class {
  static x;
  static [foo()];
}

x = class {
  static x = function*() {
    yield 1;
  };
}

x = class {
  static x = 1;
  y = 2;
  static [z] = 3;
  [p] = 4;
  static [foo()] = 5;
  [bar()] = 6;
}
