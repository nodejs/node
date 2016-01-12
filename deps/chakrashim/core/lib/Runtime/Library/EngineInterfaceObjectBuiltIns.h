//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
GlobalBuiltInConstructor(Boolean)
GlobalBuiltInConstructor(Object)
GlobalBuiltInConstructor(Number)
GlobalBuiltInConstructor(RegExp)
GlobalBuiltInConstructor(String)
GlobalBuiltInConstructor(Date)
GlobalBuiltInConstructor(Error) /*This was added back in to allow assert errors*/

GlobalBuiltIn(Math,Abs)
GlobalBuiltIn(Math,Floor)
GlobalBuiltIn(Math,Max)
GlobalBuiltIn(Math,Pow)

GlobalBuiltIn(JavascriptObject, EntryDefineProperty)
GlobalBuiltIn(JavascriptObject, EntryGetPrototypeOf)
GlobalBuiltIn(JavascriptObject, EntryIsExtensible)
GlobalBuiltIn(JavascriptObject, EntryGetOwnPropertyNames)
GlobalBuiltIn(JavascriptObject, EntryHasOwnProperty)

GlobalBuiltIn(JavascriptArray, EntryForEach)
GlobalBuiltIn(JavascriptArray, EntryIndexOf)
GlobalBuiltIn(JavascriptArray, EntryPush)
GlobalBuiltIn(JavascriptArray, EntryJoin)

GlobalBuiltIn(JavascriptFunction, EntryBind)
GlobalBuiltIn(JavascriptDate, EntryGetDate)
GlobalBuiltIn(JavascriptDate, EntryNow)

GlobalBuiltIn(JavascriptString, EntryReplace)
GlobalBuiltIn(JavascriptString, EntryToLowerCase)
GlobalBuiltIn(JavascriptString, EntryToUpperCase)

GlobalBuiltIn(GlobalObject, EntryIsFinite)
GlobalBuiltIn(GlobalObject, EntryIsNaN)

BuiltInRaiseException(TypeError, NeedObject)
BuiltInRaiseException2(TypeError, ObjectIsAlreadyInitialized)
BuiltInRaiseException3(RangeError, OptionValueOutOfRange)
BuiltInRaiseException(RangeError, OptionValueOutOfRange)
BuiltInRaiseException1(TypeError, NeedObjectOrString)
BuiltInRaiseException1(RangeError, LocaleNotWellFormed)
BuiltInRaiseException1(TypeError, This_NullOrUndefined)
BuiltInRaiseException1(TypeError, NotAConstructor)
BuiltInRaiseException1(TypeError, ObjectIsNonExtensible)
BuiltInRaiseException2(TypeError, NeedObjectOfType)
BuiltInRaiseException1(RangeError, InvalidCurrencyCode)
BuiltInRaiseException(TypeError, MissingCurrencyCode)
BuiltInRaiseException(RangeError, InvalidDate)
