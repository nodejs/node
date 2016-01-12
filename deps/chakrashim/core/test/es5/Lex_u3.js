//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


// auto ';' in return
var a = 1 
var b = 3
function foo()
{
if(true)
return /*afsdf 
sdafa */ a+b;
}

write(foo())
//full suport for >LS><PS>
eval("function foo(){if(true) return \/*afsdf \u2028 sdafa *\/ a+b;} write(foo());")

var x= "str const Left \u000A str const right"
write(x)
try
{
eval('x= \"str const Left \u2028 str const right\";write(escape(x))');

}
catch(e)
{
write("LS in string -  compile failure in ES5: expected." + e)
}

var re = /falls/

try
{
eval('x= \/str const regex \u2028 regex const right\/;write(escape(x))');

}
catch(e)
{
write("LS in regex literal -  compile failure in ES5: expected." + e)
}

try
{
eval('x= \"LS in escape sequence string literal \\\u2028 :more string\";write(escape(x))');
}
catch(e)
{
write("LS in escape sequence string literal -  compile failure in ES5: not expected." + e)
}

//BOM in strings

try
{
eval('x= 25 + \ufeff 66 ;write(\"BOM is WS :  \" + escape(x))');
}
catch(e)
{
write("BOM is WS in unicode 3 -  compile failure in ES5: not expected." + e)
}

//<ZVN><ZVNJ>

try
{
eval('var x = new Object(); x.aa\u200cbb = 6 ; for(m in x ) {write(\"ZVNJ is id part :  \" + escape(m));}');
}
catch(e)
{
write("ZVNJ is id part in unicode 3 -  compile failure in ES5: not expected." + e)
}


function write(a) {
    if (this.WScript == undefined) {
        document.write(a);
        document.write("</br>");
    }
    else
        WScript.Echo(a)
}
