//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

  // return double 
  // do while and while loops
  function AsmModule() {
    "use asm";      
    var x1 = 10;
    function f3(x,y){
        x = x|0;
        y = +y;
        var m = 1000;
        var n = 10;
        var z = 1.1;

       a: while( (x|0) < 30)
        {
            x = (x+1)|0
            if( (x|0) > 10)
            {
                do
                {
                    if((n|0) > 50)
                        return +y;
                    x = (x+1)|0;
                    y = +(y * z) 
                    n = (n + 1)|0;
                }while((n|0) < 100)
            }            
        }
        return +y;
    }
    
    function bar(k)
    {
        k = k|0;
        if( (k|0) > 5) 
            return +f3(1,1.5);  
        else 
            return 1.5;
    }
    
    return bar
}

var bar = AsmModule();
print(bar(1))  
print(bar(1))   
print(bar(10))   
print(bar(10))   