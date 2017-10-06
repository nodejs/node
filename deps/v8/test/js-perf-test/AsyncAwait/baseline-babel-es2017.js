// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


new BenchmarkSuite('BaselineES2017', [1000], [
  new Benchmark('Basic', false, false, 0, Basic, Setup),
]);

function _asyncToGenerator(fn) {
  return function () {
    var gen = fn.apply(this, arguments);
    return new Promise(function (resolve, reject) {
      function step(key, arg) {
        try {
          var info = gen[key](arg);
          var value = info.value;
        } catch (error) {
          reject(error);
          return;
        }
        if (info.done) {
          resolve(value);
        } else {
          return Promise.resolve(value)
              .then(function (value) {
                step("next", value);
              }, function (err) {
                step("throw", err);
              });
        }
      }
      return step("next");
    });
  };
}

var a, b, c, d, e, f, g, h, i, j, x;

function Setup() {
  x = Promise.resolve();

  j = (() => {
    var _ref = _asyncToGenerator(function* () {
      return x;
    });

    function j() {
      return _ref.apply(this, arguments);
    }

    return j;
  })();
  i = (() => {
    var _ref2 = _asyncToGenerator(function* () {
      yield j();
      yield j();
      yield j();
      yield j();
      yield j();
      yield j();
      yield j();
      yield j();
      yield j();
      return j();
    });

    function i() {
      return _ref2.apply(this, arguments);
    }

    return i;
  })();
  h = (() => {
    var _ref3 = _asyncToGenerator(function* () {
      return i();
    });

    function h() {
      return _ref3.apply(this, arguments);
    }

    return h;
  })();
  g = (() => {
    var _ref4 = _asyncToGenerator(function* () {
      return h();
    });

    function g() {
      return _ref4.apply(this, arguments);
    }

    return g;
  })();
  f = (() => {
    var _ref5 = _asyncToGenerator(function* () {
      return g();
    });

    function f() {
      return _ref5.apply(this, arguments);
    }

    return f;
  })();
  e = (() => {
    var _ref6 = _asyncToGenerator(function* () {
      return f();
    });

    function e() {
      return _ref6.apply(this, arguments);
    }

    return e;
  })();
  d = (() => {
    var _ref7 = _asyncToGenerator(function* () {
      return e();
    });

    function d() {
      return _ref7.apply(this, arguments);
    }

    return d;
  })();
  c = (() => {
    var _ref8 = _asyncToGenerator(function* () {
      return d();
    });

    function c() {
      return _ref8.apply(this, arguments);
    }

    return c;
  })();
  b = (() => {
    var _ref9 = _asyncToGenerator(function* () {
      return c();
    });

    function b() {
      return _ref9.apply(this, arguments);
    }

    return b;
  })();
  a = (() => {
    var _ref10 = _asyncToGenerator(function* () {
      return b();
    });

    function a() {
      return _ref10.apply(this, arguments);
    }

    return a;
  })();

  %RunMicrotasks();
}

function Basic() {
  a();
  %RunMicrotasks();
}
