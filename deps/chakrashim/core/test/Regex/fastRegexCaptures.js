//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

writeLine(/.*(aaa.*aaa)/.exec("aaaaaa"));
writeLine(/.*a(.*aaa.*)a.*/.exec("aaaaa"));
writeLine(/.*(\.facebook\..*)/.exec("www.facebook.com"));
writeLine(/.*(aamber aamber aamber)/.exec("aamber aamber aamber."));
writeLine(/.*(this (is this is) this)/.exec("this is this is this"));
writeLine(/.*(this is top tier toy)/.exec("this is top tier toy"));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

function writeLine(str)
{
    WScript.Echo("" + str);
}
