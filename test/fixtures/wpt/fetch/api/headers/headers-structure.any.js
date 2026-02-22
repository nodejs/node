// META: title=Headers basic
// META: global=window,worker

"use strict";

var headers = new Headers();
var methods = ["append",
                "delete",
                "get",
                "has",
                "set",
                //Headers is iterable
                "entries",
                "keys",
                "values"
                ];
for (var idx in methods)
  test(function() {
    assert_true(methods[idx] in headers, "headers has " + methods[idx] + " method");
  }, "Headers has " + methods[idx] + " method");
