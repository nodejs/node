//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


// normal function
function test1() {
    print('*** test1');
    var o = function (a, b) {
        this.x = a;
        this.y = b;
    }
    Object.defineProperty(o, Symbol.hasInstance, {
        value: function () {
            print('checked Symbol.hasInstance');
            return true;
        }
    });
    print(({}) instanceof o);
}

// bounded function
function test2() {
    print('*** test2');
    var o = function (a, b) {
        this.x = a;
        this.y = b;
    }
    var bounded = o.bind(1, 2);
    Object.defineProperty(o, Symbol.hasInstance, {
        value: function () {
            print('checked Symbol.hasInstance');
            return true;
        }
    });

    var x = Object.create(bounded);
    print((x) instanceof o);
    print((bounded) instanceof o);
}

// proxy of function
function test3() {
    print('*** test3');
    function foo() { };
    Object.defineProperty(foo, Symbol.hasInstance, {
        value: function () {
            print('checked Symbol.hasInstance');
            return false;
        }
    });
    var proxy = new Proxy(foo, {
        get : function (target, property){
            print(property.toString());
            return Reflect.get(target, property);
        }
    });

    var x = new proxy();
    print((x) instanceof proxy);
}

// proxy of bounded function
function test4() {
    print('*** test4');
    function foo() { };
    Object.defineProperty(foo, Symbol.hasInstance, {
        value: function () {
            print('checked Symbol.hasInstance');
            return false;
        }
    });
    var proxy = new Proxy(foo, {
        get: function (target, property) {
            print(property.toString());
            return Reflect.get(target, property);
        }
    });

    var x = proxy.bind();
    print((x) instanceof proxy);
}

test1();
test2();
test3();
test4();