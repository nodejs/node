//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

////////////////////////
//Note RegExp.lastIndex does not exist any more, returning undefined is expected behaviour
function write(o) {
    try {
        document.write(o + "<br/>");
        write = function(o) { document.write(o + "<br/>"); };
    } catch (ex) {
        try {
            WScript.Echo("" + o);
            write = function(o) { WScript.Echo("" + o); };
        } catch (ex2) {
            print("" + o);
            write = function(o) { print("" + o); };
        }
    }
}


var re;
var arr;
var str;
write("******** replace tests");
str = "JavaScript is more fun than Java";
var strResult = str.replace(/\b\w+\b/g, function(word) {
    return word.substring(0, 1).toUpperCase() +
                                  word.substring(1);
});
write(strResult);


str = "Doe, John"
write (str.replace(/(\w+)\s*,\s*(\w+)/, "$2 $1"));

str = "$1,$2";
write(str.replace(/(\$(\d))/g, "$$1-$1$2"));

str = "Doe, John Abe, gold  C,B  alan, bart"
write ("original string:  " + str + "  replaced string:  " +str.replace(new RegExp( '(\\w+)\\s*,\\s*(\\w+)', "g"), "$2 $1"));

write ("original string:  " + str + " function replaced string:  " +str.replace(new RegExp( '(\\w+)\\s*,\\s*(\\w+)', "g"), function(word, c1, c2, offset, org) {
    write( "fn trace :  matched str: " + word + " capture1: " + c1 + " capture2: " + c2 + " offset: " + offset + " org str: " + org);
    return word.substring(0, 1).toUpperCase() + word.substring(1);
}));

str = "a   b";
write ("original string:  " + str + "  replaced string:  " +str.replace(/\s*(\s|$)\s*/g, "$1")); 

write("******** split tests");
str = "hello <b>world</b>";
re = /(<[^>]*>)/;
arr = str.split(re);  // Returns ["hello ","<b>","world","</b>",""]
WriteAllProps(re, arr);


re = /\s*,\s*/;
arr = "1, 2, 3, 4, 5".split(/\s*,\s*/);
WriteAllProps(re, arr);

re = /a*?/;
arr = "ab".split(re);
WriteAllProps(re, arr);

re = /a*/;
arr = "ab".split(re);
WriteAllProps(re, arr);

re = /<(\/)?([^<>]+)>/;
arr = "A<B>bold</B>and<CODE>coded</CODE>".split(re);
WriteAllProps(re, arr);

write("******** string.match, regexp.exec tests   ");  
re = new RegExp("d(b+)(d)", "ig");
str = "cdbBdbsbdbdz";
arr = re.exec(str);
WriteAllProps(re, arr);

arr = str.match(re);
WriteAllProps(re, arr);

re.lastIndex = 7;
arr = re.exec(str);
WriteAllProps(re, arr);

re = new RegExp("d(b+)(d)", "i");
arr = str.match(re);
WriteAllProps(re, arr);

s = "cdbBdbsbdbdz";
r = s.match();
write('result match empty: ' + r);

var pattern = /Java/g;
var text = "JavaScript is more fun than Java!";
var result;
while ((result = pattern.exec(text)) != null) {
    write("Matched '" + result[0] + "'" +
          " at position " + result.index +
          "; next search begins at " + pattern.lastIndex);
}

pattern = /dddd|ddd|dd|d|MMMM|MMM|MM|M|yyyy|yy|y|hh|h|HH|H|mm|m|ss|s|tt|t|fff|ff|f|zzz|zz|z/g;
result = pattern.exec("MM/dd/yyyy");
write("Result matching MMMM|MMM|MM|M with MM/dd/yyyy: " + result);

pattern =  /aa|a/;
result = pattern.exec("aa");
write("Result match aa|a with aa: " + result);

write("******** test empty regex expressions.");
var r0 = new RegExp("");
write(r0.test("foo"))
var r1 = new RegExp();
write(r1.test("foo"))
write(r1.test(""))

write("******** search tests");
var re1 = /[Jj]ava([Ss]cript)?(?=\:)/;
write(re1.test("JavaScript: The Definitive Guide"))

re = new RegExp("d(b+)(d)", "ig");
r = s.search(re);            //Search the string.
write('result search: ' + r);
write(RegExp.input);
write(RegExp.$_);

s = "The rain in Spain falls mainly in the plain.";
re = /falls/mi;            //Create regular expression pattern.
r = s.search(re);            //Search the string.
write('result search: ' + r);

var re1 = /[Jj]ava([Ss]cript)?(?=\:)/;
write(re1.test("JavaScript: The Definitive Guide"))
function SearchDemo(s, re) {
    var r;                   //Declare variables.
    r = s.search(re);            //Search the string.

    write('result search: ' + r);
    WriteAllProps(re, null);
    //delete RegExp.input;
    RegExp.input = "foooooooooo";
    WriteAllProps(re, null);
    return (r);                   //Return the Boolean result.
}

SearchDemo("The rain in Spain falls mainly in the plain.", /falls/mi);
SearchDemo("1@2@3@4@5@6@7@8@9@1@523", /(\d)@(\d)@(\d)@(\d)@(\d)@(\d)@(\d)@(\d)@(\d)@(\d)@(\d+)/g);

write("******** Match Demo indirect names");
function matchDemo(re, pat, v1, v2, v3) {
    var s;s
    var str = "cdbBdbsbdbdz";
    var arr = re.exec(str);
    s = "first dolar arg contains: " + pat[v1] + "\n";
    s += "second dolar arg contains: " + pat[v2] + "\n";
    s += "third dolar arg contains: " + pat[v3];
    return (s);
}

//    function matchDemo() {
//        var s;
//        var re = new RegExp("d(b+)(d)", "ig");
//        var str = "cdbBdbsbdbdz";
//        var arr = re.exec(str);
//        s = "$1 contains: " + RegExp.$1 + "\n";
//        s += "$2 contains: " + RegExp.$2 + "\n";
//        s += "$3 contains: " + RegExp.$3;
//        return (s);
//    }


var re = new RegExp("d(b+)(d)", "ig");
var sresult = matchDemo(re, RegExp, "$" + "1", "$" + "2", "$" + "3");
write("reg match: " + sresult);

write("******** Misc");
var x = /abc/;
write(x.toString());
write("javscriptSting".search(/[utg]/));
write("jaaaavscriptSaating".match(/a+/));
write("jaaaavscriptSaating".match(/a+?/g));

function WriteAllProps(reg, arr) {
    write('regObjInst.lastIndex: ' + reg.lastIndex);
    write('regObjInst.Source: ' + reg.source);
    write('regObjInst.global: ' + reg.global);
    write('regObjInst.ignoreCase: ' + reg.ignoreCase);
    write('regObjInst.multiline: ' + reg.multiline);
    write('regObjInst.options: ' + reg.options);

    write('RegExp.input: ' + RegExp.input);
    write('RegExp.input, $_: ' + RegExp.$_);
    write('RegExp.index: ' + RegExp.index);
    write('RegExp.lastIndex: ' + RegExp.lastIndex);

    write('RegExp.lastMatch: ' + RegExp.lastMatch);
    write('RegExp.lastMatch, $&: ' + RegExp['$&']);

    write('RegExp.lastParen: ' + RegExp.lastParen);
    write('RegExp.lastParen $+: ' + RegExp['$+']);

    write('RegExp.leftContext: ' + RegExp.leftContext);
    write('RegExp.leftContext $`: ' + RegExp['$`']);

    write('RegExp.rightContext: ' + RegExp.rightContext);
    write('RegExp.rightContext $\': ' + RegExp["$'"]);

    write('RegExp.$1: ' + RegExp.$1);
    write('RegExp.$2: ' + RegExp.$2);
    write('RegExp.$3: ' + RegExp.$3);
    write('RegExp.$4: ' + RegExp.$4);
    write('RegExp.$5: ' + RegExp.$5);
    write('RegExp.$6: ' + RegExp.$6);
    write('RegExp.$7: ' + RegExp.$7);
    write('RegExp.$8: ' + RegExp.$8);
    write('RegExp.$9: ' + RegExp.$9);
    write("");
    if (arr) {
        write('Array : ' + arr);
        write('Array input: ' + arr.input);
        write('Array index: ' + arr.index);
        write('Array lastIndex: ' + arr.lastIndex);
    }
    write("");
}