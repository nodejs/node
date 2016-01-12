//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function sameValue(x, y) {
  if (x == y)
    return x != 0 || y != 0 || (1/x == 1/y); // 0.0 != -0.0
 
  return x != x && y != y;  // NaN == NaN
}

function equal(v, ev) {
    var eps = 0.000001;
    
    if (sameValue(ev, v))
        return true;
    else if ((ev == 0.0 || v == 0.0) && Math.abs(v - ev) <= eps) // -0.0 covered here
        return true;
    else if (Math.abs(v - ev) / ev <= eps)
        return true;
    else
        return false;
}

function equalSimd(values, simdValue, type, msg)
{   var ok = true;
    
    if (type == SIMD.Float32x4 || type === SIMD.Int32x4)
        length = 4;
    
    if (Array.isArray(values))
    {
        for ( var i = 0; i < length; i ++)
        {
            if(!equal(values[i],type.extractLane(simdValue, i)))
            {
                ok = false;
            }
        }
        if (ok)
            return;
        else
        {
            
            WScript.Echo(">> Fail!");
            if (msg !== undefined)
            {
                WScript.Echo(msg);
                
            }
            printSimd(simdValue, type);
        }
    }
    else
    {
        type.check(values);
        
        for ( var i = 0; i < length; i ++)
        {
            if(!equal(type.extractLane(values, i),type.extractLane(simdValue, i)))
            {
                ok = false;
            }
        }
        if (ok)
            return;
        else
        {
            WScript.Echo(">> Fail!");
            if (msg !== undefined)
            {
                WScript.Echo(msg);
            }
            printSimd(simdValue, type);
        }
    }
}

function printSimd(simdValue, type)
{
    var length;
    var vals = "";
    
    if (type === SIMD.Float32x4 || type === SIMD.Int32x4)
    {
        length = 4;
    }
    for (var i = 0; i < length; i ++)
    {
        vals += type.extractLane(simdValue,i);
        if (i < 3)
            vals += ", "
    }
    WScript.Echo(type.toString() + "(" + vals + ")");
}
