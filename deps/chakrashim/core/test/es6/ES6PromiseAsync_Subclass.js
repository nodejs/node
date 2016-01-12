//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Promise async tests -- verifies functionality of promise async operations with subclassable features

function echo(str) {
    WScript.Echo(str);
}

function getAsyncFulfilledPromise(t, v, fail) {
    fail = fail || false;
    var p = new Promise(
        function(resolve,reject) {
            if (fail) {
                WScript.SetTimeout(function() { reject(v) }, 0);
            } else {
                WScript.SetTimeout(function() { resolve(v) }, 0);
            }
        }
    );
    
    p.then(
        function(result) {
            echo(t + v + ' success: ' + result);
        },
        function(err) {
            echo(t + v + ' failure: ' + err);
        }
    );
    
    return p;
}

function getAsyncResolvePromise(t, v) {
    return getAsyncFulfilledPromise(t, v, false);
}

function getAsyncRejectPromise(t, v) {
    return getAsyncFulfilledPromise(t, v, true);
}

var tests = [
    {
        name: "Promise created via Promise[@@create] can be initialized with Promise constructor",
        body: function (index) {
            var promise = Promise[Symbol.create]();
            promise = Promise.call(
                promise, 
                function(resolve,reject) {
                    echo('Test #' + index + ' - Executor function called synchronously');
                    resolve('success');
                }
            );
            
            promise.then(
                function(result) {
                    echo('Test #' + index + ' - Success handler called with result = ' + result);
                },
                function(err) {
                    echo('Test #' + index + ' - Error handler called with err = ' + err);
                }
            );
        }
    },
    {
        name: "Subclass of Promise can be created and used",
        body: function (index) {
            class MyPromise extends Promise {
                constructor(executor) {
                    super(executor);
                    
                    this.val = 'some value';
                }
                
                value() { 
                    return this.val;
                }
            }

            var m = new MyPromise(
                function(res,rej) {
                    echo('Test #' + index + ' - Executor function called from subclass');
                    res('subclass of Promise');
                }
            );
            var s = m.then(
                function(v) {
                    echo('Test #' + index + ' - Success handler called with result = ' + v);
                },
                function(err) {
                    echo('Test #' + index + ' - Error handler called with err = ' + err);
                }
            );
            echo('Test #' + index + ' - Promise.prototype.then returns subclass object s. s.value() = ' + s.value());
        }
    },
    {
        name: "Subclass constructor has @@create method removed after Promise subclass object creation",
        body: function (index) {
            class MyPromise extends Promise {
                constructor(executor) {
                    super(executor);
                }
            }

            var p = new MyPromise(
                function(res, rej) {
                    res('something');
                }
            );

            Object.defineProperty(MyPromise, Symbol.create, { value: function() { echo('Test #' + index + ' - overwritten @@create called'); } });

            var s = p.then(
                function(result) {
                    echo('Test #' + index + ' - Success handler called with result = ' + result);
                }
            );
        }
    },
];

var index = 1;

function runTest(test) {
    echo('Executing test #' + index + ' - ' + test.name);
    
    try {
        test.body(index);
    } catch(e) {
        echo('Caught exception: ' + e);
    }
    
    index++;
}

tests.forEach(runTest);
    
echo('\r\nCompletion Results:');
