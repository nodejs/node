// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestFunctionWithContext() {
  function createObjects() {
    globalThis.foo = {
      key: (function () {
        let result = 'bar';
        function inner() { return result; }
        return inner;
      })(),
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo.key());
})();

(function TestInnerFunctionWithContextAndParentContext() {
  function createObjects() {
    globalThis.foo = {
      key: (function () {
        let part1 = 'snap';
        function inner() {
          let part2 = 'shot';
          function innerinner() {
            return part1 + part2;
          }
          return innerinner;
        }
        return inner();
      })()
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('snapshot', foo.key());
})();

(function TestTopLevelFunctionWithContext() {
  function createObjects() {
    globalThis.foo = (function () {
      let result = 'bar';
      function inner() { return result; }
      return inner;
    })();
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo());
})();

(function TestContextTree() {
  function createObjects() {
    (function outer() {
      let a = 10;
      let b = 20;
      (function inner1() {
        let c = 5;
        globalThis.f1 = function() { return a + b + c; };
      })();
      (function inner2() {
        let d = 10;
        globalThis.f2 = function() { return a - b - d; };
      })();
    })();
  }
  const {f1, f2} = takeAndUseWebSnapshot(createObjects, ['f1', 'f2']);
  assertEquals(35, f1());
  assertEquals(-20, f2());
})();

(function TestContextReferringToFunction() {
  function createObjects() {
    (function outer() {
      let a = function() { return 10; }
      globalThis.f = function() { return a(); };
    })();
  }
  const {f} = takeAndUseWebSnapshot(createObjects, ['f']);
  assertEquals(10, f());
})();

(function TestNonInlinedScopeInfoInContext() {
  function createObjects() {
    globalThis.bar = (function() {
      let a1 = 1;
      let a2 = 1;
      let a3 = 1;
      let a4 = 1;
      let a5 = 1;
      let a6 = 1;
      let a7 = 1;
      let a8 = 1;
      let a9 = 1;
      let a10 = 1;
      let a11 = 1;
      let a12 = 1;
      let a13 = 1;
      let a14 = 1;
      let a15 = 1;
      let a16 = 1;
      let a17 = 1;
      let a18 = 1;
      let a19 = 1;
      let a20 = 1;
      let a21 = 1;
      let a22 = 1;
      let a23 = 1;
      let a24 = 1;
      let a25 = 1;
      let a26 = 1;
      let a27 = 1;
      let a28 = 1;
      let a29 = 1;
      let a30 = 1;
      let a31 = 1;
      let a32 = 1;
      let a33 = 1;
      let a34 = 1;
      let a35 = 1;
      let a36 = 1;
      let a37 = 1;
      let a38 = 1;
      let a39 = 1;
      let a40 = 1;
      let a41 = 1;
      let a42 = 1;
      let a43 = 1;
      let a44 = 1;
      let a45 = 1;
      let a46 = 1;
      let a47 = 1;
      let a48 = 1;
      let a49 = 1;
      let a50 = 1;
      let a51 = 1;
      let a52 = 1;
      let a53 = 1;
      let a54 = 1;
      let a55 = 1;
      let a56 = 1;
      let a57 = 1;
      let a58 = 1;
      let a59 = 1;
      let a60 = 1;
      let a61 = 1;
      let a62 = 1;
      let a63 = 1;
      let a64 = 1;
      let a65 = 1;
      let a66 = 1;
      let a67 = 1;
      let a68 = 1;
      let a69 = 1;
      let a70 = 1;
      let a71 = 1;
      let a72 = 1;
      let a73 = 1;
      let a74 = 1;
      let a75 = 1;
      function inner1() {
        return a1;
      }
      function inner2() {
        return a2;
      }
      function inner3() {
        return a3;
      }
      function inner4() {
        return a4;
      }
      function inner5() {
        return a5;
      }
      function inner6() {
        return a6;
      }
      function inner7() {
        return a7;
      }
      function inner8() {
        return a8;
      }
      function inner9() {
        return a9;
      }
      function inner10() {
        return a10;
      }
      function inner11() {
        return a11;
      }
      function inner12() {
        return a12;
      }
      function inner13() {
        return a13;
      }
      function inner14() {
        return a14;
      }
      function inner15() {
        return a15;
      }
      function inner16() {
        return a16;
      }
      function inner17() {
        return a17;
      }
      function inner18() {
        return a18;
      }
      function inner19() {
        return a19;
      }
      function inner20() {
        return a20;
      }
      function inner21() {
        return a21;
      }
      function inner22() {
        return a22;
      }
      function inner23() {
        return a23;
      }
      function inner24() {
        return a24;
      }
      function inner25() {
        return a25;
      }
      function inner26() {
        return a26;
      }
      function inner27() {
        return a27;
      }
      function inner28() {
        return a28;
      }
      function inner29() {
        return a29;
      }
      function inner30() {
        return a30;
      }
      function inner31() {
        return a31;
      }
      function inner32() {
        return a32;
      }
      function inner33() {
        return a33;
      }
      function inner34() {
        return a34;
      }
      function inner35() {
        return a35;
      }
      function inner36() {
        return a36;
      }
      function inner37() {
        return a37;
      }
      function inner38() {
        return a38;
      }
      function inner39() {
        return a39;
      }
      function inner40() {
        return a40;
      }
      function inner41() {
        return a41;
      }
      function inner42() {
        return a42;
      }
      function inner43() {
        return a43;
      }
      function inner44() {
        return a44;
      }
      function inner45() {
        return a45;
      }
      function inner46() {
        return a46;
      }
      function inner47() {
        return a47;
      }
      function inner48() {
        return a48;
      }
      function inner49() {
        return a49;
      }
      function inner50() {
        return a50;
      }
      function inner51() {
        return a51;
      }
      function inner52() {
        return a52;
      }
      function inner53() {
        return a53;
      }
      function inner54() {
        return a54;
      }
      function inner55() {
        return a55;
      }
      function inner56() {
        return a56;
      }
      function inner57() {
        return a57;
      }
      function inner58() {
        return a58;
      }
      function inner59() {
        return a59;
      }
      function inner60() {
        return a60;
      }
      function inner61() {
        return a61;
      }
      function inner62() {
        return a62;
      }
      function inner63() {
        return a63;
      }
      function inner64() {
        return a64;
      }
      function inner65() {
        return a65;
      }
      function inner66() {
        return a66;
      }
      function inner67() {
        return a67;
      }
      function inner68() {
        return a68;
      }
      function inner69() {
        return a69;
      }
      function inner70() {
        return a70;
      }
      function inner71() {
        return a71;
      }
      function inner72() {
        return a72;
      }
      function inner73() {
        return a73;
      }
      function inner74() {
        return a74;
      }
      function inner75() {
        return a75;
      }
      return inner1;
    })()
  }
  const {bar} = takeAndUseWebSnapshot(createObjects, ['bar']);
  assertEquals(1, bar());
})();

(function TestMoreThanOneScopeLocalInContext() {
  function createObjects() {
    globalThis.foo = (function() {
      let result = 'bar';
      let a = '1';
      function inner() {
        return result;
      }
      function inner2() {
        return a;
      }
      return inner;
    })();
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo());
})();

(function TestContextReferencingArray() {
  function createObjects() {
    function outer() {
      let o = [11525];
      function inner() { return o; }
      return inner;
    }
    globalThis.foo = {
      func: outer()
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(11525, foo.func()[0]);
})();

(function TestContextReferencingObject() {
  function createObjects() {
    function outer() {
      let o = { value: 11525 };
      function inner() { return o; }
      return inner;
    }
    globalThis.foo = {
      func: outer()
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(11525, foo.func().value);
})();
