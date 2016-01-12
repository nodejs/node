//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


write("\nJ*************************** JSON test parse simple literals ***************");

write(JSON.stringify(JSON.parse("{ \"memberNum\" : -0.1}")));



var rev1 = function(name, value) {
    write("+++in reviver")
    //    for (a in this) {
    //        write(a)
    //        write(this[a])
    //    }
    if (this[name] != value) {
        write("error");
    }
    write(name);
    if (value === undefined) {
        write("undefined");
    }
    else if (value === null) {
        write("null");
    }
    else {
        write(value);
    }
    

    write("+++out reviver");
    if (value == 3) {
        return undefined;
    }
    else if (value == true) {
        return 99;
    }
    return value;

}


write("");
write("");
var jsObjString = "{\"\" : 7, \"memberNullFirst\" : null, \"memberNum\" : 3, \"memberNegNum\" : -98765,\"memberStr\"  : \"StringJSON\", \"memberBool\" : true , \"memberObj\" : { \"mm\" : 1, \"mb\" : false}, \"memberX\" : {}, \"memberArray\" : [33, \"StringTst\",null,{}], \"memberNull\" : null}";
//var jsObjString = "{\"memberNullFirst\" : null}";
write("\nJ*************************** JSON test parse simple with no reviver ***************");
write("");
write("JSON Parse__  original= ");
write(jsObjString);
var jsObjStringParsed = JSON.parse(jsObjString);
var jsObjStringBack = JSON.stringify(jsObjStringParsed);
write("");
write(" __Parsed and stringify back= ");
write( jsObjStringBack);


write("");
write("");
write("\nJ*************************** JSON test parse simple with tracing reviver ***************");
write("");
write("JSON Parse__  original= ");
write(jsObjString);
jsObjStringParsed = JSON.parse(jsObjString, rev1);
jsObjStringBack = JSON.stringify(jsObjStringParsed);
write("");
write(" __Parsed with tracing reviver and stringify back = ");
write( jsObjStringBack);

write("");
write("");
write("\nJ*************************** JSON test parse simple with data restore reviver ***************");

jsObjString = "{\"\" : 7, \"memberNullFirst\" : null, \"dateMember\" : \"2008-05-30T07:00:59Z\", \"memberNum\" : 3, \"memberStr\"  : \"StringJSON\", \"memberBool\" : true , \"memberObj\" : { \"mm\" : 1, \"mb\" : false}, \"memberX\" : {}, \"memberArray\" : [33, \"StringTst\",null,{}], \"memberNull\" : null}";
write("");
jsObjStringParsed = JSON.parse(jsObjString, function(key, value) {
    var a;
    if (typeof value === 'string') {
        a = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2}(?:\.\d*)?)Z$/.exec(value);
        if (a) {
            return new Date(Date.UTC(+a[1], +a[2] - 1, +a[3], +a[4],
                            +a[5], +a[6]));
        }
    }
    return value;
}
);
jsObjStringBack = JSON.stringify(jsObjStringParsed);
write("");
write("JSON Parse__  original= ");
write(jsObjString);
write("");
write(" __Parsed and stringify back with Date rev = ");
write(jsObjStringBack);
write("VERIFICATION:  restored date year(if this doesn't throw we know the filter worked)= ");
write(jsObjStringParsed.dateMember.getUTCFullYear());


var rev2 = function(name, value) {
    if (this[name] != value) {
        write("error");
    }
    if (value == 3.14) {
        return undefined;
    }
    else if (value == true) {
        return 99;
    }
    return value;
}
write("");
write("");
write("\nJ*************************** JSON test parse simple with  reviver2: replace(delete) 3.14 by undefined, replace'true' by 99 ***************");
write("");
jsObjStringParsed = JSON.parse(jsObjString, rev2);
jsObjStringBack = JSON.stringify(jsObjStringParsed);
write("");
write("JSON Parse__  original= ");
write(jsObjString);
write("");
write(" __Parsed with reviver2 and stringify back= ");
write(jsObjStringBack);


var num = 12345;
var bfalse = false;
var jundef;

function f() { return 0; }

var arrSimple = new Array();
arrSimple[0] = "document.location";
arrSimple[1] = "foolish";

var simpleObj = { MemberNo1: '"data"' };
simpleObj.dateMember = new Date(2008, 3, 1);
simpleObj.functionMember = f;
simpleObj.nullMember = null;
simpleObj.undefinedMember = jundef;
simpleObj.stringmember = "this string ends the obj. You should not see functionMember and undefinedMember";

var simpleObjN = { MemberNo1: '"data"' };
simpleObjN.dateMember = new Date(2008, 3, 1);
simpleObjN.functionMember = f;
simpleObjN.nullMember = null;
simpleObjN.arr = arrSimple;
simpleObjN.undefinedMember = jundef;
simpleObjN.nested = { nestedM1: {}, nestedM2: 1234, nestedM3: { a: true, b: false} };
simpleObjN.a = { a: {}, nestedM2: 1234, b: { a: true, b: false, c : 3.14} };
simpleObjN.stringmember = "this string ends the obj. You should not see functionMember and undefinedMember";


var objNull = null;


var objArrMem = new Object();
objArrMem.intMember = 3;
objArrMem.strMember = "string_member_in_object";
var arr = new Array();
arr[0] = "document.location";
arr[1] = "foolish";
arr[2] = 12.3;
arr[3] = new Date(2008, 9, 9);
arr[4] = new Object();
arr[6] = objArrMem;
arr[7] = objNull;
arr[8] = jundef;
arr[9] = f;
arr[10] = new Array();
arr[10][0] = "NestedArray_1stParamString";
arr[10][1] = 3.14;
arr[10][2] = { a: "nested object in array", c: true };
arr[10][4] = new Array();
arr[11] = "[0]-document.location, [1]-string, [2]-number, [3]-date, [4]-empty obj, [5]-missing, [6]-obj, [7]-null, [8]-undef, [9]-function, [10]-nestedArray, [11]-this";


function repf(key, value) {
    if (key == "a" && (this.b != undefined)) {
        return this.b;
    }
    return value;
}


var objectArray = [
{ obj: "SampleTest", desc: "String simple" },
{ obj: "/test ze\0ro\vString\n_u4:\u0061_u2:\xbc_u1:\x0e_u2clean:\x8f", desc: "String complex" },
{ obj: 1,           desc: "Number(1)" },
{ obj: num,         desc: "Number(1234)" },
{ obj: 3.14,        desc: "Number(3.14)" },
{ obj: Number.NaN,  desc: "Number(NaN)" },
{ obj: Number.POSITIVE_INFINITY, desc: "Number(POSITIVE_INFINITY)" },
{ obj: true,        desc: "bool(true)" },
{ obj: bfalse,      desc: "bool(false)" },
{ obj: null,        desc: "null" },
{ obj: jundef,      desc: "undefined" },
{ obj: new Date(2008, 10, 10), desc: "Date(2008, 10, 10)" },
{ obj: new Object("hello"), desc: "string in Object" },
{ obj: new Number(33), desc: "number in Object" },
{ obj: new Object(true), desc: "bool in Object" },
{ obj: simpleObj,   desc: "SimpleObject" },
{ obj: simpleObjN,  desc: "Object with nested objects and array" },
{ obj: arrSimple,   desc: "Simple array" },
{ obj: arr,         desc: "Complex array" }


];

var replacerArray = [
{ obj: null, desc: "null" },
{ obj: [], desc: "array replacer: []" },
{ obj: ["a", "b"], desc: "array replacer: [\"a\",\"b\"]" },
{ obj: ["a", "b", "a", "a"], desc: "array replacer: [\"a\",\"b\",\"a\",\"a\"]" },
{ obj: repf, desc: "replacer function, if the key is 'a' and the holder has a prop 'b', replace the value of the prop 'a' with the value of prop b " }
]


var spaceArrary = [
{ obj: null, desc: "null" },
{ obj: 4, desc: "number 4" },
{ obj: 24, desc: "number 24" },
{ obj: "........................", desc: "string : ........................" }
]
var stringifiedObj;
var parsedObj;
var reStringified;

write("");
write("");
write("\n%%%%%%%%%%%%%%%%%         Matrix Testing  %%%%%%%%%%%%%%%%% ");
write("");
write("");


write("\nJ*************************** JSON test stringify - simple, no space, not replacer *********************** ");
for (var i = 0; i < objectArray.length; i++) {
    try {
        write("");
        write("------ JSON test stringify: " + objectArray[i].desc + "  ------");
        stringifiedObj = JSON.stringify(objectArray[i].obj);
        write(stringifiedObj);
        parsedObj = JSON.parse(stringifiedObj);
        reStringified = JSON.stringify(parsedObj);
        write("=== Parsed and restringified :")
        write(reStringified);
        
        parsedObj = JSON.parse(stringifiedObj, rev2);
        reStringified = JSON.stringify(parsedObj);
        write("=== Parsed with reviver and restringified :")
        write(reStringified);
    }
    catch (e) {
        write("!!Exception: " + e);
    }
}

for (var k = 0; k < replacerArray.length; k++) {
    for (var j = 0; j < spaceArrary.length; j++) {
        write("");
        write("");
        write("\n*************************** JSON test stringify:  replacer: " + replacerArray[k].desc + " space: " + spaceArrary[j].desc + " *********************** ");
        for (var i = 0; i < objectArray.length; i++) {
            try {
                write("");
                write("------ JSON test stringify: " + objectArray[i].desc + "  ------");
                stringifiedObj = JSON.stringify(objectArray[i].obj, replacerArray[k].obj, spaceArrary[j].obj);
                write(stringifiedObj);
                parsedObj = JSON.parse(stringifiedObj);
                reStringified = JSON.stringify(parsedObj);
                write("=== Parsed with no reviver and restringified :")
                
                write(reStringified);
                parsedObj = JSON.parse(stringifiedObj, rev2);
                reStringified = JSON.stringify(parsedObj);
                write("=== Parsed with reviver2 and restringified :")
                write(reStringified);
            }
            catch (e) {
                write("!!Exception: " + e);
            }
        }
    }
}


function write(a) {
    if (this.WScript == undefined) {
        document.write(a);
        document.write("\n");
    }
    else
        WScript.Echo(a)
}
