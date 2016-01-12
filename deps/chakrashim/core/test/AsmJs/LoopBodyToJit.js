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
                    y = +(y * (+ bar(1,z))) 
                    n = (n + 1)|0;
                }while((n|0) < 100)
            }            
        }
        return +y;
    }
    
    function bar(k,d)
    {
        k = k|0;
        d = +d;
        return  + (d * d)      
    }
    
    return {bar:bar,f3:f3}
}

var obj = AsmModule();
print(obj.bar(1,10.5))  
print(obj.bar(1,10.5))   // jit bar
print(obj.f3(1,1.5))   
print(obj.f3(1,1.5))  