//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

  // return double 
  // do while and while loops
  function AsmModule(stdlib) {
    "use asm";      
    var x1 = 10;
    var fr = stdlib.Math.fround;
    function f3(x,y){
        x = fr(x);
        y = fr(y);
        var m = 1;
        var n = 10;
        var z = 1.1;

       a: while( (m|0) < 30)
        {
            x = fr(x+y);
            do
            {
                if((n|0) > 50)
                    return fr(x);
                x = fr(x+y);                
                n = (n+1)|0
            }while((n|0) < 100)
        }
        return fr(x);
    }
    
    return f3
}
var stdlib = {Math:Math}
var f3 = AsmModule(stdlib);
print(f3  (1,1.5))  
print(f3  (1,1.5))   