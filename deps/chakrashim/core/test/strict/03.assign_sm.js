//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict";

function write(v) { WScript.Echo(v + ""); }

(function Test1() {
    var str = "Unresolvable reference";
    try {
        test1_value = 'test1 value...';
    } catch (e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();

(function Test1_eval() {
    var str = "Test1_eval: Unresolvable reference";
    try {
        eval("test1_eval_value = 10");
    } catch (e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();

(function Test1_1() {
    var str = "Test1_1: Globally resolvable reference";
    try {
        test1_1_value = 'value...'; // declared below
    } catch (e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();
var test1_1_value; // declared globally

(function Test1_2() {
    (function g() {
        var str = "Test1_2: Parent resolvable reference";
        try {
            test1_2_value = 'value...'; // declared below
        } catch (e) {
            write("Exception: "  + str);
            return;
        }
        write("Return: "  + str);
    })();
    var test1_2_value = 0; // declared in parent
})();

var glo = this;
(function Test1_3() {
    (function g() {
        var str = "Test1_3: Explicitly bound reference";
        try {
            glo.test1_3_value = 'value...';
        } catch (e) {
            write("Exception: "  + str);
            return;
        }
        write("Return: "  + str);
    })();
})();

(function Test1_3_eval() {
    (function g() {
        var str = "Test1_3_eval: Explicitly bound reference";
        try {
            eval("this.test1_3_eval_value = 10");
        } catch (e) {
            write("Exception: "  + str);
            return;
        }
        write("Return: "  + str);
    })();
})();

(function Test2(){
    var str = "Readonly property";
    var obj = new Object();
    Object.defineProperty(obj, "foo", {
        writable:false,
        value:20
    });

    try {
        obj.foo = 30;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_1(){
    var str = "Test2_1: Readonly property on global";
    Object.defineProperty(glo, "foo", {
        writable:false,
        value:20
    });

    try {
        foo = 30; // Implicitly assign to global
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_2(){
    var str = "Test2_2: Readonly property on global";
    try {
        glo.foo = 30; // Explicitly assign to global
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_3(){
    var str = "Test2_3: Readonly property on prototype";
    var proto = Object.create(Object.prototype, {
        "foo": {
            writable:false,
            value:20
        }
    });
    var obj = Object.create(proto);

    try {
        obj.foo = 30;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_3_int(){
    var str = "Test2_3_int: Readonly property on Number prototype";
    Object.defineProperty(Number.prototype, "foo", {
        writable:false,
        configurable:true,
        value:20
    });

    try {
        123["foo"] = 23;
    } catch(e) {
        write("Exception: "  + str);
        return;
    } finally {
        delete Number.prototype.foo;
    }

    write("Return: "  + str);
})();

(function Test2_4(){
    var str = "Test2_4: Readonly property with index property name";
    var prop = "7"; // Use a string
    var obj = Object.create(Object.prototype, {
        "7": {
            writable:false,
            value:20
        }
    });

    try {
        obj[prop] = 24;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_4_arr(){
    var str = "Test2_4_arr: Readonly property on array with index property name";
    var prop = "7"; // Use a string
    var obj = [0,1,2,3,4,5,6,7,8,9];
    Object.defineProperty(obj, prop, {
        writable:false,
        value:20
    });

    try {
        obj[prop] = 24;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_4_eval(){
    var str = "Test2_4_eval: Readonly property with index property name";
    var prop = "7"; // Use a string
    var obj = Object.create(Object.prototype, {
        "7": {
            writable:false,
            value:20
        }
    });

    try {
        eval("obj[prop] = 24");
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_5(){
    var str = "Test2_5: Readonly property with index property name";
    var prop = 3; // Use an integer
    var obj = Object.create(Object.prototype, {
        "3": {
            writable:false,
            value:20
        }
    });

    try {
        obj[prop] = 25;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_5_arr(){
    var str = "Test2_5_arr: Readonly property on array with index property name";
    var prop = 3; // Use an integer
    var obj = [];
    Object.defineProperty(obj, prop, {
        writable:false,
        value:20
    });

    try {
        obj[prop] = 25;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_5_eval(){
    var str = "Test2_5_eval: Readonly property with index property name";
    var prop = 3; // Use an integer
    var obj = Object.create(Object.prototype, {
        "3": {
            writable:false,
            value:20
        }
    });

    try {
        eval("obj[prop] = 25");
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_6(){
    var str = "Test2_6: Readonly property on arguments (empty)";
    Object.defineProperty(arguments, "1", {
        writable:false,
        value:20
    });

    try {
        arguments[1] = 26;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test2_7(a,b,c){
    var str = "Test2_7: Readonly property on arguments (with formals)";
    Object.defineProperty(arguments, "1", {
        writable:false,
        value:20
    });

    try {
        arguments[1] = 27;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})(270,271,272);

(function Test2_8(a,b,c){
    var str = "Test2_8: Undefined setter on arguments (with formals)";
    Object.defineProperty(arguments, "1", {
        get: function() { return "arguments[1] value"; } // Only getter specified
    });

    try {
        arguments[1] = 28;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})(280,281,282);

(function Test2_9() {
    var str = "Test2_9: Readonly property indexed by variable";
    var prop = "prop"; // Use a string
    var obj = {};
    Object.defineProperty(obj, prop, {
        writable: false,
        value: 20
    });

    try {
        obj[prop] = 25;
    } catch (e) {
        write("Exception: " + str);
        return;
    }

    write("Return: " + str);
})();

(function Test3(){
    var str = "Setter undefined";
    var obj = new Object();
    Object.defineProperty(obj, "foo", {
        set: undefined
    });

    try {
        obj.foo = 30;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_eval(){
    var str = "Test3_eval: Setter undefined";
    var obj = new Object();
    Object.defineProperty(obj, "foo", {
        set: undefined
    });

    try {
        eval("obj.foo = 30");
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_1(){
    var str = "Test3_1: Setter undefined";
    var obj = new Object();
    Object.defineProperty(obj, "foo", {
        get: function() { return "foo value"; } // Only getter specified
    });

    try {
        obj.foo = 30;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_2(){
    var str = "Test3_2: Setter undefined on prototype";
    var proto = Object.create(Object.prototype, {
        "foo": {
            get: function() { return "foo value"; } // Only getter specified
        }
    });
    var obj = Object.create(proto);

    try {
        obj.foo = 30;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_2_int(){
    var str = "Test3_2_int: Setter undefined on Number prototype";
    Object.defineProperty(Number.prototype, "foo", {
        get: function() { return "foo value"; }, // Only getter defined
        configurable:true
    });

    try {
        123["foo"] = 32;
    } catch(e) {
        write("Exception: "  + str);
        return;
    } finally {
        delete Number.prototype.foo;
    }

    write("Return: "  + str);
})();

(function Test3_3(){
    var str = "Test3_3: Setter undefined on index property name";
    var prop = "7"; // Use a string
    var obj = Object.create(Object.prototype, {
        "7": {
            get: function() { return "foo value"; } // Only getter specified
        }
    });

    try {
        obj[prop] = 33;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_3_arr(){
    var str = "Test3_3_arr: Setter undefined on array with index property name";
    var prop = "7"; // Use a string
    var obj = [0,1,2,3,4,5,6,7,8];
    Object.defineProperty(obj, prop, {
        get: function() { return "foo value"; } // Only getter specified
    });

    try {
        obj[prop] = 33;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_4(){
    var str = "Test3_4: Setter undefined on index property name";
    var prop = 7; // Use a integer
    var obj = Object.create(Object.prototype, {
        "7": {
            get: function() { return "foo value"; } // Only getter specified
        }
    });

    try {
        obj[prop] = 34;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_4_arr(){
    var str = "Test3_4_arr: Setter undefined on array with index property name";
    var prop = 3; // Use a integer
    var obj = [];
    Object.defineProperty(obj, prop, {
        get: function() { return "foo value"; } // Only getter specified
    });

    try {
        obj[prop] = 34;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }

    write("Return: "  + str);
})();

(function Test3_5() {
    var str = "Test3_5: Setter undefined and indexed by variable";
    var prop = "prop"; // Use a string
    var obj = {};
    Object.defineProperty(obj, prop, {
        get: function () { return "foo value"; } // Only getter specified
    });

    try {
        obj[prop] = 25;
    } catch (e) {
        write("Exception: " + str);
        return;
    }

    write("Return: " + str);
})();

(function Test4() {
    var str = "Adding non-existent property to non-extensible object";
    var obj = new Object();
    Object.preventExtensions(obj);

    try {
        obj.foo = 20;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();

(function Test4_1(){
    var str = "Test4_1: Adding non-existent index property to non-extensible object";
    var obj = new Object();
    Object.preventExtensions(obj);

    try {
        obj[3] = 20;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();

(function Test4_arr_1(){
    var str = "Test4_arr_1: Adding non-existent property to non-extensible array with index property name";
    var obj = [];
    var prop = "7"; // Use a string
    Object.preventExtensions(obj);

    try {
        obj[prop] = 4;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();

(function Test4_arr_2(){
    var str = "Test4_arr_2: Adding non-existent property to non-extensible array with index property name";
    var obj = [];
    var prop = 3; // Use an integer
    Object.preventExtensions(obj);

    try {
        obj[prop] = 4;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
})();

(function Test5(){
    var str = "Postfix increment to non-writable property";
    var obj = new Object();
    Object.defineProperty(obj, "foo", {
        writable:false,
        value:20
    });
    try {
        obj.foo++;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
} )();

(function Test6(){
    var str = "Postfix increment on non-extensible object's non-existent property";
    var obj = new Object();
    Object.preventExtensions(obj);

    try {
        obj.foo++;
    } catch(e) {
        write("Exception: "  + str);
        return;
    }
    write("Return: "  + str);
} )();

(function Test7(){
    var str = "Assign NaN of globalObject via property";
    var globalObject = Function("return this;")();
    try {
        globalObject.NaN = "blah";
    } catch(e) {
        write("Exception: " + str);
        return;
    }
    write("Return: "  + str);
} )();

(function Test8(){
    var str = "Assign Infinity of globalObject via indexer/literal";
    var globalObject = Function("return this;")();
    try {
        globalObject[Infinity] = "blah";
    } catch(e) {
        write("Exception: " + str);
        return;
    }
    write("Return: "  + str);
} )();

(function Test9(){
    var str = "Assign Infinity of globalObject via indexer/string";
    var globalObject = Function("return this;")();
    try {
        globalObject["Infinity"] = "blah";
    } catch(e) {
        write("Exception: " + str);
        return;
    }
    write("Return: "  + str);
} )();
