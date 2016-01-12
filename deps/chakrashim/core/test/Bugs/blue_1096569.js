//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var print = function(){return this};
var x = function(){return this};
var obj = {};
/*@cc_on @*/

a=10;
function test3(){

(function(){/*sStart*/;try{try{with({}) { try{throw StopIteration;}catch(e){} } }catch(e){}try{delete w.z;}catch(e){}}catch(e){};;/*sEnd*/})();
(function(){/*sStart*/;if(shouldBailout){undefined--}/*sEnd*/})();
(function(){/*sStart*/;"use strict"; /*tLoop*/for (let amspyz in [null, null, new Number(1)]) { if(!shouldBailout){function shapeyConstructor(jcmmhu){this.y = "udb6fudff4";if ( "" ) for (var ytqsfigbn in this) { }return this; }; shapeyConstructor(a);}; };;/*sEnd*/})();

};

//Profile
test3();
test3();

//Bailout
shouldBailout = true;
test3();

WScript.Echo("Success");
