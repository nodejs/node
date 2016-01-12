//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var c=0;
for (var i=0;i<10;i++) {
   c=c+i;
   if (c>12)
     break;
}
WScript.Echo(c);

c=0;
L1:for (var j=0;j<4;j++) {
    L5:for (var i=0;i<10;i++) {
        c = c + i;
        WScript.Echo(c);
        if ((c > 12) && (c < 20))
            break;
        else continue L5;
    }
   L2: for (var k=0;k<2;k++) {
       WScript.Echo(k);
   }
   L3: WScript.Echo("before continue");
   if (j<2)
       continue L1;
   L4:WScript.Echo(j);
}


a: if (false) b = null;
   else
   {
       break a;
   }
b: for (i = 0; i < 10; i++)    if (i == 5) break b;
d: for (i = 0; i < 10; i++)    if (i == 5) break ;

c: while (i != 5) if (i== 11) break c;
e: while (i != 5) if (i== 11) break ;

// Some fuzzer-style stuff to try and confuse the return value
WScript.Echo(undefined === eval('L: {if(c) { break L; } else {break L; } foo() }'));
WScript.Echo(undefined === eval('L: {try { break L; } catch(q) { foo(); break L; } foo() }'));
WScript.Echo(c === eval('L: {do { try { throw c; break L; } catch(q) { q; break L; } foo();} while(1); }'));
WScript.Echo(undefined === eval('L: {while(1) { try { throw c; break L; } catch(q) { break L; } foo();} }'));
