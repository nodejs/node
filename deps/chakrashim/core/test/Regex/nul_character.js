//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var output = "";

function write(str) {
    if (typeof (WScript) == "undefined") {
        output += str + "\n";
        document.getElementById("writer").innerText = output; // .replace("\0", '\\0');
    } else {
        WScript.Echo(str);
    }
}

write("--- 1 ---");
try { write(eval('/a\0b/').toString().length); } catch (e) { write(e); }              // 5               !
try { write(eval("/\n/")); } catch (e) { write(e); }                                  // !               !
try { write(eval("/\r/")); } catch (e) { write(e); }                                  // !               !
write("--- 2 ---");
try { write(eval("/\\\0/").toString().length); } catch (e) { write(e); }              // 4               4
try { write(eval("/\\\n/")); } catch (e) { write(e); }                                // !               !
try { write(eval("/\\\r/")); } catch (e) { write(e); }                                // !               !
try { write(eval("/\\\u2028/")); } catch (e) { write(e); }                            // !               /\ /
try { write(eval("/\\\u2029/")); } catch (e) { write(e); }                            // !               /\ /
try { write(eval("/\\"));        } catch (e) { write(e); }                            // !               !
try { write(eval("/\\\0/").toString().length);     } catch (e) { write(e); }          // 4               4
try { write(eval("/\\\0a/").toString().length); } catch (e) { write(e); }             // 5               5
write("--- 3 ---");
try { write(eval("/[\n]/")); } catch (e) { write(e); }                                // !               !
try { write(eval("/[\r]/")); } catch (e) { write(e); }                                // !               !
try { write(eval("/[\u2028]/")); } catch (e) { write(e); }                            // !               /[ ]/
try { write(eval("/[\u2029]/")); } catch (e) { write(e); }                            // !               /[ ]/
try { write(eval("/[\0]/").toString().length); } catch (e) { write(e); }              // 5               !
try { write(eval("/[").toString().length); } catch (e) { write(e); }                  // !               !
try { write(eval("/a)/")); } catch (e) { write(e); }                                  // !               !
write("--- 4 ---");
try { write(new RegExp('\u2028*').toString().length) } catch (e) { write(e); }        // 4               4
try { write(new RegExp('\u2029*').toString().length) } catch (e) { write(e); }        // 4               4
try { write(new RegExp('\r*').toString().length) } catch (e) { write(e); }            // 4               4
try { write(new RegExp('\n*').toString().length) } catch (e) { write(e); }            // 4               4
try { write(new RegExp('\0*').toString().length) } catch (e) { write(e); }            // 4               4
try { write(new RegExp('\0a*').toString().length) } catch (e) { write(e); }           // 5               5
write("--- 5 ---");
try { write(new RegExp('\\\0*').toString().length) } catch (e) { write(e); }          // 5               5
try { write(new RegExp('\\\r*').toString().length) } catch (e) { write(e); }          // 5               5
try { write(new RegExp('\\\n*').toString().length) } catch (e) { write(e); }          // 5               5
try { write(new RegExp('[\r]').toString().length) } catch (e) { write(e); }           // 5               5
try { write(new RegExp('[\n]').toString().length) } catch (e) { write(e); }           // 5               5
try { write(new RegExp('[\0]').toString().length) } catch (e) { write(e); }           // 5               5
