// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// This test checks the ToPrimitive behavior for Date objects during string
// addition. Unlike other objects, Date objects default to a 'string' hint
// for ToPrimitive, meaning toString() should be called instead of valueOf().
{
  let log = [];
  const date = new Date();
  date.toString = function() {
    log.push('toString');
    return 'date-as-string';
  };
  date.valueOf = function() {
    log.push('valueOf');
    return 12345;
  };

  function add_string_and_date(d) {
    return "prefix-" + d;
  }

  %PrepareFunctionForOptimization(add_string_and_date);
  log = [];
  assertEquals('prefix-date-as-string', add_string_and_date(date));
  assertEquals(['toString'], log);

  %OptimizeFunctionOnNextCall(add_string_and_date);
  log = [];
  assertEquals('prefix-date-as-string', add_string_and_date(date));
  assertEquals(['toString'], log);

  // Check with date on the left side as well.
  function add_date_and_string(d) {
    return d + "-suffix";
  }

  %PrepareFunctionForOptimization(add_date_and_string);
  log = [];
  assertEquals('date-as-string-suffix', add_date_and_string(date));
  assertEquals(['toString'], log);

  %OptimizeFunctionOnNextCall(add_date_and_string);
  log = [];
  assertEquals('date-as-string-suffix', add_date_and_string(date));
  assertEquals(['toString'], log);
}

// For comparison, a plain object defaults to a 'number' hint, so valueOf()
// is called.
{
  let log = [];
  const obj = {
    toString() {
      log.push('toString');
      return 'obj-as-string';
    },
    valueOf() {
      log.push('valueOf');
      return 9876;
    }
  };

  function add_string_and_obj(o) {
    return "prefix-" + o;
  }

  %PrepareFunctionForOptimization(add_string_and_obj);
  log = [];
  assertEquals('prefix-9876', add_string_and_obj(obj));
  assertEquals(['valueOf'], log);

  %OptimizeFunctionOnNextCall(add_string_and_obj);
  log = [];
  assertEquals('prefix-9876', add_string_and_obj(obj));
  assertEquals(['valueOf'], log);
}
