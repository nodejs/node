//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo(msg) {
    WScript.Echo(msg);
}

function guarded_call(func) {
    try {
        func();
    } catch(e) {
        echo("Exception: " + e.name + " : " + e.message);
    }
}

function dump_args() {
    var args = "";
    for (var i = 0; i < arguments.length; i++) {
        if (i > 0) {
            args += ", ";
        }
        args += arguments[i];
    }
    echo("Called with this: " + typeof this + "[" + this + "], args: [" + args + "]");
}

// 1. If IsCallable(func) is false, throw TypeError
var noncallable = {
    apply: Function.prototype.apply
};
echo("--- f is not callable ---");
guarded_call(function() {
    noncallable.apply();
});
guarded_call(function() {
    noncallable.apply({}, [1, 2, 3]);
});

// 2. If argArray is null or undefined, call func with an emply list of arguments
var o = {};
echo("\n--- f.apply(x) ---");
guarded_call(function() {
    dump_args.apply(o);
});
echo("\n--- f.apply(x, null), f.apply(x, undefined) ---");
guarded_call(function() {
    dump_args.apply(o, null);
});
guarded_call(function() {
    dump_args.apply(o, undefined);
});

// 3. Type(argArray) is invalid
echo("\n--- f.apply(x, 123), f.apply(x, 'string'), f.apply(x, true) ---");
guarded_call(function() {
    dump_args.apply(o, 123);
});
guarded_call(function() {
    dump_args.apply(o, 'string');
});
guarded_call(function() {
    dump_args.apply(o, true);
});

// 5, 7 argArray.length is invalid
echo("\n--- f.apply(x, obj), obj.length is null/undefined/NaN/string/OutOfRange ---");
guarded_call(function() {
    dump_args.apply(o, {length: null});
});
guarded_call(function() {
    dump_args.apply(o, {length: undefined});
});
guarded_call(function() {
    dump_args.apply(o, {length: NaN});
});
guarded_call(function() {
    dump_args.apply(o, {length: 'string'});
});
guarded_call(function() {
    dump_args.apply(o, {length: 4294967295 + 1}); //UINT32_MAX + 1
});
guarded_call(function() {
    dump_args.apply(o, {length: -1});
});

echo("\n--- f.apply(x, arr), arr.length is huge ---");
var huge_array_length = [];
huge_array_length.length = 2147483647; //INT32_MAX
guarded_call(function() {
    dump_args.apply(o, huge_array_length);
});
echo("\n--- f.apply(x, obj), obj.length is huge ---");
guarded_call(function() {
    dump_args.apply(o, {length: 4294967295}); //UINT32_MAX
});

// Normal usage -- argArray tests
echo("\n--- f.apply(x, arr) ---");
dump_args.apply(o, []);
dump_args.apply(o, [1]);
dump_args.apply(o, [2, 3, NaN, null, undefined, false, "hello", o]);

echo("\n--- f.apply(x, arr) arr.length increased ---");
var arr = [1, 2];
arr.length = 5;
dump_args.apply(o, arr);

echo("\n--- f.apply(x, arguments) ---");
function apply_arguments() {
    dump_args.apply(o, arguments);
}
apply_arguments();
apply_arguments(1);
apply_arguments(2, 3, NaN, null, undefined, false, "hello", o);

echo("\n--- f.apply(x, obj) ---");
guarded_call(function() {
    dump_args.apply(o, {
        length: 0
    });
});
guarded_call(function() {
    dump_args.apply(o, {
        length: 1,
        "0": 1
    });
});
guarded_call(function() {
    dump_args.apply(o, {
        length: 8,
        "0": 2,
        "1": 3,
        "2": NaN,
        "3": null,
        "4": undefined,
        "5": false,
        "6": "hello",
        "7": o
    });
});

// Normal usage -- thisArg tests
function f1() {
    this.x1 = "hello";
}

echo("\n--- f.apply(), f.apply(null), f.apply(undefined), global x1 should be changed ---");
f1.apply();
echo("global x1 : " + x1);

x1 = 0;
f1.apply(null);
echo("global x1 : " + x1);

x1 = 0;
f1.apply(undefined);
echo("global x1 : " + x1);

echo("\n--- f.apply(x), global x1 should NOT be changed ---");
var o = {};
x1 = 0;
f1.apply(o);
echo("global x1 : " + x1);
echo("o.x1 : " + o.x1);

// apply on non-objects -- test thisArg
function apply_non_object(func, doEcho) {

    var echo_if = function(msg) {
        if (doEcho) {
            echo(msg);
        }
    };

    guarded_call(function() {
        echo_if(func.apply());
    });
    guarded_call(function() {
        echo_if(func.apply(null));
    });
    guarded_call(function() {
        echo_if(func.apply(undefined));
    });
    guarded_call(function() {
        echo_if(func.apply(123));
    });
    guarded_call(function() {
        echo_if(func.apply(true));
    });
    guarded_call(function() {
        echo_if(func.apply("string"));
    });
}

echo("\n--- f.apply(v), v is missing/null/undefined/123/true/'string' ---");
apply_non_object(dump_args);

//
// ES5: String.prototype.charCodeAt calls CheckObjectCoercible(thisArg). It should throw
// when thisArg is missing/null/undefined.
//
echo("\n--- f.apply(v), v is missing/null/undefined/123/true/'string', f: string.charCodeAt ---");
apply_non_object(String.prototype.charCodeAt, true);

echo("\n--- f.apply(v), v is missing/null/undefined/123/true/'string', f: string.charAt ---");
apply_non_object(String.prototype.charAt, true);

//
// Similarly, test thisArg behavior in Function.prototype.call
//
// call on non-objects -- test thisArg
function call_non_object(func, doEcho) {

    var echo_if = function(msg) {
        if (doEcho) {
            echo(msg);
        }
    };

    guarded_call(function() {
        echo_if(func.call());
    });
    guarded_call(function() {
        echo_if(func.call(null));
    });
    guarded_call(function() {
        echo_if(func.call(undefined));
    });
    guarded_call(function() {
        echo_if(func.call(123));
    });
    guarded_call(function() {
        echo_if(func.call(true));
    });
    guarded_call(function() {
        echo_if(func.call("string"));
    });
}

echo("\n--- f.call(v), v is missing/null/undefined/123/true/'string' ---");
call_non_object(dump_args);

echo("\n--- f.call(v), v is missing/null/undefined/123/true/'string', f: string.charCodeAt ---");
call_non_object(String.prototype.charCodeAt, true);

echo("\n--- f.call(v), v is missing/null/undefined/123/true/'string', f: string.charAt ---");
call_non_object(String.prototype.charAt, true);
