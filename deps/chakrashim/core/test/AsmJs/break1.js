//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// nested for loop with break
function AsmModule() {
    "use asm";
    var x1 = 10;
    function f3(x,y){
        x = x|0;
        y = +y;
        var m = 1000;
        var n = 10;
        var z = 11;

        a: for(m = 0; (m|0) < 50 ; m = (m+1)|0)
        {
            x = (x+1)|0
            if((x|0) > 10)
            {
                for(n = 0; (n|0) < 100 ; n = (n+1)|0)
                {
                    if((n|0)< 50)
                        break a;
                    x = (x+1)|0;
                    z = (z+1)|0;
                }
            }
        }
        return (x + z)|0;
    }

    return f3
}

var f3 = AsmModule();
print(f3(1,1.5))
print(f3(1,1.5))
