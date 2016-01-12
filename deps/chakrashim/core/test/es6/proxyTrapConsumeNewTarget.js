//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [{
    name: "Proxy construct trap consumes new.target",
    body: function() {

        let result = "";

        class A {
            constructor() {
                assert.areEqual(B, new.target, "The whole point of the test is to make sure new.target flow through proxy!");
                result += "A";
            }
        }

        var proxyObject = new Proxy(A, {
            construct: function(target, argumentsList, newTarget) {
                result += "proxyObject";
                assert.areEqual(A, target, "A is the target in this case");
                assert.areEqual(0, argumentsList.length, "No arguments are passed");
                assert.areEqual(B, newTarget, "B is also the new.target in this case");
                return Reflect.construct(target, argumentsList, newTarget);
            }
        });

        class B extends proxyObject {
            constructor() {
                result += "B";
                super();
            }
        }

        new B();
        assert.areEqual("BproxyObjectA", result, "Test indeed ran the code I expect it to");
    }
}, {
    name: "Proxy construct trap consumes overriden new.target",
    body: function() {

        let testCompleted = false;

        function MyNewTarget() {
            assert.isTrue(false, "We should not be creating instance of MyNewTarget");
        }

        function MyConstructor() {
            assert.areEqual(MyNewTarget, new.target, "myNewTarget is overridden in this case");
            testCompleted = true;
        }

        Reflect.construct(MyConstructor, [], MyNewTarget);

        assert.isTrue(testCompleted, "Test indeed ran the code I expect it to");
    }
}, {
    name: "Proxy construct trap spread case",
    body: function() {
        let result = "";

        function MyConstructor() {
            assert.areEqual(proxyObject, new.target, "myNewTarget is overridden in this case");
            result += "MyConstructor";
        }

        var proxyObject = new Proxy(MyConstructor, {
            construct: function(target, argumentsList, newTarget) {
                result += "proxyObject";
                assert.areEqual(4, argumentsList.length, "spreaded arguments count should be right");
                assert.areEqual(1, argumentsList[0], "spreaded arguments[0] should be right");
                assert.areEqual(2.25, argumentsList[1], "spreaded arguments[1] should be right");
                assert.areEqual(undefined, argumentsList[2], "spreaded arguments[2] should be right");
                assert.areEqual('hello', argumentsList[3], "spreaded arguments[3] should be right");
                return Reflect.construct(target, argumentsList, newTarget);
            }
        });

        var args = [1, 2.25, undefined, 'hello'];
        var newProxyObject = new proxyObject(...args);
        assert.areEqual("proxyObjectMyConstructor", result, "Test indeed ran the code I expect it to");
    }
}];

testRunner.runTests(tests, {
    verbose: WScript.Arguments[0] != "summary"
});
