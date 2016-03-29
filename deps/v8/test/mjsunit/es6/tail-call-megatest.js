// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls --no-turbo-inlining


Error.prepareStackTrace = (error,stack) => {
  error.strace = stack;
  return error.message + "\n    at " + stack.join("\n    at ");
}


function CheckStackTrace(expected) {
  var e = new Error();
  e.stack;  // prepare stack trace
  var stack = e.strace;
  assertEquals("CheckStackTrace", stack[0].getFunctionName());
  for (var i = 0; i < expected.length; i++) {
    assertEquals(expected[i].name, stack[i + 1].getFunctionName());
  }
}
%NeverOptimizeFunction(CheckStackTrace);


function CheckArguments(expected, args) {
  args = Array.prototype.slice.call(args);
  assertEquals(expected, args);
}
%NeverOptimizeFunction(CheckArguments);


var CAN_INLINE_COMMENT  = "// Let it be inlined.";
var DONT_INLINE_COMMENT = (function() {
  var line = "// Don't inline. Don't inline. Don't inline. Don't inline.";
  for (var i = 0; i < 4; i++) {
    line += "\n  " + line;
  }
  return line;
})();


function ident_source(source, ident) {
  ident = " ".repeat(ident);
  return ident + source.replace(/\n/gi, "\n" + ident);
}

var global = Function('return this')();
var the_receiver = {receiver: 1};

function run_tests() {
  function inlinable_comment(inlinable) {
    return inlinable ? CAN_INLINE_COMMENT : DONT_INLINE_COMMENT;
  }

  var f_cfg_sloppy = {
    func_name: 'f',
    source_template: function(cfg) {
      var receiver = cfg.f_receiver != undefined ? cfg.f_receiver
                                                 : "global";
      var lines = [
        `function f(a) {`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  assertEquals(${receiver}, this);`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  %DeoptimizeNow();`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  return 42;`,
        `}`,
      ];
      return lines.join("\n");
    },
  };

  var f_cfg_strict = {
    func_name: 'f',
    source_template: function(cfg) {
      var receiver = cfg.f_receiver != undefined ? cfg.f_receiver
                                                 : "undefined";
      var lines = [
        `function f(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  assertEquals(${receiver}, this);`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  %DeoptimizeNow();`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  return 42;`,
        `}`,
      ];
      return lines.join("\n");
    },
  };

  var f_cfg_possibly_eval = {
    func_name: 'eval',
    source_template: function(cfg) {
      var receiver = cfg.f_receiver != undefined ? cfg.f_receiver
                                                 : "global";
      var lines = [
        `function f(a) {`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  assertEquals(${receiver}, this);`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  %DeoptimizeNow();`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  return 42;`,
        `}`,
        `var eval = f;`,
      ];
      return lines.join("\n");
    },
  };

  var f_cfg_bound = {
    func_name: 'bound',
    source_template: function(cfg) {
      var lines = [
        `function f(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  assertEquals(receiver, this);`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  %DeoptimizeNow();`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  return 42;`,
        `}`,
        `var receiver = {a: 153};`,
        `var bound = f.bind(receiver);`,
      ];
      return lines.join("\n");
    },
  };

  var f_cfg_proxy = {
    func_name: 'p',
    source_template: function(cfg) {
      var receiver = cfg.f_receiver != undefined ? cfg.f_receiver
                                                 : "global";
      var lines = [
        `function f(a) {`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  assertEquals(${receiver}, this);`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  %DeoptimizeNow();`,
        `  CheckArguments([${cfg.f_args}], arguments);`,
        `  CheckStackTrace([f, test]);`,
        `  return 42;`,
        `}`,
        `var p = new Proxy(f, {});`,
      ];
      return lines.join("\n");
    },
  };

  var g_cfg_normal = {
    receiver: undefined,
    source_template: function(cfg) {
      var lines = [
        `function g(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.g_inlinable)}`,
        `  CheckArguments([${cfg.g_args}], arguments);`,
        `  return ${cfg.f_name}(${cfg.f_args});`,
        `}`,
      ];
      return lines.join("\n");
    },
  };


  var g_cfg_function_apply = {
    receiver: "the_receiver",
    source_template: function(cfg) {
      var lines = [
        `function g(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.g_inlinable)}`,
        `  CheckArguments([${cfg.g_args}], arguments);`,
        `  return ${cfg.f_name}.apply(the_receiver, [${cfg.f_args}]);`,
        `}`,
      ];
      return lines.join("\n");
    },
  };


  var g_cfg_function_call = {
    receiver: "the_receiver",
    source_template: function(cfg) {
      var f_args = "the_receiver";
      if (cfg.f_args !== "") f_args += ", ";
      f_args += cfg.f_args;

      var lines = [
        `function g(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.g_inlinable)}`,
        `  CheckArguments([${cfg.g_args}], arguments);`,
        `  return ${cfg.f_name}.call(${f_args});`,
        `}`,
      ];
      return lines.join("\n");
    },
  };


  function test_template(cfg) {
    var f_source = cfg.f_source_template(cfg);
    var g_source = cfg.g_source_template(cfg);
    f_source = ident_source(f_source, 2);
    g_source = ident_source(g_source, 2);

    var lines = [
      `(function() {`,
      f_source,
      g_source,
      `  function test() {`,
      `    "use strict";`,
      `    assertEquals(42, g(${cfg.g_args}));`,
      `  }`,
      `  ${cfg.f_inlinable ? "%SetForceInlineFlag(f)" : ""};`,
      `  ${cfg.g_inlinable ? "%SetForceInlineFlag(g)" : ""};`,
      ``,
      `  test();`,
      `  %OptimizeFunctionOnNextCall(test);`,
      `  %OptimizeFunctionOnNextCall(f);`,
      `  %OptimizeFunctionOnNextCall(g);`,
      `  test();`,
      `})();`,
      ``,
    ];
    var source = lines.join("\n");
    return source;
  }

  // TODO(v8:4698), TODO(ishell): support all commented cases.
  var f_args_variants = ["", "1", "1, 2"];
  var g_args_variants = [/*"",*/ "10", /*"10, 20"*/];
  var f_inlinable_variants = [/*true,*/ false];
  var g_inlinable_variants = [true, false];
  var f_variants = [
      f_cfg_sloppy,
      f_cfg_strict,
      f_cfg_bound,
      f_cfg_proxy,
      f_cfg_possibly_eval,
  ];
  var g_variants = [
      g_cfg_normal,
      g_cfg_function_call,
      g_cfg_function_apply,
  ];

  f_variants.forEach((f_cfg) => {
    g_variants.forEach((g_cfg) => {
      f_args_variants.forEach((f_args) => {
        g_args_variants.forEach((g_args) => {
          f_inlinable_variants.forEach((f_inlinable) => {
            g_inlinable_variants.forEach((g_inlinable) => {
              var cfg = {
                f_source_template: f_cfg.source_template,
                f_inlinable,
                f_args,
                f_name: f_cfg.func_name,
                f_receiver: g_cfg.receiver,
                g_source_template: g_cfg.source_template,
                g_inlinable,
                g_args,
              };
              var source = test_template(cfg);
              print("====================");
              print(source);
              eval(source);
            });
          });
        });
      });
    });
  });
}

run_tests();
