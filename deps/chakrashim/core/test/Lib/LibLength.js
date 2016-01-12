//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



WScript.Echo("Test length : eval:               " + eval.length);
WScript.Echo("Test length : parseInt:           " + parseInt.length);
WScript.Echo("Test length : parseFloat:         " +  parseFloat.length);
WScript.Echo("Test length : isNaN:              " +  isNaN.length);
WScript.Echo("Test length : isFinite:           " +  isFinite.length);
WScript.Echo("Test length : decodeURI:          " +  decodeURI.length);
WScript.Echo("Test length : decodeURIComponent: " + decodeURIComponent.length);
WScript.Echo("Test length : encodeURI:          " +  encodeURI.length);
WScript.Echo("Test length : encodeURIComponent: " + encodeURIComponent.length);
WScript.Echo("");

WScript.Echo("Test length : Object:                                 " + Object.length);
WScript.Echo("Test length : Object.prototype.constructor:           " + Object.prototype.constructor.length);
WScript.Echo("Test length : Object.prototype.hasOwnProperty:        " + Object.prototype.hasOwnProperty.length);
WScript.Echo("Test length : Object.prototype.propertyIsEnumerable:  " + Object.prototype.propertyIsEnumerable.length);
WScript.Echo("Test length : Object.prototype.isPrototypeOf:         " + Object.prototype.isPrototypeOf.length);
WScript.Echo("Test length : Object.prototype.toString:              " + Object.prototype.toString.length);
WScript.Echo("Test length : Object.prototype.toLocaleString:        " + Object.prototype.toLocaleString.length);
WScript.Echo("Test length : Object.prototype.valueOf:               " + Object.prototype.valueOf.length);
WScript.Echo("");

WScript.Echo("Test length : Array:                          " + Array.length);
WScript.Echo("Test length : Array.prototype.constructor:    " + Array.prototype.constructor.length);
WScript.Echo("Test length : Array.prototype.concat:         " + Array.prototype.concat.length);
//WScript.Echo("Test length : Array.prototype.every:        " + Array.prototype.every.length); 
//WScript.Echo("Test length : Array.prototype.filter:       " + Array.prototype.filter.length); 
//WScript.Echo("Test length : Array.prototype.forEach:      " + Array.prototype.forEach.length); 
//WScript.Echo("Test length : Array.prototype.indexOf:      " + Array.prototype.indexOf.length); 
WScript.Echo("Test length : Array.prototype.join:           " + Array.prototype.join.length); 
//WScript.Echo("Test length : Array.prototype.lastIndexOf:  " + Array.prototype.lastIndexOf.length); 
//WScript.Echo("Test length : Array.prototype.map:          " + Array.prototype.map.length); 
//WScript.Echo("Test length : Array.prototype.pop:          " + Array.prototype.pop.length); 
//WScript.Echo("Test length : Array.prototype.push:         " + Array.prototype.push.length); 
//WScript.Echo("Test length : Array.prototype.reduce:       " + Array.prototype.reduce.length);
//WScript.Echo("Test length : Array.prototype.reduceRight:  " + Array.prototype.reduceRight.length); 
WScript.Echo("Test length : Array.prototype.reverse:        " + Array.prototype.reverse.length); 
WScript.Echo("Test length : Array.prototype.shift:          " + Array.prototype.shift.length); 
WScript.Echo("Test length : Array.prototype.slice:          " + Array.prototype.slice.length); 
//WScript.Echo("Test length : Array.prototype.some:         " + Array.prototype.some.length); 
WScript.Echo("Test length : Array.prototype.sort:           " + Array.prototype.sort.length); 
WScript.Echo("Test length : Array.prototype.splice:         " + Array.prototype.splice.length);
WScript.Echo("Test length : Array.prototype.toLocaleString: " + Array.prototype.toLocaleString.length);
WScript.Echo("Test length : Array.prototype.toString:       " + Array.prototype.toString.length); 
WScript.Echo("Test length : Array.prototype.unshift:        " + Array.prototype.unshift.length);
WScript.Echo("");

WScript.Echo("Test length : Error :                                 " +  Error.length);
WScript.Echo("Test length : Error.prototype.constructor :           " + Error.prototype.constructor.length);
WScript.Echo("Test length : Error.prototype.toString :              " + Error.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : EvalError :                             " +  EvalError.length);
WScript.Echo("Test length : EvalError.prototype.constructor :       " + EvalError.prototype.constructor.length);
WScript.Echo("Test length : EvalError.prototype.toString :          " + EvalError.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : RangeError :                            " +  RangeError.length);
WScript.Echo("Test length : RangeError.prototype.constructor :      " + RangeError.prototype.constructor.length);
WScript.Echo("Test length : RangeError.prototype.toString :         " + RangeError.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : ReferenceError :                        " +  ReferenceError.length);
WScript.Echo("Test length : ReferenceError.prototype.constructor :  " + ReferenceError.prototype.constructor.length);
WScript.Echo("Test length : ReferenceError.prototype.toString :     " + ReferenceError.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : SyntaxError :                           " +  SyntaxError.length);
WScript.Echo("Test length : SyntaxError.prototype.constructor :     " + SyntaxError.prototype.constructor.length);
WScript.Echo("Test length : SyntaxError.prototype.toString :        " + SyntaxError.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : TypeError :                             " +  TypeError.length);
WScript.Echo("Test length : TypeError.prototype.constructor :       " + TypeError.prototype.constructor.length);
WScript.Echo("Test length : TypeError.prototype.toString :          " + TypeError.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : URIError :                              " + URIError.length);
WScript.Echo("Test length : URIError.prototype.constructor :        " + URIError.prototype.constructor.length);
WScript.Echo("Test length : URIError.prototype.toString :           " + URIError.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : Boolean :                               " + Boolean.length);                       
WScript.Echo("Test length : Boolean.prototype.constructor :         " + Boolean.prototype.constructor.length); 
//WScript.Echo("Test length : Boolean.prototype.toJSON :              " + Boolean.prototype.toJSON.length);      
WScript.Echo("Test length : Boolean.prototype.toString :            " + Boolean.prototype.toString.length);
WScript.Echo("Test length : Boolean.prototype.valueOf :             " + Boolean.prototype.valueOf.length);
WScript.Echo("");

WScript.Echo("Test length : Date :                              " +  Date.length);            
WScript.Echo("Test length : Date.prototype.constructor :        " +  Date.prototype.constructor.length);
WScript.Echo("Test length : Date.parse :                        " +  Date.parse.length);
//WScript.Echo("Test length : Date.now :                          " +  Date.now.length);
WScript.Echo("Test length : Date.UTC :                          " +  Date.UTC.length);    
WScript.Echo("Test length : Date.prototype.getDate :            " +  Date.prototype.getDate.length);            
WScript.Echo("Test length : Date.prototype.getDay :             " +  Date.prototype.getDay.length);             
WScript.Echo("Test length : Date.prototype.getFullYear :        " +  Date.prototype.getFullYear.length);        
WScript.Echo("Test length : Date.prototype.getHours :           " +  Date.prototype.getHours.length);           
WScript.Echo("Test length : Date.prototype.getMilliseconds :    " +  Date.prototype.getMilliseconds.length);    
WScript.Echo("Test length : Date.prototype.getMinutes :         " +  Date.prototype.getMinutes.length);         
WScript.Echo("Test length : Date.prototype.getMonth :           " +  Date.prototype.getMonth.length);           
WScript.Echo("Test length : Date.prototype.getSeconds :         " +  Date.prototype.getSeconds.length);         
WScript.Echo("Test length : Date.prototype.getTime :            " +  Date.prototype.getTime.length);            
WScript.Echo("Test length : Date.prototype.getTimezoneOffset :  " +  Date.prototype.getTimezoneOffset.length);  
WScript.Echo("Test length : Date.prototype.getUTCDate :         " +  Date.prototype.getUTCDate.length);         
WScript.Echo("Test length : Date.prototype.getUTCDay :          " +  Date.prototype.getUTCDay.length);          
WScript.Echo("Test length : Date.prototype.getUTCFullYear :     " +  Date.prototype.getUTCFullYear.length);     
WScript.Echo("Test length : Date.prototype.getUTCHours :        " +  Date.prototype.getUTCHours.length);        
WScript.Echo("Test length : Date.prototype.getUTCMilliseconds : " +  Date.prototype.getUTCMilliseconds.length); 
WScript.Echo("Test length : Date.prototype.getUTCMinutes :      " +  Date.prototype.getUTCMinutes.length);      
WScript.Echo("Test length : Date.prototype.getUTCMonth :        " +  Date.prototype.getUTCMonth.length);        
WScript.Echo("Test length : Date.prototype.getUTCSeconds :      " +  Date.prototype.getUTCSeconds.length);      
WScript.Echo("Test length : Date.prototype.getYear :            " +  Date.prototype.getYear.length);            
WScript.Echo("Test length : Date.prototype.setDate :            " +  Date.prototype.setDate.length);            
WScript.Echo("Test length : Date.prototype.setFullYear :        " +  Date.prototype.setFullYear.length);        
WScript.Echo("Test length : Date.prototype.setHours :           " +  Date.prototype.setHours.length);           
WScript.Echo("Test length : Date.prototype.setMilliseconds :    " +  Date.prototype.setMilliseconds.length);    
WScript.Echo("Test length : Date.prototype.setMinutes :         " +  Date.prototype.setMinutes.length);         
WScript.Echo("Test length : Date.prototype.setMonth :           " +  Date.prototype.setMonth.length);           
WScript.Echo("Test length : Date.prototype.setSeconds :         " +  Date.prototype.setSeconds.length);         
WScript.Echo("Test length : Date.prototype.setTime :            " +  Date.prototype.setTime.length);            
WScript.Echo("Test length : Date.prototype.setUTCDate :         " +  Date.prototype.setUTCDate.length);         
WScript.Echo("Test length : Date.prototype.setUTCFullYear :     " +  Date.prototype.setUTCFullYear.length);     
WScript.Echo("Test length : Date.prototype.setUTCHours :        " +  Date.prototype.setUTCHours.length);        
WScript.Echo("Test length : Date.prototype.setUTCMilliseconds : " +  Date.prototype.setUTCMilliseconds.length); 
WScript.Echo("Test length : Date.prototype.setUTCMinutes :      " +  Date.prototype.setUTCMinutes.length);      
WScript.Echo("Test length : Date.prototype.setUTCMonth :        " +  Date.prototype.setUTCMonth.length);        
WScript.Echo("Test length : Date.prototype.setUTCSeconds :      " +  Date.prototype.setUTCSeconds.length);      
WScript.Echo("Test length : Date.prototype.setYear :            " +  Date.prototype.setYear.length);            
WScript.Echo("Test length : Date.prototype.toDateString :       " +  Date.prototype.toDateString.length);       
//WScript.Echo("Test length : Date.prototype.toISOString :        " +  Date.prototype.toISOString.length);        
//WScript.Echo("Test length : Date.prototype.toJSON :             " +  Date.prototype.toJSON.length);             
WScript.Echo("Test length : Date.prototype.toLocaleDateString : " +  Date.prototype.toLocaleDateString.length); 
WScript.Echo("Test length : Date.prototype.toLocaleString :     " +  Date.prototype.toLocaleString.length);     
WScript.Echo("Test length : Date.prototype.toLocaleTimeString : " +  Date.prototype.toLocaleTimeString.length); 
WScript.Echo("Test length : Date.prototype.toString :           " +  Date.prototype.toString.length);           
WScript.Echo("Test length : Date.prototype.toTimeString :       " +  Date.prototype.toTimeString.length);       
WScript.Echo("Test length : Date.prototype.toUTCString :        " +  Date.prototype.toUTCString.length);
WScript.Echo("Test length : Date.prototype.valueOf :            " +  Date.prototype.valueOf.length);
WScript.Echo("");

WScript.Echo("Test length : Function :                          " +  Function.length);                      
WScript.Echo("Test length : Function.prototype.constructor :    " +  Function.prototype.constructor.length);
WScript.Echo("Test length : Function.apply :                    " +  Function.apply.length);                
//WScript.Echo("Test length : Function.bind :                     " +  Function.bind.length);                 
WScript.Echo("Test length : Function.call :                     " +  Function.call.length);
WScript.Echo("Test length : Function.toString :                 " + Function.toString.length);
WScript.Echo("");

WScript.Echo("Test length : Math.abs :    " +  Math.abs.length);   
WScript.Echo("Test length : Math.acos :   " +  Math.acos.length);  
WScript.Echo("Test length : Math.asin :   " +  Math.asin.length);  
WScript.Echo("Test length : Math.atan :   " +  Math.atan.length);  
WScript.Echo("Test length : Math.atan2 :  " +  Math.atan2.length); 
WScript.Echo("Test length : Math.ceil :   " +  Math.ceil.length);  
WScript.Echo("Test length : Math.cos :    " +  Math.cos.length);   
WScript.Echo("Test length : Math.exp :    " +  Math.exp.length);   
WScript.Echo("Test length : Math.floor :  " +  Math.floor.length); 
WScript.Echo("Test length : Math.log :    " +  Math.log.length);   
WScript.Echo("Test length : Math.max :    " +  Math.max.length);   
WScript.Echo("Test length : Math.min :    " +  Math.min.length);   
WScript.Echo("Test length : Math.pow :    " +  Math.pow.length);   
WScript.Echo("Test length : Math.random : " +  Math.random.length);
WScript.Echo("Test length : Math.round :  " +  Math.round.length); 
WScript.Echo("Test length : Math.sin :    " +  Math.sin.length);   
WScript.Echo("Test length : Math.sqrt :   " +  Math.sqrt.length);  
WScript.Echo("Test length : Math.tan :    " +  Math.tan.length);
WScript.Echo("");

WScript.Echo("Test length : Number :                            " +  Number.length);                         
WScript.Echo("Test length : Number.prototype.constructor :      " +  Number.prototype.constructor.length);   
WScript.Echo("Test length : Number.prototype.toExponential :    " +  Number.prototype.toExponential.length); 
WScript.Echo("Test length : Number.prototype.toFixed :          " +  Number.prototype.toFixed.length);       
WScript.Echo("Test length : Number.prototype.toPrecision :      " +  Number.prototype.toPrecision.length);   
//WScript.Echo("Test length : Number.prototype.toJSON :           " +  Number.prototype.toJSON.length);        
WScript.Echo("Test length : Number.prototype.toLocaleString :   " +  Number.prototype.toLocaleString.length);
WScript.Echo("Test length : Number.prototype.toString :         " +  Number.prototype.toString.length);      
WScript.Echo("Test length : Number.prototype.valueOf :          " +  Number.prototype.valueOf.length);       
WScript.Echo("");

WScript.Echo("Test length : RegExp :                        " +  RegExp.length);
WScript.Echo("Test length : RegExp.prototype.constructor :  " +  RegExp.prototype.constructor.length);         
WScript.Echo("Test length : RegExp.prototype.exec :         " +  RegExp.prototype.exec.length);    
WScript.Echo("Test length : RegExp.prototype.test :         " +  RegExp.prototype.test.length);    
WScript.Echo("Test length : RegExp.prototype.toString :     " +  RegExp.prototype.toString.length);
WScript.Echo("");

WScript.Echo("Test length : String :                             " +  String.length);                            
WScript.Echo("Test length : String.prototype.constructor :       " +  String.prototype.constructor.length);      
WScript.Echo("Test length : String.fromCharCode :                " +  String.fromCharCode.length);               
WScript.Echo("Test length : String.prototype.charAt :            " +  String.prototype.charAt.length);           
WScript.Echo("Test length : String.prototype.charCodeAt :        " +  String.prototype.charCodeAt.length);       
WScript.Echo("Test length : String.prototype.concat :            " +  String.prototype.concat.length);           
WScript.Echo("Test length : String.prototype.indexOf :           " +  String.prototype.indexOf.length);          
WScript.Echo("Test length : String.prototype.lastIndexOf :       " +  String.prototype.lastIndexOf.length);      
WScript.Echo("Test length : String.prototype.localeCompare :     " +  String.prototype.localeCompare.length);    
WScript.Echo("Test length : String.prototype.match :             " +  String.prototype.match.length);            
WScript.Echo("Test length : String.prototype.replace :           " +  String.prototype.replace.length);          
WScript.Echo("Test length : String.prototype.search :            " +  String.prototype.search.length);           
WScript.Echo("Test length : String.prototype.slice :             " +  String.prototype.slice.length);            
WScript.Echo("Test length : String.prototype.split :             " +  String.prototype.split.length);            
WScript.Echo("Test length : String.prototype.substring :         " +  String.prototype.substring.length);        
WScript.Echo("Test length : String.prototype.substr :            " +  String.prototype.substr.length);           
//WScript.Echo("Test length : String.prototype.toJSON :            " +  String.prototype.toJSON.length);           
WScript.Echo("Test length : String.prototype.toLocaleLowerCase : " +  String.prototype.toLocaleLowerCase.length);
WScript.Echo("Test length : String.prototype.toLocaleUpperCase : " +  String.prototype.toLocaleUpperCase.length);
WScript.Echo("Test length : String.prototype.toLowerCase :       " +  String.prototype.toLowerCase.length);      
WScript.Echo("Test length : String.prototype.toString :          " +  String.prototype.toString.length);         
WScript.Echo("Test length : String.prototype.toUpperCase :       " +  String.prototype.toUpperCase.length);      
//WScript.Echo("Test length : String.prototype.trim :              " +  String.prototype.trim.length);             
WScript.Echo("Test length : String.prototype.valueOf :           " +  String.prototype.valueOf.length);
WScript.Echo("");

WScript.Echo("Test length : ActiveX :                             " + String.length);
WScript.Echo("Test length : ActiveX.prototype.constructor :       " + String.prototype.constructor.length);      



           
  
          
        
        
   