//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Async Await tests -- verifies functionality of async/await

function echo(str) {
    WScript.Echo(str);
}

var tests = [
    {
        name: "Async keyword with a lambda expressions",
        body: function (index) {
            var x = 12;
            var y = 14;
            var lambdaParenNoArg = async() => x < y;
            var lambdaArgs = async(a, b, c) => a + b + c;

            lambdaParenNoArg().then(result => {
                echo(`Test #${index} - Success lambda expression with no argument called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with no argument called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with no argument called with err = ${err}`);
            });

            lambdaArgs(10, 20, 30).then(result => {
                echo(`Test #${index} - Success lambda expression with several arguments called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with several arguments called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with several arguments called with err = ${err}`);
            });
        }
    },
    {
        name: "Async keyword with a lambda expressions and local variable captured and shadowed",
        body: function (index) {
            var x = 12;
            var lambdaSingleArgNoParen = async x => x;
            var lambdaSingleArg = async(x) => x;

            lambdaSingleArgNoParen(x).then(result => {
                echo(`Test #${index} - Success lambda expression with single argument and no paren called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with single argument and no paren called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with single argument and no paren called with err = ${err}`);
            });

            lambdaSingleArg(x).then(result => {
                echo(`Test #${index} - Success lambda expression with a single argument a called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with a single argument called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with a single argument called with err = ${err}`);
            });
        }
    },
    {
        name: "Async function in a statement",
        body: function (index) {
            {
                var namedNonAsyncMethod = function async(x, y) { return x + y; }
                var unnamedAsyncMethod = async function (x, y) { return x + y; }
                async function async(x, y) { return x - y; }

                var result = namedNonAsyncMethod(10, 20);
                echo(`Test #${index} - Success function #1 called with result = '${result}'`);

                unnamedAsyncMethod(10, 20).then(result => {
                    echo(`Test #${index} - Success function #2 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error function #2 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch function #2 called with err = ${err}`);
                });

                async(10, 20).then(result => {
                    echo(`Test #${index} - Success function #3 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error function #3 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch function #3 called with err = ${err}`);
                });
            }
            {
                async function async() { return 12; }

                async().then(result => {
                   echo(`Test #${index} - Success function #4 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error function #4 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch function #4 called with err = ${err}`);
                });
            }
            {
                function async() { return 12; }

                var result = namedNonAsyncMethod(10, 20);
                echo(`Test #${index} - Success function #5 called with result = '${result}'`);
            }
        }
    },
    {
        name: "Async function in an object",
        body: function (index) {
            var object = {
                async async() { return 12; }
            };

            var object2 = {
                async() { return 12; }
            };

            var object3 = {
                async "a"() { return 12; },
                async 0() { return 12; },
                async 3.14() { return 12; },
                async else() { return 12; },
            };

            object.async().then(result => {
                echo(`Test #${index} - Success function in a object #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #1 called with err = ${err}`);
            });

            var result = object2.async();
            echo(`Test #${index} - Success function in a object #2 called with result = '${result}'`);

            object3.a().then(result => {
                echo(`Test #${index} - Success function in a object #3 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #3 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #3 called with err = ${err}`);
            });

            object3['0']().then(result => {
                echo(`Test #${index} - Success function in a object #4 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #4 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #4 called with err = ${err}`);
            });

            object3['3.14']().then(result => {
                echo(`Test #${index} - Success function in a object #5 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #5 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #5 called with err = ${err}`);
            });

            object3.else().then(result => {
                echo(`Test #${index} - Success function in a object #6 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #6 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #6 called with err = ${err}`);
            });
        }
    },
    {
        name: "Async classes",
        body: function (index) {
            class MyClass {
                async asyncMethod(a) { return a; }
                async async(a) { return a; }
                async "a"() { return 12; }
                async 0() { return 12; }
                async 3.14() { return 12; }
                async else() { return 12; }
                static async staticAsyncMethod(a) { return a; }
            }

            class MySecondClass {
                async(a) { return a; }
            }

            class MyThirdClass {
                static async(a) { return a; }
            }

            var x = "foo";
            class MyFourthClass {
                async [x](a) { return a; }
            }

            var myClassInstance = new MyClass();
            var mySecondClassInstance = new MySecondClass();
            var myFourthClassInstance = new MyFourthClass();

            myClassInstance.asyncMethod(10).then(result => {
                echo(`Test #${index} - Success async in a class #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #1 called with err = ${err}`);
            });

            myClassInstance.async(10).then(result => {
                echo(`Test #${index} - Success async in a class #2 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #2 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #2 called with err = ${err}`);
            });

            myClassInstance.a().then(result => {
                echo(`Test #${index} - Success async in a class #3 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #3 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #3 called with err = ${err}`);
            });

            myClassInstance['0']().then(result => {
                echo(`Test #${index} - Success async in a class #4 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #4 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #4 called with err = ${err}`);
            });

            myClassInstance['3.14']().then(result => {
                echo(`Test #${index} - Success async in a class #5 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #5 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #5 called with err = ${err}`);
            });

            myClassInstance.else().then(result => {
                echo(`Test #${index} - Success async in a class #6 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #6 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #6 called with err = ${err}`);
            });

            MyClass.staticAsyncMethod(10).then(result => {
                echo(`Test #${index} - Success async in a class #7 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #7 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #7 called with err = ${err}`);
            });

            var result = mySecondClassInstance.async(10);
            echo(`Test #${index} - Success async in a class #8 called with result = '${result}'`);

            var result = MyThirdClass.async(10);
            echo(`Test #${index} - Success async in a class #9 called with result = '${result}'`);

            myFourthClassInstance.foo(10).then(result => {
                echo(`Test #${index} - Success async in a class #10 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #10 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #10 called with err = ${err}`);
            });
        }
    },
    {
        name: "Await in an async function",
        body: function (index) {
            async function asyncMethod(val, factor) {
                val = val * factor;
                if (val > 0)
                     val = await asyncMethod(val, -1);
                return val;
            }

            function await(x) {
                return x;
            }

            async function secondAsyncMethod(x) {
                return await(x);
            }

            function rejectedPromiseMethod() {
                return new Promise(function (resolve, reject) {
                    reject(Error('My Error'));
                });
            }

            async function rejectAwaitMethod() {
                return await rejectedPromiseMethod();
            }

            async function asyncThrowingMethod() {
                throw 32;
            }

            async function throwAwaitMethod() {
                return await asyncThrowingMethod();
            }

            asyncMethod(2, 2).then(result => {
                echo(`Test #${index} - Success await in an async function #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error await in an async function #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch await in an async function #1 called with err = ${err}`);
            });

            secondAsyncMethod(2).then(result => {
                echo(`Test #${index} - Success await in an async function #2 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error await in an async function #2 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch await in an async function #2 called with err = ${err}`);
            });

            rejectAwaitMethod(2).then(result => {
                echo(`Test #${index} - Failed await in an async function doesn't catch a rejected Promise. Result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Success await in an async function catch a rejected Promise in 'err'. Error = '${err}'`);
            }).catch(err => {
                echo(`Test #${index} - Success await in an async function catch a rejected Promise in 'catch'. Error = '${err}'`);
            });

            throwAwaitMethod(2).then(result => {
                echo(`Test #${index} - Failed await in an async function doesn't catch an error. Result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Success await in an async function catch an error in 'err'. Error = '${err}'`);
            }).catch(err => {
                echo(`Test #${index} - Success await in an async function catch an error in 'catch'. Error = '${err}'`);
            });
        }
    },
    {
        name: "Await keyword with a lambda expressions",
        body: function (index) {
            {
                async function asyncMethod(x, y, z) {
                    var lambdaExp = async(a, b, c) => a * b * c; 
                    var lambdaResult = await lambdaExp(x, y, z);
                    return lambdaResult;
                }

                asyncMethod(5, 5, 5).then(result => {
                    echo(`Test #${index} - Success await keyword with a lambda expressions #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error await keyword with a lambda expressions #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch await keyword with a lambda expressions #1 called with err = ${err}`);
                });
            };
            {
                async function await(lambda, x, y, z) {
                    return await lambda(x, y, z);
                }

                await(async(a, b, c) => a + b + c, 10, 20, 30).then(result => {
                    echo(`Test #${index} - Success await keyword with a lambda expressions #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error await keyword with a lambda expressions #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch await keyword with a lambda expressions #1 called with err = ${err}`);
                });
            };
        }
    },
    {
        name: "Async function with default arguments's value",
        body: function (index) {
            {
                function thrower() {
                    throw "expected error"
                }

                async function asyncMethod(argument = thrower()) {
                    return true;
                }

                async function secondAsyncMethod(argument = false) {
                    return true;
                }

                asyncMethod(true).then(result => {
                    echo(`Test #${index} - Success async function with default arguments's value overwritten #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error async function with default arguments's value overwritten #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch async function with default arguments's value overwritten #1 called with err = ${err}`);
                });

                asyncMethod().then(result => {
                    echo(`Test #${index} - Failed async function with default arguments's value has not been rejected as expected #2 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Success async function with default arguments's value has been rejected as expected by 'err' #2 called with err = '${err}'`);
                }).catch(err => {
                    echo(`Test #${index} - Success async function with default arguments's value has been rejected as expected by 'catch' #2 called with err = '${err}'`);
                });

                secondAsyncMethod().then(result => {
                    echo(`Test #${index} - Success async function with default arguments's value #3 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error async function with default arguments's value #3 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch async function with default arguments's value #3 called with err = ${err}`);
                });
            };
        }
    },
    {
        name: "Promise in an Async function",
        body: function (index) {
            {
                async function asyncMethodResolved() {
                    let p = new Promise(function (resolve, reject) {
                        resolve("resolved");
                    });

                    return p.then(function (result) {
                        return result;
                    });
                }

                async function asyncMethodResolvedWithAwait() {
                    let p = new Promise(function (resolve, reject) {
                        resolve("resolved");
                    });

                    return await p;
                }

                async function asyncMethodRejected() {
                    let p = new Promise(function (resolve, reject) {
                        reject("rejected");
                    });

                    return p.then(function (result) {
                        return result;
                    });
                }

                asyncMethodResolved().then(result => {
                    echo(`Test #${index} - Success resolved promise in an async function #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error resolved promise in an async function #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch resolved promise in an async function #1 called with err = ${err}`);
                });

                asyncMethodResolvedWithAwait().then(result => {
                    echo(`Test #${index} - Success resolved promise in an async function #2 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error resolved promise in an async function #2 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch resolved promise in an async function #2 called with err = ${err}`);
                });

                asyncMethodRejected().then(result => {
                    echo(`Test #${index} - Failed promise in an async function has not been rejected as expected #3 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Success promise in an async function has been rejected as expected by 'err' #3 called with err = '${err}'`);
                }).catch(err => {
                    echo(`Test #${index} - Success promise in an async function has been rejected as expected by 'catch' #3 called with err = '${err}'`);
                });
            };
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

echo('\nCompletion Results:');