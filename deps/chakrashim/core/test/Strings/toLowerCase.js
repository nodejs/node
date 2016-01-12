//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//String.prototype.toLowerCase()
//TO DO  : Need to add Unicode and Upper Ascii Characters test cases and also Test cases that would throw exception(NaN and undefined Objects)

var id=0;
function verify(get_actual,get_expected,testid,testdesc)
{

    if(get_actual!=get_expected)
        WScript.Echo(testid+":"+testdesc+"\t"+"failed"+"\n"+"got"+get_actual+"\t for\t"+get_expected)
}

//test 1

verify("\tMICROSOFT".toLowerCase(), "\tmicrosoft", id++, "\"Testing Escape character tab\"")

//test 2
verify("\nMICROSOFT".toLowerCase(), "\nmicrosoft", id++, "\"Testing Escape character new line\"")

//test3

verify("\rMICROSOFT".toLowerCase(), "\rmicrosoft", id++, "\"Testing Escape character return \"")

//test 4
verify("\'MICROSOFT\'".toLowerCase(), "\'microsoft\'", id++, "\"Testing Escape character single quote\"")

//test 5
verify("MICROO\bSOFT".toLowerCase(), "microo\bsoft", id++, "\"Testing Escape character backspace\"")

//test 6

verify("\"MICROSOFT\"".toLowerCase(), "\"microsoft\"", id++, "\"Testing Escape character double quote\"")

//test 7

verify("microsoft".toLowerCase(), "microsoft", id++, "\"Testing passing lower case characters\"")

//test 8
verify("ABCDEFGHIJKLMNOPQRSTUVWXYZ".toLowerCase(), "abcdefghijklmnopqrstuvwxyz", id++, "\"Testing passing uppercase case characters\"")

//test 9
verify("(!@#$%^&*<,()+;:>?/)".toLowerCase(), "(!@#$%^&*<,()+;:>?/)", id++, "\" Testing passing Special Characters \"")

//test 10

verify("REDMOND@MICROSOFT.COM".toLowerCase(), "redmond@microsoft.com", id++, "\"Testing mix of characters eg email id\"");

//test 11

verify("ONEMICROSOFTWAY,156THNE31STPL,WA98054".toLowerCase(), "onemicrosoftway,156thne31stpl,wa98054", id++, "\"Testing mix of characters eg address\"");

//test 12

verify("1-800-CALL-HSBC".toLowerCase(), "1-800-call-hsbc", id++, id++, "\"Testing mix of characters eg phone number\" ");

//test 13: Coercing Other Object types : Arrays

var arr=new Array(3);
arr[0]="JSCRIPT";
arr[1]=12345;
arr[2]="123@MiCrOSOFT.com";
Array.prototype.toLowerCase=String.prototype.toLowerCase;  //the prototype method of string  can now be called from the array object
verify(arr.toLowerCase(), "jscript,12345,123@microsoft.com", id++, "\"Testing Coercible Objects eg Array\" ");

//test 14 Coercing Other Object types : Number

var num=new Number();
num=12345
Number.prototype.toLowerCase=String.prototype.toLowerCase;
verify(num.toLowerCase(), "12345", id++, "\"Testing Coercible Objects eg Number\" ");

//test 15 Coercing Other Object types : Boolean

var mybool=new Boolean(false);
Boolean.prototype.toLowerCase=String.prototype.toLowerCase;
verify(mybool.toLowerCase(), "false", id++, "\"Testing Coercible Objects eg Boolean\" ");

//test 16 Coercing Other Object types : Object

var obj=new Object()
Object.prototype.toLowerCase=String.prototype.toLowerCase;
verify(obj.toLowerCase(), "[object object]", id++, "\"Testing Coercible Objects eg Object\" ");

//Need to test for null and undefined but have to know the error mesage

//test 17 Concatenated String

verify(("CONCATENATED"+"STRING").toLowerCase(), "concatenatedstring", id++, "\" Testing Concatenated String\"");

//test 18 Indirect Call through Function

var Foo=function(){}
Foo.prototype.test=function(){return "MYSTRING";}
var fun=new Foo()
verify(fun.test().toLowerCase(), "mystring", id++, "\"Testing indirect calling eg function\"")

//test 19 Indirect call through property

var myobj=new Object();
myobj.prop="STRING";
verify(myobj.prop.toLowerCase(), "string", id++, "\"Testing indirect calling eg property\"");

WScript.Echo("done");

//test 20 implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.toLowerCase = String.prototype.toLowerCase;
var f = obj.toLowerCase();
WScript.Echo (a);
