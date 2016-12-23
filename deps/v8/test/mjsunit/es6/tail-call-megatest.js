// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls


Error.prepareStackTrace = (error,stack) => {
  error.strace = stack;
  return error.message + "\n    at " + stack.join("\n    at ");
}

var verbose = typeof(arguments) !== "undefined" && arguments.indexOf("-v") >= 0;

function checkStackTrace(expected) {
  var e = new Error();
  e.stack;  // prepare stack trace
  var stack = e.strace;
  assertEquals("checkStackTrace", stack[0].getFunctionName());
  for (var i = 0; i < expected.length; i++) {
    assertEquals(expected[i].name, stack[i + 1].getFunctionName());
  }
}


var CAN_INLINE_COMMENT  = "// Let it be inlined.";
var DONT_INLINE_COMMENT = (function() {
  var line = "1";
  for (var i = 0; i < 200; ++i) {
    line += "," + i;
  }
  line += ";\n";
  return line;
})();


function ident_source(source, ident) {
  ident = " ".repeat(ident);
  return ident + source.replace(/\n/gi, "\n" + ident);
}

var SHARDS_COUNT = 10;

function run_tests(shard) {
  function inlinable_comment(inlinable) {
    return inlinable ? CAN_INLINE_COMMENT : DONT_INLINE_COMMENT;
  }

  // Check arguments manually to avoid bailing out with reason "bad value
  // context for arguments value".
  function check_arguments_template(expected_name) {
    var lines = [
      `  assertEquals_(${expected_name}.length, arguments.length);`,
      `  for (var i = 0; i < ${expected_name}.length; i++) {`,
      `    assertEquals_(${expected_name}[i], arguments[i]);`,
      `  }`,
    ];
    return lines.join("\n");
  }
  var check_arguments = check_arguments_template("expected_args");

  function deopt_template(deopt_mode) {
    switch(deopt_mode) {
      case "none":
        return "  // Don't deoptimize";
      case "f":
      case "g":
      case "test":
        return `  %DeoptimizeFunction(${deopt_mode});`;
      default:
        assertUnreachable();
    }
  }

  var f_cfg_sloppy = {
    func_name: 'f',
    source_template: function(cfg) {
      var receiver = cfg.f_receiver != undefined ? cfg.f_receiver
                                                 : "global";
      var do_checks = [
        `  assertEquals_(${receiver}, this);`,
        `  ${!cfg.check_new_target ? "// " : ""}assertEquals_(undefined, new.target);`,
        check_arguments,
        `  checkStackTrace_([f, test]);`,
      ].join("\n");

      var lines = [
        `function f(a) {`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  counter++;`,
        `  var expected_args = [${cfg.f_args}];`,
        do_checks,
        deopt_template(cfg.deopt_mode),
        do_checks,
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
      var do_checks = [
        `  assertEquals_(${receiver}, this);`,
        `  ${!cfg.check_new_target ? "// " : ""}assertEquals_(undefined, new.target);`,
        check_arguments,
        `  checkStackTrace_([f, test]);`,
      ].join("\n");

      var lines = [
        `function f(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  counter++;`,
        `  var expected_args = [${cfg.f_args}];`,
        do_checks,
        deopt_template(cfg.deopt_mode),
        do_checks,
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
      var do_checks = [
        `  assertEquals_(${receiver}, this);`,
        `  ${!cfg.check_new_target ? "// " : ""}assertEquals_(undefined, new.target);`,
        check_arguments,
        `  checkStackTrace_([f, test]);`,
      ].join("\n");

      var lines = [
        `function f(a) {`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  counter++;`,
        `  var expected_args = [${cfg.f_args}];`,
        do_checks,
        deopt_template(cfg.deopt_mode),
        do_checks,
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
      var do_checks = [
        `  assertEquals_(receiver, this);`,
        `  ${!cfg.check_new_target ? "// " : ""}assertEquals_(undefined, new.target);`,
        check_arguments,
        `  checkStackTrace_([f, test]);`,
      ].join("\n");

      var lines = [
        `function f(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  counter++;`,
        `  var expected_args = [${cfg.f_args}];`,
        do_checks,
        deopt_template(cfg.deopt_mode),
        do_checks,
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
      var do_checks = [
        `  assertEquals_(${receiver}, this);`,
        `  ${!cfg.check_new_target ? "// " : ""}assertEquals_(undefined, new.target);`,
        check_arguments,
        `  checkStackTrace_([f, test]);`,
      ].join("\n");

      var lines = [
        `function f(a) {`,
        `  ${inlinable_comment(cfg.f_inlinable)}`,
        `  counter++;`,
        `  var expected_args = [${cfg.f_args}];`,
        do_checks,
        deopt_template(cfg.deopt_mode),
        do_checks,
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
        `  var expected_args = [${cfg.g_args}];`,
        check_arguments,
        `  return ${cfg.f_name}(${cfg.f_args});`,
        `}`,
      ];
      return lines.join("\n");
    },
  };


  var g_cfg_reflect_apply = {
    receiver: "the_receiver",
    source_template: function(cfg) {
      var lines = [
        `function g(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.g_inlinable)}`,
        `  var expected_args = [${cfg.g_args}];`,
        check_arguments,
        `  return Reflect.apply(${cfg.f_name}, the_receiver, [${cfg.f_args}]);`,
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
        `  var expected_args = [${cfg.g_args}];`,
        check_arguments,
        `  return ${cfg.f_name}.apply(the_receiver, [${cfg.f_args}]);`,
        `}`,
      ];
      return lines.join("\n");
    },
  };


  var g_cfg_function_apply_arguments_object = {
    receiver: "the_receiver",
    source_template: function(cfg) {
      cfg.f_args = cfg.g_args;
      var lines = [
        `function g(a) {`,
        `  "use strict";`,
        `  ${inlinable_comment(cfg.g_inlinable)}`,
        `  var expected_args = [${cfg.g_args}];`,
        check_arguments,
        `  return ${cfg.f_name}.apply(the_receiver, arguments);`,
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
        `  var expected_args = [${cfg.g_args}];`,
        check_arguments,
        `  return ${cfg.f_name}.call(${f_args});`,
        `}`,
      ];
      return lines.join("\n");
    },
  };


  function test_template(cfg) {
    // Note: g_source_template modifies cfg.f_args in some cases.
    var g_source = cfg.g_source_template(cfg);
    g_source = ident_source(g_source, 2);

    var f_source = cfg.f_source_template(cfg);
    f_source = ident_source(f_source, 2);

    var lines = [
      `(function() {`,
      `  // Avoid bailing out because of "Reference to a variable which requires dynamic lookup".`,
      `  var assertEquals_ = assertEquals;`,
      `  var checkStackTrace_ = checkStackTrace;`,
      `  var undefined = void 0;`,
      `  var global = Function('return this')();`,
      `  var the_receiver = {receiver: 1};`,
      `  var counter = 0;`,
      ``,
      `  // Don't inline helper functions`,
      `  %NeverOptimizeFunction(assertEquals);`,
      `  %NeverOptimizeFunction(checkStackTrace);`,
      ``,
      f_source,
      g_source,
      `  function test() {`,
      `    "use strict";`,
      `    assertEquals_(42, g(${cfg.g_args}));`,
      `  }`,
      `  ${"test();".repeat(cfg.test_warmup_count)}`,
      `  ${cfg.f_inlinable ? "%SetForceInlineFlag(f)" : "%OptimizeFunctionOnNextCall(f)"};`,
      `  ${cfg.g_inlinable ? "%SetForceInlineFlag(g)" : "%OptimizeFunctionOnNextCall(g)"};`,
      `  %OptimizeFunctionOnNextCall(test);`,
      `  test();`,
      `  assertEquals(${1 + cfg.test_warmup_count}, counter);`,
      `})();`,
      ``,
    ];
    var source = lines.join("\n");
    return source;
  }

  var f_args_variants = [/*"", "1",*/ "1, 2"];
  var g_args_variants = [/*"", "10",*/ "10, 20"];
  var f_inlinable_variants = [true, false];
  var g_inlinable_variants = [true, false];
  // This is to avoid bailing out because of referencing new.target.
  var check_new_target_variants = [/*true,*/ false];
  var deopt_mode_variants = ["none", "f", "g", "test"];
  var f_variants = [
      f_cfg_sloppy,
      f_cfg_strict,
      f_cfg_bound,
      f_cfg_proxy,
//      f_cfg_possibly_eval,
  ];
  var g_variants = [
      g_cfg_normal,
//      g_cfg_reflect_apply,
      g_cfg_function_apply,
//      g_cfg_function_apply_arguments_object,
      g_cfg_function_call,
  ];
  var test_warmup_counts = [0, 1, 2];

  var iter = 0;
  var tests_executed = 0;
  if (verbose && shard !== undefined) {
    print("Running shard #" + shard);
  }
  f_variants.forEach((f_cfg) => {
    check_new_target_variants.forEach((check_new_target) => {
      deopt_mode_variants.forEach((deopt_mode) => {
        g_variants.forEach((g_cfg) => {
          f_args_variants.forEach((f_args) => {
            g_args_variants.forEach((g_args) => {
              f_inlinable_variants.forEach((f_inlinable) => {
                g_inlinable_variants.forEach((g_inlinable) => {
                  test_warmup_counts.forEach((test_warmup_count) => {
                    if (shard !== undefined && (iter++) % SHARDS_COUNT != shard) {
                      if (verbose) {
                        print("skipping...");
                      }
                      return;
                    }
                    tests_executed++;
                    var cfg = {
                      f_source_template: f_cfg.source_template,
                      f_inlinable,
                      f_args,
                      f_name: f_cfg.func_name,
                      f_receiver: g_cfg.receiver,
                      g_source_template: g_cfg.source_template,
                      g_inlinable,
                      g_args,
                      test_warmup_count,
                      check_new_target,
                      deopt_mode,
                    };
                    var source = test_template(cfg);
                    if (verbose) {
                      // print("====================");
                      // print(source);
                    }
                    eval(source);
                  });
                });
              });
            });
          });
        });
      });
    });
  });
  if (verbose) {
    print("Number of tests executed: " + tests_executed);
  }
}

// Uncomment to run all the tests at once or use shard runners.
//run_tests();
