//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.defineProperty(RegExp, "fakeProp", {
    value: 101,
    writable: true,
    enumerable: true,
    configurable: true
});

var propertyConstructorArray = ["fakeProp","$_","$*","$&","$+","$`","$'","input",
                                "lastMatch","lastParen","leftContext","rightContext",
                                "index","length","prototype","constructor"
                               ];

var propertyInstanceArray = ["global","ignoreCase","lastIndex","multiline","source","sticky"];

function RegexTests(i,propertyArray)
{
    WScript.Echo("starting Property[",i,"]: ",propertyArray[i]);
    //Does the property exist
    var doesPropExist = RegExp.hasOwnProperty(propertyArray[i]);
    WScript.Echo("Does Property exist: ",doesPropExist);

    if(!doesPropExist) return;
    //Is the property configurable
    var isPropConfig = Object.getOwnPropertyDescriptor(RegExp, propertyArray[i]).configurable;
    WScript.Echo("Is the Property configurable: ",isPropConfig);

    var canRedefine = false;
    var canDelete   = false;
    if(isPropConfig)
    {
        canRedefine = true;
        canDelete   = true;
    }

    var propValueBefore = RegExp[propertyArray[i]];
    try
    {
        Object.defineProperty(RegExp, propertyArray[i], { get : function () { return 'OVERRIDE' } });

    }
    catch(err)
    {
        if(isPropConfig) WScript.Echo("Fail");
        else WScript.Echo("PASS, Not Configurable and will not allow redefinition");
    }
    var deleteProp = false;
    if(isPropConfig)
    {
        if(RegExp[propertyArray[i]] =="OVERRIDE") WScript.Echo("PASS");
        else WScript.Echo("FAIL, currently equals: ",RegExp[propertyArray[i]]);
    }
    else
    {
        if(RegExp[propertyArray[i]] ==propValueBefore) WScript.Echo("PASS");
        else WScript.Echo("FAIL, currently equals: ",RegExp[propertyArray[i]]);
    }
    deleteProp = delete RegExp[propertyArray[i]];

    WScript.Echo("can you delete the property: ",canDelete, "did it actually delete?",deleteProp);
    if(deleteProp ==canDelete) WScript.Echo("Pass Delete Test");
    else WScript.Echo("Fail Delete Test");
}

function RegexInstanceTests(i,propertyArray)
{
    var pattern1=new RegExp("e");
    WScript.Echo("starting Property[",i,"]: ",propertyArray[i]);
    //Does the property exist
    var doesPropExist = pattern1.hasOwnProperty(propertyArray[i]);
    WScript.Echo("Does Property exist: ",doesPropExist);

    if(!doesPropExist) return;
    var isPropConfig = Object.getOwnPropertyDescriptor(pattern1, propertyArray[i]).configurable;
    WScript.Echo("Is the Property configurable: ",isPropConfig);

    var canRedefine = false;
    var canDelete   = false;
    if(isPropConfig)
    {
        canRedefine = true;
        canDelete   = true;
    }

    var propValueBefore = pattern1[propertyArray[i]];
    try
    {
        Object.defineProperty(pattern1, propertyArray[i], { get : function () { return 'OVERRIDE' } });
    }
    catch(err)
    {
        if(isPropConfig) WScript.Echo("Fail");
        else WScript.Echo("PASS, Not Configurable and will not allow redefinition");
    }

    var deleteProp = false;
    if(isPropConfig)
    {
        if(pattern1[propertyArray[i]] =="OVERRIDE") WScript.Echo("PASS");
        else WScript.Echo("FAIL, currently equals: ",pattern1[propertyArray[i]]);
    }
    else
    {
        if(pattern1[propertyArray[i]] ==propValueBefore) WScript.Echo("PASS");
        else WScript.Echo("FAIL, currently equals: ",pattern1[propertyArray[i]]);
    }

    deleteProp = delete pattern1[propertyArray[i]];

    WScript.Echo("can you delete the property: ",canDelete, "did it actually delete?",deleteProp);
    if(deleteProp ==canDelete) WScript.Echo("Pass Delete Test");
    else WScript.Echo("Fail Delete Test");

}

for(var i = 0; i < propertyConstructorArray.length;i++)
{
    RegexTests(i,propertyConstructorArray);
    WScript.Echo("\n");
}
for(var i = 0; i < propertyInstanceArray.length;i++)
{
    RegexTests(i,propertyInstanceArray);
    WScript.Echo("\n");
}
for(var i = 0; i < propertyInstanceArray.length;i++)
{
    RegexInstanceTests(i,propertyInstanceArray);
    WScript.Echo("\n");
}

