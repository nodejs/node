//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function doEval(str)
{
    //write(str);
    write(str + " : " + eval(str));
}


var overwrite = "hello";

write("Global object builtin properties");

var globProps = [ "NaN", "Infinity", "undefined"];

for(var i=0;i<globProps.length;i++)
{
   doEval("delete " +  globProps[i]);
   doEval(globProps[i]);
}

for(var i=0;i<globProps.length;i++)
{
   doEval(globProps[i] + "= \"hello\";");
   doEval(globProps[i]);
}

write("Math Object builtin properties");

var mathProps = ["PI", "E", "LN10", "LN2", "LOG2E", "LOG10E", "SQRT1_2", "SQRT2"];


for(var i=0;i<mathProps.length;i++)
{
   doEval("Math." + mathProps[i] + " = overwrite");
   doEval("Math." + mathProps[i]);
}

for(var i=0;i<mathProps.length;i++)
{
   doEval("delete Math." +  mathProps[i]);
   doEval("Math." + mathProps[i]);
}

write("Number Object builtin properties");

var numberProps = ["MAX_VALUE", "MIN_VALUE", "NaN", "NEGATIVE_INFINITY", "POSITIVE_INFINITY"];


for(var i=0;i<numberProps.length;i++)
{
   doEval("Number." + numberProps[i] + " = overwrite");
   doEval("Number." + numberProps[i]);
}

for(var i=0;i<mathProps.length;i++)
{
   doEval("delete Number." +  numberProps[i]);
   doEval("Number." + numberProps[i]);
}
