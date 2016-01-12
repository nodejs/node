//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function get_n_copies_of(ch, n)
{
    var powers = new Array();

    powers[0] = ch;
    for (var i = 1; (1<<i) < n; i++)
    {
        powers[i] = powers[i-1] + powers[i-1];
    }

    var out = '';

    for (var i = powers.length-1; i >= 0; i--)
    {
        if ((1 << i) > n)
            continue;

        out += powers[i];
        n -= (1 << i);
    }

    return out;
}

function exploit()
{
    // The choice of character is somewhat important -- we need
    // something that expands out to 3 bytes in UTF-8 encoding.
    // In this case, U+20AC satisfies that requirement.
    var s1 = "\u20ac";
    var ss;
 
    try
    {
        ss = get_n_copies_of(s1, 477218589);
    }
    catch (e)
    {
        WScript.Echo("You don't have enough free memory or VA to run this -- you'll need as much as possible.");
        return;
    }
    
    WScript.Echo("SS length = " + ss.length + "<br/>");

    // encodeURI sums (3 * [number of UTF-8 bytes required]) for each character
    // Since we use a char with 3 bytes required, that means the encodeURI memory
    // allocation is 3 * 3 * 477218589 = 0x100000005.
    // This truncates when fit into a ulong to just 5.
    WScript.Echo(encodeURI(ss).length);
}

try {
exploit();
}
catch (e)
{
   WScript.Echo("Message: " + e.message);
}
