//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
Below test cases verifies the combination of sticky+multiline flag  when used.
*/
var testCount = 0;
function test(re, str, lastIndex, loopCount) 
{
    var formattedStr = str.replace('\n', '\\n');
    WScript.Echo('********** Test #' + ++testCount + " **********");
    
    re.lastIndex = lastIndex;
    for(var i = 0; i < loopCount; i++) {
        WScript.Echo(' ***   Iteration#' + (i+1))
        WScript.Echo(' var re=' + re);
        WScript.Echo(' var str=\'' + formattedStr + '\'');
        WScript.Echo(' re.lastIndex = '+re.lastIndex);
        WScript.Echo(' Result = ' + re.exec(str));
        WScript.Echo(' re.lastIndex = ' + re.lastIndex);
    }
}

 // no-^, /y
 test(/b12/y, "b12asd\nb12", 1, 4);
 test(/b12/y, "ab12asd\nb12", 1, 4);
 test(/b/y, "ab", 1, 4);
 test(/abc/y, "abcabcababc", 3, 4);

 // no-^, /my
 test(/b12/my, "ab12asd\nb12", 0, 4); 
 test(/b12/my, "ab12asd\nb12", 1, 4); 
 test(/b12/my, "b12asd\nb12", 1, 4);

 // ^, /y
 test(/^b12/y, "b12asd\nb12", 1, 4);
 test(/^b12/y, "ab12asd\nb12", 0, 4); 
 test(/^b12/y, "ab12asd\nb12", 1, 4); 
 test(/^b12/y, "b12b12", 3, 4); 
 test(/a|^b/gy, "baba", 0, 4); 

 // ^, /my
 test(/^b12/my, "b12asd\nb12", 0, 4); 
 test(/^b12/my, "b12asd\nb12", 1, 4); 
 test(/^b12/my, "b12asd\nb12", 7, 4); 
 test(/^b12/my, "asdsa123asd\nb12", 1, 4); 
 test(/^b12/my, "ab12asd\nb12", 1, 4); 
 test(/^b12/my, "ab12asd\nb12", 0, 4); 
 test(/^b/my, "a\nb", 2, 4);

WScript.Echo("abc\ndef\nghi\njkl\nmno\npqr\nstu\nvwx\nyz".match(/^.*\n/myg));

// BOILiteral2
 test(/^ba/my, "ba\nba", 0, 4);
 test(/^ba/my, "ba\nba", 1, 4);

// BoundedWordTag
 test(/\b\w+\b/y, "( ab )", 0, 4);
 test(/\b\w+\b/y, "( ab )", 2, 4);

// SingleCharTag
 test(/b/my, "ba\nb", 0, 4);
 test(/b/my, "ba\nb", 1, 4);
 test(/b/y, "ba\nb", 0, 4);
 test(/b/y, "ba\nb", 1, 4);
 test(/b/y, "a\nb", 0, 4);
 test(/b/my, "a\nb", 0, 4);

//LeadingTrailingSpacesTag (already taken care because of trailing ^)
 var re = /^\s*|\s*$/;
 test(/^\s*|\s*$/y, " ab", 1, 1);
