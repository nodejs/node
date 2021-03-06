// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function PrintDesc(desc, s) {
  var json;
  if (desc) {
    json = JSON.stringify(desc);
  } else {
    json = "<no such property>";
  }
  if (s === undefined) {
    print(json);
  } else {
    print(s + ": " + json);
  }
}


var counters;
var test_realm;
var cfg;


function GetDescriptor() {
  var code = 'Object.getOwnPropertyDescriptor(global, "x")';
  var desc = Realm.eval(test_realm, code);
//  PrintDesc(desc);
  return desc;
}

function SetUp() {
  counters = {};
  Realm.shared = {counters: counters};
  test_realm = Realm.create();
  Realm.eval(test_realm, 'var global = Realm.global(Realm.current());');
  print("=====================");
  print("Test realm: " + test_realm);
  assertEquals(undefined, GetDescriptor());
}

function TearDown() {
  Realm.dispose(test_realm);
  print("OK");
}


function AddStrict(code, cfg) {
  return cfg.strict ? '"use strict"; ' + code : code;
}

function ForceMutablePropertyCellType() {
  Realm.eval(test_realm, 'global.x = {}; global.x = undefined;');
}

function DeclareVar() {
  var code = 'var x;';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function DefineVar(v) {
  var code = 'var x = ' + v;
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function DefineLoadVar() {
  var name = 'LoadVar_' + test_realm;
  var code =
      'var x;' +
      'function ' + name + '() {' +
      '  return x;' +
      '};'
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function LoadVar() {
  var name = 'LoadVar_' + test_realm;
  var code = (cfg.optimize ? '%PrepareFunctionForOptimization(' + name + ');' : '') +
      (cfg.optimize ? '%OptimizeFunctionOnNextCall(' + name + ');' : '') +
      name + '();';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function DefineStoreVar() {
  var name = 'StoreVar_' + test_realm;
  var code = 'var g = (Function("return this"))();' +
      'var x;' +
      'function ' + name + '(v) {' +
      '  return x = v;' +
      '};';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function StoreVar(v) {
  var name = 'StoreVar_' + test_realm;
  var code = (cfg.optimize ? '%PrepareFunctionForOptimization(' + name + ');' : '') +
      (cfg.optimize ? '%OptimizeFunctionOnNextCall(' + name + ');' : '') +
      name + '(' + v + ');';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

// It does 13 iterations which results in 27 loads
// and 14 stores.
function LoadStoreLoop() {
  var code = 'for(var x = 0; x < 13; x++);';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function DefineRWDataProperty() {
  var code =
      'Object.defineProperty(global, "x", { ' +
      '  value: 42, ' +
      '  writable: true, ' +
      '  enumerable: true, ' +
      '  configurable: true ' +
      '});';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function DefineRODataProperty() {
  var code =
      'Object.defineProperty(global, "x", { ' +
      '  value: 42, ' +
      '  writable: false, ' +
      '  enumerable: true, ' +
      '  configurable: true ' +
      '});';
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function SetX_(v) {
  var code =
      'global.x_ = ' + v + '; ';
  return Realm.eval(test_realm, code);
}

function DefineRWAccessorProperty() {
  var code =
      'Object.defineProperty(global, "x", {' +
      '  get: function() { Realm.shared.counters.get_count++; return this.x_; },' +
      '  set: function(v) { Realm.shared.counters.set_count++; this.x_ = v; },' +
      '  enumerable: true, configurable: true' +
      '});';
  counters.get_count = 0;
  counters.set_count = 0;
  return Realm.eval(test_realm, AddStrict(code, cfg));
}

function DefineROAccessorProperty() {
  var code =
      'Object.defineProperty(global, "x", {' +
      '  get: function() { Realm.shared.counters.get_count++; return this.x_; },' +
      '  enumerable: true, configurable: true' +
      '});';
  counters.get_count = 0;
  counters.set_count = 0;
  return Realm.eval(test_realm, AddStrict(code, cfg));
}


function testSuite(opt_cfg) {
  //
  // Non strict.
  //

  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: false};
    DeclareVar();
    DefineLoadVar();
    DefineStoreVar();
    assertEquals(undefined, LoadVar());
    assertEquals(false, GetDescriptor().configurable);

    // Force property cell type to kMutable.
    DefineVar(undefined);
    DefineVar(153);
    assertEquals(false, GetDescriptor().configurable);

    assertEquals(153, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(113, LoadVar());
    LoadStoreLoop();
    assertEquals(13, LoadVar());
    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: false};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineRWDataProperty();
    assertEquals(42, LoadVar());
    assertEquals(true, GetDescriptor().configurable);

    DefineVar(153);
    assertEquals(true, GetDescriptor().configurable);

    assertEquals(153, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(113, LoadVar());
    LoadStoreLoop();
    assertEquals(13, LoadVar());

    // Now reconfigure to accessor.
    DefineRWAccessorProperty();
    assertEquals(undefined, GetDescriptor().value);
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(0, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(undefined, LoadVar());
    assertEquals(1, counters.get_count);
    assertEquals(0, counters.set_count);

    LoadStoreLoop();
    assertEquals(28, counters.get_count);
    assertEquals(14, counters.set_count);

    assertEquals(13, LoadVar());
    assertEquals(29, counters.get_count);
    assertEquals(14, counters.set_count);

    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: false};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineRODataProperty();
    assertEquals(42, LoadVar());
    assertEquals(true, GetDescriptor().configurable);

    DefineVar(153);

    assertEquals(42, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(42, LoadVar());
    LoadStoreLoop();
    assertEquals(42, LoadVar());

    // Now reconfigure to accessor property.
    DefineRWAccessorProperty();
    assertEquals(undefined, GetDescriptor().value);
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(0, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(undefined, LoadVar());
    assertEquals(1, counters.get_count);
    assertEquals(0, counters.set_count);

    LoadStoreLoop();
    assertEquals(28, counters.get_count);
    assertEquals(14, counters.set_count);

    assertEquals(13, LoadVar());
    assertEquals(29, counters.get_count);
    assertEquals(14, counters.set_count);

    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: false};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineRWAccessorProperty();
    assertEquals(0, counters.get_count);
    assertEquals(0, counters.set_count);
    assertEquals(true, GetDescriptor().configurable);

    assertEquals(undefined, LoadVar());
    assertEquals(1, counters.get_count);
    assertEquals(0, counters.set_count);

    DefineVar(153);
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(1, counters.get_count);
    assertEquals(1, counters.set_count);

    assertEquals(153, LoadVar());
    assertEquals(2, counters.get_count);
    assertEquals(1, counters.set_count);

    assertEquals(113, StoreVar(113));
    assertEquals(2, counters.get_count);
    assertEquals(2, counters.set_count);

    assertEquals(113, LoadVar());
    assertEquals(3, counters.get_count);
    assertEquals(2, counters.set_count);

    LoadStoreLoop();
    assertEquals(30, counters.get_count);
    assertEquals(16, counters.set_count);

    assertEquals(13, LoadVar());
    assertEquals(31, counters.get_count);
    assertEquals(16, counters.set_count);

    // Now reconfigure to data property.
    DefineRWDataProperty();
    assertEquals(42, GetDescriptor().value);
    assertEquals(42, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(31, counters.get_count);
    assertEquals(16, counters.set_count);

    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: false};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineROAccessorProperty();
    assertEquals(0, counters.get_count);
    assertEquals(0, counters.set_count);
    assertEquals(true, GetDescriptor().configurable);

    assertEquals(undefined, LoadVar());
    assertEquals(1, counters.get_count);
    assertEquals(0, counters.set_count);

    SetX_(42);
    assertEquals(42, LoadVar());
    assertEquals(2, counters.get_count);
    assertEquals(0, counters.set_count);

    DefineVar(153);
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(2, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(42, LoadVar());
    assertEquals(3, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(113, StoreVar(113));
    assertEquals(3, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(42, LoadVar());
    assertEquals(4, counters.get_count);
    assertEquals(0, counters.set_count);

    LoadStoreLoop();
    assertEquals(5, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(42, LoadVar());
    assertEquals(6, counters.get_count);
    assertEquals(0, counters.set_count);

    // Now reconfigure to data property.
    DefineRWDataProperty();
    assertEquals(42, GetDescriptor().value);
    assertEquals(42, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(6, counters.get_count);
    assertEquals(0, counters.set_count);

    TearDown();
  })();


  //
  // Strict.
  //

  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: true};
    DeclareVar();
    DefineLoadVar();
    DefineStoreVar();
    assertEquals(undefined, LoadVar());
    assertEquals(false, GetDescriptor().configurable);

    // Force property cell type to kMutable.
    DefineVar(undefined);
    DefineVar(153);
    assertEquals(false, GetDescriptor().configurable);

    assertEquals(153, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(113, LoadVar());
    LoadStoreLoop();
    assertEquals(13, LoadVar());
    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: true};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineRWDataProperty();
    assertEquals(42, LoadVar());
    assertEquals(true, GetDescriptor().configurable);

    DefineVar(153);
    assertEquals(true, GetDescriptor().configurable);

    assertEquals(153, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(113, LoadVar());
    LoadStoreLoop();
    assertEquals(13, LoadVar());
    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: true};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineRWDataProperty();
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(true, GetDescriptor().writable);
    assertEquals(113, StoreVar(113));

    DefineRODataProperty();
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(false, GetDescriptor().writable);

    assertEquals(42, LoadVar());
    assertEquals(true, GetDescriptor().configurable);
    assertThrows('DefineVar(153)');
    assertEquals(42, LoadVar());
    assertThrows('StoreVar(113)');
    assertThrows('StoreVar(113)');
    assertEquals(42, LoadVar());
    assertThrows('StoreVar(42)');
    assertEquals(42, LoadVar());
    assertThrows('LoadStoreLoop()');
    assertEquals(42, LoadVar());
    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: true};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineRWAccessorProperty();
    assertEquals(0, counters.get_count);
    assertEquals(0, counters.set_count);
    assertEquals(true, GetDescriptor().configurable);

    assertEquals(undefined, LoadVar());
    assertEquals(1, counters.get_count);
    assertEquals(0, counters.set_count);

    DefineVar(153);
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(1, counters.get_count);
    assertEquals(1, counters.set_count);

    assertEquals(153, LoadVar());
    assertEquals(2, counters.get_count);
    assertEquals(1, counters.set_count);

    assertEquals(113, StoreVar(113));
    assertEquals(2, counters.get_count);
    assertEquals(2, counters.set_count);

    assertEquals(113, LoadVar());
    assertEquals(3, counters.get_count);
    assertEquals(2, counters.set_count);

    LoadStoreLoop();
    assertEquals(30, counters.get_count);
    assertEquals(16, counters.set_count);

    assertEquals(13, LoadVar());
    assertEquals(31, counters.get_count);
    assertEquals(16, counters.set_count);

    // Now reconfigure to data property.
    DefineRWDataProperty();
    assertEquals(42, GetDescriptor().value);
    assertEquals(42, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(31, counters.get_count);
    assertEquals(16, counters.set_count);

    TearDown();
  })();


  (function() {
    SetUp();
    cfg = {optimize: opt_cfg.optimize, strict: true};
    ForceMutablePropertyCellType();
    DefineLoadVar();
    DefineStoreVar();
    DefineROAccessorProperty();
    assertEquals(0, counters.get_count);
    assertEquals(0, counters.set_count);
    assertEquals(true, GetDescriptor().configurable);

    assertEquals(undefined, LoadVar());
    assertEquals(1, counters.get_count);
    assertEquals(0, counters.set_count);

    SetX_(42);
    assertEquals(42, LoadVar());
    assertEquals(2, counters.get_count);
    assertEquals(0, counters.set_count);

    assertThrows('DefineVar(153)');
    assertEquals(true, GetDescriptor().configurable);
    assertEquals(2, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(42, LoadVar());
    assertEquals(3, counters.get_count);
    assertEquals(0, counters.set_count);

    assertThrows('StoreVar(113)');
    assertEquals(3, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(42, LoadVar());
    assertEquals(4, counters.get_count);
    assertEquals(0, counters.set_count);

    assertThrows('LoadStoreLoop()');
    assertEquals(4, counters.get_count);
    assertEquals(0, counters.set_count);

    assertEquals(42, LoadVar());
    assertEquals(5, counters.get_count);
    assertEquals(0, counters.set_count);

    // Now reconfigure to data property.
    DefineRWDataProperty();
    assertEquals(42, GetDescriptor().value);
    assertEquals(42, LoadVar());
    assertEquals(113, StoreVar(113));
    assertEquals(5, counters.get_count);
    assertEquals(0, counters.set_count);

    TearDown();
  })();

}  // testSuite


testSuite({optimize: false});
testSuite({optimize: true});
