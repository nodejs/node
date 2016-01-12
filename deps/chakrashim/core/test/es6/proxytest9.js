//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test0() {
    print('test0 : Object.keys with symbols');
    var sym = Symbol();
    var o = {};
    o[sym] = "blah";

    var p = new Proxy(o, {});
    WScript.Echo(Object.keys(p).length);
}

function test1() {
    print('test1: Object.prototype.propertyIsEnumerable');
    var sym = Symbol();
    var o = {};
    Object.defineProperty(o, sym, { value: 5, enumerable: true });
    print(o.propertyIsEnumerable(sym));
}

function test2() {
    print('test2: Object.getOwnPropertyDescriptor');
    var desc = { value: new Proxy({}, {}), writable: true, enumerable: true, configurable: true};

    var traps =
    {
        getOwnPropertyDescriptor: function () { WScript.Echo("getown"); return desc; }
    };

    var p = new Proxy({}, traps);
    WScript.Echo(Object.getOwnPropertyDescriptor(p).value);

    traps.getOwnPropertyDescriptor = function () {
        WScript.Echo("proxy getown");
        desc.get = function () { return 5; };
        return new Proxy(desc, {  });
    }

    try {
        Object.getOwnPropertyDescriptor(p);
        print('Expected to throw TypeError');
    } catch (e) {
        if (e instanceof TypeError) {
            if (e.message !== 'Property cannot have both accessors and a value') {
                print('FAIL');
            }
        } else {
            print('FAIL');
        }
    }
}

function test3(){
    var traps = {
        has: function (target, prop) {
            print('has trap for prop :' + prop);
            return Reflect.has(target, prop);
        },

        getOwnPropertyDescriptor: function (target, prop) {
            print('getOwnPropertyDescriptor trap for prop: ' + prop);
            return new Proxy(desc, traps);
        }
    };

    var desc = { value: 1, writable: true, configurable : true };
    desc.a = 1;
    var p = new Proxy(desc, traps);
    Object.getOwnPropertyDescriptor(p,"a");
}

function test4() {
    var keys = ["a"];
    var traps =
    {
        ownKeys : function() { WScript.Echo("plain key trap!"); return keys; },
        getOwnPropertyDescriptor: function (target, prop) {
            WScript.Echo("getOwn");
            return { enumerable: true, configurable: true }
        }
    };
    var p = new Proxy({}, traps);
    WScript.Echo(Object.keys(p).length);
    traps.ownKeys = function (target, prop)
    {
        WScript.Echo("proxy key trap!");
        return new Proxy(keys, {});
    }
    WScript.Echo(Object.keys(p).length);
}

function test5() {
    var keys = ["a"];
    var traps =
    {
        ownKeys: function () { WScript.Echo("plain key trap!"); return keys; },
        getOwnPropertyDescriptor: function (target, prop) {
            WScript.Echo("getOwn :" + prop);
            return { enumerable: true, configurable: true }
        }
    };
    var p = new Proxy({}, traps);
    //WScript.Echo(Object.keys(p).length);
    traps.ownKeys = function (target, prop) {
        WScript.Echo("proxy key trap!");
        return { 0: "a",  2: "3", length : 2 }
    }
    WScript.Echo(Object.keys(p).length);
}

function test6() {
    var arr = [1, 2, 3];
    Math.max.apply(null, new Proxy(arr, {
        get: function (target, prop) {
            print('get trap : ' + prop);
            if (prop == 'length') {
                return target.length;
            }
        }
    }));
}

function test7() {
    var traps = {
        get: function (target, prop) {
            print('get trap :' + prop);
            return Reflect.get(target, prop);
        },
        ownKeys: function (target) {
            print('ownKeys trap : ');
            return Reflect.ownKeys(target);
        },
        getOwnPropertyDescriptor: function (target, prop) {
            print('getOwnPropertyDescriptor trap : ' + prop.toString());
            return Reflect.getOwnPropertyDescriptor(target, prop);
        }
    };

    var proto = { inherited: "blah" };
    var props = Object.create(proto);
    var sym1 = Symbol();

    Object.defineProperty(props, "a", { value: 5 });
    Object.defineProperty(props, "b", { value: 5 });
    Object.defineProperty(props, sym1, { value: 5 });

    var proxy_props = new Proxy(props, traps)
    var o1 = Object.create(proto, proxy_props);
    var o2 = Object.defineProperties({}, proxy_props);
}

function test8() {
    var test = function () { print('test') };
    var p = new Proxy(test, {
        has: function (target, prop) {
            print('has');
        },

        get: function (target, prop) {
            print('get : ' + prop);
            return Reflect.get(target, prop);
        }
    })
    p.bind({});
}

function test9() {
    var test = function () { print('test'); }
    var p = new Proxy(test, {
        apply: function (target) {
            print('apply');
        }
    });
    p.call();
}


// Function.bind with proxy
function test10() {
    function test() { print('test called'); }
    var p = new Proxy(test, {});
    var x = p.bind({}, 1, 2);
    var proxy_x = new Proxy(x, {});
    print(x.name);
    print(proxy_x.name);
    print(p.name);
    p();
    x();
    proxy_x();
}

function test11() {
    var trap = {
        get: function (target, property) {
            print('get trap: ' + property);
            return Reflect.get(target, property);
        },

        getPrototypeOf: function (target) {
            print('getPrototypeOf trap');
            return { a: "a" };
        },

        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor trap: ' + property);
            return Reflect.getOwnPropertyDescriptor(target, property);
        }
    }
    function test(a, b) {
    }

    var t = test.bind({}, 1);
    var p = new Proxy(test, trap);
    var x = p.bind({}, 1);
    var proxy_x = new Proxy(x, {});
    print(Object.getPrototypeOf(proxy_x).a === "a");
    print(Object.getPrototypeOf(x).a === "a");
}

function test12() {
    var o = {};
    Object.defineProperty(o, "A", { get: function () { return 5; }, set: function (val) { } });
    var p = new Proxy(o, {
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor trap :' + property);
            return Reflect.getOwnPropertyDescriptor(target, property);
        },
        get: function (target, property) {
            print('get trap :' + property);
            return Reflect.get(target, property);
        }
    })

    p.__lookupGetter__("A");
    p.__lookupSetter__("A");
}

function test13() {
    function Foo() { }

    Object.defineProperty(Foo, 'length', { value: 123, enumerable: true, configurable: false });
    print(Foo.length);

    var x = new Proxy(Foo, {
        ownKeys: function (target) {
            print("my proxy ownKeys");
            return Reflect.ownKeys(target);
        }
    });
    print(Object.keys(x));
}

function test14() {
    var x = function() {
        this.foo = "f1";
        this.bar = "f2";
    }

    var x1 = new Proxy(x, {
        construct: function (target, argumentList) {
            print('construct x');
            return Reflect.construct(target, argumentList);
        }
    });

    var p = new Proxy(x1, {
        construct: function (target, argumentList) {
            print('construct x1');
            return Reflect.construct(target, argumentList);
        }
    });
    var a = new p();
    print(a.foo + ":" + a.bar);
}


var handler = 
{
    get  : function(target, property) {
        print('get trap ' + property);
        var x = Reflect.get(target, property);
        if(property == 'constructor') 
        {
            x = new Proxy(x, handler);
        }
        return x;
    },
    construct: function(target, args) {
        print('constructor trap');
        return Reflect.construct(target, args);
    },
    apply : function(target, thisArg, argsList) {
        print('apply trap'  );
        return Reflect.apply(target, thisArg, argsList) 
    }
};
    
function test15() 
{
    var a = [1,2,3];
    var  p = new Proxy(Array, handler);
    p.of = Array.of;
    print(p.of(1,2));
}

function test16() 
{
    var a = [1,2,3];
    var p = new Proxy(Array, handler);
    p.from = Array.from;
    print(p.from([1,2]));
    
}

function test17() 
{
    function foo() { this.x = 1};
    // proxy of foo
    var pFoo = new Proxy(foo, handler);
    
    // proxy of proxy of foo
    var proxyOfpFoo = new Proxy(pFoo, handler);
    
    // bind
    var x = proxyOfpFoo.bind(1);
    
    // proxy of bound function
    var y = new Proxy(x, handler);
    
    print((new y()).x == 1);
}

function test18()
{
    var Obj = { a: 'foo', m: function () { } };
    var p = new Proxy(Obj, handler);
    p.m = Obj.m;
    // Here p.m should not be copy-proped from Obj.m
    p.m();
}

// Verify if targetFunction of bound function is a proxy to function
function test19() 
{
    function foo(a) {
       this.abc = a;
     };
    var _ = new Proxy(foo, {});
    var p = _.bind();
    var x = new p('def');
    print(x.abc);
}

// Verify if targetFunction passed to Reflect.construct is a proxy to function
function test20() 
{
    function foo(a) {this.abc = a;};
    var _ = new Proxy(foo, {});
    var p = _.bind();
    var x = Reflect.construct(p, ["ade"]);
    print(x.abc);
}

// Verify that constructor do return an object.
function test21() 
{
    function foo() {
        this.a = "b";
    }
    var x = new Proxy(foo, {});
    var y = new x();
    print(y.a);
}

// some basic test262 test cases
function test22(){
    
    //1. Proxy.length is configurable
    var x = Object.getOwnPropertyDescriptor(Proxy, 'length');
    print('value : ' + x.value);
    print('configurable : ' + x.configurable);
    print('writable : ' + x.writable);
    print('enumerable : ' + x.enumerable);
    
    //2. Revoked proxy passed as target/handler
    var revocable = Proxy.revocable({}, {});
    revocable.revoke();
    try{
        var x = new Proxy({}, revocable.proxy);
    } catch(e) {
        print('expected :' + e.message);
    }

    try{
        var x = new Proxy(revocable.proxy,{});
    } catch(e) {
        print('expected :' + e.message);
    }

    //3. Proxy doesn't have prototype
    print('Proxy.prototype = ' + Object.hasOwnProperty.call(Proxy, 'prototype'));

    //4. Reflect.defineProperty should not throw if target already has a property
    Reflect.defineProperty(Object.defineProperty({},"x", {value:1}), "x", {value : 2});
    
    print('done test22');
}

// Verify that Object.setPrototype takes null as newPrototype value
function test23() 
{
   var proxy = new Proxy(function() {}, {});
   Object.setPrototypeOf(proxy, null);
   print('test23 done.');
}

// Verifies ownPropertyNames, ownPropertySymbols
function test24()
{
    var o = {};
    var s1 = Symbol('b');
    var s2 = Symbol('c');
    Object.defineProperty(o, 'a', { value: 5, enumerable : true });
    Object.defineProperty(o, s1, { value: 5, enumerable: true });
    Object.defineProperty(o, s2, { value: 5, enumerable: false });
    var proxy = new Proxy(o, {});
    var propNames = Object.getOwnPropertyNames(proxy);
    var propSyms = Object.getOwnPropertySymbols(proxy);
    var propKeys = Reflect.ownKeys(proxy);
    
    print('*** ownPropertyNames');
    for (var p in propNames) {
        print(propNames[p].toString());
    }
    
    print('*** ownPropertySymbols');
    for (var p in propSyms) {
        print(propSyms[p].toString());
    }
    
    print('*** ownKeys');
    for (var p in propKeys) {
        print(propKeys[p].toString());
    }
}

function test25() {
    // CreateDynamicFunction -> GetPrototypeFromConstructor -> Get -> [[Get]]
    var get = [];
    var p = new Proxy(Function, { get: function(o, k) { get.push(k); return o[k]; }});
    new p;
    for (var x in get) {
        print(get[x].toString());
    }
    print(get.length);
    print(get);
}

function test26(){
    // SerializeJSONObject -> EnumerableOwnNames -> [[OwnPropertyKeys]]
    var ownKeysCalled = 0;
    var p = new Proxy({}, { ownKeys: function(o) { ownKeysCalled++; return Object.keys(o); }});
    JSON.stringify({ a: p, b: p });
    print(ownKeysCalled);
    print(ownKeysCalled === 2);
}

// has, deleteproperty, methodhelper
function test27() 
{   
    var handler = {
        get: function(target, property){
            print('getTrap, property : ' + property);       
            if(property == 'foo123'){
                return function() {print('foo called'); return 23;}
            }
            return Reflect.get(target, property);
        },
        has: function(target, property){
            print('hasTrap, property : ' + property);
            return Reflect.has(target, property);
        },
        deleteProperty: function (target, property) {
            print('deleteTrap, property : ' + property);
            return Reflect.deleteProperty(target, property);
        }

    };
    
    // try to have different properties for below test cases
        
        var x = 'foo123';
        var y = 'bar123';
        var o = {};
        var p = new Proxy(o, handler);
        Reflect.has(p, 'p1');
        'p2' in p;
        Reflect.deleteProperty(p, 'p3');
        typeof p[y];
        p[x]();
}

// Set property problem
function test28(){
     var o2 = { p: 43 };
    var receiver = { p: 44 };
    var result = Reflect.set(o2, 'p', 42, receiver);
    print(o2.p);
    print(receiver.p);
}

function test29() {
    // CreateDynamicFunction -> GetPrototypeFromConstructor -> Get -> [[Get]]
    var get = [];
    var p = new Proxy(Function, {});
    var funcInstance = new p('b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', "return 1;");
    if (funcInstance.length != 9)
    {
        print('FAIL');
    }  
}

test0();
test1();
test2();
test3();
test4(); 
test5();
test6();
test7();
test8();
test9();
test10();
test11();
test12();
test13();
test14();
test15();
test16();
test17();
test18();
test19();
test20();
test21();
test22();
test23();
test24();
test25();
test26();
test27();
test28();
test29();