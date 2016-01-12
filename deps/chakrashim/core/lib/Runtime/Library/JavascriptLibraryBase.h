//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// JavascriptLibraryBase.h is used by static lib shared between Trident and Chakra. We need to keep
// the size consistent and try not to change its size. We need to have matching mshtml.dll
// if the size changed here.
#pragma once

namespace Js
{
    class EngineInterfaceObject;

    class JavascriptLibraryBase : public FinalizableObject
    {
        friend class JavascriptLibrary;
        friend class ScriptSite;
    public:
        JavascriptLibraryBase(GlobalObject* globalObject):
            globalObject(globalObject)
        {
        }
        Var GetPI() { return pi; }
        Var GetNaN() { return nan; }
        Var GetNegativeInfinite() { return negativeInfinite; }
        Var GetPositiveInfinite() { return positiveInfinite; }
        Var GetMaxValue() { return maxValue; }
        Var GetMinValue() { return minValue; }
        Var GetNegativeZero() { return negativeZero; }
        RecyclableObject* GetUndefined() { return undefinedValue; }
        RecyclableObject* GetNull() { return nullValue; }
        JavascriptBoolean* GetTrue() { return booleanTrue; }
        JavascriptBoolean* GetFalse() { return booleanFalse; }

        JavascriptSymbol* GetSymbolHasInstance() { return symbolHasInstance; }
        JavascriptSymbol* GetSymbolIsConcatSpreadable() { return symbolIsConcatSpreadable; }
        JavascriptSymbol* GetSymbolIterator() { return symbolIterator; }
        JavascriptSymbol* GetSymbolToPrimitive() { return symbolToPrimitive; }
        JavascriptSymbol* GetSymbolToStringTag() { return symbolToStringTag; }
        JavascriptSymbol* GetSymbolUnscopables() { return symbolUnscopables; }

        JavascriptFunction* GetObjectConstructor() { return objectConstructor; }
        JavascriptFunction* GetArrayConstructor() { return arrayConstructor; }
        JavascriptFunction* GetBooleanConstructor() { return booleanConstructor; }
        JavascriptFunction* GetDateConstructor() { return dateConstructor; }
        JavascriptFunction* GetFunctionConstructor() { return functionConstructor; }
        JavascriptFunction* GetNumberConstructor() { return numberConstructor; }
        JavascriptRegExpConstructor* GetRegExpConstructor() { return regexConstructor; }
        JavascriptFunction* GetStringConstructor() { return stringConstructor; }
        JavascriptFunction* GetArrayBufferConstructor() { return arrayBufferConstructor; }
        JavascriptFunction* GetPixelArrayConstructor() { return pixelArrayConstructor; }
        JavascriptFunction* GetTypedArrayConstructor() const { return typedArrayConstructor; }
        JavascriptFunction* GetInt8ArrayConstructor() { return Int8ArrayConstructor; }
        JavascriptFunction* GetUint8ArrayConstructor() { return Uint8ArrayConstructor; }
        JavascriptFunction* GetUint8ClampedArrayConstructor() { return Uint8ClampedArrayConstructor; }
        JavascriptFunction* GetInt16ArrayConstructor() { return Int16ArrayConstructor; }
        JavascriptFunction* GetUint16ArrayConstructor() { return Uint16ArrayConstructor; }
        JavascriptFunction* GetInt32ArrayConstructor() { return Int32ArrayConstructor; }
        JavascriptFunction* GetUint32ArrayConstructor() { return Uint32ArrayConstructor; }
        JavascriptFunction* GetFloat32ArrayConstructor() { return Float32ArrayConstructor; }
        JavascriptFunction* GetFloat64ArrayConstructor() { return Float64ArrayConstructor; }
        JavascriptFunction* GetMapConstructor() { return mapConstructor; }
        JavascriptFunction* GetSetConstructor() { return setConstructor; }
        JavascriptFunction* GetWeakMapConstructor() { return weakMapConstructor; }
        JavascriptFunction* GetWeakSetConstructor() { return weakSetConstructor; }
        JavascriptFunction* GetSymbolConstructor() { return symbolConstructor; }
        JavascriptFunction* GetProxyConstructor() const { return proxyConstructor; }
        JavascriptFunction* GetPromiseConstructor() const { return promiseConstructor; }
        JavascriptFunction* GetGeneratorFunctionConstructor() const { return generatorFunctionConstructor; }

        JavascriptFunction* GetErrorConstructor() const { return errorConstructor; }
        JavascriptFunction* GetEvalErrorConstructor() const { return evalErrorConstructor; }
        JavascriptFunction* GetRangeErrorConstructor() const { return rangeErrorConstructor; }
        JavascriptFunction* GetReferenceErrorConstructor() const { return referenceErrorConstructor; }
        JavascriptFunction* GetSyntaxErrorConstructor() const { return syntaxErrorConstructor; }
        JavascriptFunction* GetTypeErrorConstructor() const { return typeErrorConstructor; }
        JavascriptFunction* GetURIErrorConstructor() const { return uriErrorConstructor; }

        DynamicObject* GetMathObject() { return mathObject; }
        DynamicObject* GetJSONObject() { return JSONObject; }
#ifdef ENABLE_INTL_OBJECT
        DynamicObject* GetINTLObject() { return IntlObject; }
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
        EngineInterfaceObject* GetEngineInterfaceObject() { return engineInterfaceObject; }
#endif

        DynamicObject* GetArrayPrototype() { return arrayPrototype; }
        DynamicObject* GetBooleanPrototype() { return booleanPrototype; }
        DynamicObject* GetDatePrototype() { return datePrototype; }
        DynamicObject* GetFunctionPrototype() { return functionPrototype; }
        DynamicObject* GetNumberPrototype() { return numberPrototype; }
        ObjectPrototypeObject* GetObjectPrototypeObject() { return objectPrototype; }
        DynamicObject* GetObjectPrototype();
        DynamicObject* GetRegExpPrototype() { return regexPrototype; }
        DynamicObject* GetStringPrototype() { return stringPrototype; }
        DynamicObject* GetMapPrototype() { return mapPrototype; }
        DynamicObject* GetSetPrototype() { return setPrototype; }
        DynamicObject* GetWeakMapPrototype() { return weakMapPrototype; }
        DynamicObject* GetWeakSetPrototype() { return weakSetPrototype; }
        DynamicObject* GetSymbolPrototype() { return symbolPrototype; }
        DynamicObject* GetArrayIteratorPrototype() const { return arrayIteratorPrototype; }
        DynamicObject* GetMapIteratorPrototype() const { return mapIteratorPrototype; }
        DynamicObject* GetSetIteratorPrototype() const { return setIteratorPrototype; }
        DynamicObject* GetStringIteratorPrototype() const { return stringIteratorPrototype; }
        DynamicObject* GetPromisePrototype() const { return promisePrototype; }
        DynamicObject* GetJavascriptEnumeratorIteratorPrototype() const { return javascriptEnumeratorIteratorPrototype; }
        DynamicObject* GetGeneratorFunctionPrototype() const { return generatorFunctionPrototype; }
        DynamicObject* GetGeneratorPrototype() const { return generatorPrototype; }

        DynamicObject* GetErrorPrototype() const { return errorPrototype; }
        DynamicObject* GetEvalErrorPrototype() const { return evalErrorPrototype; }
        DynamicObject* GetRangeErrorPrototype() const { return rangeErrorPrototype; }
        DynamicObject* GetReferenceErrorPrototype() const { return referenceErrorPrototype; }
        DynamicObject* GetSyntaxErrorPrototype() const { return syntaxErrorPrototype; }
        DynamicObject* GetTypeErrorPrototype() const { return typeErrorPrototype; }
        DynamicObject* GetURIErrorPrototype() const { return uriErrorPrototype; }

    protected:
        GlobalObject* globalObject;
        RuntimeFunction* mapConstructor;
        RuntimeFunction* setConstructor;
        RuntimeFunction* weakMapConstructor;
        RuntimeFunction* weakSetConstructor;
        RuntimeFunction* arrayConstructor;
        RuntimeFunction* typedArrayConstructor;
        RuntimeFunction* Int8ArrayConstructor;
        RuntimeFunction* Uint8ArrayConstructor;
        RuntimeFunction* Uint8ClampedArrayConstructor;
        RuntimeFunction* Int16ArrayConstructor;
        RuntimeFunction* Uint16ArrayConstructor;
        RuntimeFunction* Int32ArrayConstructor;
        RuntimeFunction* Uint32ArrayConstructor;
        RuntimeFunction* Float32ArrayConstructor;
        RuntimeFunction* Float64ArrayConstructor;
        RuntimeFunction* arrayBufferConstructor;
        RuntimeFunction* dataViewConstructor;
        RuntimeFunction* booleanConstructor;
        RuntimeFunction* dateConstructor;
        RuntimeFunction* functionConstructor;
        RuntimeFunction* numberConstructor;
        RuntimeFunction* objectConstructor;
        RuntimeFunction* symbolConstructor;
        JavascriptRegExpConstructor* regexConstructor;
        RuntimeFunction* stringConstructor;
        RuntimeFunction* pixelArrayConstructor;

        RuntimeFunction* errorConstructor;
        RuntimeFunction* evalErrorConstructor;
        RuntimeFunction* rangeErrorConstructor;
        RuntimeFunction* referenceErrorConstructor;
        RuntimeFunction* syntaxErrorConstructor;
        RuntimeFunction* typeErrorConstructor;
        RuntimeFunction* uriErrorConstructor;
        RuntimeFunction* proxyConstructor;
        RuntimeFunction* promiseConstructor;
        RuntimeFunction* generatorFunctionConstructor;

        JavascriptFunction* defaultAccessorFunction;
        JavascriptFunction* stackTraceAccessorFunction;
        JavascriptFunction* throwTypeErrorAccessorFunction;
        JavascriptFunction* throwTypeErrorCallerAccessorFunction;
        JavascriptFunction* throwTypeErrorCalleeAccessorFunction;
        JavascriptFunction* throwTypeErrorArgumentsAccessorFunction;
        JavascriptFunction* debugObjectNonUserGetterFunction;
        JavascriptFunction* debugObjectNonUserSetterFunction;
        JavascriptFunction* debugObjectDebugModeGetterFunction;
        JavascriptFunction* __proto__getterFunction;
        JavascriptFunction* __proto__setterFunction;
        DynamicObject* mathObject;
        // SIMD_JS
        DynamicObject* simdObject;

        DynamicObject* debugObject;
        DynamicObject* JSONObject;
#ifdef ENABLE_INTL_OBJECT
        DynamicObject* IntlObject;
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
        EngineInterfaceObject* engineInterfaceObject;
#endif
        DynamicObject* reflectObject;

        DynamicObject* arrayPrototype;

        DynamicObject* typedArrayPrototype;
        DynamicObject* Int8ArrayPrototype;
        DynamicObject* Uint8ArrayPrototype;
        DynamicObject* Uint8ClampedArrayPrototype;
        DynamicObject* Int16ArrayPrototype;
        DynamicObject* Uint16ArrayPrototype;
        DynamicObject* Int32ArrayPrototype;
        DynamicObject* Uint32ArrayPrototype;
        DynamicObject* Float32ArrayPrototype;
        DynamicObject* Float64ArrayPrototype;
        DynamicObject* Int64ArrayPrototype;
        DynamicObject* Uint64ArrayPrototype;
        DynamicObject* BoolArrayPrototype;
        DynamicObject* CharArrayPrototype;
        DynamicObject* arrayBufferPrototype;
        DynamicObject* dataViewPrototype;
        DynamicObject* pixelArrayPrototype;
        DynamicObject* booleanPrototype;
        DynamicObject* datePrototype;
        DynamicObject* functionPrototype;
        DynamicObject* numberPrototype;
        ObjectPrototypeObject* objectPrototype;
        DynamicObject* regexPrototype;
        DynamicObject* stringPrototype;
        DynamicObject* mapPrototype;
        DynamicObject* setPrototype;
        DynamicObject* weakMapPrototype;
        DynamicObject* weakSetPrototype;
        DynamicObject* symbolPrototype;
        DynamicObject* iteratorPrototype;           // aka %IteratorPrototype%
        DynamicObject* arrayIteratorPrototype;
        DynamicObject* mapIteratorPrototype;
        DynamicObject* setIteratorPrototype;
        DynamicObject* stringIteratorPrototype;
        DynamicObject* promisePrototype;
        DynamicObject* javascriptEnumeratorIteratorPrototype;
        DynamicObject* generatorFunctionPrototype;  // aka %Generator%
        DynamicObject* generatorPrototype;          // aka %GeneratorPrototype%

        DynamicObject* errorPrototype;
        DynamicObject* evalErrorPrototype;
        DynamicObject* rangeErrorPrototype;
        DynamicObject* referenceErrorPrototype;
        DynamicObject* syntaxErrorPrototype;
        DynamicObject* typeErrorPrototype;
        DynamicObject* uriErrorPrototype;

        JavascriptBoolean* booleanTrue;
        JavascriptBoolean* booleanFalse;

        Var nan;
        Var negativeInfinite;
        Var positiveInfinite;
        Var pi;
        Var minValue;
        Var maxValue;
        Var negativeZero;
        RecyclableObject* undefinedValue;
        RecyclableObject* nullValue;

        JavascriptSymbol* symbolHasInstance;
        JavascriptSymbol* symbolIsConcatSpreadable;
        JavascriptSymbol* symbolIterator;
        JavascriptSymbol* symbolSpecies;
        JavascriptSymbol* symbolToPrimitive;
        JavascriptSymbol* symbolToStringTag;
        JavascriptSymbol* symbolUnscopables;

    public:
        ScriptContext* scriptContext;

    private:
        virtual void Dispose(bool isShutdown) override;
        virtual void Finalize(bool isShutdown) override;
        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

    };
}
