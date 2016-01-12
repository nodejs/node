//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// All loop inversions require two loops and variable assignments inside

// Eval cases
// one var assignment in the inner block
eval("\"use strict\"; function f1(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }");
eval("function f2(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }");
eval("\"use strict\"; function f1a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r;  }  } }");
eval("function f2a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r;  }  } }");

// actual use of the function with the inverted loop, before function declaration
eval("\"use strict\"; f3(); function f3(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }");
eval("f4(); function f4(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }");
eval("\"use strict\"; f3a(); function f3a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r;  }  } }");
eval("f4a(); function f4a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r;  }  } }");

// actual use of the function with the inverted loop, after function declaration
eval("\"use strict\"; function f5(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }; f5();");
eval("function f6(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }; f6();");
eval("\"use strict\"; function f5a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r;  }  } }; f5a();");
eval("function f6a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r;  }  } }; f6a();");

// two var assignments in the inner block
eval("\"use strict\"; function f7(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;  }  } }");
eval("function f8(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;  }  } }");
eval("\"use strict\"; function f7a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r; b = r;  }  } }");
eval("function f8a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r; b = r;  }  } }");

// actual use of the function with the inverted loop, before function declaration
eval("\"use strict\"; f9(); function f9(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;  }  } }");
eval("f10(); function f10(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;  }  } }");
eval("\"use strict\"; f9a(); function f9a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r; b = r;  }  } }");
eval("f10a(); function f10a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r; b = r;  }  } }");

// actual use of the function with the inverted loop, after function declaration
eval("\"use strict\"; function f11(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;  }  } }; f11();");
eval("function f12(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;  }  } }; f12();");
eval("\"use strict\"; function f11a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r; b = r;  }  } }; f11a();");
eval("function f12a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { a = r; b = r;  }  } }; f12a();");

//Non eval cases
function g1(){"use strict"; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }; g1();
function g2(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }; g2();
function g1a(){"use strict"; var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r;  }  } }; g1a();
function g2a(){var a; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) {  a = r;  }  } }; g2a();

function g3(){"use strict"; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;}  } }; g3();
function g4(){for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) { var a = r; var b = r;}  } }; g4();
function g3a(){"use strict"; var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) {  a = r; b = r; }  } }; g3a();
function g4a(){var a; var b; for (var h = 0; h < 1; ++h) { for (var r = 0; r < 1; ++r) {  a = r; b = r; }  } }; g4a();

function g5()
{
        var c = 3;
        function foo() {
                for (var j = 0; j < 1; ++j) {
                        for (var i = 0; i < 1; ++i) {
                                1;
                                c = 2;
                                1;
                        }
                }; 
        };
        foo();
        WScript.Echo(c);
};
g5();


WScript.Echo("pass");