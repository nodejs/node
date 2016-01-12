//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var output = "";

function echo(o) {
    try {
        document.write(o + "<br/>");
    } catch (ex) {
        try {
            WScript.Echo("" + o);
        } catch (ex2) {
            print("" + o);
        }
    }
}

echo("--- 1 ---");
try { echo(eval('/a\0b/').toString().length); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\n/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\r/")); } catch (e) { echo("EXCEPTION"); }
echo("--- 2 ---");
try { echo(eval("/\\\0/").toString().length); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\\n/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\\r/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\\u2028/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\\u2029/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\"));        } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\\0/").toString().length);     } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/\\\0a/").toString().length); } catch (e) { echo("EXCEPTION"); }
echo("--- 3 ---");
try { echo(eval("/[\n]/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/[\r]/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/[\u2028]/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/[\u2029]/")); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/[\0]/").toString().length); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/[").toString().length); } catch (e) { echo("EXCEPTION"); }
try { echo(eval("/a)/")); } catch (e) { echo("EXCEPTION"); }
echo("--- 4 ---");
try { echo(eval('/\u2028*/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/\u2029*/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/\r*/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/\n*/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('\0*').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('\0a*').toString().length) } catch (e) { echo("EXCEPTION"); }
echo("--- 5 ---");
try { echo(eval('\\\0*').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/\\\r*/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/\\\n*/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/[\r]/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('/[\n]/').toString().length) } catch (e) { echo("EXCEPTION"); }
try { echo(eval('[\0]').toString().length) } catch (e) { echo("EXCEPTION"); }
