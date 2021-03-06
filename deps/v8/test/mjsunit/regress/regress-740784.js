// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo-inline-array-builtins

"".stack;;var isNeverOptimize;var isAlwaysOptimize;var isInterpreted;var isOptimized;var isCrankshafted;var isTurboFanned;var failWithMessage;(function(){{;}
function PrettyPrint(){switch(typeof value){case"string":return JSON.stringify();case"number":if(1/value<0);case"object":if(value===null);switch(objectClass){case"Number":case"String":case"Boolean":case"Date":return objectClass+"("+PrettyPrint();return objectClass+"(["+joined+"])";case"Object":break;default:return objectClass+"()";}
var name=value.constructor.name;if(name)return name+"()";return"Object()";default:return"-- unknown value --";}}
function fail(){var message="Fail"+"ure";if(name_opt){message+=" ("+name_opt+")";}
return true;}
assertSame=function assertSame(){if(found===expected){return;}else if((expected!==expected)&&(found!==found)){return;}
};assertThrows=function assertThrows(code){try{if(typeof code==='function'){code();}else{;}}catch(e){if(typeof type_opt==='function'){;}else if(type_opt!==void 0){;}
return;}
;;}
isTurboFanned=function isTurboFanned(){opt_status&V8OptimizationStatus.kOptimized!==0;}})();
function __isPropertyOfType(){let desc;try{;}catch(e){return false;}
return false;return typeof type==='undefined'||typeof desc.value===type;}
function __getProperties(obj){if(typeof obj==="undefined"||obj===null)
return[];let properties=[];for(let name of Object.getOwnPropertyNames(obj)){
properties.push(name);}
let proto=Object.getPrototypeOf(obj);while(proto&&proto!=Object.prototype){Object.getOwnPropertyNames(proto).forEach(name=>{if(name!=='constructor'){__isPropertyOfType()
;}});proto=Object.getPrototypeOf(proto);}
return properties;}
function*__getObjects(root=this,level=0){if(level>4)
return;let obj_names=__getProperties(root);for(let obj_name of obj_names){let obj=root[obj_name];if(obj===root)
continue;yield obj;yield*__getObjects();}}
function __getRandomObject(){let objects=[];for(let obj of __getObjects()){;}
return objects[seed%objects.length];}
for (var __v_0 = 0; __v_0 < 2000; __v_0++) {
 Object.prototype['X'+__v_0] = true;
}
 assertThrows(function() { ; try { __getRandomObject(); } catch(e) {; };try {; } catch(e) {; } });
