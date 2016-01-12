//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function ExplicitToString(value)
{
    var result;
    if (value instanceof Array)
    {
        result = "[";

        for (var idx = 0; idx < value.length; idx++)
        {
            if (idx > 0)
            {
                result += ", ";
            }

            var item = value[idx];
            result += ExplicitToString(item);
        }

        result += "]";
    }
    else if (value == null)
    {
        result = "'null'";
    }
    else if (value == undefined)
    {
        result = "'undefined'";
    }
    else
    {
        result = value /* .toString() */;
    }

    return result;
}


function Print(name, value)
{
    var result = name + " = " + ExplicitToString(value);
   
    WScript.Echo(result);
}

var a = [1, 2, 3];
Print("a", a);

var b = a.concat(4, 5, 6);
Print("b", b);

var c = [1, [2, 3]];
Print("c", c);

var d = a.concat(4, [5, [6, [7]]]);
Print("d", d);

var e = a.concat([4, 5], [6, 7], [8, [9, [10]]]);
Print("e", e);
