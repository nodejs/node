//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Library\JSON.h"
#include "Types\MissingPropertyTypeHandler.h"
#include "Types\NullTypeHandler.h"
#include "Types\SimpleTypeHandler.h"
#include "Types\DeferredTypeHandler.h"
#include "Types\PathTypeHandler.h"
#include "Types\PropertyIndexRanges.h"
#include "Types\SimpleDictionaryPropertyDescriptor.h"
#include "Types\SimpleDictionaryTypeHandler.h"

#include "Types\DynamicObjectEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumeratorWPCache.h"
#include "Library\ForInObjectEnumerator.h"
#include "Library\NullEnumerator.h"
#include "Library\EngineInterfaceObject.h"
#include "Library\IntlEngineInterfaceExtensionObject.h"
#include "Library\ThrowErrorObject.h"
#include "Library\StackScriptFunction.h"

namespace Js
{
    SimplePropertyDescriptor const JavascriptLibrary::SharedFunctionPropertyDescriptors[2] =
    {
        SimplePropertyDescriptor(BuiltInPropertyRecords::prototype, PropertyWritable),
        SimplePropertyDescriptor(BuiltInPropertyRecords::name, PropertyConfigurable | PropertyWritable)
    };

    SimpleTypeHandler<1> JavascriptLibrary::SharedPrototypeTypeHandler(BuiltInPropertyRecords::constructor, PropertyWritable | PropertyConfigurable, PropertyTypesWritableDataOnly, 4, sizeof(DynamicObject));
    SimpleTypeHandler<1> JavascriptLibrary::SharedFunctionWithoutPrototypeTypeHandler(BuiltInPropertyRecords::name, PropertyConfigurable | PropertyWritable);
    SimpleTypeHandler<1> JavascriptLibrary::SharedFunctionWithPrototypeTypeHandlerV11(BuiltInPropertyRecords::prototype, PropertyWritable);
    SimpleTypeHandler<2> JavascriptLibrary::SharedFunctionWithPrototypeTypeHandler(SharedFunctionPropertyDescriptors);
    SimpleTypeHandler<1> JavascriptLibrary::SharedIdMappedFunctionWithPrototypeTypeHandler(BuiltInPropertyRecords::prototype);
    SimpleTypeHandler<1> JavascriptLibrary::SharedFunctionWithLengthTypeHandler(BuiltInPropertyRecords::length);
    MissingPropertyTypeHandler JavascriptLibrary::MissingPropertyHolderTypeHandler;


    SimplePropertyDescriptor const JavascriptLibrary::HeapArgumentsPropertyDescriptorsV11[2] =
    {
        SimplePropertyDescriptor(BuiltInPropertyRecords::length, PropertyConfigurable | PropertyWritable),
        SimplePropertyDescriptor(BuiltInPropertyRecords::callee, PropertyConfigurable | PropertyWritable)
    };

    SimplePropertyDescriptor const JavascriptLibrary::HeapArgumentsPropertyDescriptors[3] =
    {
        SimplePropertyDescriptor(BuiltInPropertyRecords::length, PropertyConfigurable | PropertyWritable),
        SimplePropertyDescriptor(BuiltInPropertyRecords::callee, PropertyConfigurable | PropertyWritable),
        SimplePropertyDescriptor(BuiltInPropertyRecords::_symbolIterator, PropertyConfigurable | PropertyWritable)
    };

    SimplePropertyDescriptor const JavascriptLibrary::FunctionWithLengthAndPrototypeTypeDescriptors[2] =
    {
        SimplePropertyDescriptor(BuiltInPropertyRecords::prototype, PropertyNone),
        SimplePropertyDescriptor(BuiltInPropertyRecords::length, PropertyConfigurable)
    };

    void JavascriptLibrary::Initialize(ScriptContext* scriptContext, GlobalObject * globalObject)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);
#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::LibInitPhase);
#endif
        this->scriptContext = scriptContext;
        this->recycler = scriptContext->GetRecycler();
        this->undeclBlockVarSentinel = RecyclerNew(recycler, UndeclaredBlockVariable, StaticType::New(scriptContext, TypeIds_Null, nullptr, nullptr));

        typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain = RecyclerNew(recycler, JsUtil::List<Type *>, recycler);

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // WARNING: Any objects created here using DeferredTypeHandler need to appear in EnsureLibraryReadyForHybridDebugging
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        // Library is not zero-initialized. memset the memory occupied by builtinFunctions array to 0.
        memset(builtinFunctions, 0, sizeof(JavascriptFunction *) * BuiltinFunction::Count);

        funcInfoToBuiltinIdMap = Anew(scriptContext->GeneralAllocator(), FuncInfoToBuiltinIdMap, scriptContext->GeneralAllocator());
#define LIBRARY_FUNCTION(target, name, argc, flags, entry) \
    funcInfoToBuiltinIdMap->AddNew(&entry, BuiltinFunction::##target##_##name);
#include "LibraryFunction.h"
#undef LIBRARY_FUNCTION



        // Note: InitializePrototypes and InitializeTypes must be called first.
        InitializePrototypes();
        InitializeTypes();
        InitializeGlobal(globalObject);
        InitializeComplexThings();
        InitializeStaticValues();

        // Do an early check of hybrid debugging. Note that script engine is not ready, so objects that run script in initializer
        // can't be un-deferred yet. However, we can possibly mark isHybridDebugging and avoid deferring new runtime objects.
        EnsureReadyIfHybridDebugging(/*isScriptEngineReady*/false);

#if ENABLE_COPYONACCESS_ARRAY
        if (!PHASE_OFF1(CopyOnAccessArrayPhase))
        {
            this->cacheForCopyOnAccessArraySegments = RecyclerNewZ(this->recycler, CacheForCopyOnAccessArraySegments);
        }
#endif
#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::LibInitPhase);
#endif
    }

    void JavascriptLibrary::Uninitialize()
    {
        this->globalObject = nullptr;
    }

    void JavascriptLibrary::InitializePrototypes()
    {
        Recycler* recycler = this->GetRecycler();

        nullValue = RecyclerNew(recycler, RecyclableObject, StaticType::New(scriptContext, TypeIds_Null, nullptr, nullptr));
        nullValue->GetType()->SetHasSpecialPrototype(true);

        ArrayBuffer* baseArrayBuffer;

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // WARNING: Any objects here using DeferredTypeHandler need to appear in EnsureLibraryReadyForHybridDebugging
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        // The prototype property of the object prototype is null.
        objectPrototype = ObjectPrototypeObject::New(recycler,
            DynamicType::New(scriptContext, TypeIds_Object, nullValue, nullptr,
            DeferredTypeHandler<InitializeObjectPrototype>::GetDefaultInstance()));

        constructorPrototypeObjectType = DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
            &SharedPrototypeTypeHandler, true, true);

        constructorPrototypeObjectType->SetHasNoEnumerableProperties(true);

        if (scriptContext->GetConfig()->IsTypedArrayEnabled())
        {
            if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled())
            {
                arrayBufferPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeArrayBufferPrototype>::GetDefaultInstance()));

                arrayBufferType = DynamicType::New(scriptContext, TypeIds_ArrayBuffer, arrayBufferPrototype, nullptr,
                    SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

                dataViewPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeDataViewPrototype>::GetDefaultInstance()));

                typedArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeTypedArrayPrototype>::GetDefaultInstance()));

                Int8ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt8ArrayPrototype>::GetDefaultInstance()));

                Uint8ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint8ArrayPrototype>::GetDefaultInstance()));

                // If ES6 TypedArrays are enabled, we have Khronos Interop mode enabled
                Uint8ClampedArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint8ClampedArrayPrototype>::GetDefaultInstance()));

                Int16ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt16ArrayPrototype>::GetDefaultInstance()));

                Uint16ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint16ArrayPrototype>::GetDefaultInstance()));

                Int32ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt32ArrayPrototype>::GetDefaultInstance()));

                Uint32ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint32ArrayPrototype>::GetDefaultInstance()));

                Float32ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeFloat32ArrayPrototype>::GetDefaultInstance()));

                Float64ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeFloat64ArrayPrototype>::GetDefaultInstance()));

                Int64ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt64ArrayPrototype>::GetDefaultInstance()));

                Uint64ArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint64ArrayPrototype>::GetDefaultInstance()));

                BoolArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeBoolArrayPrototype>::GetDefaultInstance()));

                CharArrayPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, typedArrayPrototype, nullptr,
                    DeferredTypeHandler<InitializeCharArrayPrototype>::GetDefaultInstance()));
            }
            else
            {
                arrayBufferPrototype = JavascriptArrayBuffer::Create(0,
                    DynamicType::New(scriptContext, TypeIds_ArrayBuffer, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeArrayBufferPrototype>::GetDefaultInstance()));

                arrayBufferType = DynamicType::New(scriptContext, TypeIds_ArrayBuffer, arrayBufferPrototype, nullptr,
                    SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Int8ArrayPrototype = RecyclerNew(recycler, Int8Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Int8Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt8ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Uint8ArrayPrototype = RecyclerNew(recycler, Uint8Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Uint8Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint8ArrayPrototype>::GetDefaultInstance()));

                dataViewPrototype = DynamicObject::New(recycler,
                    DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeDataViewPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Uint8ClampedArrayPrototype = RecyclerNew(recycler, Uint8ClampedArray, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Uint8ClampedArray, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint8ClampedArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Int16ArrayPrototype = RecyclerNew(recycler, Int16Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Int16Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt16ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Uint16ArrayPrototype = RecyclerNew(recycler, Uint16Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Uint16Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint16ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Int32ArrayPrototype = RecyclerNew(recycler, Int32Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Int32Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt32ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Uint32ArrayPrototype = RecyclerNew(recycler, Uint32Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Uint32Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint32ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Float32ArrayPrototype = RecyclerNew(recycler, Float32Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Float32Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeFloat32ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Float64ArrayPrototype = RecyclerNew(recycler, Float64Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Float64Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeFloat64ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Int64ArrayPrototype = RecyclerNew(recycler, Int64Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Int64Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeInt64ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                Uint64ArrayPrototype = RecyclerNew(recycler, Uint64Array, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_Uint64Array, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeUint64ArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                BoolArrayPrototype = RecyclerNew(recycler, BoolArray, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_BoolArray, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeBoolArrayPrototype>::GetDefaultInstance()));

                baseArrayBuffer = JavascriptArrayBuffer::Create(0, arrayBufferType);
                CharArrayPrototype = RecyclerNew(recycler, CharArray, baseArrayBuffer, 0, 0,
                    DynamicType::New(scriptContext, TypeIds_CharArray, objectPrototype, nullptr,
                    DeferredTypeHandler<InitializeCharArrayPrototype>::GetDefaultInstance()));
            }
        }
        else
        {
            arrayBufferType = nullptr;

            arrayBufferPrototype = nullptr;
            dataViewPrototype = nullptr;
            Int8ArrayPrototype = nullptr;
            Uint8ArrayPrototype = nullptr;
            Uint8ClampedArrayPrototype = nullptr;
            Int16ArrayPrototype = nullptr;
            Uint16ArrayPrototype = nullptr;
            Int32ArrayPrototype = nullptr;
            Uint32ArrayPrototype = nullptr;
            Float32ArrayPrototype = nullptr;
            Float64ArrayPrototype = nullptr;
            Int64ArrayPrototype = nullptr;
            Uint64ArrayPrototype = nullptr;
            BoolArrayPrototype = nullptr;
            CharArrayPrototype = nullptr;
        }

        arrayPrototype = JavascriptArray::New<Var, JavascriptArray, 0>(0,
            DynamicType::New(scriptContext, TypeIds_Array, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeArrayPrototype>::GetDefaultInstance()), recycler);

        booleanPrototype = RecyclerNew(recycler, JavascriptBooleanObject, nullptr,
            DynamicType::New(scriptContext, TypeIds_BooleanObject, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeBooleanPrototype>::GetDefaultInstance()));

        numberPrototype = RecyclerNew(recycler, JavascriptNumberObject, TaggedInt::ToVarUnchecked(0),
            DynamicType::New(scriptContext, TypeIds_NumberObject, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeNumberPrototype>::GetDefaultInstance()));

        stringPrototype = RecyclerNew(recycler, JavascriptStringObject, nullptr,
            DynamicType::New(scriptContext, TypeIds_StringObject, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeStringPrototype>::GetDefaultInstance()));

        if (scriptContext->GetConfig()->IsES6PrototypeChain())
        {
            datePrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeDatePrototype>::GetDefaultInstance()));

            regexPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeRegexPrototype>::GetDefaultInstance()));
        }
        else
        {
            double initDateValue = JavascriptNumber::NaN;

            datePrototype = RecyclerNewZ(recycler, JavascriptDate, initDateValue,
                DynamicType::New(scriptContext, TypeIds_Date, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeDatePrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6PrototypeChain())
        {
            errorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeErrorPrototype>::GetDefaultInstance()));

            evalErrorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeEvalErrorPrototype>::GetDefaultInstance()));

            rangeErrorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeRangeErrorPrototype>::GetDefaultInstance()));

            referenceErrorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeReferenceErrorPrototype>::GetDefaultInstance()));

            syntaxErrorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeSyntaxErrorPrototype>::GetDefaultInstance()));

            typeErrorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeTypeErrorPrototype>::GetDefaultInstance()));

            uriErrorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeURIErrorPrototype>::GetDefaultInstance()));
        }
        else
        {
            errorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);

            evalErrorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeEvalErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);

            rangeErrorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeRangeErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);

            referenceErrorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeReferenceErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);

            syntaxErrorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeSyntaxErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);

            typeErrorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeTypeErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);

            uriErrorPrototype = RecyclerNew(this->GetRecycler(), JavascriptError,
                DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
                DeferredTypeHandler<InitializeURIErrorPrototype>::GetDefaultInstance()),
                /*isExternalError*/FALSE, /*isPrototype*/TRUE);
        }

        functionPrototype = RecyclerNew(recycler, JavascriptFunction,
            DynamicType::New(scriptContext, TypeIds_Function, objectPrototype, JavascriptFunction::PrototypeEntryPoint,
            DeferredTypeHandler<InitializeFunctionPrototype>::GetDefaultInstance()), &JavascriptFunction::EntryInfo::PrototypeEntryPoint);

        symbolPrototype = nullptr;
        mapPrototype = nullptr;
        setPrototype = nullptr;
        weakMapPrototype = nullptr;
        weakSetPrototype = nullptr;
        arrayIteratorPrototype = nullptr;
        mapIteratorPrototype = nullptr;
        setIteratorPrototype = nullptr;
        stringIteratorPrototype = nullptr;
        promisePrototype = nullptr;
        javascriptEnumeratorIteratorPrototype = nullptr;
        generatorFunctionPrototype = nullptr;
        generatorPrototype = nullptr;
        iteratorPrototype = nullptr;

        if (scriptContext->GetConfig()->IsES6SymbolEnabled())
        {
            symbolPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeSymbolPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6MapEnabled())
        {
            mapPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeMapPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6SetEnabled())
        {
            setPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeSetPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6WeakMapEnabled())
        {
            weakMapPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeWeakMapPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6WeakSetEnabled())
        {
            weakSetPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeWeakSetPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            iteratorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeIteratorPrototype>::GetDefaultInstance()));

            arrayIteratorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, iteratorPrototype, nullptr,
                DeferredTypeHandler<InitializeArrayIteratorPrototype>::GetDefaultInstance()));
            mapIteratorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, iteratorPrototype, nullptr,
                DeferredTypeHandler<InitializeMapIteratorPrototype>::GetDefaultInstance()));
            setIteratorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, iteratorPrototype, nullptr,
                DeferredTypeHandler<InitializeSetIteratorPrototype>::GetDefaultInstance()));
            stringIteratorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, iteratorPrototype, nullptr,
                DeferredTypeHandler<InitializeStringIteratorPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6PromiseEnabled())
        {
            promisePrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializePromisePrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6ProxyEnabled())
        {
            RecyclableObject* proto =
                scriptContext->GetConfig()->IsES6IteratorsEnabled() ?
                    iteratorPrototype :
                    objectPrototype;

            javascriptEnumeratorIteratorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, proto, nullptr,
                DeferredTypeHandler<InitializeJavascriptEnumeratorIteratorPrototype>::GetDefaultInstance()));
        }

        if (scriptContext->GetConfig()->IsES6GeneratorsEnabled())
        {
            generatorFunctionPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, functionPrototype, nullptr,
                DeferredTypeHandler<InitializeGeneratorFunctionPrototype>::GetDefaultInstance()));

            RecyclableObject* proto =
                scriptContext->GetConfig()->IsES6IteratorsEnabled() ?
                    iteratorPrototype :
                    objectPrototype;

            generatorPrototype = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, proto, nullptr,
                DeferredTypeHandler<InitializeGeneratorPrototype>::GetDefaultInstance()));
        }
    }

    void JavascriptLibrary::InitializeTypes()
    {
        Recycler* recycler = this->GetRecycler();
        ScriptConfiguration const *config = scriptContext->GetConfig();

        enumeratorType = StaticType::New(scriptContext, TypeIds_Enumerator, objectPrototype, nullptr);

        // Initialize Array/Argument types
        uint heapArgumentPropertyDescriptorsCount = 0;
        SimplePropertyDescriptor const * heapArgumentPropertyDescriptors = nullptr;
        if (config->IsES6IteratorsEnabled())
        {
            heapArgumentPropertyDescriptors = HeapArgumentsPropertyDescriptors;
            heapArgumentPropertyDescriptorsCount = _countof(HeapArgumentsPropertyDescriptors);
        }
        else
        {
            heapArgumentPropertyDescriptorsCount = _countof(HeapArgumentsPropertyDescriptorsV11);
            heapArgumentPropertyDescriptors = HeapArgumentsPropertyDescriptorsV11;
        }
        heapArgumentsType = DynamicType::New(scriptContext, TypeIds_Arguments, objectPrototype, nullptr,
            SimpleDictionaryTypeHandler::New(scriptContext, heapArgumentPropertyDescriptors, heapArgumentPropertyDescriptorsCount, 0, 0, true, true), true, true);
        activationObjectType = DynamicType::New(scriptContext, TypeIds_ActivationObject, nullValue, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        arrayType = DynamicType::New(scriptContext, TypeIds_Array, arrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        nativeIntArrayType = DynamicType::New(scriptContext, TypeIds_NativeIntArray, arrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
#if ENABLE_COPYONACCESS_ARRAY
        copyOnAccessNativeIntArrayType = DynamicType::New(scriptContext, TypeIds_CopyOnAccessNativeIntArray, arrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
#endif
        nativeFloatArrayType = DynamicType::New(scriptContext, TypeIds_NativeFloatArray, arrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

        arrayBufferType = DynamicType::New(scriptContext, TypeIds_ArrayBuffer, arrayBufferPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        dataViewType = DynamicType::New(scriptContext, TypeIds_DataView, dataViewPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

        int8ArrayType = DynamicType::New(scriptContext, TypeIds_Int8Array, Int8ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

        uint8ArrayType = DynamicType::New(scriptContext, TypeIds_Uint8Array, Uint8ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        uint8ClampedArrayType = DynamicType::New(scriptContext, TypeIds_Uint8ClampedArray, Uint8ClampedArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        int16ArrayType = DynamicType::New(scriptContext, TypeIds_Int16Array, Int16ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        uint16ArrayType = DynamicType::New(scriptContext, TypeIds_Uint16Array, Uint16ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        int32ArrayType = DynamicType::New(scriptContext, TypeIds_Int32Array, Int32ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        uint32ArrayType = DynamicType::New(scriptContext, TypeIds_Uint32Array, Uint32ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        float32ArrayType = DynamicType::New(scriptContext, TypeIds_Float32Array, Float32ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        float64ArrayType = DynamicType::New(scriptContext, TypeIds_Float64Array, Float64ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        int64ArrayType = DynamicType::New(scriptContext, TypeIds_Int64Array, Int64ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        uint64ArrayType = DynamicType::New(scriptContext, TypeIds_Uint64Array, Uint64ArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        boolArrayType = DynamicType::New(scriptContext, TypeIds_BoolArray, BoolArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        charArrayType = DynamicType::New(scriptContext, TypeIds_CharArray, CharArrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

        errorType = DynamicType::New(scriptContext, TypeIds_Error, errorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        evalErrorType = DynamicType::New(scriptContext, TypeIds_Error, evalErrorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        rangeErrorType = DynamicType::New(scriptContext, TypeIds_Error, rangeErrorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        referenceErrorType = DynamicType::New(scriptContext, TypeIds_Error, referenceErrorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        syntaxErrorType = DynamicType::New(scriptContext, TypeIds_Error, syntaxErrorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        typeErrorType = DynamicType::New(scriptContext, TypeIds_Error, typeErrorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        uriErrorType = DynamicType::New(scriptContext, TypeIds_Error, uriErrorPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

        symbolTypeStatic = nullptr;
        symbolTypeDynamic = nullptr;
        withType    = nullptr;
        proxyType   = nullptr;
        promiseType = nullptr;
        javascriptEnumeratorIteratorType = nullptr;

        // Initialize boolean types
        booleanTypeStatic = StaticType::New(scriptContext, TypeIds_Boolean, booleanPrototype, nullptr);
        booleanTypeDynamic = DynamicType::New(scriptContext, TypeIds_BooleanObject, booleanPrototype, nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);

        if (config->IsES6SymbolEnabled())
        {
            // Initialize symbol types
            symbolTypeStatic = StaticType::New(scriptContext, TypeIds_Symbol, symbolPrototype, nullptr);
            symbolTypeDynamic = DynamicType::New(scriptContext, TypeIds_SymbolObject, symbolPrototype, nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);
        }

        if (config->IsES6UnscopablesEnabled())
        {
            withType = StaticType::New(scriptContext, TypeIds_WithScopeObject, GetNull(), nullptr);
        }

        if (config->IsES6SpreadEnabled())
        {
            SpreadArgumentType = DynamicType::New(scriptContext, TypeIds_SpreadArgument, GetNull(), nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);
        }

        if (config->IsES6ProxyEnabled())
        {
            // proxy's prototype is not actually used. once a proxy is used, the GetType()->GetPrototype() is not used in instanceOf style usage as they are trapped.
            // We can use GetType()->GetPrototype() in [[get]], [[set]], and [[hasProperty]] to force the prototype walk to stop at prototype so we don't need to
            // continue prototype chain walk after proxy.
            proxyType = DynamicType::New(scriptContext, TypeIds_Proxy, GetNull(), nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);

            javascriptEnumeratorIteratorType = DynamicType::New(scriptContext, TypeIds_JavascriptEnumeratorIterator, javascriptEnumeratorIteratorPrototype, nullptr,

                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }

        if (config->IsES6PromiseEnabled())
        {
            promiseType = DynamicType::New(scriptContext, TypeIds_Promise, promisePrototype, nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);
        }

        // Initialize Date types
        dateType = DynamicType::New(scriptContext, TypeIds_Date, datePrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        variantDateType = StaticType::New(scriptContext, TypeIds_VariantDate, nullValue, nullptr);

        anonymousFunctionTypeHandler = NullTypeHandler<false>::GetDefaultInstance();
        anonymousFunctionWithPrototypeTypeHandler = &SharedFunctionWithPrototypeTypeHandlerV11;
        //  Initialize function types
        if (config->IsES6FunctionNameEnabled())
        {
            functionTypeHandler = &SharedFunctionWithoutPrototypeTypeHandler;
        }
        else
        {
            functionTypeHandler = anonymousFunctionTypeHandler;
        }

        if (config->IsES6FunctionNameEnabled())
        {
            functionWithPrototypeTypeHandler = &SharedFunctionWithPrototypeTypeHandler;
        }
        else
        {
            functionWithPrototypeTypeHandler = anonymousFunctionWithPrototypeTypeHandler;
        }
        functionWithPrototypeTypeHandler->SetHasKnownSlot0();

        externalFunctionWithDeferredPrototypeType = CreateDeferredPrototypeFunctionTypeNoProfileThunk(JavascriptExternalFunction::ExternalFunctionThunk, true /*isShared*/);
        wrappedFunctionWithDeferredPrototypeType = CreateDeferredPrototypeFunctionTypeNoProfileThunk(JavascriptExternalFunction::WrappedFunctionThunk, true /*isShared*/);
        stdCallFunctionWithDeferredPrototypeType = CreateDeferredPrototypeFunctionTypeNoProfileThunk(JavascriptExternalFunction::StdCallExternalFunctionThunk, true /*isShared*/);
        idMappedFunctionWithPrototypeType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, JavascriptExternalFunction::ExternalFunctionThunk,
            &SharedIdMappedFunctionWithPrototypeTypeHandler, true, true);
        externalConstructorFunctionWithDeferredPrototypeType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, JavascriptExternalFunction::ExternalFunctionThunk,
            Js::DeferredTypeHandler<Js::JavascriptExternalFunction::DeferredInitializer>::GetDefaultInstance(), true, true);

        if (config->IsES6FunctionNameEnabled())
        {
            boundFunctionType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, BoundFunction::NewInstance,
                GetDeferredFunctionTypeHandler(), true, true);
        }
        else
        {
            boundFunctionType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, BoundFunction::NewInstance,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }
        crossSiteDeferredPrototypeFunctionType = CreateDeferredPrototypeFunctionTypeNoProfileThunk(
            scriptContext->CurrentCrossSiteThunk, true /*isShared*/);
        crossSiteIdMappedFunctionWithPrototypeType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, scriptContext->CurrentCrossSiteThunk,
            &SharedIdMappedFunctionWithPrototypeTypeHandler, true, true);
        crossSiteExternalConstructFunctionWithPrototypeType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, scriptContext->CurrentCrossSiteThunk,
            Js::DeferredTypeHandler<Js::JavascriptExternalFunction::DeferredInitializer>::GetDefaultInstance(), true, true);

        // Initialize Number types
        numberTypeStatic = StaticType::New(scriptContext, TypeIds_Number, numberPrototype, nullptr);
        int64NumberTypeStatic = StaticType::New(scriptContext, TypeIds_Int64Number, numberPrototype, nullptr);
        uint64NumberTypeStatic = StaticType::New(scriptContext, TypeIds_UInt64Number, numberPrototype, nullptr);
        numberTypeDynamic = DynamicType::New(scriptContext, TypeIds_NumberObject, numberPrototype, nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);

        // SIMD_JS
        // Initialize types
        if (GetScriptContext()->GetConfig()->IsSimdjsEnabled())
        {
            simdFloat32x4TypeStatic = StaticType::New(scriptContext, TypeIds_SIMDFloat32x4, nullValue /*prototype*/, nullptr);
            simdFloat64x2TypeStatic = StaticType::New(scriptContext, TypeIds_SIMDFloat64x2, nullValue /*prototype*/, nullptr);
            simdInt32x4TypeStatic = StaticType::New(scriptContext, TypeIds_SIMDInt32x4, nullValue /*prototype*/, nullptr);
            simdInt8x16TypeStatic = StaticType::New(scriptContext, TypeIds_SIMDInt8x16, nullValue /*prototype*/, nullptr);
        }

        // Initialize Object types
        for (int16 i = 0; i < PreInitializedObjectTypeCount; i++)
        {
            SimplePathTypeHandler * typeHandler =
                SimplePathTypeHandler::New(
                    scriptContext,
                    scriptContext->GetRootPath(),
                    0,
                    i * InlineSlotCountIncrement,
                    sizeof(DynamicObject),
                    true,
                    true);
            typeHandler->SetIsInlineSlotCapacityLocked();
            objectTypes[i] = DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr, typeHandler, true, true);
        }
        for (int16 i = 0; i < PreInitializedObjectTypeCount; i++)
        {
            SimplePathTypeHandler * typeHandler =
                SimplePathTypeHandler::New(
                    scriptContext,
                    scriptContext->GetRootPath(),
                    0,
                    DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity() + i * InlineSlotCountIncrement,
                    DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots(),
                    true,
                    true);
            typeHandler->SetIsInlineSlotCapacityLocked();
            objectHeaderInlinedTypes[i] =
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr, typeHandler, true, true);
        }

        // Initialize regex types
        TypePath *const regexResultPath = TypePath::New(recycler);
        regexResultPath->Add(BuiltInPropertyRecords::input);
        regexResultPath->Add(BuiltInPropertyRecords::index);
        regexResultType = DynamicType::New(scriptContext, TypeIds_Array, arrayPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, regexResultPath, regexResultPath->GetPathLength(), JavascriptRegularExpressionResult::InlineSlotCount, sizeof(JavascriptArray), true, true), true, true);

        // Initialize string types
        stringTypeStatic = StaticType::New(scriptContext, TypeIds_String, stringPrototype, nullptr);
        stringTypeDynamic = DynamicType::New(scriptContext, TypeIds_StringObject, stringPrototype, nullptr, NullTypeHandler<false>::GetDefaultInstance(), true, true);

        // Initialize Throw error object type
        throwErrorObjectType = StaticType::New(scriptContext, TypeIds_Undefined, nullValue, ThrowErrorObject::DefaultEntryPoint);

        mapType = nullptr;
        setType = nullptr;
        weakMapType = nullptr;
        weakSetType = nullptr;
        iteratorResultType = nullptr;
        arrayIteratorType = nullptr;
        mapIteratorType = nullptr;
        setIteratorType = nullptr;
        stringIteratorType = nullptr;

        if (config->IsES6MapEnabled())
        {
            mapType = DynamicType::New(scriptContext, TypeIds_Map, mapPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }

        if (config->IsES6SetEnabled())
        {
            setType = DynamicType::New(scriptContext, TypeIds_Set, setPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }

        if (config->IsES6WeakMapEnabled())
        {
            weakMapType = DynamicType::New(scriptContext, TypeIds_WeakMap, weakMapPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }

        if (config->IsES6WeakSetEnabled())
        {
            weakSetType = DynamicType::New(scriptContext, TypeIds_WeakSet, weakSetPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }

        if (config->IsES6IteratorsEnabled())
        {
            TypePath *const iteratorResultPath = TypePath::New(recycler);
            iteratorResultPath->Add(BuiltInPropertyRecords::value);
            iteratorResultPath->Add(BuiltInPropertyRecords::done);
            iteratorResultType = DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, iteratorResultPath, iteratorResultPath->GetPathLength(), 2, sizeof(DynamicObject), true, true), true, true);

            arrayIteratorType = DynamicType::New(scriptContext, TypeIds_ArrayIterator, arrayIteratorPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
            mapIteratorType = DynamicType::New(scriptContext, TypeIds_MapIterator, mapIteratorPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
            setIteratorType = DynamicType::New(scriptContext, TypeIds_SetIterator, setIteratorPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
            stringIteratorType = DynamicType::New(scriptContext, TypeIds_StringIterator, stringIteratorPrototype, nullptr,
                SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
        }

        if (config->IsES6GeneratorsEnabled())
        {
            generatorConstructorPrototypeObjectType = DynamicType::New(scriptContext, TypeIds_Object, generatorPrototype, nullptr,
                NullTypeHandler<false>::GetDefaultInstance(), true, true);

            generatorConstructorPrototypeObjectType->SetHasNoEnumerableProperties(true);
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        debugDisposableObjectType = DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);

        debugFuncExecutorInDisposeObjectType = DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
#endif
    }

    void JavascriptLibrary::InitializeGeneratorFunction(DynamicObject *function, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        bool isAnonymousFunction = JavascriptGeneratorFunction::FromVar(function)->IsAnonymousFunction();

        JavascriptLibrary* javascriptLibrary = function->GetType()->GetLibrary();
        typeHandler->Convert(function, isAnonymousFunction ? javascriptLibrary->anonymousFunctionWithPrototypeTypeHandler : javascriptLibrary->functionWithPrototypeTypeHandler);
        function->SetPropertyWithAttributes(PropertyIds::prototype, javascriptLibrary->CreateGeneratorConstructorPrototypeObject(), PropertyWritable, nullptr);

        if (function->GetScriptContext()->GetConfig()->IsES6FunctionNameEnabled() && !isAnonymousFunction)
        {
            JavascriptString * functionName = nullptr;
            DebugOnly(bool status = ) ((Js::JavascriptFunction*)function)->GetFunctionName(&functionName);
            Assert(status);
            function->SetPropertyWithAttributes(PropertyIds::name,functionName, PropertyConfigurable, nullptr);
        }
    }


    template<bool addPrototype>
    void JavascriptLibrary::InitializeFunction(DynamicObject *function, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        JavascriptLibrary* javascriptLibrary = function->GetType()->GetLibrary();
        ScriptFunction *scriptFunction = nullptr;
        bool useAnonymous = false;
        if (ScriptFunction::Is(function))
        {
            scriptFunction = Js::ScriptFunction::FromVar(function);
            useAnonymous = scriptFunction->IsAnonymousFunction();
        }

        if (!addPrototype)
        {
            Assert(!useAnonymous);
            typeHandler->Convert(function, javascriptLibrary->functionTypeHandler);
        }
        else
        {
            typeHandler->Convert(function, useAnonymous ? javascriptLibrary->anonymousFunctionWithPrototypeTypeHandler : javascriptLibrary->functionWithPrototypeTypeHandler);
            function->SetProperty(PropertyIds::prototype, javascriptLibrary->CreateConstructorPrototypeObject((Js::JavascriptFunction *)function), PropertyOperation_None, nullptr);
        }

        if (scriptFunction)
        {
            if (scriptFunction->GetFunctionInfo()->IsClassConstructor())
            {
                scriptFunction->SetWritable(Js::PropertyIds::prototype, FALSE);
            }
        }

        ScriptContext *scriptContext = function->GetScriptContext();
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            if(scriptFunction && (useAnonymous || scriptFunction->GetFunctionProxy()->EnsureDeserialized()->GetIsStaticNameFunction()))
            {
                return;
            }
            JavascriptString * functionName = nullptr;
            if (((Js::JavascriptFunction*)function)->GetFunctionName(&functionName))
            {
                function->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
            }
        }
    }

    template<bool isNameAvailable, bool isPrototypeAvailable>
    class InitializeFunctionDeferredTypeHandlerFilter
    {
    public:
        static bool HasFilter() { return true; }
        static bool HasProperty(PropertyId propertyId)
        {
            switch (propertyId)
            {
            case PropertyIds::prototype:
                return isPrototypeAvailable;
            case PropertyIds::name:
                return isNameAvailable;
            }
            return false;
        }
    };

    template<bool isNameAvailable, bool isPrototypeAvailable>
    DynamicTypeHandler * JavascriptLibrary::GetDeferredFunctionTypeHandlerBase()
    {
        return DeferredTypeHandler<InitializeFunction<isPrototypeAvailable>, InitializeFunctionDeferredTypeHandlerFilter<isNameAvailable, isPrototypeAvailable>>::GetDefaultInstance();
    }

    template<bool isNameAvailable, bool isPrototypeAvailable>
    DynamicTypeHandler * JavascriptLibrary::GetDeferredGeneratorFunctionTypeHandlerBase()
    {
        return DeferredTypeHandler<InitializeGeneratorFunction, InitializeFunctionDeferredTypeHandlerFilter<isNameAvailable, isPrototypeAvailable>>::GetDefaultInstance();
    }

    DynamicTypeHandler * JavascriptLibrary::GetDeferredAnonymousPrototypeGeneratorFunctionTypeHandler()
    {
        return JavascriptLibrary::GetDeferredGeneratorFunctionTypeHandlerBase</*isNameAvailable*/ false>();
    }

    DynamicTypeHandler * JavascriptLibrary::GetDeferredPrototypeGeneratorFunctionTypeHandler(ScriptContext* scriptContext)
    {
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            return JavascriptLibrary::GetDeferredGeneratorFunctionTypeHandlerBase</*isNameAvailable*/ true>();
        }
        else
        {
            return JavascriptLibrary::GetDeferredGeneratorFunctionTypeHandlerBase</*isNameAvailable*/ false>();
        }

    }

    DynamicTypeHandler * JavascriptLibrary::GetDeferredAnonymousPrototypeFunctionTypeHandler()
    {
        return JavascriptLibrary::GetDeferredFunctionTypeHandlerBase</*isNameAvailable*/ false>();
    }

    DynamicTypeHandler * JavascriptLibrary::GetDeferredPrototypeFunctionTypeHandler(ScriptContext* scriptContext)
    {
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            return JavascriptLibrary::GetDeferredFunctionTypeHandlerBase</*isNameAvailable*/ true>();
        }
        else
        {
            return JavascriptLibrary::GetDeferredFunctionTypeHandlerBase</*isNameAvailable*/ false>();
        }
    }

    DynamicTypeHandler * JavascriptLibrary::GetDeferredAnonymousFunctionTypeHandler()
    {
        return anonymousFunctionTypeHandler;
    }

    DynamicTypeHandler * JavascriptLibrary::GetDeferredFunctionTypeHandler()
    {
        if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameEnabled())
        {
            return GetDeferredFunctionTypeHandlerBase</*isNameAvailable*/ true, /*isPrototypeAvailable*/ false>();
        }
        return functionTypeHandler;
    }

    DynamicTypeHandler * JavascriptLibrary::ScriptFunctionTypeHandler(bool hasPrototype, bool isAnonymousFunction)
    {
        DynamicTypeHandler * scriptFunctionTypeHandler = nullptr;

        if (hasPrototype)
        {
            scriptFunctionTypeHandler = isAnonymousFunction ?
                this->GetDeferredAnonymousFunctionTypeHandler() : this->GetDeferredFunctionTypeHandler();
        }
        else
        {
            scriptFunctionTypeHandler = isAnonymousFunction ?
                JavascriptLibrary::GetDeferredAnonymousPrototypeFunctionTypeHandler() : JavascriptLibrary::GetDeferredPrototypeFunctionTypeHandler(scriptContext);
        }
        return scriptFunctionTypeHandler;
    }

    DynamicType * JavascriptLibrary::CreateDeferredPrototypeGeneratorFunctionType(JavascriptMethod entrypoint, bool isAnonymousFunction, bool isShared)
    {
        return DynamicType::New(scriptContext, TypeIds_Function, generatorFunctionPrototype, entrypoint,
            isAnonymousFunction ? GetDeferredAnonymousPrototypeGeneratorFunctionTypeHandler() : GetDeferredPrototypeGeneratorFunctionTypeHandler(scriptContext), isShared, isShared);
    }

    DynamicType * JavascriptLibrary::CreateDeferredPrototypeFunctionType(JavascriptMethod entrypoint)
    {
        return CreateDeferredPrototypeFunctionTypeNoProfileThunk(this->inDispatchProfileMode ? ProfileEntryThunk : entrypoint);
    }

    DynamicType * JavascriptLibrary::CreateDeferredPrototypeFunctionTypeNoProfileThunk(JavascriptMethod entrypoint, bool isShared)
    {
        // Note: the lack of TypeHandler switching here based on the isAnonymousFunction flag is intentional.
        // We can't switch shared typeHandlers and RuntimeFunctions do not produce script code for us to know if a function is Anonymous.
        // As a result we may have an issue where hasProperty would say you have a name property but getProperty returns undefined
        return DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, entrypoint,
            GetDeferredPrototypeFunctionTypeHandler(scriptContext), isShared, isShared);
    }
    DynamicType * JavascriptLibrary::CreateFunctionType(JavascriptMethod entrypoint, RecyclableObject* prototype)
    {
        if (prototype == nullptr)
        {
            prototype = functionPrototype;
        }

        return DynamicType::New(scriptContext, TypeIds_Function, prototype, entrypoint,
            GetDeferredFunctionTypeHandler(), false, false);
    }

    DynamicType * JavascriptLibrary::CreateFunctionWithLengthType(FunctionInfo * functionInfo)
    {
        return CreateFunctionWithLengthType(this->GetFunctionPrototype(), functionInfo);
    }

    DynamicType * JavascriptLibrary::CreateFunctionWithLengthAndPrototypeType(FunctionInfo * functionInfo)
    {
        return CreateFunctionWithLengthAndPrototypeType(this->GetFunctionPrototype(), functionInfo);
    }

    DynamicType * JavascriptLibrary::CreateFunctionWithLengthType(DynamicObject * prototype, FunctionInfo * functionInfo)
    {
        Assert(!functionInfo->HasBody());
        return DynamicType::New(scriptContext, TypeIds_Function, prototype,
            this->inProfileMode? ProfileEntryThunk : functionInfo->GetOriginalEntryPoint(),
            &SharedFunctionWithLengthTypeHandler);
    }

    DynamicType * JavascriptLibrary::CreateFunctionWithLengthAndPrototypeType(DynamicObject * prototype, FunctionInfo * functionInfo)
    {
        Assert(!functionInfo->HasBody());
        return DynamicType::New(scriptContext, TypeIds_Function, prototype,
            this->inProfileMode? ProfileEntryThunk : functionInfo->GetOriginalEntryPoint(),
            SimpleDictionaryTypeHandler::New(scriptContext, FunctionWithLengthAndPrototypeTypeDescriptors, _countof(FunctionWithLengthAndPrototypeTypeDescriptors), 0, 0));
    }

    void JavascriptLibrary::InitializeProperties(ThreadContext * threadContext)
    {
        if ( threadContext->GetMaxPropertyId() < PropertyIds::_countJSOnlyProperty )
        {
            threadContext->UncheckedAddBuiltInPropertyId();
        }
    }

    DynamicObject * JavascriptLibraryBase::GetObjectPrototype()
    {
        return GetObjectPrototypeObject();
    }

    void JavascriptLibraryBase::Finalize(bool isShutdown)
    {
        if (scriptContext)
        {
            // Clear the weak reference dictionary so we don't need to clean them
            // during PostCollectCallBack before Dispose deleting the script context.
            scriptContext->ResetWeakReferenceDicitionaryList();
        }
    }

    void JavascriptLibraryBase::Dispose(bool isShutdown)
    {
        if (scriptContext)
        {
            if (isShutdown)
            {
                // during shut down the global object might not be closed yet.
                // Clear the global object from the script context so it doesn't
                // get unpinned (which may fail because the recycler is shutting down)
                scriptContext->Close(true);
                scriptContext->ClearGlobalObject();
            }
            else
            {
                Assert(scriptContext->IsClosed());
            }
            HeapDelete(scriptContext);
            scriptContext = nullptr;
        }
    }

    JavascriptEnumerator * JavascriptLibrary::GetNullEnumerator() const
    {
        return nullEnumerator;
    }

#define  ADD_TYPEDARRAY_CONSTRUCTOR(typedarrayConstructor, TypedArray) \
    typedarrayConstructor = CreateBuiltinConstructor(&TypedArray##::EntryInfo::NewInstance, \
    DeferredTypeHandler<Initialize##TypedArray##Constructor>::GetDefaultInstance()); \
            AddFunction(globalObject, PropertyIds::TypedArray, typedarrayConstructor); \

    void JavascriptLibrary::InitializeGlobal(GlobalObject * globalObject)
    {
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // WARNING: Any objects here using DeferredTypeHandler need to appear in EnsureLibraryReadyForHybridDebugging
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        RecyclableObject* globalObjectPrototype = GetObjectPrototype();
        globalObject->SetPrototype(globalObjectPrototype);
        Recycler * recycler = this->GetRecycler();

        pi = JavascriptNumber::New(Math::PI, scriptContext);
        nan = JavascriptNumber::New(JavascriptNumber::NaN, scriptContext);
        negativeInfinite = JavascriptNumber::New(JavascriptNumber::NEGATIVE_INFINITY, scriptContext);
        positiveInfinite = JavascriptNumber::New(JavascriptNumber::POSITIVE_INFINITY, scriptContext);
        minValue = JavascriptNumber::New(JavascriptNumber::MIN_VALUE, scriptContext);
        maxValue = JavascriptNumber::New(JavascriptNumber::MAX_VALUE, scriptContext);
        negativeZero = JavascriptNumber::New(JavascriptNumber::NEG_ZERO, scriptContext);
        undefinedValue = RecyclerNew(recycler, RecyclableObject,
            StaticType::New(scriptContext, TypeIds_Undefined, nullValue, nullptr));
        missingPropertyHolder = RecyclerNewPlus(recycler, sizeof(Var), DynamicObject,
            DynamicType::New(scriptContext, TypeIds_Object, nullValue, nullptr, &MissingPropertyHolderTypeHandler));
        MissingPropertyTypeHandler::SetUndefinedPropertySlot(missingPropertyHolder);

        emptyString = CreateEmptyString(); // Must be created before other calls to CreateString
        nullString = CreateEmptyString(); // Must be distinct from emptyString (for the DOM)
        quotesString = CreateStringFromCppLiteral(L"\"\"");
        whackString = CreateStringFromCppLiteral(L"/");
        undefinedDisplayString = CreateStringFromCppLiteral(L"undefined");
        nanDisplayString = CreateStringFromCppLiteral(L"NaN");
        nullDisplayString = CreateStringFromCppLiteral(L"null");
        unknownDisplayString = CreateStringFromCppLiteral(L"unknown");
        commaDisplayString = CreateStringFromCppLiteral(L",");
        commaSpaceDisplayString = CreateStringFromCppLiteral(L", ");
        trueDisplayString = CreateStringFromCppLiteral(L"true");
        falseDisplayString = CreateStringFromCppLiteral(L"false");
        lengthDisplayString = CreateStringFromCppLiteral(L"length");
        objectDisplayString = CreateStringFromCppLiteral(L"[object Object]");
        errorDisplayString = CreateStringFromCppLiteral(L"[object Error]");
        stringTypeDisplayString = CreateStringFromCppLiteral(L"string");
        functionPrefixString = CreateStringFromCppLiteral(L"function ");
        generatorFunctionPrefixString = CreateStringFromCppLiteral(L"function* ");
        functionDisplayString = CreateStringFromCppLiteral(JS_DISPLAY_STRING_FUNCTION_ANONYMOUS);
        xDomainFunctionDisplayString = CreateStringFromCppLiteral(L"\012function anonymous() {\012    [x-domain code]\012}\012");
        invalidDateString = CreateStringFromCppLiteral(L"Invalid Date");
        objectTypeDisplayString = CreateStringFromCppLiteral(L"object");
        functionTypeDisplayString = CreateStringFromCppLiteral(L"function");
        booleanTypeDisplayString = CreateStringFromCppLiteral(L"boolean");
        numberTypeDisplayString = CreateStringFromCppLiteral(L"number");
        promiseResolveFunction = nullptr;

        if (GetScriptContext()->GetConfig()->IsSimdjsEnabled())
        {
            simdFloat32x4DisplayString = CreateStringFromCppLiteral(L"Float32x4");
            simdFloat64x2DisplayString = CreateStringFromCppLiteral(L"Float64x2");
            simdInt32x4DisplayString = CreateStringFromCppLiteral(L"Int32x4");
        }

        if (scriptContext->GetConfig()->IsES6SymbolEnabled())
        {
            symbolTypeDisplayString = CreateStringFromCppLiteral(L"symbol");

            symbolHasInstance = CreateSymbol(BuiltInPropertyRecords::_symbolHasInstance);
            symbolIsConcatSpreadable = CreateSymbol(BuiltInPropertyRecords::_symbolIsConcatSpreadable);
            symbolIterator = CreateSymbol(BuiltInPropertyRecords::_symbolIterator);
            symbolSpecies = CreateSymbol(BuiltInPropertyRecords::_symbolSpecies);
            symbolToPrimitive = CreateSymbol(BuiltInPropertyRecords::_symbolToPrimitive);
            symbolToStringTag = CreateSymbol(BuiltInPropertyRecords::_symbolToStringTag);
            symbolUnscopables = CreateSymbol(BuiltInPropertyRecords::_symbolUnscopables);
        }
        else
        {
            symbolTypeDisplayString = nullptr;
            symbolHasInstance = nullptr;
            symbolIsConcatSpreadable = nullptr;
            symbolIterator = nullptr;
            symbolSpecies = nullptr;
            symbolToPrimitive = nullptr;
            symbolToStringTag = nullptr;
            symbolUnscopables = nullptr;
        }

        debuggerDeadZoneBlockVariableString = CreateStringFromCppLiteral(L"[Uninitialized block variable]");
        defaultAccessorFunction = CreateNonProfiledFunction(&JavascriptOperators::EntryInfo::DefaultAccessor);

        if (scriptContext->GetConfig()->IsErrorStackTraceEnabled())
        {
            stackTraceAccessorFunction = CreateNonProfiledFunction(&JavascriptExceptionOperators::EntryInfo::StackTraceAccessor);
            stackTraceAccessorFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);
        }

        throwTypeErrorAccessorFunction = CreateNonProfiledFunction(&JavascriptExceptionOperators::EntryInfo::ThrowTypeErrorAccessor);
        throwTypeErrorAccessorFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

        throwTypeErrorCallerAccessorFunction = CreateNonProfiledFunction(&JavascriptExceptionOperators::EntryInfo::ThrowTypeErrorCallerAccessor);
        throwTypeErrorCallerAccessorFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

        throwTypeErrorCalleeAccessorFunction = CreateNonProfiledFunction(&JavascriptExceptionOperators::EntryInfo::ThrowTypeErrorCalleeAccessor);
        throwTypeErrorCalleeAccessorFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

        throwTypeErrorArgumentsAccessorFunction = CreateNonProfiledFunction(&JavascriptExceptionOperators::EntryInfo::ThrowTypeErrorArgumentsAccessor);
        throwTypeErrorArgumentsAccessorFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

        if (scriptContext->GetConfig()->Is__proto__Enabled())
        {
            __proto__getterFunction = CreateNonProfiledFunction(&ObjectPrototypeObject::EntryInfo::__proto__getter);
            __proto__getterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

            __proto__setterFunction = CreateNonProfiledFunction(&ObjectPrototypeObject::EntryInfo::__proto__setter);
            __proto__setterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6PromiseEnabled())
        {
            identityFunction = CreateNonProfiledFunction(&JavascriptPromise::EntryInfo::Identity);
            identityFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone, nullptr);

            throwerFunction = CreateNonProfiledFunction(&JavascriptPromise::EntryInfo::Thrower);
            throwerFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone, nullptr);
        }

        booleanTrue = RecyclerNew(recycler, JavascriptBoolean, true, booleanTypeStatic);
        booleanFalse = RecyclerNew(recycler, JavascriptBoolean, false, booleanTypeStatic);
        randSeed = 0;

        AddMember(globalObject, PropertyIds::NaN, nan, PropertyNone);
        AddMember(globalObject, PropertyIds::Infinity, positiveInfinite, PropertyNone);
        AddMember(globalObject, PropertyIds::undefined, undefinedValue, PropertyNone);

        // Note: Any global function added/removed/changed here should also be updated in JavascriptLibrary::ProfilerRegisterBuiltinFunctions
        // so that the new functions show up in the profiler too.
        JavascriptFunction ** builtinFuncs = this->GetBuiltinFunctions();

        evalFunctionObject = AddFunctionToLibraryObject(globalObject, PropertyIds::eval, &GlobalObject::EntryInfo::Eval, 1);
        parseIntFunctionObject = AddFunctionToLibraryObject(globalObject, PropertyIds::parseInt, &GlobalObject::EntryInfo::ParseInt, 2);
        builtinFuncs[BuiltinFunction::GlobalObject_ParseInt] = parseIntFunctionObject;
        parseFloatFunctionObject = AddFunctionToLibraryObject(globalObject, PropertyIds::parseFloat, &GlobalObject::EntryInfo::ParseFloat, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::isNaN, &GlobalObject::EntryInfo::IsNaN, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::isFinite, &GlobalObject::EntryInfo::IsFinite, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::decodeURI, &GlobalObject::EntryInfo::DecodeURI, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::decodeURIComponent, &GlobalObject::EntryInfo::DecodeURIComponent, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::encodeURI, &GlobalObject::EntryInfo::EncodeURI, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::encodeURIComponent, &GlobalObject::EntryInfo::EncodeURIComponent, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::escape, &GlobalObject::EntryInfo::Escape, 1);
        AddFunctionToLibraryObject(globalObject, PropertyIds::unescape, &GlobalObject::EntryInfo::UnEscape, 1);

        if (scriptContext->GetConfig()->SupportsCollectGarbage())
        {
            AddFunctionToLibraryObject(globalObject, PropertyIds::CollectGarbage, &GlobalObject::EntryInfo::CollectGarbage, 0);
        }

#ifdef IR_VIEWER
        if (Js::Configuration::Global.flags.IsEnabled(Js::IRViewerFlag))
        {
            AddFunctionToLibraryObjectWithPropertyName(globalObject, L"parseIR", &GlobalObject::EntryInfo::ParseIR, 1);
            AddFunctionToLibraryObjectWithPropertyName(globalObject, L"functionList", &GlobalObject::EntryInfo::FunctionList, 1);
            AddFunctionToLibraryObjectWithPropertyName(globalObject, L"rejitFunction", &GlobalObject::EntryInfo::RejitFunction, 2);
        }
#endif /* IR_VIEWER */

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        builtInConstructorCache = RecyclerNew(this->GetRecycler(), ConstructorCache);
        builtInConstructorCache->PopulateForSkipDefaultNewObject(this->GetScriptContext());

        objectConstructor = CreateBuiltinConstructor(&JavascriptObject::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeObjectConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::Object, objectConstructor);
        arrayConstructor = CreateBuiltinConstructor(&JavascriptArray::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeArrayConstructor>::GetDefaultInstance());
        SetArrayObjectHasUserDefinedSpecies(false);
        AddFunction(globalObject, PropertyIds::Array, arrayConstructor);
        booleanConstructor = CreateBuiltinConstructor(&JavascriptBoolean::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeBooleanConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::Boolean, booleanConstructor);

        symbolConstructor = nullptr;
        proxyConstructor = nullptr;
        promiseConstructor = nullptr;
        reflectObject = nullptr;

        if (scriptContext->GetConfig()->IsES6SymbolEnabled())
        {
            symbolConstructor = CreateBuiltinConstructor(&JavascriptSymbol::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeSymbolConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::Symbol, symbolConstructor);
        }

        if (scriptContext->GetConfig()->IsES6ProxyEnabled())
        {
            proxyConstructor = CreateBuiltinConstructor(&JavascriptProxy::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeProxyConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::Proxy, proxyConstructor);
            reflectObject = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeReflectObject>::GetDefaultInstance()));
            AddMember(globalObject, PropertyIds::Reflect, reflectObject);
        }

        if (scriptContext->GetConfig()->IsES6PromiseEnabled())
        {
            promiseConstructor = CreateBuiltinConstructor(&JavascriptPromise::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializePromiseConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::Promise, promiseConstructor);
        }

        dateConstructor = CreateBuiltinConstructor(&JavascriptDate::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeDateConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::Date, dateConstructor);
        functionConstructor = CreateBuiltinConstructor(&JavascriptFunction::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeFunctionConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::Function, functionConstructor);
        mathObject = DynamicObject::New(recycler,
            DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeMathObject>::GetDefaultInstance()));
        AddMember(globalObject, PropertyIds::Math, mathObject);

#if ENABLE_NATIVE_CODEGEN
        // SIMD_JS
        // we declare global objects and lib functions only if SSE2 is available. Else, we use the polyfill.
        if (AutoSystemInfo::Data.SSE2Available() && GetScriptContext()->GetConfig()->IsSimdjsEnabled())
        {
            simdObject = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeSIMDObject>::GetDefaultInstance()));

            AddMember(globalObject, PropertyIds::SIMD, simdObject, PropertyNone);

            // Initialize toString functions. Do it here instead of InitializeSIMDObject, since we can possible access those fields before SIMD object is deferred initialized (e.g. SIMD object coming from ASMJS code).
            simdFloat32x4ToStringFunction = DefaultCreateFunction(&JavascriptSIMDFloat32x4::EntryInfo::ToString, 1, nullptr, nullptr, PropertyIds::toString);
            simdFloat64x2ToStringFunction = DefaultCreateFunction(&JavascriptSIMDFloat64x2::EntryInfo::ToString, 1, nullptr, nullptr, PropertyIds::toString);
            simdInt32x4ToStringFunction = DefaultCreateFunction(&JavascriptSIMDInt32x4::EntryInfo::ToString, 1, nullptr, nullptr, PropertyIds::toString);
            simdInt8x16ToStringFunction = DefaultCreateFunction(&JavascriptSIMDInt8x16::EntryInfo::ToString, 1, nullptr, nullptr, PropertyIds::toString);
        }
#endif

        debugObject = nullptr;

        numberConstructor = CreateBuiltinConstructor(&JavascriptNumber::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeNumberConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::Number, numberConstructor);
        stringConstructor = CreateBuiltinConstructor(&JavascriptString::EntryInfo::NewInstance,
            DeferredTypeHandler<InitializeStringConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::String, stringConstructor);
        regexConstructorType = DynamicType::New(scriptContext, TypeIds_Function, functionPrototype, JavascriptRegExp::NewInstance,
            DeferredTypeHandler<InitializeRegexConstructor>::GetDefaultInstance());
        regexConstructor = RecyclerNewEnumClass(recycler, EnumFunctionClass, JavascriptRegExpConstructor, regexConstructorType);
        AddFunction(globalObject, PropertyIds::RegExp, regexConstructor);

        if (scriptContext->GetConfig()->IsTypedArrayEnabled())
        {
            arrayBufferConstructor = CreateBuiltinConstructor(&ArrayBuffer::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeArrayBufferConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::ArrayBuffer, arrayBufferConstructor);
            dataViewConstructor = CreateBuiltinConstructor(&DataView::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeDataViewConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::DataView, dataViewConstructor);

            if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled())
            {
                typedArrayConstructor = CreateBuiltinConstructor(&TypedArrayBase::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeTypedArrayConstructor>::GetDefaultInstance(),
                    functionPrototype);

                Int8ArrayConstructor = CreateBuiltinConstructor(&Int8Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeInt8ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Int8Array, Int8ArrayConstructor);

                Uint8ArrayConstructor = CreateBuiltinConstructor(&Uint8Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeUint8ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Uint8Array, Uint8ArrayConstructor);

                Uint8ClampedArrayConstructor = CreateBuiltinConstructor(&Uint8ClampedArray::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeUint8ClampedArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Uint8ClampedArray, Uint8ClampedArrayConstructor);

                Int16ArrayConstructor = CreateBuiltinConstructor(&Int16Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeInt16ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Int16Array, Int16ArrayConstructor);

                Uint16ArrayConstructor = CreateBuiltinConstructor(&Uint16Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeUint16ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Uint16Array, Uint16ArrayConstructor);

                Int32ArrayConstructor = CreateBuiltinConstructor(&Int32Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeInt32ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Int32Array, Int32ArrayConstructor);

                Uint32ArrayConstructor = CreateBuiltinConstructor(&Uint32Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeUint32ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Uint32Array, Uint32ArrayConstructor);

                Float32ArrayConstructor = CreateBuiltinConstructor(&Float32Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeFloat32ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Float32Array, Float32ArrayConstructor);

                Float64ArrayConstructor = CreateBuiltinConstructor(&Float64Array::EntryInfo::NewInstance,
                    DeferredTypeHandler<InitializeFloat64ArrayConstructor>::GetDefaultInstance(),
                    typedArrayConstructor);
                AddFunction(globalObject, PropertyIds::Float64Array, Float64ArrayConstructor);
            }
            else
            {
                ADD_TYPEDARRAY_CONSTRUCTOR(Int8ArrayConstructor, Int8Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Uint8ArrayConstructor, Uint8Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Uint8ClampedArrayConstructor, Uint8ClampedArray);
                ADD_TYPEDARRAY_CONSTRUCTOR(Int16ArrayConstructor, Int16Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Uint16ArrayConstructor, Uint16Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Int32ArrayConstructor, Int32Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Uint32ArrayConstructor, Uint32Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Float32ArrayConstructor, Float32Array);
                ADD_TYPEDARRAY_CONSTRUCTOR(Float64ArrayConstructor, Float64Array);
            }
        }
        else
        {
            arrayBufferConstructor = nullptr;
            Int8ArrayConstructor = nullptr;
            Uint8ArrayConstructor = nullptr;
            Uint8ClampedArrayConstructor = nullptr;
            Int16ArrayConstructor = nullptr;
            Uint16ArrayConstructor = nullptr;
            Int32ArrayConstructor = nullptr;
            Uint32ArrayConstructor = nullptr;
            Float32ArrayConstructor = nullptr;
            Float64ArrayConstructor = nullptr;
            dataViewConstructor = nullptr;
        }


        JSONObject = DynamicObject::New(recycler,
            DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeJSONObject>::GetDefaultInstance()));
        AddMember(globalObject, PropertyIds::JSON, JSONObject);

#ifdef ENABLE_INTL_OBJECT
        if (scriptContext->GetConfig()->IsIntlEnabled())
        {
            IntlObject = DynamicObject::New(recycler,
                DynamicType::New(scriptContext, TypeIds_Object, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeIntlObject>::GetDefaultInstance()));
            AddMember(globalObject, PropertyIds::Intl, IntlObject);
        }
        else
        {
            IntlObject = nullptr;
        }
#endif

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
        engineInterfaceObject = EngineInterfaceObject::New(recycler,
            DynamicType::New(scriptContext, TypeIds_EngineInterfaceObject, objectPrototype, nullptr,
            DeferredTypeHandler<InitializeEngineInterfaceObject>::GetDefaultInstance()));

#ifdef ENABLE_INTL_OBJECT
        IntlEngineInterfaceExtensionObject* intlExtension = RecyclerNew(recycler, IntlEngineInterfaceExtensionObject, scriptContext);
        engineInterfaceObject->SetEngineExtension(EngineInterfaceExtensionKind_Intl, intlExtension);
#endif
#endif

        mapConstructor = nullptr;
        setConstructor = nullptr;
        weakMapConstructor = nullptr;
        weakSetConstructor = nullptr;
        generatorFunctionConstructor = nullptr;

        if (scriptContext->GetConfig()->IsES6MapEnabled())
        {
            mapConstructor = CreateBuiltinConstructor(&JavascriptMap::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeMapConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::Map, mapConstructor);
        }

        if (scriptContext->GetConfig()->IsES6SetEnabled())
        {
            setConstructor = CreateBuiltinConstructor(&JavascriptSet::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeSetConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::Set, setConstructor);
        }

        if (scriptContext->GetConfig()->IsES6WeakMapEnabled())
        {
            weakMapConstructor = CreateBuiltinConstructor(&JavascriptWeakMap::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeWeakMapConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::WeakMap, weakMapConstructor);
        }

        if (scriptContext->GetConfig()->IsES6WeakSetEnabled())
        {
            weakSetConstructor = CreateBuiltinConstructor(&JavascriptWeakSet::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeWeakSetConstructor>::GetDefaultInstance());
            AddFunction(globalObject, PropertyIds::WeakSet, weakSetConstructor);
        }

        if (scriptContext->GetConfig()->IsES6GeneratorsEnabled())
        {
            generatorFunctionConstructor = CreateBuiltinConstructor(&JavascriptGeneratorFunction::EntryInfo::NewInstance,
                DeferredTypeHandler<InitializeGeneratorFunctionConstructor>::GetDefaultInstance(),
                functionConstructor);
            // GeneratorFunction is not a global property by ES6 spec so don't add it to the global object
        }

        errorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewErrorInstance,
            DeferredTypeHandler<InitializeErrorConstructor>::GetDefaultInstance());
        AddFunction(globalObject, PropertyIds::Error, errorConstructor);

        RuntimeFunction* nativeErrorPrototype = nullptr;
        if (scriptContext->GetConfig()->IsES6PrototypeChain())
        {
            nativeErrorPrototype = errorConstructor;
        }

        evalErrorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewEvalErrorInstance,
            DeferredTypeHandler<InitializeEvalErrorConstructor>::GetDefaultInstance(),
            nativeErrorPrototype);
        AddFunction(globalObject, PropertyIds::EvalError, evalErrorConstructor);

        rangeErrorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewRangeErrorInstance,
            DeferredTypeHandler<InitializeRangeErrorConstructor>::GetDefaultInstance(),
            nativeErrorPrototype);
        AddFunction(globalObject, PropertyIds::RangeError, rangeErrorConstructor);

        referenceErrorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewReferenceErrorInstance,
            DeferredTypeHandler<InitializeReferenceErrorConstructor>::GetDefaultInstance(),
            nativeErrorPrototype);
        AddFunction(globalObject, PropertyIds::ReferenceError, referenceErrorConstructor);

        syntaxErrorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewSyntaxErrorInstance,
            DeferredTypeHandler<InitializeSyntaxErrorConstructor>::GetDefaultInstance(),
            nativeErrorPrototype);
        AddFunction(globalObject, PropertyIds::SyntaxError, syntaxErrorConstructor);

        typeErrorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewTypeErrorInstance,
            DeferredTypeHandler<InitializeTypeErrorConstructor>::GetDefaultInstance(),
            nativeErrorPrototype);
        AddFunction(globalObject, PropertyIds::TypeError, typeErrorConstructor);

        uriErrorConstructor = CreateBuiltinConstructor(&JavascriptError::EntryInfo::NewURIErrorInstance,
            DeferredTypeHandler<InitializeURIErrorConstructor>::GetDefaultInstance(),
            nativeErrorPrototype);
        AddFunction(globalObject, PropertyIds::URIError, uriErrorConstructor);

        nullEnumerator = RecyclerNew(this->recycler, NullEnumerator, scriptContext);
    }

    void JavascriptLibrary::EnsureDebugObject(DynamicObject* newDebugObject)
    {
        Assert(!debugObject);

        if (!debugObject)
        {
            this->debugObject = newDebugObject;
            AddMember(globalObject, PropertyIds::Debug, debugObject);
        }
    }

    void JavascriptLibrary::SetDebugObjectNonUserAccessor(FunctionInfo *funcGetter, FunctionInfo *funcSetter)
    {
        Assert(funcGetter);
        Assert(funcSetter);

        debugObjectNonUserGetterFunction = CreateNonProfiledFunction(funcGetter);
        debugObjectNonUserGetterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

        debugObjectNonUserSetterFunction = CreateNonProfiledFunction(funcSetter);
        debugObjectNonUserSetterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone, nullptr);
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void JavascriptLibrary::SetDebugObjectFaultInjectionCookieGetterAccessor(FunctionInfo *funcGetter, FunctionInfo *funcSetter)
    {
        Assert(funcGetter);
        Assert(funcSetter);

        debugObjectFaultInjectionCookieGetterFunction = CreateNonProfiledFunction(funcGetter);
        debugObjectFaultInjectionCookieGetterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);

        debugObjectFaultInjectionCookieSetterFunction = CreateNonProfiledFunction(funcSetter);
        debugObjectFaultInjectionCookieSetterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone, nullptr);
    }
#endif

    void JavascriptLibrary::SetDebugObjectDebugModeAccessor(FunctionInfo *funcGetter)
    {
        Assert(funcGetter);

        debugObjectDebugModeGetterFunction = CreateNonProfiledFunction(funcGetter);
        debugObjectDebugModeGetterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);
    }

    void JavascriptLibrary::InitializeArrayConstructor(DynamicObject* arrayConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(arrayConstructor, mode, 6);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterArray
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = arrayConstructor->GetScriptContext();
        JavascriptLibrary* library = arrayConstructor->GetLibrary();
        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();

        library->AddMember(arrayConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(arrayConstructor, PropertyIds::prototype, scriptContext->GetLibrary()->arrayPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(arrayConstructor, PropertyIds::_symbolSpecies, &JavascriptArray::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(arrayConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Array), PropertyConfigurable);
        }
        builtinFuncs[BuiltinFunction::Array_IsArray] = library->AddFunctionToLibraryObject(arrayConstructor, PropertyIds::isArray, &JavascriptArray::EntryInfo::IsArray, 1);

        // Array.from and Array.of are implemented as part of the ES6 TypedArray feature
        if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled())
        {
            library->AddFunctionToLibraryObject(arrayConstructor, PropertyIds::from, &JavascriptArray::EntryInfo::From, 1);
            library->AddFunctionToLibraryObject(arrayConstructor, PropertyIds::of, &JavascriptArray::EntryInfo::Of, 0);
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        arrayConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeArrayPrototype(DynamicObject* arrayPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(arrayPrototype, mode, 24);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterArray
        // so that the update is in sync with profiler

        ScriptContext* scriptContext = arrayPrototype->GetScriptContext();
        JavascriptLibrary* library = arrayPrototype->GetLibrary();
        library->AddMember(arrayPrototype, PropertyIds::constructor, library->arrayConstructor);

        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();

        builtinFuncs[BuiltinFunction::Array_Push]               = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::push,            &JavascriptArray::EntryInfo::Push,              1);
        builtinFuncs[BuiltinFunction::Array_Concat]             = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::concat,          &JavascriptArray::EntryInfo::Concat,            1);
        builtinFuncs[BuiltinFunction::Array_Join]               = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::join,            &JavascriptArray::EntryInfo::Join,              1);
        builtinFuncs[BuiltinFunction::Array_Pop]                = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::pop,             &JavascriptArray::EntryInfo::Pop,               0);
        builtinFuncs[BuiltinFunction::Array_Reverse]            = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::reverse,         &JavascriptArray::EntryInfo::Reverse,           0);
        builtinFuncs[BuiltinFunction::Array_Shift]              = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::shift,           &JavascriptArray::EntryInfo::Shift,             0);
        builtinFuncs[BuiltinFunction::Array_Slice]              = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::slice,           &JavascriptArray::EntryInfo::Slice,             2);
        /* No inlining                Array_Sort               */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::sort,            &JavascriptArray::EntryInfo::Sort,              1);
        builtinFuncs[BuiltinFunction::Array_Splice]             = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::splice,          &JavascriptArray::EntryInfo::Splice,            2);

        // The toString and toLocaleString properties are shared between Array.prototype and %TypedArray%.prototype.
        // Whichever prototype is initialized first will create the functions, the other should just load the existing function objects.
        if (library->arrayPrototypeToStringFunction == nullptr)
        {
            Assert(library->arrayPrototypeToLocaleStringFunction == nullptr);

            library->arrayPrototypeToLocaleStringFunction = /* No inlining       Array_ToLocaleString */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::toLocaleString, &JavascriptArray::EntryInfo::ToLocaleString, 0);
            library->arrayPrototypeToStringFunction =       /* No inlining       Array_ToString       */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::toString, &JavascriptArray::EntryInfo::ToString, 0);
        }
        else
        {
            Assert(library->arrayPrototypeToLocaleStringFunction);

            library->AddMember(arrayPrototype, PropertyIds::toLocaleString, library->arrayPrototypeToLocaleStringFunction);
            library->AddMember(arrayPrototype, PropertyIds::toString, library->arrayPrototypeToStringFunction);
        }

        builtinFuncs[BuiltinFunction::Array_Unshift]            = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::unshift,         &JavascriptArray::EntryInfo::Unshift,           1);


        builtinFuncs[BuiltinFunction::Array_IndexOf]        = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::indexOf,         &JavascriptArray::EntryInfo::IndexOf,           1);
        /* No inlining                Array_Every          */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::every,           &JavascriptArray::EntryInfo::Every,             1);
        /* No inlining                Array_Filter         */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::filter,          &JavascriptArray::EntryInfo::Filter,            1);
        /* No inlining                Array_ForEach        */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::forEach,         &JavascriptArray::EntryInfo::ForEach,           1);
        builtinFuncs[BuiltinFunction::Array_LastIndexOf]    = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::lastIndexOf,     &JavascriptArray::EntryInfo::LastIndexOf,       1);
        /* No inlining                Array_Map            */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::map,             &JavascriptArray::EntryInfo::Map,               1);
        /* No inlining                Array_Reduce         */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::reduce,          &JavascriptArray::EntryInfo::Reduce,            1);
        /* No inlining                Array_ReduceRight    */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::reduceRight,     &JavascriptArray::EntryInfo::ReduceRight,       1);
        /* No inlining                Array_Some           */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::some,            &JavascriptArray::EntryInfo::Some,              1);

        if (scriptContext->GetConfig()->IsES6StringExtensionsEnabled()) // This is not a typo, Array.prototype.find and .findIndex are part of the ES6 Improved String APIs feature
        {
            /* No inlining            Array_Find           */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::find,            &JavascriptArray::EntryInfo::Find,              1);
            /* No inlining            Array_FindIndex      */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::findIndex,       &JavascriptArray::EntryInfo::FindIndex,         1);
        }

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            /* No inlining            Array_Entries        */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::entries, &JavascriptArray::EntryInfo::Entries, 0);
            /* No inlining            Array_Keys           */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::keys, &JavascriptArray::EntryInfo::Keys, 0);

            JavascriptFunction *values = library->arrayPrototypeValuesFunction ? library->arrayPrototypeValuesFunction : /* No inlining Array_Values     */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::values, &JavascriptArray::EntryInfo::Values, 0);
            /* No inlining            Array_SymbolIterator */ library->AddMember(arrayPrototype, PropertyIds::_symbolIterator, values);
        }

        if (scriptContext->GetConfig()->IsES6UnscopablesEnabled())
        {
            DynamicType* dynamicType = DynamicType::New(scriptContext, TypeIds_Object, library->nullValue, nullptr, NullTypeHandler<false>::GetDefaultInstance(), false);
            DynamicObject* unscopables_blacklist = DynamicObject::New(library->GetRecycler(), dynamicType);
            unscopables_blacklist->SetProperty(PropertyIds::find, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            unscopables_blacklist->SetProperty(PropertyIds::findIndex, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            unscopables_blacklist->SetProperty(PropertyIds::fill, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            unscopables_blacklist->SetProperty(PropertyIds::copyWithin, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            unscopables_blacklist->SetProperty(PropertyIds::entries, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            unscopables_blacklist->SetProperty(PropertyIds::keys, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            unscopables_blacklist->SetProperty(PropertyIds::values, JavascriptBoolean::ToVar(true, scriptContext), PropertyOperation_None, nullptr);
            library->AddMember(arrayPrototype, PropertyIds::_symbolUnscopables, unscopables_blacklist, PropertyConfigurable);
        }

        if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled()) // This is not a typo, Array.prototype.fill and .copyWithin are part of the ES6 TypedArray feature
        {
            /* No inlining            Array_Fill           */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::fill, &JavascriptArray::EntryInfo::Fill, 1);
            /* No inlining            Array_CopyWithin     */ library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::copyWithin, &JavascriptArray::EntryInfo::CopyWithin, 2);
        }

        if (scriptContext->GetConfig()->IsES7BuiltinsEnabled())
        {
            builtinFuncs[BuiltinFunction::Array_Includes] = library->AddFunctionToLibraryObject(arrayPrototype, PropertyIds::includes, &JavascriptArray::EntryInfo::Includes, 1);
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        arrayPrototype->SetHasNoEnumerableProperties(true);
    }

    void  JavascriptLibrary::InitializeArrayBufferConstructor(DynamicObject* arrayBufferConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(arrayBufferConstructor, mode, 4);

        ScriptContext* scriptContext = arrayBufferConstructor->GetScriptContext();
        JavascriptLibrary* library = arrayBufferConstructor->GetLibrary();
        library->AddMember(arrayBufferConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(arrayBufferConstructor, PropertyIds::prototype, scriptContext->GetLibrary()->arrayBufferPrototype, PropertyNone);

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(arrayBufferConstructor, PropertyIds::_symbolSpecies, &ArrayBuffer::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(arrayBufferConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::ArrayBuffer), PropertyConfigurable);
        }
        library->AddFunctionToLibraryObject(arrayBufferConstructor, PropertyIds::isView, &ArrayBuffer::EntryInfo::IsView, 1);

        if (scriptContext->GetConfig()->IsArrayBufferTransferEnabled())
        {
            library->AddFunctionToLibraryObject(arrayBufferConstructor, PropertyIds::transfer, &ArrayBuffer::EntryInfo::Transfer, 2);
        }

        arrayBufferConstructor->SetHasNoEnumerableProperties(true);
    }

    void  JavascriptLibrary::InitializeArrayBufferPrototype(DynamicObject* arrayBufferPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(arrayBufferPrototype, mode, 2);

        ScriptContext* scriptContext = arrayBufferPrototype->GetScriptContext();
        JavascriptLibrary* library = arrayBufferPrototype->GetLibrary();
        library->AddMember(arrayBufferPrototype, PropertyIds::constructor, library->arrayBufferConstructor);

        library->AddFunctionToLibraryObject(arrayBufferPrototype, PropertyIds::slice, &ArrayBuffer::EntryInfo::Slice, 2);

        if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled())
        {
            library->AddAccessorsToLibraryObject(arrayBufferPrototype, PropertyIds::byteLength, &ArrayBuffer::EntryInfo::GetterByteLength, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(arrayBufferPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"ArrayBuffer"), PropertyConfigurable);
        }

        arrayBufferPrototype->SetHasNoEnumerableProperties(true);
    }

    void  JavascriptLibrary::InitializeDataViewConstructor(DynamicObject* dataViewConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(dataViewConstructor, mode, 3);

        ScriptContext* scriptContext = dataViewConstructor->GetScriptContext();
        JavascriptLibrary* library = dataViewConstructor->GetLibrary();
        library->AddMember(dataViewConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(dataViewConstructor, PropertyIds::prototype, scriptContext->GetLibrary()->dataViewPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(dataViewConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::DataView), PropertyConfigurable);
        }
        dataViewConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeDataViewPrototype(DynamicObject* dataViewPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(dataViewPrototype, mode, 2);

        ScriptContext* scriptContext = dataViewPrototype->GetScriptContext();
        JavascriptLibrary* library = dataViewPrototype->GetLibrary();
        library->AddMember(dataViewPrototype, PropertyIds::constructor, library->dataViewConstructor);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setInt8, &DataView::EntryInfo::SetInt8, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setUint8, &DataView::EntryInfo::SetUint8, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setInt16, &DataView::EntryInfo::SetInt16, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setUint16, &DataView::EntryInfo::SetUint16, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setInt32, &DataView::EntryInfo::SetInt32, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setUint32, &DataView::EntryInfo::SetUint32, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setFloat32, &DataView::EntryInfo::SetFloat32, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::setFloat64, &DataView::EntryInfo::SetFloat64, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getInt8, &DataView::EntryInfo::GetInt8, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getUint8, &DataView::EntryInfo::GetUint8, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getInt16, &DataView::EntryInfo::GetInt16, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getUint16, &DataView::EntryInfo::GetUint16, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getInt32, &DataView::EntryInfo::GetInt32, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getUint32, &DataView::EntryInfo::GetUint32, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getFloat32, &DataView::EntryInfo::GetFloat32, 1);
        library->AddFunctionToLibraryObject(dataViewPrototype, PropertyIds::getFloat64, &DataView::EntryInfo::GetFloat64, 1);

        library->AddAccessorsToLibraryObject(dataViewPrototype, PropertyIds::buffer, &DataView::EntryInfo::GetterBuffer, nullptr);
        library->AddAccessorsToLibraryObject(dataViewPrototype, PropertyIds::byteLength, &DataView::EntryInfo::GetterByteLength, nullptr);
        library->AddAccessorsToLibraryObject(dataViewPrototype, PropertyIds::byteOffset, &DataView::EntryInfo::GetterByteOffset, nullptr);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(dataViewPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"DataView"), PropertyConfigurable);
        }

        dataViewPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeTypedArrayConstructor(DynamicObject* typedArrayConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(typedArrayConstructor, mode, 5);

        ScriptContext* scriptContext = typedArrayConstructor->GetScriptContext();
        JavascriptLibrary* library = typedArrayConstructor->GetLibrary();

        Assert(scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled());

        library->AddMember(typedArrayConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(3), PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(typedArrayConstructor, PropertyIds::name, library->CreateStringFromCppLiteral(L"TypedArray"), PropertyConfigurable);
        }
        library->AddMember(typedArrayConstructor, PropertyIds::prototype, library->typedArrayPrototype, PropertyNone);

        library->AddFunctionToLibraryObject(typedArrayConstructor, PropertyIds::from, &TypedArrayBase::EntryInfo::From, 1);
        library->AddFunctionToLibraryObject(typedArrayConstructor, PropertyIds::of, &TypedArrayBase::EntryInfo::Of, 0);
        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(typedArrayConstructor, PropertyIds::_symbolSpecies, &TypedArrayBase::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        typedArrayConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeTypedArrayPrototype(DynamicObject* typedarrayPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(typedarrayPrototype, mode, 31);

        ScriptContext* scriptContext = typedarrayPrototype->GetScriptContext();
        JavascriptLibrary* library = typedarrayPrototype->GetLibrary();

        Assert(scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled());

        library->AddMember(typedarrayPrototype, PropertyIds::constructor, library->typedArrayConstructor);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::set, &TypedArrayBase::EntryInfo::Set, 2);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::subarray, &TypedArrayBase::EntryInfo::Subarray, 2);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::copyWithin, &TypedArrayBase::EntryInfo::CopyWithin, 2);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::every, &TypedArrayBase::EntryInfo::Every, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::fill, &TypedArrayBase::EntryInfo::Fill, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::filter, &TypedArrayBase::EntryInfo::Filter, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::find, &TypedArrayBase::EntryInfo::Find, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::findIndex, &TypedArrayBase::EntryInfo::FindIndex, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::forEach, &TypedArrayBase::EntryInfo::ForEach, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::indexOf, &TypedArrayBase::EntryInfo::IndexOf, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::join, &TypedArrayBase::EntryInfo::Join, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::lastIndexOf, &TypedArrayBase::EntryInfo::LastIndexOf, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::map, &TypedArrayBase::EntryInfo::Map, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::reduce, &TypedArrayBase::EntryInfo::Reduce, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::reduceRight, &TypedArrayBase::EntryInfo::ReduceRight, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::reverse, &TypedArrayBase::EntryInfo::Reverse, 0);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::slice, &TypedArrayBase::EntryInfo::Slice, 2);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::some, &TypedArrayBase::EntryInfo::Some, 1);
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::sort, &TypedArrayBase::EntryInfo::Sort, 1);
        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::entries, &TypedArrayBase::EntryInfo::Entries, 0);
            library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::keys, &TypedArrayBase::EntryInfo::Keys, 0);
            JavascriptFunction* valuesFunc = library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::values, &TypedArrayBase::EntryInfo::Values, 0);
            library->AddMember(typedarrayPrototype, PropertyIds::_symbolIterator, valuesFunc);
        }

        if (scriptContext->GetConfig()->IsES7BuiltinsEnabled())
        {
            library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::includes, &TypedArrayBase::EntryInfo::Includes, 1);
        }


        library->AddAccessorsToLibraryObject(typedarrayPrototype, PropertyIds::buffer, &TypedArrayBase::EntryInfo::GetterBuffer, nullptr);
        library->AddAccessorsToLibraryObject(typedarrayPrototype, PropertyIds::byteLength, &TypedArrayBase::EntryInfo::GetterByteLength, nullptr);
        library->AddAccessorsToLibraryObject(typedarrayPrototype, PropertyIds::byteOffset, &TypedArrayBase::EntryInfo::GetterByteOffset, nullptr);
        library->AddAccessorsToLibraryObject(typedarrayPrototype, PropertyIds::length, &TypedArrayBase::EntryInfo::GetterLength, nullptr);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddAccessorsToLibraryObjectWithName(typedarrayPrototype, PropertyIds::_symbolToStringTag,
                PropertyIds::_RuntimeFunctionNameId_toStringTag, &TypedArrayBase::EntryInfo::GetterSymbolToStringTag, nullptr);
        }
        // The toString and toLocaleString properties are shared between Array.prototype and %TypedArray%.prototype.
        // Whichever prototype is initialized first will create the functions, the other should just load the existing function objects.
        if (library->arrayPrototypeToStringFunction == nullptr)
        {
            Assert(library->arrayPrototypeToLocaleStringFunction == nullptr);

            library->arrayPrototypeToLocaleStringFunction = library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::toLocaleString, &JavascriptArray::EntryInfo::ToLocaleString, 0);
            library->arrayPrototypeToStringFunction = library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::toString, &JavascriptArray::EntryInfo::ToString, 0);
        }
        else
        {
            Assert(library->arrayPrototypeToLocaleStringFunction);

            library->AddMember(typedarrayPrototype, PropertyIds::toLocaleString, library->arrayPrototypeToLocaleStringFunction);
            library->AddMember(typedarrayPrototype, PropertyIds::toString, library->arrayPrototypeToStringFunction);
        }

        typedarrayPrototype->SetHasNoEnumerableProperties(true);
    }

#define INIT_TYPEDARRAY_CONSTRUCTOR(typedArray, typedarrayPrototype, TypeName) \
    void  JavascriptLibrary::Initialize##typedArray##Constructor(DynamicObject* typedArrayConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode) \
    { \
        typeHandler->Convert(typedArrayConstructor, mode, 4); \
        ScriptContext* scriptContext = typedArrayConstructor->GetScriptContext(); \
        JavascriptLibrary* library = typedArrayConstructor->GetLibrary(); \
        if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled()) \
        { \
            library->AddMember(typedArrayConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(3), PropertyNone); \
            if (scriptContext->GetConfig()->IsES6FunctionNameEnabled()) \
            { \
                library->AddMember(typedArrayConstructor, PropertyIds::name, library->CreateStringFromCppLiteral(L#typedArray), PropertyConfigurable); \
            } \
        } \
        else \
        { \
            library->AddMember(typedArrayConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone); \
        } \
        library->AddMember(typedArrayConstructor, PropertyIds::BYTES_PER_ELEMENT, TaggedInt::ToVarUnchecked(sizeof(TypeName)), PropertyNone); \
        library->AddMember(typedArrayConstructor, PropertyIds::prototype, scriptContext->GetLibrary()->##typedarrayPrototype##, PropertyNone); \
        typedArrayConstructor->SetHasNoEnumerableProperties(true); \
    } \

    INIT_TYPEDARRAY_CONSTRUCTOR(Int8Array, Int8ArrayPrototype, int8);
    INIT_TYPEDARRAY_CONSTRUCTOR(Uint8Array, Uint8ArrayPrototype, uint8);
    INIT_TYPEDARRAY_CONSTRUCTOR(Uint8ClampedArray, Uint8ClampedArrayPrototype, uint8);
    INIT_TYPEDARRAY_CONSTRUCTOR(Int16Array, Int16ArrayPrototype, int16);
    INIT_TYPEDARRAY_CONSTRUCTOR(Uint16Array, Uint16ArrayPrototype, uint16);
    INIT_TYPEDARRAY_CONSTRUCTOR(Int32Array, Int32ArrayPrototype, int32);
    INIT_TYPEDARRAY_CONSTRUCTOR(Uint32Array, Uint32ArrayPrototype, uint32);
    INIT_TYPEDARRAY_CONSTRUCTOR(Float32Array, Float32ArrayPrototype, float);
    INIT_TYPEDARRAY_CONSTRUCTOR(Float64Array, Float64ArrayPrototype, double);

#define INIT_TYPEDARRAY_PROTOTYPE(typedArray, typedarrayPrototype, TypeName) \
    void JavascriptLibrary::Initialize##typedarrayPrototype##(DynamicObject* typedarrayPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode) \
    { \
        typeHandler->Convert(typedarrayPrototype, mode, 2); \
        ScriptContext* scriptContext = typedarrayPrototype->GetScriptContext(); \
        JavascriptLibrary* library = typedarrayPrototype->GetLibrary(); \
        library->AddMember(typedarrayPrototype, PropertyIds::constructor, library->##typedArray##Constructor); \
        if (scriptContext->GetConfig()->IsES6TypedArrayExtensionsEnabled()) \
        { \
            library->AddMember(typedarrayPrototype, PropertyIds::BYTES_PER_ELEMENT, TaggedInt::ToVarUnchecked(sizeof(TypeName)), PropertyNone); \
        } \
        else \
        { \
            library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::set, &##typedArray##::EntryInfo::Set, 2); \
            library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::subarray, &##typedArray##::EntryInfo::Subarray, 2); \
        } \
        typedarrayPrototype->SetHasNoEnumerableProperties(true); \
    } \

    INIT_TYPEDARRAY_PROTOTYPE(Int8Array, Int8ArrayPrototype, int8);
    INIT_TYPEDARRAY_PROTOTYPE(Uint8Array, Uint8ArrayPrototype, uint8);
    INIT_TYPEDARRAY_PROTOTYPE(Uint8ClampedArray, Uint8ClampedArrayPrototype, uint8);
    INIT_TYPEDARRAY_PROTOTYPE(Int16Array, Int16ArrayPrototype, int16);
    INIT_TYPEDARRAY_PROTOTYPE(Uint16Array, Uint16ArrayPrototype, uint16);
    INIT_TYPEDARRAY_PROTOTYPE(Int32Array, Int32ArrayPrototype, int32);
    INIT_TYPEDARRAY_PROTOTYPE(Uint32Array, Uint32ArrayPrototype, uint32);
    INIT_TYPEDARRAY_PROTOTYPE(Float32Array, Float32ArrayPrototype, float);
    INIT_TYPEDARRAY_PROTOTYPE(Float64Array, Float64ArrayPrototype, double);

    // For Microsoft extension typed array, like Int64Array, BoolArray, we don't have constructor as they can only be created from the projection arguments.
    // there is no subarray method either as that's another way to create typed array.
#define INIT_MSINTERNAL_TYPEDARRAY_PROTOTYPE(typedArray, typedarrayPrototype) \
    void JavascriptLibrary::Initialize##typedarrayPrototype##(DynamicObject* typedarrayPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)  \
    {   \
        typeHandler->Convert(typedarrayPrototype, mode, 1); \
        JavascriptLibrary* library = typedarrayPrototype->GetLibrary(); \
        library->AddFunctionToLibraryObject(typedarrayPrototype, PropertyIds::set, &##typedArray##::EntryInfo::Set, 2); \
        typedarrayPrototype->SetHasNoEnumerableProperties(true); \
    }   \

    INIT_MSINTERNAL_TYPEDARRAY_PROTOTYPE(Int64Array, Int64ArrayPrototype);
    INIT_MSINTERNAL_TYPEDARRAY_PROTOTYPE(Uint64Array, Uint64ArrayPrototype);
    INIT_MSINTERNAL_TYPEDARRAY_PROTOTYPE(BoolArray, BoolArrayPrototype);
    INIT_MSINTERNAL_TYPEDARRAY_PROTOTYPE(CharArray, CharArrayPrototype);

    void JavascriptLibrary::InitializeErrorConstructor(DynamicObject* constructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(constructor, mode, 4);

        ScriptContext* scriptContext = constructor->GetScriptContext();
        JavascriptLibrary* library = constructor->GetLibrary();

        library->AddMember(constructor, PropertyIds::prototype, library->errorPrototype, PropertyNone);
        library->AddMember(constructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(constructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Error), PropertyConfigurable);
        }
        if (scriptContext->GetConfig()->IsErrorStackTraceEnabled())
        {
            library->AddMember(constructor, PropertyIds::stackTraceLimit, JavascriptNumber::ToVar(JavascriptExceptionOperators::DefaultStackTraceLimit, scriptContext), PropertyConfigurable | PropertyWritable | PropertyEnumerable);
        }

        constructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeErrorPrototype(DynamicObject* prototype, DeferredTypeHandlerBase* typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(prototype, mode, 4);

        JavascriptLibrary* library = prototype->GetLibrary();

        library->AddMember(prototype, PropertyIds::constructor, library->errorConstructor);

        bool hasNoEnumerableProperties = true;
        PropertyAttributes prototypeNameMessageAttributes = PropertyConfigurable | PropertyWritable;

        library->AddMember(prototype, PropertyIds::name, library->CreateStringFromCppLiteral(L"Error"), prototypeNameMessageAttributes);
        library->AddMember(prototype, PropertyIds::message, library->GetEmptyString(), prototypeNameMessageAttributes);
        library->AddFunctionToLibraryObject(prototype, PropertyIds::toString, &JavascriptError::EntryInfo::ToString, 0);

        prototype->SetHasNoEnumerableProperties(hasNoEnumerableProperties);
    }

#define INIT_ERROR_CONSTRUCTOR(error) \
    void JavascriptLibrary::Initialize##error##Constructor(DynamicObject* constructor, DeferredTypeHandlerBase* typeHandler, DeferredInitializeMode mode) \
    { \
        typeHandler->Convert(constructor, mode, 3); \
        ScriptContext* scriptContext = constructor->GetScriptContext(); \
        JavascriptLibrary* library = constructor->GetLibrary(); \
        library->AddMember(constructor, PropertyIds::prototype, library->Get##error##Prototype(), PropertyNone); \
        library->AddMember(constructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone); \
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled()) \
        { \
            PropertyAttributes prototypeNameMessageAttributes = PropertyConfigurable; \
            library->AddMember(constructor, PropertyIds::name, library->CreateStringFromCppLiteral(L#error), prototypeNameMessageAttributes); \
        } \
        constructor->SetHasNoEnumerableProperties(true); \
    } \

    INIT_ERROR_CONSTRUCTOR(EvalError);
    INIT_ERROR_CONSTRUCTOR(RangeError);
    INIT_ERROR_CONSTRUCTOR(ReferenceError);
    INIT_ERROR_CONSTRUCTOR(SyntaxError);
    INIT_ERROR_CONSTRUCTOR(TypeError);
    INIT_ERROR_CONSTRUCTOR(URIError);

#define INIT_ERROR_PROTOTYPE(error) \
    void JavascriptLibrary::Initialize##error##Prototype(DynamicObject* prototype, DeferredTypeHandlerBase* typeHandler, DeferredInitializeMode mode) \
    { \
        typeHandler->Convert(prototype, mode, 4); \
        JavascriptLibrary* library = prototype->GetLibrary(); \
        library->AddMember(prototype, PropertyIds::constructor, library->Get##error##Constructor()); \
        bool hasNoEnumerableProperties = true; \
        PropertyAttributes prototypeNameMessageAttributes = PropertyConfigurable | PropertyWritable; \
        library->AddMember(prototype, PropertyIds::name, library->CreateStringFromCppLiteral(L#error), prototypeNameMessageAttributes); \
        library->AddMember(prototype, PropertyIds::message, library->GetEmptyString(), prototypeNameMessageAttributes); \
        library->AddFunctionToLibraryObject(prototype, PropertyIds::toString, &JavascriptError::EntryInfo::ToString, 0); \
        prototype->SetHasNoEnumerableProperties(hasNoEnumerableProperties); \
    } \

    INIT_ERROR_PROTOTYPE(EvalError);
    INIT_ERROR_PROTOTYPE(RangeError);
    INIT_ERROR_PROTOTYPE(ReferenceError);
    INIT_ERROR_PROTOTYPE(SyntaxError);
    INIT_ERROR_PROTOTYPE(TypeError);
    INIT_ERROR_PROTOTYPE(URIError);

    void JavascriptLibrary::InitializeBooleanConstructor(DynamicObject* booleanConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(booleanConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterBoolean
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = booleanConstructor->GetScriptContext();
        JavascriptLibrary* library = booleanConstructor->GetLibrary();
        library->AddMember(booleanConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(booleanConstructor, PropertyIds::prototype, library->booleanPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(booleanConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Boolean), PropertyConfigurable);
        }

        booleanConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeBooleanPrototype(DynamicObject* booleanPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(booleanPrototype, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterBoolean
        // so that the update is in sync with profiler
        JavascriptLibrary* library = booleanPrototype->GetLibrary();
        ScriptContext* scriptContext = booleanPrototype->GetScriptContext();
        library->AddMember(booleanPrototype, PropertyIds::constructor, library->booleanConstructor);
        scriptContext->SetBuiltInLibraryFunction(JavascriptBoolean::EntryInfo::ValueOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(booleanPrototype, PropertyIds::valueOf, &JavascriptBoolean::EntryInfo::ValueOf, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptBoolean::EntryInfo::ToString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(booleanPrototype, PropertyIds::toString, &JavascriptBoolean::EntryInfo::ToString, 0));

        booleanPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeSymbolConstructor(DynamicObject* symbolConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(symbolConstructor, mode, 12);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSymbol
        // so that the update is in sync with profiler
        JavascriptLibrary* library = symbolConstructor->GetLibrary();
        ScriptContext* scriptContext = symbolConstructor->GetScriptContext();
        library->AddMember(symbolConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone);
        library->AddMember(symbolConstructor, PropertyIds::prototype, library->symbolPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(symbolConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Symbol), PropertyConfigurable);
        }
        if (scriptContext->GetConfig()->IsES6HasInstanceEnabled())
        {
            library->AddMember(symbolConstructor, PropertyIds::hasInstance, library->GetSymbolHasInstance(), PropertyNone);
        }
        if (scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
        {
            library->AddMember(symbolConstructor, PropertyIds::isConcatSpreadable, library->GetSymbolIsConcatSpreadable(), PropertyNone);
        }
        library->AddMember(symbolConstructor, PropertyIds::iterator, library->GetSymbolIterator(), PropertyNone);
        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddMember(symbolConstructor, PropertyIds::species, library->GetSymbolSpecies(), PropertyNone);
        }

        if (scriptContext->GetConfig()->IsES6ToPrimitiveEnabled())
        {
            library->AddMember(symbolConstructor, PropertyIds::toPrimitive, library->GetSymbolToPrimitive(), PropertyNone);
        }

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(symbolConstructor, PropertyIds::toStringTag, library->GetSymbolToStringTag(), PropertyNone);
        }
        library->AddMember(symbolConstructor, PropertyIds::unscopables, library->GetSymbolUnscopables(), PropertyNone);

        library->AddFunctionToLibraryObject(symbolConstructor, PropertyIds::for_, &JavascriptSymbol::EntryInfo::For, 1);
        library->AddFunctionToLibraryObject(symbolConstructor, PropertyIds::keyFor, &JavascriptSymbol::EntryInfo::KeyFor, 1);

        symbolConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeSymbolPrototype(DynamicObject* symbolPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(symbolPrototype, mode, 5);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSymbol
        // so that the update is in sync with profiler
        JavascriptLibrary* library = symbolPrototype->GetLibrary();
        ScriptContext* scriptContext = symbolPrototype->GetScriptContext();

        library->AddMember(symbolPrototype, PropertyIds::constructor, library->symbolConstructor);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(symbolPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Symbol"), PropertyConfigurable);
        }
        scriptContext->SetBuiltInLibraryFunction(JavascriptSymbol::EntryInfo::ValueOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(symbolPrototype, PropertyIds::valueOf, &JavascriptSymbol::EntryInfo::ValueOf, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptSymbol::EntryInfo::ToString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(symbolPrototype, PropertyIds::toString, &JavascriptSymbol::EntryInfo::ToString, 0));

        if (scriptContext->GetConfig()->IsES6ToPrimitiveEnabled())
        {
            scriptContext->SetBuiltInLibraryFunction(JavascriptSymbol::EntryInfo::SymbolToPrimitive.GetOriginalEntryPoint(),
                library->AddFunctionToLibraryObjectWithName(symbolPrototype, PropertyIds::_symbolToPrimitive, PropertyIds::_RuntimeFunctionNameId_toPrimitive,
                &JavascriptSymbol::EntryInfo::SymbolToPrimitive, 1));
            symbolPrototype->SetWritable(PropertyIds::_symbolToPrimitive, false);
        }
        symbolPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializePromiseConstructor(DynamicObject* promiseConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(promiseConstructor, mode, 8);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterPromise
        // so that the update is in sync with profiler
        JavascriptLibrary* library = promiseConstructor->GetLibrary();
        ScriptContext* scriptContext = promiseConstructor->GetScriptContext();
        library->AddMember(promiseConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(promiseConstructor, PropertyIds::prototype, library->promisePrototype, PropertyNone);

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(promiseConstructor, PropertyIds::_symbolSpecies, &JavascriptPromise::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(promiseConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Promise), PropertyConfigurable);
        }

        library->AddFunctionToLibraryObject(promiseConstructor, PropertyIds::all, &JavascriptPromise::EntryInfo::All, 1);
        library->AddFunctionToLibraryObject(promiseConstructor, PropertyIds::race, &JavascriptPromise::EntryInfo::Race, 1);
        library->AddFunctionToLibraryObject(promiseConstructor, PropertyIds::reject, &JavascriptPromise::EntryInfo::Reject, 1);
        library->AddMember(promiseConstructor, PropertyIds::resolve, library->EnsurePromiseResolveFunction(), PropertyBuiltInMethodDefaults);

        promiseConstructor->SetHasNoEnumerableProperties(true);
    }

    JavascriptFunction* JavascriptLibrary::EnsurePromiseResolveFunction()
    {
        if (promiseResolveFunction == nullptr)
        {
            promiseResolveFunction = DefaultCreateFunction(&JavascriptPromise::EntryInfo::Resolve, 1, nullptr, nullptr, PropertyIds::resolve);
        }
        return promiseResolveFunction;
    }

    void JavascriptLibrary::InitializePromisePrototype(DynamicObject* promisePrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(promisePrototype, mode, 4);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterPromise
        // so that the update is in sync with profiler
        JavascriptLibrary* library = promisePrototype->GetLibrary();
        ScriptContext* scriptContext = promisePrototype->GetScriptContext();

        library->AddMember(promisePrototype, PropertyIds::constructor, library->promiseConstructor);
        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(promisePrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Promise"), PropertyConfigurable);
        }
        scriptContext->SetBuiltInLibraryFunction(JavascriptPromise::EntryInfo::Catch.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(promisePrototype, PropertyIds::catch_, &JavascriptPromise::EntryInfo::Catch, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptPromise::EntryInfo::Then.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(promisePrototype, PropertyIds::then, &JavascriptPromise::EntryInfo::Then, 2));

        promisePrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeGeneratorFunctionConstructor(DynamicObject* generatorFunctionConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(generatorFunctionConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterGeneratorFunction
        // so that the update is in sync with profiler
        JavascriptLibrary* library = generatorFunctionConstructor->GetLibrary();
        ScriptContext* scriptContext = generatorFunctionConstructor->GetScriptContext();
        library->AddMember(generatorFunctionConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyConfigurable);
        library->AddMember(generatorFunctionConstructor, PropertyIds::prototype, library->generatorFunctionPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(generatorFunctionConstructor, PropertyIds::name, library->CreateStringFromCppLiteral(L"GeneratorFunction"), PropertyConfigurable);
        }
        generatorFunctionConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeGeneratorFunctionPrototype(DynamicObject* generatorFunctionPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(generatorFunctionPrototype, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterGeneratorFunction
        // so that the update is in sync with profiler
        JavascriptLibrary* library = generatorFunctionPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        library->AddMember(generatorFunctionPrototype, PropertyIds::constructor, library->generatorFunctionConstructor, PropertyConfigurable);
        library->AddMember(generatorFunctionPrototype, PropertyIds::prototype, library->generatorPrototype, PropertyConfigurable);
        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(generatorFunctionPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"GeneratorFunction"), PropertyConfigurable);
        }
        generatorFunctionPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeGeneratorPrototype(DynamicObject* generatorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(generatorPrototype, mode, 5);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterGenerator
        // so that the update is in sync with profiler
        JavascriptLibrary* library = generatorPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        library->AddMember(generatorPrototype, PropertyIds::constructor, library->generatorFunctionPrototype, PropertyConfigurable);
        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(generatorPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Generator"), PropertyConfigurable);
        }
        library->AddFunctionToLibraryObject(generatorPrototype, PropertyIds::next, &JavascriptGenerator::EntryInfo::Next, 1);
        library->AddFunctionToLibraryObject(generatorPrototype, PropertyIds::return_, &JavascriptGenerator::EntryInfo::Return, 1);
        library->AddFunctionToLibraryObject(generatorPrototype, PropertyIds::throw_, &JavascriptGenerator::EntryInfo::Throw, 1);

        generatorPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeProxyConstructor(DynamicObject* proxyConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(proxyConstructor, mode, 4);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSymbol
        // so that the update is in sync with profiler
        JavascriptLibrary* library = proxyConstructor->GetLibrary();
        ScriptContext* scriptContext = proxyConstructor->GetScriptContext();
        library->AddMember(proxyConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(2), PropertyConfigurable);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(proxyConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Proxy), PropertyConfigurable);
        }
        library->AddFunctionToLibraryObject(proxyConstructor, PropertyIds::revocable, &JavascriptProxy::EntryInfo::Revocable, PropertyNone);

        proxyConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeProxyPrototype(DynamicObject* proxyPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(proxyPrototype, mode, 1);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSymbol
        // so that the update is in sync with profiler
        JavascriptLibrary* library = proxyPrototype->GetLibrary();
        library->AddMember(proxyPrototype, PropertyIds::constructor, library->proxyConstructor);

        proxyPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeDateConstructor(DynamicObject* dateConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(dateConstructor, mode, 6);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterDate
        // so that the update is in sync with profiler
        JavascriptLibrary* library = dateConstructor->GetLibrary();
        ScriptContext* scriptContext = dateConstructor->GetScriptContext();
        library->AddMember(dateConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(7), PropertyNone);
        library->AddMember(dateConstructor, PropertyIds::prototype, library->datePrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(dateConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Date), PropertyConfigurable);
        }
        library->AddFunctionToLibraryObject(dateConstructor, PropertyIds::parse, &JavascriptDate::EntryInfo::Parse, 1); // should be static
        library->AddFunctionToLibraryObject(dateConstructor, PropertyIds::now, &JavascriptDate::EntryInfo::Now, 0);     // should be static
        library->AddFunctionToLibraryObject(dateConstructor, PropertyIds::UTC, &JavascriptDate::EntryInfo::UTC, 7);     // should be static

        dateConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeDatePrototype(DynamicObject* datePrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(datePrototype, mode, 51);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterDate
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = datePrototype->GetScriptContext();
        JavascriptLibrary* library = datePrototype->GetLibrary();
        library->AddMember(datePrototype, PropertyIds::constructor, library->dateConstructor);
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetDate.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getDate, &JavascriptDate::EntryInfo::GetDate, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetDay.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getDay, &JavascriptDate::EntryInfo::GetDay, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetFullYear.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getFullYear, &JavascriptDate::EntryInfo::GetFullYear, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetHours.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getHours, &JavascriptDate::EntryInfo::GetHours, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetMilliseconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getMilliseconds, &JavascriptDate::EntryInfo::GetMilliseconds, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetMinutes.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getMinutes, &JavascriptDate::EntryInfo::GetMinutes, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetMonth.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getMonth, &JavascriptDate::EntryInfo::GetMonth, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetSeconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getSeconds, &JavascriptDate::EntryInfo::GetSeconds, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetTime.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getTime, &JavascriptDate::EntryInfo::GetTime, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetTimezoneOffset.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getTimezoneOffset, &JavascriptDate::EntryInfo::GetTimezoneOffset, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCDate.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCDate, &JavascriptDate::EntryInfo::GetUTCDate, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCDay.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCDay, &JavascriptDate::EntryInfo::GetUTCDay, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCFullYear.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCFullYear, &JavascriptDate::EntryInfo::GetUTCFullYear, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCHours.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCHours, &JavascriptDate::EntryInfo::GetUTCHours, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCMilliseconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCMilliseconds, &JavascriptDate::EntryInfo::GetUTCMilliseconds, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCMinutes.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCMinutes, &JavascriptDate::EntryInfo::GetUTCMinutes, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCMonth.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCMonth, &JavascriptDate::EntryInfo::GetUTCMonth, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetUTCSeconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getUTCSeconds, &JavascriptDate::EntryInfo::GetUTCSeconds, 0));
        if (scriptContext->GetConfig()->SupportsES3Extensions())
        {
            scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetVarDate.GetOriginalEntryPoint(),
                library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getVarDate, &JavascriptDate::EntryInfo::GetVarDate, 0));
        }
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::GetYear.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::getYear, &JavascriptDate::EntryInfo::GetYear, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetDate.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setDate, &JavascriptDate::EntryInfo::SetDate, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetFullYear.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setFullYear, &JavascriptDate::EntryInfo::SetFullYear, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetHours.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setHours, &JavascriptDate::EntryInfo::SetHours, 4));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetMilliseconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setMilliseconds, &JavascriptDate::EntryInfo::SetMilliseconds, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetMinutes.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setMinutes, &JavascriptDate::EntryInfo::SetMinutes, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetMonth.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setMonth, &JavascriptDate::EntryInfo::SetMonth, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetSeconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setSeconds, &JavascriptDate::EntryInfo::SetSeconds, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetTime.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setTime, &JavascriptDate::EntryInfo::SetTime, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCDate.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCDate, &JavascriptDate::EntryInfo::SetUTCDate, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCFullYear.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCFullYear, &JavascriptDate::EntryInfo::SetUTCFullYear, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCHours.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCHours, &JavascriptDate::EntryInfo::SetUTCHours, 4));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCMilliseconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCMilliseconds, &JavascriptDate::EntryInfo::SetUTCMilliseconds, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCMinutes.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCMinutes, &JavascriptDate::EntryInfo::SetUTCMinutes, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCMonth.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCMonth, &JavascriptDate::EntryInfo::SetUTCMonth, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetUTCSeconds.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setUTCSeconds, &JavascriptDate::EntryInfo::SetUTCSeconds, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SetYear.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::setYear, &JavascriptDate::EntryInfo::SetYear, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToDateString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toDateString, &JavascriptDate::EntryInfo::ToDateString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToISOString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toISOString, &JavascriptDate::EntryInfo::ToISOString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToJSON.GetOriginalEntryPoint(),
             library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toJSON, &JavascriptDate::EntryInfo::ToJSON, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToLocaleDateString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toLocaleDateString, &JavascriptDate::EntryInfo::ToLocaleDateString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToLocaleString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toLocaleString, &JavascriptDate::EntryInfo::ToLocaleString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToLocaleTimeString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toLocaleTimeString, &JavascriptDate::EntryInfo::ToLocaleTimeString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toString, &JavascriptDate::EntryInfo::ToString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToTimeString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toTimeString, &JavascriptDate::EntryInfo::ToTimeString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ToUTCString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toUTCString, &JavascriptDate::EntryInfo::ToUTCString, 0));
        library->AddFunctionToLibraryObject(datePrototype, PropertyIds::toGMTString, &JavascriptDate::EntryInfo::ToGMTString, 0);
        scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::ValueOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(datePrototype, PropertyIds::valueOf, &JavascriptDate::EntryInfo::ValueOf, 0));

        if (scriptContext->GetConfig()->IsES6ToPrimitiveEnabled())
        {
            scriptContext->SetBuiltInLibraryFunction(JavascriptDate::EntryInfo::SymbolToPrimitive.GetOriginalEntryPoint(),
                library->AddFunctionToLibraryObjectWithName(datePrototype, PropertyIds::_symbolToPrimitive, PropertyIds::_RuntimeFunctionNameId_toPrimitive,
                &JavascriptDate::EntryInfo::SymbolToPrimitive, 1));
            datePrototype->SetWritable(PropertyIds::_symbolToPrimitive, false);
        }
        datePrototype->SetHasNoEnumerableProperties(true);
    }


    void JavascriptLibrary::InitializeFunctionConstructor(DynamicObject* functionConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(functionConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterFunction
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = functionConstructor->GetScriptContext();
        JavascriptLibrary* library = functionConstructor->GetLibrary();
        library->AddMember(functionConstructor, PropertyIds::prototype, library->functionPrototype, PropertyNone);
        library->AddMember(functionConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyConfigurable);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(functionConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Function), PropertyConfigurable);
        }
        functionConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeFunctionPrototype(DynamicObject* functionPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(functionPrototype, mode, 7);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterFunction
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = functionPrototype->GetScriptContext();
        JavascriptLibrary* library = functionPrototype->GetLibrary();
        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();

        library->AddMember(functionPrototype, PropertyIds::constructor, library->functionConstructor);
        library->AddMember(functionPrototype, PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyConfigurable);

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(functionPrototype, PropertyIds::name, LiteralString::CreateEmptyString(scriptContext->GetLibrary()->GetStringTypeStatic()), PropertyConfigurable);
        }

        JavascriptFunction *func = library->AddFunctionToLibraryObject(functionPrototype, PropertyIds::apply, &JavascriptFunction::EntryInfo::Apply, 2);
        builtinFuncs[BuiltinFunction::Function_Apply] = func;

        library->AddFunctionToLibraryObject(functionPrototype, PropertyIds::bind, &JavascriptFunction::EntryInfo::Bind, 1);
        func = library->AddFunctionToLibraryObject(functionPrototype, PropertyIds::call, &JavascriptFunction::EntryInfo::Call, 1);
        builtinFuncs[BuiltinFunction::Function_Call] = func;
        library->AddFunctionToLibraryObject(functionPrototype, PropertyIds::toString, &JavascriptFunction::EntryInfo::ToString, 0);

        if (scriptContext->GetConfig()->IsES6HasInstanceEnabled())
        {
            scriptContext->SetBuiltInLibraryFunction(JavascriptFunction::EntryInfo::SymbolHasInstance.GetOriginalEntryPoint(),
                                                     library->AddFunctionToLibraryObjectWithName(functionPrototype, PropertyIds::_symbolHasInstance, PropertyIds::_RuntimeFunctionNameId_hasInstance,
                                                                                                 &JavascriptFunction::EntryInfo::SymbolHasInstance, 1));
            functionPrototype->SetWritable(PropertyIds::_symbolHasInstance, false);
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        functionPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeComplexThings()
    {
        emptyRegexPattern = RegexHelper::CompileDynamic(scriptContext, L"", 0, L"", 0, false);

        Recycler *const recycler = GetRecycler();

        const ScriptConfiguration *scriptConfig = scriptContext->GetConfig();

        // Creating the regex prototype object requires compiling an empty regex, which may require error types to be
        // initialized first (such as when a stack probe fails). So, the regex prototype and other things that depend on it are
        // initialized here, which will be after the dependency types are initialized.
        //
        // In ES6, RegExp.prototype is not a RegExp object itself so we do not need to wait and create an empty RegExp.
        // Instead, we just create an ordinary object prototype for RegExp.prototype in InitializePrototypes.
        if (!scriptConfig->IsES6PrototypeChain() && regexPrototype == nullptr)
        {
            regexPrototype = RecyclerNew(recycler, JavascriptRegExp, emptyRegexPattern,
                DynamicType::New(scriptContext, TypeIds_RegEx, objectPrototype, nullptr,
                DeferredTypeHandler<InitializeRegexPrototype>::GetDefaultInstance()));
        }

        SimplePathTypeHandler *typeHandler =
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true);
        // See JavascriptRegExp::IsWritable for property writability
        if (!scriptConfig->IsES6RegExPrototypePropertiesEnabled())
        {
            typeHandler->ClearHasOnlyWritableDataProperties();
        }

        regexType = DynamicType::New(scriptContext, TypeIds_RegEx, regexPrototype, nullptr, typeHandler, true, true);
    }

    void JavascriptLibrary::InitializeMathObject(DynamicObject* mathObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(mathObject, mode, 42);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterMath
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = mathObject->GetScriptContext();
        JavascriptLibrary* library = mathObject->GetLibrary();

        library->AddMember(mathObject, PropertyIds::E,       JavascriptNumber::New(Math::E,       scriptContext), PropertyNone);
        library->AddMember(mathObject, PropertyIds::LN10,    JavascriptNumber::New(Math::LN10,    scriptContext), PropertyNone);
        library->AddMember(mathObject, PropertyIds::LN2,     JavascriptNumber::New(Math::LN2,     scriptContext), PropertyNone);
        library->AddMember(mathObject, PropertyIds::LOG2E,   JavascriptNumber::New(Math::LOG2E,   scriptContext), PropertyNone);
        library->AddMember(mathObject, PropertyIds::LOG10E,  JavascriptNumber::New(Math::LOG10E,  scriptContext), PropertyNone);
        library->AddMember(mathObject, PropertyIds::PI,      library->pi,                                         PropertyNone);
        library->AddMember(mathObject, PropertyIds::SQRT1_2, JavascriptNumber::New(Math::SQRT1_2, scriptContext), PropertyNone);
        library->AddMember(mathObject, PropertyIds::SQRT2,   JavascriptNumber::New(Math::SQRT2,   scriptContext), PropertyNone);

        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();

        builtinFuncs[BuiltinFunction::Math_Abs]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::abs,    &Math::EntryInfo::Abs,    1);
        builtinFuncs[BuiltinFunction::Math_Acos]   = library->AddFunctionToLibraryObject(mathObject, PropertyIds::acos,   &Math::EntryInfo::Acos,   1);
        builtinFuncs[BuiltinFunction::Math_Asin]   = library->AddFunctionToLibraryObject(mathObject, PropertyIds::asin,   &Math::EntryInfo::Asin,   1);
        builtinFuncs[BuiltinFunction::Math_Atan]   = library->AddFunctionToLibraryObject(mathObject, PropertyIds::atan,   &Math::EntryInfo::Atan,   1);
        builtinFuncs[BuiltinFunction::Math_Atan2]  = library->AddFunctionToLibraryObject(mathObject, PropertyIds::atan2,  &Math::EntryInfo::Atan2,  2);
        builtinFuncs[BuiltinFunction::Math_Cos]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::cos,    &Math::EntryInfo::Cos,    1);
        builtinFuncs[BuiltinFunction::Math_Ceil]   = library->AddFunctionToLibraryObject(mathObject, PropertyIds::ceil,   &Math::EntryInfo::Ceil,   1);
        builtinFuncs[BuiltinFunction::Math_Exp]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::exp,    &Math::EntryInfo::Exp,    1);
        builtinFuncs[BuiltinFunction::Math_Floor]  = library->AddFunctionToLibraryObject(mathObject, PropertyIds::floor,  &Math::EntryInfo::Floor,  1);
        builtinFuncs[BuiltinFunction::Math_Log]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::log,    &Math::EntryInfo::Log,    1);
        builtinFuncs[BuiltinFunction::Math_Max]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::max,    &Math::EntryInfo::Max,    2);
        builtinFuncs[BuiltinFunction::Math_Min]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::min,    &Math::EntryInfo::Min,    2);
        builtinFuncs[BuiltinFunction::Math_Pow]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::pow,    &Math::EntryInfo::Pow,    2);
        builtinFuncs[BuiltinFunction::Math_Random] = library->AddFunctionToLibraryObject(mathObject, PropertyIds::random, &Math::EntryInfo::Random, 0);
        builtinFuncs[BuiltinFunction::Math_Round]  = library->AddFunctionToLibraryObject(mathObject, PropertyIds::round,  &Math::EntryInfo::Round,  1);
        builtinFuncs[BuiltinFunction::Math_Sin]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::sin,    &Math::EntryInfo::Sin,    1);
        builtinFuncs[BuiltinFunction::Math_Sqrt]   = library->AddFunctionToLibraryObject(mathObject, PropertyIds::sqrt,   &Math::EntryInfo::Sqrt,   1);
        builtinFuncs[BuiltinFunction::Math_Tan]    = library->AddFunctionToLibraryObject(mathObject, PropertyIds::tan,    &Math::EntryInfo::Tan,    1);

        if (scriptContext->GetConfig()->IsES6MathExtensionsEnabled())
        {
            builtinFuncs[BuiltinFunction::Math_Imul] = library->AddFunctionToLibraryObject(mathObject, PropertyIds::imul, &Math::EntryInfo::Imul, 2);
            builtinFuncs[BuiltinFunction::Math_Fround] = library->AddFunctionToLibraryObject(mathObject, PropertyIds::fround, &Math::EntryInfo::Fround, 1);
            /*builtinFuncs[BuiltinFunction::Math_Log10] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::log10, &Math::EntryInfo::Log10, 1);
            /*builtinFuncs[BuiltinFunction::Math_Log2]  =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::log2,  &Math::EntryInfo::Log2,  1);
            /*builtinFuncs[BuiltinFunction::Math_Log1p] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::log1p, &Math::EntryInfo::Log1p, 1);
            /*builtinFuncs[BuiltinFunction::Math_Expm1] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::expm1, &Math::EntryInfo::Expm1, 1);
            /*builtinFuncs[BuiltinFunction::Math_Cosh]  =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::cosh,  &Math::EntryInfo::Cosh,  1);
            /*builtinFuncs[BuiltinFunction::Math_Sinh]  =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::sinh,  &Math::EntryInfo::Sinh,  1);
            /*builtinFuncs[BuiltinFunction::Math_Tanh]  =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::tanh,  &Math::EntryInfo::Tanh,  1);
            /*builtinFuncs[BuiltinFunction::Math_Acosh] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::acosh, &Math::EntryInfo::Acosh, 1);
            /*builtinFuncs[BuiltinFunction::Math_Asinh] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::asinh, &Math::EntryInfo::Asinh, 1);
            /*builtinFuncs[BuiltinFunction::Math_Atanh] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::atanh, &Math::EntryInfo::Atanh, 1);
            /*builtinFuncs[BuiltinFunction::Math_Hypot] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::hypot, &Math::EntryInfo::Hypot, 2);
            /*builtinFuncs[BuiltinFunction::Math_Trunc] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::trunc, &Math::EntryInfo::Trunc, 1);
            /*builtinFuncs[BuiltinFunction::Math_Sign]  =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::sign,  &Math::EntryInfo::Sign,  1);
            /*builtinFuncs[BuiltinFunction::Math_Cbrt]  =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::cbrt,  &Math::EntryInfo::Cbrt,  1);
            /*builtinFuncs[BuiltinFunction::Math_Clz32] =*/ library->AddFunctionToLibraryObject(mathObject, PropertyIds::clz32, &Math::EntryInfo::Clz32, 1);
        }

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(mathObject, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Math"), PropertyConfigurable);
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        mathObject->SetHasNoEnumerableProperties(true);
    }

#if ENABLE_NATIVE_CODEGEN
    // SIMD_JS
    void JavascriptLibrary::InitializeSIMDObject(DynamicObject* simdObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        // Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSIMD so that the update is in sync with profiler
        typeHandler->Convert(simdObject, mode, 2);
        JavascriptLibrary* library = simdObject->GetLibrary();

        // only functions to be inlined to be added to builtinFuncs
        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();

        // Float32x4
        JavascriptFunction* float32x4Function = library->AddFunctionToLibraryObject(simdObject, PropertyIds::Float32x4, &SIMDFloat32x4Lib::EntryInfo::Float32x4, 5, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::check, &SIMDFloat32x4Lib::EntryInfo::Check, 2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::zero, &SIMDFloat32x4Lib::EntryInfo::Zero, 1, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::splat, &SIMDFloat32x4Lib::EntryInfo::Splat, 2, PropertyNone);

        // Lane Access
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::extractLane, &SIMDFloat32x4Lib::EntryInfo::ExtractLane, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::replaceLane, &SIMDFloat32x4Lib::EntryInfo::ReplaceLane, 4, PropertyNone);


        // type conversions
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::fromFloat64x2,     &SIMDFloat32x4Lib::EntryInfo::FromFloat64x2,     2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::fromFloat64x2Bits, &SIMDFloat32x4Lib::EntryInfo::FromFloat64x2Bits, 2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::fromInt32x4,       &SIMDFloat32x4Lib::EntryInfo::FromInt32x4,       2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::fromInt32x4Bits,   &SIMDFloat32x4Lib::EntryInfo::FromInt32x4Bits,   2, PropertyNone);

        // binary ops
        builtinFuncs[BuiltinFunction::SIMD_Float32x4_Add] = library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::add, &SIMDFloat32x4Lib::EntryInfo::Add, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::sub,    &SIMDFloat32x4Lib::EntryInfo::Sub,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::mul,    &SIMDFloat32x4Lib::EntryInfo::Mul,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::div,    &SIMDFloat32x4Lib::EntryInfo::Div,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::and,    &SIMDFloat32x4Lib::EntryInfo::And,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::or,     &SIMDFloat32x4Lib::EntryInfo::Or,    3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::xor,    &SIMDFloat32x4Lib::EntryInfo::Xor,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::min,    &SIMDFloat32x4Lib::EntryInfo::Min,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::max,    &SIMDFloat32x4Lib::EntryInfo::Max,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::scale,  &SIMDFloat32x4Lib::EntryInfo::Scale, 3, PropertyNone);

        // unary ops
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::abs,            &SIMDFloat32x4Lib::EntryInfo::Abs,            2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::neg,            &SIMDFloat32x4Lib::EntryInfo::Neg,            2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::not,            &SIMDFloat32x4Lib::EntryInfo::Not,            2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::sqrt,           &SIMDFloat32x4Lib::EntryInfo::Sqrt,           2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::reciprocal,     &SIMDFloat32x4Lib::EntryInfo::Reciprocal,     2, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::reciprocalSqrt, &SIMDFloat32x4Lib::EntryInfo::ReciprocalSqrt, 2, PropertyNone);
        // compare ops
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::lessThan,           &SIMDFloat32x4Lib::EntryInfo::LessThan,          3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::lessThanOrEqual,    &SIMDFloat32x4Lib::EntryInfo::LessThanOrEqual,   3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::equal,              &SIMDFloat32x4Lib::EntryInfo::Equal,             3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::notEqual,           &SIMDFloat32x4Lib::EntryInfo::NotEqual,          3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::greaterThan,        &SIMDFloat32x4Lib::EntryInfo::GreaterThan,       3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::greaterThanOrEqual, &SIMDFloat32x4Lib::EntryInfo::GreaterThanOrEqual,3, PropertyNone);

        // others
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::swizzle,            &SIMDFloat32x4Lib::EntryInfo::Swizzle, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::shuffle,            &SIMDFloat32x4Lib::EntryInfo::Shuffle, 4, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::clamp,              &SIMDFloat32x4Lib::EntryInfo::Clamp,   4, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::select,             &SIMDFloat32x4Lib::EntryInfo::Select,  4, PropertyNone);

        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::load,  &SIMDFloat32x4Lib::EntryInfo::Load,  3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::load1, &SIMDFloat32x4Lib::EntryInfo::Load1, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::load2, &SIMDFloat32x4Lib::EntryInfo::Load2, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::load3, &SIMDFloat32x4Lib::EntryInfo::Load3, 3, PropertyNone);

        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::store,  &SIMDFloat32x4Lib::EntryInfo::Store,  4, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::store1, &SIMDFloat32x4Lib::EntryInfo::Store1, 4, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::store2, &SIMDFloat32x4Lib::EntryInfo::Store2, 4, PropertyNone);
        library->AddFunctionToLibraryObject(float32x4Function, PropertyIds::store3, &SIMDFloat32x4Lib::EntryInfo::Store3, 4, PropertyNone);
        // end Float32x4

        // Float64x2
        JavascriptFunction* float64x2Function = library->AddFunctionToLibraryObject(simdObject, PropertyIds::Float64x2, &SIMDFloat64x2Lib::EntryInfo::Float64x2, 3, PropertyNone);

        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::check, &SIMDFloat64x2Lib::EntryInfo::Check, 2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::zero, &SIMDFloat64x2Lib::EntryInfo::Zero, 1, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::splat, &SIMDFloat64x2Lib::EntryInfo::Splat, 2, PropertyNone);

        // type conversions
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::fromFloat32x4,      &SIMDFloat64x2Lib::EntryInfo::FromFloat32x4,        2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::fromFloat32x4Bits,  &SIMDFloat64x2Lib::EntryInfo::FromFloat32x4Bits,    2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::fromInt32x4,        &SIMDFloat64x2Lib::EntryInfo::FromInt32x4,          2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::fromInt32x4Bits,    &SIMDFloat64x2Lib::EntryInfo::FromInt32x4Bits,      2, PropertyNone);

        // binary ops
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::add, &SIMDFloat64x2Lib::EntryInfo::Add, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::sub, &SIMDFloat64x2Lib::EntryInfo::Sub, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::mul, &SIMDFloat64x2Lib::EntryInfo::Mul, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::div, &SIMDFloat64x2Lib::EntryInfo::Div, 3, PropertyNone);

        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::min,    &SIMDFloat64x2Lib::EntryInfo::Min,      3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::max,    &SIMDFloat64x2Lib::EntryInfo::Max,      3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::scale,  &SIMDFloat64x2Lib::EntryInfo::Scale,    3, PropertyNone);
        // unary ops
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::abs,            &SIMDFloat64x2Lib::EntryInfo::Abs,              2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::neg,            &SIMDFloat64x2Lib::EntryInfo::Neg,              2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::sqrt,           &SIMDFloat64x2Lib::EntryInfo::Sqrt,             2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::reciprocal,     &SIMDFloat64x2Lib::EntryInfo::Reciprocal,       2, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::reciprocalSqrt, &SIMDFloat64x2Lib::EntryInfo::ReciprocalSqrt,   2, PropertyNone);

        // compare ops
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::lessThan,           &SIMDFloat64x2Lib::EntryInfo::LessThan,             3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::lessThanOrEqual,    &SIMDFloat64x2Lib::EntryInfo::LessThanOrEqual,      3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::equal,              &SIMDFloat64x2Lib::EntryInfo::Equal,                3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::notEqual,           &SIMDFloat64x2Lib::EntryInfo::NotEqual,             3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::greaterThan,        &SIMDFloat64x2Lib::EntryInfo::GreaterThan,          3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::greaterThanOrEqual, &SIMDFloat64x2Lib::EntryInfo::GreaterThanOrEqual,   3, PropertyNone);

        // others
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::swizzle,    &SIMDFloat64x2Lib::EntryInfo::Swizzle,  3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::shuffle,    &SIMDFloat64x2Lib::EntryInfo::Shuffle,  4, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::clamp,      &SIMDFloat64x2Lib::EntryInfo::Clamp,    4, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::select,     &SIMDFloat64x2Lib::EntryInfo::Select,   4, PropertyNone);

        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::load,  &SIMDFloat64x2Lib::EntryInfo::Load, 3, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::load1, &SIMDFloat64x2Lib::EntryInfo::Load1, 3, PropertyNone);

        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::store,  &SIMDFloat64x2Lib::EntryInfo::Store, 4, PropertyNone);
        library->AddFunctionToLibraryObject(float64x2Function, PropertyIds::store1, &SIMDFloat64x2Lib::EntryInfo::Store1, 4, PropertyNone);

        // end Float64x2

        // Int32x4
        JavascriptFunction* int32x4Function = library->AddFunctionToLibraryObject(simdObject, PropertyIds::Int32x4, &SIMDInt32x4Lib::EntryInfo::Int32x4, 5, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::check,        &SIMDInt32x4Lib::EntryInfo::Check,      2, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::zero,         &SIMDInt32x4Lib::EntryInfo::Zero,       1, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::splat,        &SIMDInt32x4Lib::EntryInfo::Splat,      2, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::bool_,        &SIMDInt32x4Lib::EntryInfo::Bool,       5, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::withFlagX,    &SIMDInt32x4Lib::EntryInfo::WithFlagX,  3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::withFlagY,    &SIMDInt32x4Lib::EntryInfo::WithFlagY,  3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::withFlagZ,    &SIMDInt32x4Lib::EntryInfo::WithFlagZ,  3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::withFlagW,    &SIMDInt32x4Lib::EntryInfo::WithFlagW,  3, PropertyNone);

        // Lane Access
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::extractLane, &SIMDInt32x4Lib::EntryInfo::ExtractLane, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::replaceLane, &SIMDInt32x4Lib::EntryInfo::ReplaceLane, 4, PropertyNone);

        // type conversions
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::fromFloat64x2, &SIMDInt32x4Lib::EntryInfo::FromFloat64x2,         2, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::fromFloat64x2Bits, &SIMDInt32x4Lib::EntryInfo::FromFloat64x2Bits, 2, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::fromFloat32x4, &SIMDInt32x4Lib::EntryInfo::FromFloat32x4,         2, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::fromFloat32x4Bits, &SIMDInt32x4Lib::EntryInfo::FromFloat32x4Bits, 2, PropertyNone);

        // binary ops
        builtinFuncs[BuiltinFunction::SIMD_Int32x4_Add] = library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::add, &SIMDInt32x4Lib::EntryInfo::Add, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::sub, &SIMDInt32x4Lib::EntryInfo::Sub, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::mul, &SIMDInt32x4Lib::EntryInfo::Mul, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::and, &SIMDInt32x4Lib::EntryInfo::And, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::or,  &SIMDInt32x4Lib::EntryInfo::Or,  3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::xor, &SIMDInt32x4Lib::EntryInfo::Xor, 3, PropertyNone);

        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::neg, &SIMDInt32x4Lib::EntryInfo::Neg, 2, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::not, &SIMDInt32x4Lib::EntryInfo::Not, 2, PropertyNone);

        // compare ops
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::lessThan,     &SIMDInt32x4Lib::EntryInfo::LessThan,    3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::equal,        &SIMDInt32x4Lib::EntryInfo::Equal,       3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::greaterThan,  &SIMDInt32x4Lib::EntryInfo::GreaterThan, 3, PropertyNone);

        // others
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::swizzle,      &SIMDInt32x4Lib::EntryInfo::Swizzle,     3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::shuffle,      &SIMDInt32x4Lib::EntryInfo::Shuffle,     4, PropertyNone);

        // shift
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::shiftLeft,            &SIMDInt32x4Lib::EntryInfo::ShiftLeft,              3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::shiftRightLogical,    &SIMDInt32x4Lib::EntryInfo::ShiftRightLogical,      3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::shiftRightArithmetic, &SIMDInt32x4Lib::EntryInfo::ShiftRightArithmetic,   3, PropertyNone);

        // select
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::select, &SIMDInt32x4Lib::EntryInfo::Select, 4, PropertyNone);

        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::load,  &SIMDInt32x4Lib::EntryInfo::Load, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::load1, &SIMDInt32x4Lib::EntryInfo::Load1, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::load2, &SIMDInt32x4Lib::EntryInfo::Load2, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::load3, &SIMDInt32x4Lib::EntryInfo::Load3, 3, PropertyNone);

        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::store,  &SIMDInt32x4Lib::EntryInfo::Store, 4, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::store1, &SIMDInt32x4Lib::EntryInfo::Store1, 4, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::store2, &SIMDInt32x4Lib::EntryInfo::Store2, 4, PropertyNone);
        library->AddFunctionToLibraryObject(int32x4Function, PropertyIds::store3, &SIMDInt32x4Lib::EntryInfo::Store3, 4, PropertyNone);

        // end Int32x4

        // Int8x16
        JavascriptFunction* int8x16Function = library->AddFunctionToLibraryObject(simdObject, PropertyIds::Int8x16, &SIMDInt8x16Lib::EntryInfo::Int8x16, 17, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::check, &SIMDInt8x16Lib::EntryInfo::Check, 2, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::zero, &SIMDInt8x16Lib::EntryInfo::Zero, 1, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::splat, &SIMDInt8x16Lib::EntryInfo::Splat, 2, PropertyNone);

        // type conversions
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::fromFloat32x4Bits, &SIMDInt8x16Lib::EntryInfo::FromFloat32x4Bits, 2, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::fromInt32x4Bits, &SIMDInt8x16Lib::EntryInfo::FromInt32x4Bits, 2, PropertyNone);

        // binary ops
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::add, &SIMDInt8x16Lib::EntryInfo::Add, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::sub, &SIMDInt8x16Lib::EntryInfo::Sub, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::mul, &SIMDInt8x16Lib::EntryInfo::Mul, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::and, &SIMDInt8x16Lib::EntryInfo::And, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::or , &SIMDInt8x16Lib::EntryInfo::Or , 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::xor, &SIMDInt8x16Lib::EntryInfo::Xor, 3, PropertyNone);

        // unary ops
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::neg, &SIMDInt8x16Lib::EntryInfo::Neg, 2, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::not, &SIMDInt8x16Lib::EntryInfo::Not, 2, PropertyNone);

        // compare ops
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::lessThan, &SIMDInt8x16Lib::EntryInfo::LessThan, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::equal   , &SIMDInt8x16Lib::EntryInfo::Equal   , 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::greaterThan, &SIMDInt8x16Lib::EntryInfo::GreaterThan, 3, PropertyNone);

        // shift
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::shiftLeftByScalar, &SIMDInt8x16Lib::EntryInfo::ShiftLeftByScalar, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::shiftRightLogicalByScalar, &SIMDInt8x16Lib::EntryInfo::ShiftRightLogicalByScalar, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::shiftRightArithmeticByScalar, &SIMDInt8x16Lib::EntryInfo::ShiftRightArithmeticByScalar, 3, PropertyNone);

        // Lane Access
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::extractLane, &SIMDInt8x16Lib::EntryInfo::ExtractLane, 3, PropertyNone);
        library->AddFunctionToLibraryObject(int8x16Function, PropertyIds::replaceLane, &SIMDInt8x16Lib::EntryInfo::ReplaceLane, 4, PropertyNone);

        // end Int8x16
    }
#endif

    void JavascriptLibrary::InitializeReflectObject(DynamicObject* reflectObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(reflectObject, mode, 12);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterReflect
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = reflectObject->GetScriptContext();
        JavascriptLibrary* library = reflectObject->GetLibrary();
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::DefineProperty.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::defineProperty, &JavascriptReflect::EntryInfo::DefineProperty, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::DeleteProperty.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::deleteProperty, &JavascriptReflect::EntryInfo::DeleteProperty, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::Enumerate.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::enumerate, &JavascriptReflect::EntryInfo::Enumerate, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::Get.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::get, &JavascriptReflect::EntryInfo::Get, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::GetOwnPropertyDescriptor.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::getOwnPropertyDescriptor, &JavascriptReflect::EntryInfo::GetOwnPropertyDescriptor, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::GetPrototypeOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::getPrototypeOf, &JavascriptReflect::EntryInfo::GetPrototypeOf, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::Has.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::has, &JavascriptReflect::EntryInfo::Has, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::IsExtensible.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::isExtensible, &JavascriptReflect::EntryInfo::IsExtensible, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::OwnKeys.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::ownKeys, &JavascriptReflect::EntryInfo::OwnKeys, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::PreventExtensions.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::preventExtensions, &JavascriptReflect::EntryInfo::PreventExtensions, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::Set.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::set, &JavascriptReflect::EntryInfo::Set, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::SetPrototypeOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::setPrototypeOf, &JavascriptReflect::EntryInfo::SetPrototypeOf, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::Apply.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::apply, &JavascriptReflect::EntryInfo::Apply, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptReflect::EntryInfo::Construct.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(reflectObject, PropertyIds::construct, &JavascriptReflect::EntryInfo::Construct, 2));
    }

    void JavascriptLibrary::InitializeStaticValues()
    {
        constructorCacheDefaultInstance = &Js::ConstructorCache::DefaultInstance;
        absDoubleCst = Js::JavascriptNumber::AbsDoubleCst;
        uintConvertConst = Js::JavascriptNumber::UIntConvertConst;

        defaultPropertyDescriptor.SetValue(undefinedValue);
        defaultPropertyDescriptor.SetWritable(false);
        defaultPropertyDescriptor.SetGetter(defaultAccessorFunction);
        defaultPropertyDescriptor.SetSetter(defaultAccessorFunction);
        defaultPropertyDescriptor.SetEnumerable(false);
        defaultPropertyDescriptor.SetConfigurable(false);

#if !defined(_M_X64_OR_ARM64)
        vtableAddresses[VTableValue::VtableJavascriptNumber] = VirtualTableInfo<Js::JavascriptNumber>::Address;
#endif
        vtableAddresses[VTableValue::VtableDynamicObject] = VirtualTableInfo<Js::DynamicObject>::Address;
        vtableAddresses[VTableValue::VtableInvalid] = Js::ScriptContextOptimizationOverrideInfo::InvalidVtable;
        vtableAddresses[VTableValue::VtablePropertyString] = VirtualTableInfo<Js::PropertyString>::Address;
        vtableAddresses[VTableValue::VtableJavascriptBoolean] = VirtualTableInfo<Js::JavascriptBoolean>::Address;
        vtableAddresses[VTableValue::VtableSmallDynamicObjectSnapshotEnumeratorWPCache] = VirtualTableInfo<Js::DynamicObjectSnapshotEnumeratorWPCache<Js::BigPropertyIndex,true,false>>::Address;
        vtableAddresses[VTableValue::VtableJavascriptArray] = VirtualTableInfo<Js::JavascriptArray>::Address;
        vtableAddresses[VTableValue::VtableInt8Array] = VirtualTableInfo<Js::Int8Array>::Address;
        vtableAddresses[VTableValue::VtableUint8Array] = VirtualTableInfo<Js::Uint8Array>::Address;
        vtableAddresses[VTableValue::VtableUint8ClampedArray] = VirtualTableInfo<Js::Uint8ClampedArray>::Address;
        vtableAddresses[VTableValue::VtableInt16Array] = VirtualTableInfo<Js::Int16Array>::Address;
        vtableAddresses[VTableValue::VtableUint16Array] = VirtualTableInfo<Js::Uint16Array>::Address;
        vtableAddresses[VTableValue::VtableInt32Array] = VirtualTableInfo<Js::Int32Array>::Address;
        vtableAddresses[VTableValue::VtableUint32Array] = VirtualTableInfo<Js::Uint32Array>::Address;
        vtableAddresses[VTableValue::VtableFloat32Array] = VirtualTableInfo<Js::Float32Array>::Address;
        vtableAddresses[VTableValue::VtableFloat64Array] = VirtualTableInfo<Js::Float64Array>::Address;
        vtableAddresses[VTableValue::VtableInt64Array] = VirtualTableInfo<Js::Int64Array>::Address;
        vtableAddresses[VTableValue::VtableUint64Array] = VirtualTableInfo<Js::Uint64Array>::Address;

        vtableAddresses[VTableValue::VtableInt8VirtualArray] = VirtualTableInfo<Js::Int8VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableUint8VirtualArray] = VirtualTableInfo<Js::Uint8VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableUint8ClampedVirtualArray] = VirtualTableInfo<Js::Uint8ClampedVirtualArray>::Address;
        vtableAddresses[VTableValue::VtableInt16VirtualArray] = VirtualTableInfo<Js::Int16VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableUint16VirtualArray] = VirtualTableInfo<Js::Uint16VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableInt32VirtualArray] = VirtualTableInfo<Js::Int32VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableUint32VirtualArray] = VirtualTableInfo<Js::Uint32VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableFloat32VirtualArray] = VirtualTableInfo<Js::Float32VirtualArray>::Address;
        vtableAddresses[VTableValue::VtableFloat64VirtualArray] = VirtualTableInfo<Js::Float64VirtualArray>::Address;

        vtableAddresses[VTableValue::VtableBoolArray] = VirtualTableInfo<Js::BoolArray>::Address;
        vtableAddresses[VTableValue::VtableCharArray] = VirtualTableInfo<Js::CharArray>::Address;
        vtableAddresses[VTableValue::VtableNativeIntArray] = VirtualTableInfo<Js::JavascriptNativeIntArray>::Address;
#if ENABLE_COPYONACCESS_ARRAY
        vtableAddresses[VTableValue::VtableCopyOnAccessNativeIntArray] = VirtualTableInfo<Js::JavascriptCopyOnAccessNativeIntArray>::Address;
#endif
        vtableAddresses[VTableValue::VtableNativeFloatArray] = VirtualTableInfo<Js::JavascriptNativeFloatArray>::Address;
        vtableAddresses[VTableValue::VtableJavascriptNativeIntArray] = VirtualTableInfo<Js::JavascriptNativeIntArray>::Address;
        vtableAddresses[VTableValue::VtableJavascriptRegExp] = VirtualTableInfo<Js::JavascriptRegExp>::Address;
        vtableAddresses[VTableValue::VtableStackScriptFunction] = VirtualTableInfo<Js::StackScriptFunction>::Address;
        vtableAddresses[VTableValue::VtableConcatStringMulti] = VirtualTableInfo<Js::ConcatStringMulti>::Address;
        vtableAddresses[VTableValue::VtableCompoundString] = VirtualTableInfo<Js::CompoundString>::Address;

        // SIMD_JS
        vtableAddresses[VTableValue::VtableSimd128F4] = VirtualTableInfo<Js::JavascriptSIMDFloat32x4>::Address;
        vtableAddresses[VTableValue::VtableSimd128I4] = VirtualTableInfo<Js::JavascriptSIMDInt32x4>::Address;
    }

    //
    // Ensure library ready if started hybrid debugging. Call this to support hybrid debugging when engine starts debug mode.
    //
    HRESULT JavascriptLibrary::EnsureReadyIfHybridDebugging(bool isScriptEngineReady /*= true*/)
    {
        HRESULT hr = S_OK;

        this->isHybridDebugging = Js::Configuration::Global.IsHybridDebugging();

        // If just started hybrid debugging, ensure library objects ready (but only if we can run script now)
        if (this->isHybridDebugging && !this->isLibraryReadyForHybridDebugging && isScriptEngineReady)
        {
            BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(scriptContext, /*doCleanup*/false)
            {
                EnsureLibraryReadyForHybridDebugging();
            }
            END_JS_RUNTIME_CALL_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr)
        }

        return hr;
    }

    //
    // If under hybrid debugging, ensure one object ready (not using deferred type handler).
    //
    DynamicObject* JavascriptLibrary::EnsureReadyIfHybridDebugging(DynamicObject* obj)
    {
        if (IsHybridDebugging())
        {
            Assert(!obj->GetScriptContext()->GetThreadContext()->IsDisableImplicitCall());
            if (!obj->GetTypeHandler()->EnsureObjectReady(obj))
            {
                return obj;
            }
        }
        return obj;
    }

    //
    // When starting hybrid debugging, ensure all library objects ready (not using deferred type handler).
    //
    void JavascriptLibrary::EnsureLibraryReadyForHybridDebugging()
    {
        Assert(IsHybridDebugging() && !isLibraryReadyForHybridDebugging);

        // Note: List all library objects that use DeferredTypeHandler
        DynamicObject* objects[] =
        {
            this->objectPrototype,
            this->arrayPrototype,
            this->arrayBufferPrototype,
            this->dataViewPrototype,
            this->typedArrayPrototype,
            this->Int8ArrayPrototype,
            this->Uint8ArrayPrototype,
            this->Uint8ClampedArrayPrototype,
            this->Int16ArrayPrototype,
            this->Uint16ArrayPrototype,
            this->Int32ArrayPrototype,
            this->Uint32ArrayPrototype,
            this->Float32ArrayPrototype,
            this->Float64ArrayPrototype,
            this->Int64ArrayPrototype,
            this->Uint64ArrayPrototype,
            this->BoolArrayPrototype,
            this->CharArrayPrototype,
            this->booleanPrototype,
            this->datePrototype,
            this->functionPrototype,
            this->numberPrototype,
            this->stringPrototype,
            this->mapPrototype,
            this->setPrototype,
            this->weakMapPrototype,
            this->weakSetPrototype,
            this->regexPrototype,
            this->symbolPrototype,
            this->arrayIteratorPrototype,
            this->promisePrototype,
            this->javascriptEnumeratorIteratorPrototype,
            this->generatorFunctionPrototype,
            this->generatorPrototype,
            this->errorPrototype,
            this->evalErrorPrototype,
            this->rangeErrorPrototype,
            this->referenceErrorPrototype,
            this->syntaxErrorPrototype,
            this->typeErrorPrototype,
            this->uriErrorPrototype,
            this->objectConstructor,
            this->arrayConstructor,
            this->booleanConstructor,
            this->dateConstructor,
            this->functionConstructor,
            this->debugObject,
            this->mathObject,
            this->numberConstructor,
            this->stringConstructor,
            this->regexConstructor,
            this->arrayBufferConstructor,
            this->dataViewConstructor,
            this->typedArrayConstructor,
            this->Int8ArrayConstructor,
            this->Uint8ArrayConstructor,
            this->Uint8ClampedArrayConstructor,
            this->Int16ArrayConstructor,
            this->Uint16ArrayConstructor,
            this->Int32ArrayConstructor,
            this->Uint32ArrayConstructor,
            this->Float32ArrayConstructor,
            this->Float64ArrayConstructor,
            this->JSONObject,
#ifdef ENABLE_INTL_OBJECT
            this->IntlObject,
#endif
            this->mapConstructor,
            this->setConstructor,
            this->weakMapConstructor,
            this->weakSetConstructor,
            this->symbolConstructor,
            this->promiseConstructor,
            this->proxyConstructor,
            this->generatorFunctionConstructor,
            this->errorConstructor,
            this->evalErrorConstructor,
            this->rangeErrorConstructor,
            this->referenceErrorConstructor,
            this->syntaxErrorConstructor,
            this->typeErrorConstructor,
            this->uriErrorConstructor,
        };

        Assert(!scriptContext->GetThreadContext()->IsDisableImplicitCall());
        for (int i = 0; i < _countof(objects); i++)
        {
            if (objects[i]) // may be NULL for compat mode
            {
                objects[i]->GetTypeHandler()->EnsureObjectReady(objects[i]);
            }
        }

        isLibraryReadyForHybridDebugging = true;
    }

    // Note: This function is only used in float preferencing scenarios. Should remove it once we do away with float preferencing.

    // Cases like,
    // case PropertyIds::concat:
    // case PropertyIds::indexOf:
    // case PropertyIds::lastIndexOf:
    // case PropertyIds::slice:
    // which have same names for Array and String cannot be resolved just by the property id

    BuiltinFunction JavascriptLibrary::GetBuiltinFunctionForPropId(PropertyId id)
    {
        switch (id)
        {
        case PropertyIds::abs:
            return BuiltinFunction::Math_Abs;

        // For now, avoid mapping Math.atan2 to a direct CRT call, as the
        // fast CRT helper doesn't handle denormals correctly.
        // case PropertyIds::atan2:
        //    return BuiltinFunction::Atan2;

        case PropertyIds::acos:
            return BuiltinFunction::Math_Acos;

        case PropertyIds::asin:
            return BuiltinFunction::Math_Asin;

        case PropertyIds::atan:
            return BuiltinFunction::Math_Atan;

        case PropertyIds::cos:
            return BuiltinFunction::Math_Cos;

        case PropertyIds::exp:
            return BuiltinFunction::Math_Exp;

        case PropertyIds::log:
            return BuiltinFunction::Math_Log;

        case PropertyIds::pow:
            return BuiltinFunction::Math_Pow;

        case PropertyIds::random:
            return BuiltinFunction::Math_Random;

        case PropertyIds::sin:
            return BuiltinFunction::Math_Sin;

        case PropertyIds::sqrt:
            return BuiltinFunction::Math_Sqrt;

        case PropertyIds::tan:
            return BuiltinFunction::Math_Tan;

        case PropertyIds::floor:
            return BuiltinFunction::Math_Floor;

        case PropertyIds::ceil:
            return BuiltinFunction::Math_Ceil;

        case PropertyIds::round:
            return BuiltinFunction::Math_Round;

         case PropertyIds::max:
            return BuiltinFunction::Math_Max;

        case PropertyIds::min:
            return BuiltinFunction::Math_Min;

        case PropertyIds::imul:
            return BuiltinFunction::Math_Imul;

        case PropertyIds::fround:
            return BuiltinFunction::Math_Fround;

        case PropertyIds::codePointAt:
            return BuiltinFunction::String_CodePointAt;

        case PropertyIds::push:
            return BuiltinFunction::Array_Push;

        case PropertyIds::concat:
            return BuiltinFunction::Array_Concat;

        case PropertyIds::indexOf:
            return BuiltinFunction::Array_IndexOf;

        case PropertyIds::includes:
            return BuiltinFunction::Array_Includes;

        case PropertyIds::isArray:
            return BuiltinFunction::Array_IsArray;

        case PropertyIds::join:
            return BuiltinFunction::Array_Join;

        case PropertyIds::lastIndexOf:
            return BuiltinFunction::Array_LastIndexOf;

        case PropertyIds::reverse:
            return BuiltinFunction::Array_Reverse;

        case PropertyIds::shift:
            return BuiltinFunction::Array_Shift;

        case PropertyIds::slice:
            return BuiltinFunction::Array_Slice;

        case PropertyIds::splice:
            return BuiltinFunction::Array_Splice;

        case PropertyIds::unshift:
            return BuiltinFunction::Array_Unshift;

        case PropertyIds::apply:
            return BuiltinFunction::Function_Apply;

        case PropertyIds::charAt:
            return BuiltinFunction::String_CharAt;

        case PropertyIds::charCodeAt:
            return BuiltinFunction::String_CharCodeAt;

        case PropertyIds::fromCharCode:
            return BuiltinFunction::String_FromCharCode;

        case PropertyIds::fromCodePoint:
                return BuiltinFunction::String_FromCodePoint;

        case PropertyIds::link:
            return BuiltinFunction::String_Link;

        case PropertyIds::localeCompare:
            return BuiltinFunction::String_LocaleCompare;

        case PropertyIds::match:
            return BuiltinFunction::String_Match;

        case PropertyIds::replace:
            return BuiltinFunction::String_Replace;

        case PropertyIds::search:
            return BuiltinFunction::String_Search;

        case PropertyIds::split:
            return BuiltinFunction::String_Split;

        case PropertyIds::substr:
            return BuiltinFunction::String_Substr;

        case PropertyIds::substring:
            return BuiltinFunction::String_Substring;

        case PropertyIds::toLocaleLowerCase:
            return BuiltinFunction::String_ToLocaleLowerCase;

        case PropertyIds::toLocaleUpperCase:
            return BuiltinFunction::String_ToLocaleUpperCase;

        case PropertyIds::toLowerCase:
            return BuiltinFunction::String_ToLowerCase;

        case PropertyIds::toUpperCase:
            return BuiltinFunction::String_ToUpperCase;

        case PropertyIds::trim:
            return BuiltinFunction::String_Trim;

        case PropertyIds::trimLeft:
            return BuiltinFunction::String_TrimLeft;

        case PropertyIds::trimRight:
            return BuiltinFunction::String_TrimRight;

        case PropertyIds::exec:
            return BuiltinFunction::RegExp_Exec;

        default:
            return BuiltinFunction::None;
        }
    }

    // Returns built-in enum value for given funcInfo. Ultimately this will work for all built-ins (not only Math.*).
    // Used by inliner.
    //static
    BuiltinFunction JavascriptLibrary::GetBuiltInForFuncInfo(FunctionInfo* funcInfo, ScriptContext *scriptContext)
    {
        Assert(funcInfo);

        return scriptContext->GetLibrary()->funcInfoToBuiltinIdMap->Item(funcInfo);
    }

    // Returns true if the function's return type is always float.
    BOOL JavascriptLibrary::IsFltFunc(BuiltinFunction index)
    {
        // Note: MathFuncion is one of built-ins.
        if (!JavascriptLibrary::CanFloatPreferenceFunc(index))
        {
            return FALSE;
        }

        Js::BuiltInFlags builtInFlags = JavascriptLibrary::GetFlagsForBuiltIn(index);
        Js::BuiltInArgSpecizationType dstType = Js::JavascriptLibrary::GetBuiltInArgType(builtInFlags, Js::BuiltInArgShift::BIAS_Dst);
        bool isFloatFunc = dstType == Js::BuiltInArgSpecizationType::BIAST_Float;
        return isFloatFunc;
    }

    size_t const JavascriptLibrary::LibraryFunctionArgC[] = {
#define LIBRARY_FUNCTION(obj, name, argc, flags, entry) argc,
#include "LibraryFunction.h"
#undef LIBRARY_FUNCTION
        0
    };

#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t const * const JavascriptLibrary::LibraryFunctionName[] = {
#define LIBRARY_FUNCTION(obj, name, argc, flags, entry) L#obj L"." L#name,
#include "LibraryFunction.h"
#undef LIBRARY_FUNCTION
        0
    };
#endif

    int const JavascriptLibrary::LibraryFunctionFlags[] = {
#define LIBRARY_FUNCTION(obj, name, argc, flags, entry) flags,
#include "LibraryFunction.h"
#undef LIBRARY_FUNCTION
        BIF_None
    };

    bool JavascriptLibrary::IsFloatFunctionCallsite(BuiltinFunction index, size_t argc)
    {
        if (IsFltFunc(index))
        {
            Assert(index < BuiltinFunction::Count);
            if (argc)
            {
                return JavascriptLibrary::LibraryFunctionArgC[index] <= (argc - 1 /* this */);
            }
        }

        return false;
    }

    // For abs, min, max -- return can be int or float, but still return true from here.
    BOOL JavascriptLibrary::CanFloatPreferenceFunc(BuiltinFunction index)
    {
        // Shortcut the common case:
        if (index == BuiltinFunction::None)
        {
            return FALSE;
        }

        switch (index)
        {
        case BuiltinFunction::Math_Abs:
        case BuiltinFunction::Math_Acos:
        case BuiltinFunction::Math_Asin:
        case BuiltinFunction::Math_Atan:
        case BuiltinFunction::Math_Cos:
        case BuiltinFunction::Math_Exp:
        case BuiltinFunction::Math_Log:
        case BuiltinFunction::Math_Min:
        case BuiltinFunction::Math_Max:
        case BuiltinFunction::Math_Pow:
        case BuiltinFunction::Math_Random:
        case BuiltinFunction::Math_Sin:
        case BuiltinFunction::Math_Sqrt:
        case BuiltinFunction::Math_Tan:
        case BuiltinFunction::Math_Fround:
            return TRUE;
        }
        return FALSE;
    }

    bool JavascriptLibrary::IsFltBuiltInConst(PropertyId propertyId)
    {
        switch (propertyId)
        {
        case Js::PropertyIds::E:
        case Js::PropertyIds::LN10:
        case Js::PropertyIds::LN2:
        case Js::PropertyIds::LOG2E:
        case Js::PropertyIds::LOG10E:
        case Js::PropertyIds::PI:
        case Js::PropertyIds::SQRT1_2:
        case Js::PropertyIds::SQRT2:
            return true;
        }
        return false;
    }

    void JavascriptLibrary::TypeAndPrototypesAreEnsuredToHaveOnlyWritableDataProperties(Type *const type)
    {
        Assert(type);
        Assert(type->GetScriptContext() == scriptContext);
        Assert(type->AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties());
        Assert(!scriptContext->IsClosed());

        if(typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain->Count() == 0)
        {
            scriptContext->RegisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext();
        }
        typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain->Add(type);
    }

    void JavascriptLibrary::NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties()
    {
        for(int i = 0; i < typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain->Count(); ++i)
        {
            typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain
                ->Item(i)
                ->SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(false);
        }
        typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain->ClearAndZero();
    }

    void JavascriptLibrary::InitializeNumberConstructor(DynamicObject* numberConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(numberConstructor, mode, 17);

        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterNumber
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = numberConstructor->GetScriptContext();
        JavascriptLibrary* library = numberConstructor->GetLibrary();
        library->AddMember(numberConstructor, PropertyIds::length,            TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(numberConstructor, PropertyIds::prototype,         library->numberPrototype,     PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(numberConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Number), PropertyConfigurable);
        }
        library->AddMember(numberConstructor, PropertyIds::MAX_VALUE,         library->maxValue,            PropertyNone);
        library->AddMember(numberConstructor, PropertyIds::MIN_VALUE,         library->minValue,            PropertyNone);
        library->AddMember(numberConstructor, PropertyIds::NaN,               library->nan,                 PropertyNone);
        library->AddMember(numberConstructor, PropertyIds::NEGATIVE_INFINITY, library->negativeInfinite,    PropertyNone);
        library->AddMember(numberConstructor, PropertyIds::POSITIVE_INFINITY, library->positiveInfinite,    PropertyNone);

        if (scriptContext->GetConfig()->IsES6NumberExtensionsEnabled())
        {
#ifdef DBG
            double epsilon = 0.0;
            for (double next = 1.0; next + 1.0 != 1.0; next = next / 2.0)
            {
                epsilon = next;
            }
            Assert(epsilon == Math::EPSILON);
#endif
            library->AddMember(numberConstructor, PropertyIds::EPSILON,     JavascriptNumber::New(Math::EPSILON,     scriptContext), PropertyNone);
            library->AddMember(numberConstructor, PropertyIds::MAX_SAFE_INTEGER, JavascriptNumber::New(Math::MAX_SAFE_INTEGER, scriptContext), PropertyNone);
            library->AddMember(numberConstructor, PropertyIds::MIN_SAFE_INTEGER, JavascriptNumber::New(Math::MIN_SAFE_INTEGER, scriptContext), PropertyNone);

            AssertMsg(library->parseIntFunctionObject != nullptr, "Where is parseIntFunctionObject? Should have been initialized with Global object initialization");
            AssertMsg(library->parseFloatFunctionObject != nullptr, "Where is parseIntFunctionObject? Should have been initialized with Global object initialization");
            library->AddMember(numberConstructor, PropertyIds::parseInt, library->parseIntFunctionObject);
            library->AddMember(numberConstructor, PropertyIds::parseFloat, library->parseFloatFunctionObject);
            library->AddFunctionToLibraryObject(numberConstructor, PropertyIds::isNaN, &JavascriptNumber::EntryInfo::IsNaN, 1);
            library->AddFunctionToLibraryObject(numberConstructor, PropertyIds::isFinite, &JavascriptNumber::EntryInfo::IsFinite, 1);
            library->AddFunctionToLibraryObject(numberConstructor, PropertyIds::isInteger, &JavascriptNumber::EntryInfo::IsInteger, 1);
            library->AddFunctionToLibraryObject(numberConstructor, PropertyIds::isSafeInteger, &JavascriptNumber::EntryInfo::IsSafeInteger, 1);
        }

        numberConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeNumberPrototype(DynamicObject* numberPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(numberPrototype, mode, 8);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterNumber
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = numberPrototype->GetScriptContext();
        JavascriptLibrary* library = numberPrototype->GetLibrary();
        library->AddMember(numberPrototype, PropertyIds::constructor, library->numberConstructor);
        scriptContext->SetBuiltInLibraryFunction(JavascriptNumber::EntryInfo::ToExponential.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(numberPrototype, PropertyIds::toExponential, &JavascriptNumber::EntryInfo::ToExponential, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptNumber::EntryInfo::ToFixed.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(numberPrototype, PropertyIds::toFixed, &JavascriptNumber::EntryInfo::ToFixed, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptNumber::EntryInfo::ToPrecision.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(numberPrototype, PropertyIds::toPrecision, &JavascriptNumber::EntryInfo::ToPrecision, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptNumber::EntryInfo::ToLocaleString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(numberPrototype, PropertyIds::toLocaleString, &JavascriptNumber::EntryInfo::ToLocaleString, 0));
        scriptContext->SetBuiltInLibraryFunction(JavascriptNumber::EntryInfo::ToString.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(numberPrototype, PropertyIds::toString, &JavascriptNumber::EntryInfo::ToString, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptNumber::EntryInfo::ValueOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(numberPrototype, PropertyIds::valueOf, &JavascriptNumber::EntryInfo::ValueOf, 0));

        numberPrototype->SetHasNoEnumerableProperties(true);
    }


    void JavascriptLibrary::InitializeObjectConstructor(DynamicObject* objectConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterObject
        // so that the update is in sync with profiler
        JavascriptLibrary* library = objectConstructor->GetLibrary();
        ScriptContext* scriptContext = objectConstructor->GetScriptContext();
        int propertyCount = 17;
        if (scriptContext->GetConfig()->IsES6SymbolEnabled())
        {
            propertyCount += 1;
        }
        if (scriptContext->GetConfig()->IsES6ObjectExtensionsEnabled())
        {
            propertyCount += 2;
        }

        typeHandler->Convert(objectConstructor, mode, propertyCount);

        library->AddMember(objectConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(objectConstructor, PropertyIds::prototype, library->objectPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(objectConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Object), PropertyConfigurable);
        }

        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::DefineProperty.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::defineProperty, &JavascriptObject::EntryInfo::DefineProperty, 3));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::GetOwnPropertyDescriptor.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::getOwnPropertyDescriptor, &JavascriptObject::EntryInfo::GetOwnPropertyDescriptor, 2));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::DefineProperties.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::defineProperties, &JavascriptObject::EntryInfo::DefineProperties, 2));
        library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::create, &JavascriptObject::EntryInfo::Create, 2);
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::Seal.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::seal, &JavascriptObject::EntryInfo::Seal, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::Freeze.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::freeze, &JavascriptObject::EntryInfo::Freeze, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::PreventExtensions.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::preventExtensions, &JavascriptObject::EntryInfo::PreventExtensions, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::IsSealed.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::isSealed, &JavascriptObject::EntryInfo::IsSealed, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::IsFrozen.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::isFrozen, &JavascriptObject::EntryInfo::IsFrozen, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::IsExtensible.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::isExtensible, &JavascriptObject::EntryInfo::IsExtensible, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::GetPrototypeOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::getPrototypeOf, &JavascriptObject::EntryInfo::GetPrototypeOf, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::Keys.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::keys, &JavascriptObject::EntryInfo::Keys, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::GetOwnPropertyNames.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::getOwnPropertyNames, &JavascriptObject::EntryInfo::GetOwnPropertyNames, 1));
        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::SetPrototypeOf.GetOriginalEntryPoint(),
            library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::setPrototypeOf, &JavascriptObject::EntryInfo::SetPrototypeOf, 2));
        if (scriptContext->GetConfig()->IsES6SymbolEnabled())
        {
            scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::GetOwnPropertySymbols.GetOriginalEntryPoint(),
                library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::getOwnPropertySymbols, &JavascriptObject::EntryInfo::GetOwnPropertySymbols, 1));
        }
        if (scriptContext->GetConfig()->IsES6ObjectExtensionsEnabled())
        {
            scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::Is.GetOriginalEntryPoint(),
                library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::is, &JavascriptObject::EntryInfo::Is, 2));
            scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::Assign.GetOriginalEntryPoint(),
                library->AddFunctionToLibraryObject(objectConstructor, PropertyIds::assign, &JavascriptObject::EntryInfo::Assign, 2));
        }

        objectConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeObjectPrototype(DynamicObject* objectPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        JavascriptLibrary* library = objectPrototype->GetLibrary();
        ScriptContext* scriptContext = objectPrototype->GetScriptContext();

        bool hasAccessors = scriptContext->GetConfig()->Is__proto__Enabled();

        typeHandler->Convert(objectPrototype, mode, 11, hasAccessors);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterObject
        // so that the update is in sync with profiler
        library->AddMember(objectPrototype, PropertyIds::constructor, library->objectConstructor);
        library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::hasOwnProperty, &JavascriptObject::EntryInfo::HasOwnProperty, 1);
        library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::propertyIsEnumerable, &JavascriptObject::EntryInfo::PropertyIsEnumerable, 1);
        library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::isPrototypeOf, &JavascriptObject::EntryInfo::IsPrototypeOf, 1);
        library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::toLocaleString, &JavascriptObject::EntryInfo::ToLocaleString, 0);

        library->objectToStringFunction = library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::toString, &JavascriptObject::EntryInfo::ToString, 0);
        library->objectValueOfFunction = library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::valueOf, &JavascriptObject::EntryInfo::ValueOf, 0);

        scriptContext->SetBuiltInLibraryFunction(JavascriptObject::EntryInfo::ToString.GetOriginalEntryPoint(), library->objectToStringFunction);

        if (scriptContext->GetConfig()->Is__proto__Enabled())
        {
            bool hadOnlyWritableDataProperties = objectPrototype->GetDynamicType()->GetTypeHandler()->GetHasOnlyWritableDataProperties();
            objectPrototype->SetAccessors(PropertyIds::__proto__, library->Get__proto__getterFunction(), library->Get__proto__setterFunction(), PropertyOperation_NonFixedValue);
            objectPrototype->SetEnumerable(PropertyIds::__proto__, FALSE);
            // Let's pretend __proto__ is actually writable.  We'll make sure we always go through a special code path when writing to it.
            if (hadOnlyWritableDataProperties)
            {
                objectPrototype->GetDynamicType()->GetTypeHandler()->SetHasOnlyWritableDataProperties();
            }
        }

        if (scriptContext->GetConfig()->IsDefineGetterSetterEnabled())
        {
            library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::__defineGetter__, &JavascriptObject::EntryInfo::DefineGetter, 2);
            library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::__defineSetter__, &JavascriptObject::EntryInfo::DefineSetter, 2);
            library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::__lookupGetter__, &JavascriptObject::EntryInfo::LookupGetter, 1);
            library->AddFunctionToLibraryObject(objectPrototype, PropertyIds::__lookupSetter__, &JavascriptObject::EntryInfo::LookupSetter, 1);
        }

        objectPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeRegexConstructor(DynamicObject* regexConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        JavascriptLibrary* library = regexConstructor->GetLibrary();
        ScriptContext* scriptContext = regexConstructor->GetScriptContext();
        typeHandler->Convert(regexConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterRegExp
        // so that the update is in sync with profiler
        library->AddMember(regexConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(2), PropertyNone);
        library->AddMember(regexConstructor, PropertyIds::prototype, library->regexPrototype, PropertyNone);

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(regexConstructor, PropertyIds::_symbolSpecies, &JavascriptRegExp::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(regexConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::RegExp), PropertyConfigurable);
        }

        regexConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeRegexPrototype(DynamicObject* regexPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(regexPrototype, mode, 5);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterRegExp
        // so that the update is in sync with profiler
        JavascriptFunction * func;
        JavascriptLibrary* library = regexPrototype->GetLibrary();
        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();

        library->AddMember(regexPrototype, PropertyIds::constructor, library->regexConstructor);
        func = library->AddFunctionToLibraryObject(regexPrototype, PropertyIds::exec, &JavascriptRegExp::EntryInfo::Exec, 1);
        builtinFuncs[BuiltinFunction::RegExp_Exec] = func;
        library->AddFunctionToLibraryObject(regexPrototype, PropertyIds::test, &JavascriptRegExp::EntryInfo::Test, 1);
        library->AddFunctionToLibraryObject(regexPrototype, PropertyIds::toString, &JavascriptRegExp::EntryInfo::ToString, 0);
        // This is deprecated. Should be guarded with appropriate version flag.
        library->AddFunctionToLibraryObject(regexPrototype, PropertyIds::compile, &JavascriptRegExp::EntryInfo::Compile, 2);

        const ScriptConfiguration* scriptConfig = regexPrototype->GetScriptContext()->GetConfig();
        if (scriptConfig->IsES6RegExPrototypePropertiesEnabled())
        {
            library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::global, &JavascriptRegExp::EntryInfo::GetterGlobal, nullptr);
            library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::ignoreCase, &JavascriptRegExp::EntryInfo::GetterIgnoreCase, nullptr);
            library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::multiline, &JavascriptRegExp::EntryInfo::GetterMultiline, nullptr);
            library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::options, &JavascriptRegExp::EntryInfo::GetterOptions, nullptr);
            library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::source, &JavascriptRegExp::EntryInfo::GetterSource, nullptr);

            if (scriptConfig->IsES6RegExStickyEnabled())
            {
                library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::sticky, &JavascriptRegExp::EntryInfo::GetterSticky, nullptr);
            }

            if (scriptConfig->IsES6UnicodeExtensionsEnabled())
            {
                library->AddAccessorsToLibraryObject(regexPrototype, PropertyIds::unicode, &JavascriptRegExp::EntryInfo::GetterUnicode, nullptr);
            }
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, library->GetScriptContext()));

        regexPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeStringConstructor(DynamicObject* stringConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(stringConstructor, mode, 6);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterString
        // so that the update is in sync with profiler
        JavascriptLibrary* library = stringConstructor->GetLibrary();
        ScriptContext* scriptContext = stringConstructor->GetScriptContext();

        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();
        library->AddMember(stringConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone);
        library->AddMember(stringConstructor, PropertyIds::prototype, library->stringPrototype, PropertyNone);

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(stringConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::String), PropertyConfigurable);
        }


        builtinFuncs[BuiltinFunction::String_FromCharCode]  = library->AddFunctionToLibraryObject(stringConstructor, PropertyIds::fromCharCode,  &JavascriptString::EntryInfo::FromCharCode,  1);
        if(scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
        {
            builtinFuncs[BuiltinFunction::String_FromCodePoint] = library->AddFunctionToLibraryObject(stringConstructor, PropertyIds::fromCodePoint, &JavascriptString::EntryInfo::FromCodePoint, 1);
        }

        if (scriptContext->GetConfig()->IsES6StringTemplateEnabled())
        {
            /* No inlining                String_Raw           */ library->AddFunctionToLibraryObject(stringConstructor, PropertyIds::raw,           &JavascriptString::EntryInfo::Raw,           1);
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        stringConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeStringPrototype(DynamicObject* stringPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(stringPrototype, mode, 38);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterString
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = stringPrototype->GetScriptContext();
        JavascriptLibrary* library = stringPrototype->GetLibrary();
        JavascriptFunction ** builtinFuncs = library->GetBuiltinFunctions();
        library->AddMember(stringPrototype, PropertyIds::constructor, library->stringConstructor);

        builtinFuncs[BuiltinFunction::String_IndexOf]       = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::indexOf,            &JavascriptString::EntryInfo::IndexOf,              1);
        builtinFuncs[BuiltinFunction::String_LastIndexOf]   = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::lastIndexOf,        &JavascriptString::EntryInfo::LastIndexOf,          1);
        builtinFuncs[BuiltinFunction::String_Replace]       = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::replace,            &JavascriptString::EntryInfo::Replace,              2);
        builtinFuncs[BuiltinFunction::String_Search]        = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::search,             &JavascriptString::EntryInfo::Search,               1);
        builtinFuncs[BuiltinFunction::String_Slice]         = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::slice,              &JavascriptString::EntryInfo::Slice,                2);

        if (CONFIG_FLAG(ES6Unicode))
        {
            builtinFuncs[BuiltinFunction::String_CodePointAt]   = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::codePointAt,        &JavascriptString::EntryInfo::CodePointAt,          1);
            /* builtinFuncs[BuiltinFunction::String_Normalize] =*/library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::normalize,          &JavascriptString::EntryInfo::Normalize,            0);
        }

        builtinFuncs[BuiltinFunction::String_CharAt]            = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::charAt,             &JavascriptString::EntryInfo::CharAt,               1);
        builtinFuncs[BuiltinFunction::String_CharCodeAt]        = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::charCodeAt,         &JavascriptString::EntryInfo::CharCodeAt,           1);
        builtinFuncs[BuiltinFunction::String_Concat]            = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::concat,             &JavascriptString::EntryInfo::Concat,               1);
        builtinFuncs[BuiltinFunction::String_LocaleCompare]     = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::localeCompare,      &JavascriptString::EntryInfo::LocaleCompare,        1);
        builtinFuncs[BuiltinFunction::String_Match]             = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::match,              &JavascriptString::EntryInfo::Match,                1);
        builtinFuncs[BuiltinFunction::String_Split]             = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::split,              &JavascriptString::EntryInfo::Split,                2);
        builtinFuncs[BuiltinFunction::String_Substring]         = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::substring,          &JavascriptString::EntryInfo::Substring,            2);
        builtinFuncs[BuiltinFunction::String_Substr]            = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::substr,             &JavascriptString::EntryInfo::Substr,               2);
        builtinFuncs[BuiltinFunction::String_ToLocaleLowerCase] = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::toLocaleLowerCase,  &JavascriptString::EntryInfo::ToLocaleLowerCase,    0);
        builtinFuncs[BuiltinFunction::String_ToLocaleUpperCase] = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::toLocaleUpperCase,  &JavascriptString::EntryInfo::ToLocaleUpperCase,    0);
        builtinFuncs[BuiltinFunction::String_ToLowerCase]       = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::toLowerCase,        &JavascriptString::EntryInfo::ToLowerCase,          0);
        scriptContext->SetBuiltInLibraryFunction(JavascriptString::EntryInfo::ToString.GetOriginalEntryPoint(),
                                                                  library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::toString,           &JavascriptString::EntryInfo::ToString,             0));
        builtinFuncs[BuiltinFunction::String_ToUpperCase]       = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::toUpperCase,        &JavascriptString::EntryInfo::ToUpperCase,          0);
        builtinFuncs[BuiltinFunction::String_Trim]              = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::trim,               &JavascriptString::EntryInfo::Trim,                 0);

        scriptContext->SetBuiltInLibraryFunction(JavascriptString::EntryInfo::ValueOf.GetOriginalEntryPoint(),
                                                                  library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::valueOf,            &JavascriptString::EntryInfo::ValueOf,              0));

        if (scriptContext->GetConfig()->SupportsES3Extensions())
        {
            /* No inlining                String_Anchor        */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::anchor,             &JavascriptString::EntryInfo::Anchor,               1);
            /* No inlining                String_Big           */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::big,                &JavascriptString::EntryInfo::Big,                  0);
            /* No inlining                String_Blink         */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::blink,              &JavascriptString::EntryInfo::Blink,                0);
            /* No inlining                String_Bold          */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::bold,               &JavascriptString::EntryInfo::Bold,                 0);
            /* No inlining                String_Fixed         */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::fixed,              &JavascriptString::EntryInfo::Fixed,                0);
            /* No inlining                String_FontColor     */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::fontcolor,          &JavascriptString::EntryInfo::FontColor,            1);
            /* No inlining                String_FontSize      */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::fontsize,           &JavascriptString::EntryInfo::FontSize,             1);
            /* No inlining                String_Italics       */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::italics,            &JavascriptString::EntryInfo::Italics,              0);
            builtinFuncs[BuiltinFunction::String_Link]          = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::link,               &JavascriptString::EntryInfo::Link,                 1);
            /* No inlining                String_Small         */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::Small,              &JavascriptString::EntryInfo::Small,                0);
            /* No inlining                String_Strike        */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::strike,             &JavascriptString::EntryInfo::Strike,               0);
            /* No inlining                String_Sub           */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::sub,                &JavascriptString::EntryInfo::Sub,                  0);
            /* No inlining                String_Sup           */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::sup,                &JavascriptString::EntryInfo::Sup,                  0);
        }

        if (scriptContext->GetConfig()->IsES6StringExtensionsEnabled())
        {
            /* No inlining                String_Repeat        */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::repeat,             &JavascriptString::EntryInfo::Repeat,               1);
            /* No inlining                String_StartsWith    */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::startsWith,         &JavascriptString::EntryInfo::StartsWith,           1);
            /* No inlining                String_EndsWith      */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::endsWith,           &JavascriptString::EntryInfo::EndsWith,             1);
            /* No inlining                String_Includes      */ library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::includes,           &JavascriptString::EntryInfo::Includes,             1);
            builtinFuncs[BuiltinFunction::String_TrimLeft]      = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::trimLeft,           &JavascriptString::EntryInfo::TrimLeft,             0);
            builtinFuncs[BuiltinFunction::String_TrimRight]     = library->AddFunctionToLibraryObject(stringPrototype, PropertyIds::trimRight,          &JavascriptString::EntryInfo::TrimRight,            0);
        }

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            library->AddFunctionToLibraryObjectWithName(stringPrototype, PropertyIds::_symbolIterator, PropertyIds::_RuntimeFunctionNameId_iterator,
                &JavascriptString::EntryInfo::SymbolIterator, 0);
        }

        DebugOnly(CheckRegisteredBuiltIns(builtinFuncs, scriptContext));

        stringPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeMapConstructor(DynamicObject* mapConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(mapConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterMap
        // so that the update is in sync with profiler
        JavascriptLibrary* library = mapConstructor->GetLibrary();
        ScriptContext* scriptContext = mapConstructor->GetScriptContext();
        library->AddMember(mapConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone);
        library->AddMember(mapConstructor, PropertyIds::prototype, library->mapPrototype, PropertyNone);

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(mapConstructor, PropertyIds::_symbolSpecies, &JavascriptMap::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(mapConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Map), PropertyConfigurable);
        }

        mapConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeMapPrototype(DynamicObject* mapPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(mapPrototype, mode, 13, true);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterMap
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = mapPrototype->GetScriptContext();
        JavascriptLibrary* library = mapPrototype->GetLibrary();
        library->AddMember(mapPrototype, PropertyIds::constructor, library->mapConstructor);

        library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::clear, &JavascriptMap::EntryInfo::Clear, 0);
        library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::delete_, &JavascriptMap::EntryInfo::Delete, 1);
        library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::forEach, &JavascriptMap::EntryInfo::ForEach, 1);
        library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::get, &JavascriptMap::EntryInfo::Get, 1);
        library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::has, &JavascriptMap::EntryInfo::Has, 1);
        library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::set, &JavascriptMap::EntryInfo::Set, 2);

        library->AddAccessorsToLibraryObject(mapPrototype, PropertyIds::size, &JavascriptMap::EntryInfo::SizeGetter, nullptr);

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            JavascriptFunction* entriesFunc;
            entriesFunc = library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::entries, &JavascriptMap::EntryInfo::Entries, 0);
            library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::keys, &JavascriptMap::EntryInfo::Keys, 0);
            library->AddFunctionToLibraryObject(mapPrototype, PropertyIds::values, &JavascriptMap::EntryInfo::Values, 0);
            library->AddMember(mapPrototype, PropertyIds::_symbolIterator, entriesFunc);
        }
        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(mapPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Map"), PropertyConfigurable);
        }

        mapPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeSetConstructor(DynamicObject* setConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(setConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSet
        // so that the update is in sync with profiler
        JavascriptLibrary* library = setConstructor->GetLibrary();
        ScriptContext* scriptContext = setConstructor->GetScriptContext();
        library->AddMember(setConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone);
        library->AddMember(setConstructor, PropertyIds::prototype, library->setPrototype, PropertyNone);

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {
            library->AddAccessorsToLibraryObject(setConstructor, PropertyIds::_symbolSpecies, &JavascriptSet::EntryInfo::GetterSymbolSpecies, nullptr);
        }

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(setConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::Set), PropertyConfigurable);
        }

        setConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeSetPrototype(DynamicObject* setPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(setPrototype, mode, 12, true);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSet
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = setPrototype->GetScriptContext();
        JavascriptLibrary* library = setPrototype->GetLibrary();
        library->AddMember(setPrototype, PropertyIds::constructor, library->setConstructor);

        library->AddFunctionToLibraryObject(setPrototype, PropertyIds::add, &JavascriptSet::EntryInfo::Add, 1);
        library->AddFunctionToLibraryObject(setPrototype, PropertyIds::clear, &JavascriptSet::EntryInfo::Clear, 0);
        library->AddFunctionToLibraryObject(setPrototype, PropertyIds::delete_, &JavascriptSet::EntryInfo::Delete, 1);
        library->AddFunctionToLibraryObject(setPrototype, PropertyIds::forEach, &JavascriptSet::EntryInfo::ForEach, 1);
        library->AddFunctionToLibraryObject(setPrototype, PropertyIds::has, &JavascriptSet::EntryInfo::Has, 1);

        library->AddAccessorsToLibraryObject(setPrototype, PropertyIds::size, &JavascriptSet::EntryInfo::SizeGetter, nullptr);

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            JavascriptFunction* valuesFunc;
            library->AddFunctionToLibraryObject(setPrototype, PropertyIds::entries, &JavascriptSet::EntryInfo::Entries, 0);
            valuesFunc = library->AddFunctionToLibraryObject(setPrototype, PropertyIds::values, &JavascriptSet::EntryInfo::Values, 0);
            library->AddMember(setPrototype, PropertyIds::keys, valuesFunc);
            library->AddMember(setPrototype, PropertyIds::_symbolIterator, valuesFunc);
        }

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(setPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Set"), PropertyConfigurable);
        }

        setPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeWeakMapConstructor(DynamicObject* weakMapConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(weakMapConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterWeakMap
        // so that the update is in sync with profiler
        JavascriptLibrary* library = weakMapConstructor->GetLibrary();
        ScriptContext* scriptContext = weakMapConstructor->GetScriptContext();
        library->AddMember(weakMapConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone);
        library->AddMember(weakMapConstructor, PropertyIds::prototype, library->weakMapPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(weakMapConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::WeakMap), PropertyConfigurable);
        }

        weakMapConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeWeakMapPrototype(DynamicObject* weakMapPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(weakMapPrototype, mode, 6);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterWeakMap
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = weakMapPrototype->GetScriptContext();
        JavascriptLibrary* library = weakMapPrototype->GetLibrary();
        library->AddMember(weakMapPrototype, PropertyIds::constructor, library->weakMapConstructor);

        library->AddFunctionToLibraryObject(weakMapPrototype, PropertyIds::delete_, &JavascriptWeakMap::EntryInfo::Delete, 1);
        library->AddFunctionToLibraryObject(weakMapPrototype, PropertyIds::get, &JavascriptWeakMap::EntryInfo::Get, 1);
        library->AddFunctionToLibraryObject(weakMapPrototype, PropertyIds::has, &JavascriptWeakMap::EntryInfo::Has, 1);
        library->AddFunctionToLibraryObject(weakMapPrototype, PropertyIds::set, &JavascriptWeakMap::EntryInfo::Set, 2);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(weakMapPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"WeakMap"), PropertyConfigurable);
        }

        weakMapPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeWeakSetConstructor(DynamicObject* weakSetConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(weakSetConstructor, mode, 3);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterWeakSet
        // so that the update is in sync with profiler
        JavascriptLibrary* library = weakSetConstructor->GetLibrary();
        ScriptContext* scriptContext = weakSetConstructor->GetScriptContext();
        library->AddMember(weakSetConstructor, PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone);
        library->AddMember(weakSetConstructor, PropertyIds::prototype, library->weakSetPrototype, PropertyNone);
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            library->AddMember(weakSetConstructor, PropertyIds::name, scriptContext->GetPropertyString(PropertyIds::WeakSet), PropertyConfigurable);
        }

        weakSetConstructor->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeWeakSetPrototype(DynamicObject* weakSetPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(weakSetPrototype, mode, 5);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterWeakSet
        // so that the update is in sync with profiler
        ScriptContext* scriptContext = weakSetPrototype->GetScriptContext();
        JavascriptLibrary* library = weakSetPrototype->GetLibrary();
        library->AddMember(weakSetPrototype, PropertyIds::constructor, library->weakSetConstructor);

        library->AddFunctionToLibraryObject(weakSetPrototype, PropertyIds::add, &JavascriptWeakSet::EntryInfo::Add, 1);
        library->AddFunctionToLibraryObject(weakSetPrototype, PropertyIds::delete_, &JavascriptWeakSet::EntryInfo::Delete, 1);
        library->AddFunctionToLibraryObject(weakSetPrototype, PropertyIds::has, &JavascriptWeakSet::EntryInfo::Has, 1);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(weakSetPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"WeakSet"), PropertyConfigurable);
        }

        weakSetPrototype->SetHasNoEnumerableProperties(true);
    }

    void JavascriptLibrary::InitializeIteratorPrototype(DynamicObject* iteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(iteratorPrototype, mode, 1);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterIterator
        // so that the update is in sync with profiler

        JavascriptLibrary* library = iteratorPrototype->GetLibrary();

        library->AddFunctionToLibraryObjectWithName(iteratorPrototype, PropertyIds::_symbolIterator, PropertyIds::_RuntimeFunctionNameId_iterator,
            &JavascriptIterator::EntryInfo::SymbolIterator, 0);
    }

    void JavascriptLibrary::InitializeArrayIteratorPrototype(DynamicObject* arrayIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(arrayIteratorPrototype, mode, 2);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterArrayIterator
        // so that the update is in sync with profiler

        JavascriptLibrary* library = arrayIteratorPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        library->AddFunctionToLibraryObject(arrayIteratorPrototype, PropertyIds::next, &JavascriptArrayIterator::EntryInfo::Next, 0);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(arrayIteratorPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Array Iterator"), PropertyConfigurable);
        }
    }

    void JavascriptLibrary::InitializeMapIteratorPrototype(DynamicObject* mapIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(mapIteratorPrototype, mode, 2);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterMapIterator
        // so that the update is in sync with profiler

        JavascriptLibrary* library = mapIteratorPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        library->AddFunctionToLibraryObject(mapIteratorPrototype, PropertyIds::next, &JavascriptMapIterator::EntryInfo::Next, 0);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(mapIteratorPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Map Iterator"), PropertyConfigurable);
        }
    }

    void JavascriptLibrary::InitializeSetIteratorPrototype(DynamicObject* setIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(setIteratorPrototype, mode, 2);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterSetIterator
        // so that the update is in sync with profiler

        JavascriptLibrary* library = setIteratorPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();
        library->AddFunctionToLibraryObject(setIteratorPrototype, PropertyIds::next, &JavascriptSetIterator::EntryInfo::Next, 0);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(setIteratorPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Set Iterator"), PropertyConfigurable);
        }
    }

    void JavascriptLibrary::InitializeStringIteratorPrototype(DynamicObject* stringIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(stringIteratorPrototype, mode, 2);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterStringIterator
        // so that the update is in sync with profiler

        JavascriptLibrary* library = stringIteratorPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();
        library->AddFunctionToLibraryObject(stringIteratorPrototype, PropertyIds::next, &JavascriptStringIterator::EntryInfo::Next, 0);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(stringIteratorPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"String Iterator"), PropertyConfigurable);
        }
    }

    void JavascriptLibrary::InitializeJavascriptEnumeratorIteratorPrototype(DynamicObject* javascriptEnumeratorIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(javascriptEnumeratorIteratorPrototype, mode, 2);
        // Note: Any new function addition/deletion/modification should also be updated in JavascriptLibrary::ProfilerRegisterEnumeratorIterator
        // so that the update is in sync with profiler

        JavascriptLibrary* library = javascriptEnumeratorIteratorPrototype->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();
        library->AddFunctionToLibraryObject(javascriptEnumeratorIteratorPrototype, PropertyIds::next, &JavascriptEnumeratorIterator::EntryInfo::Next, 0);

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(javascriptEnumeratorIteratorPrototype, PropertyIds::_symbolToStringTag, library->CreateStringFromCppLiteral(L"Enumerator Iterator"), PropertyConfigurable);
        }
    }

    RuntimeFunction* JavascriptLibrary::CreateBuiltinConstructor(FunctionInfo * functionInfo, DynamicTypeHandler * typeHandler, DynamicObject* prototype)
    {
        Assert((functionInfo->GetAttributes() & FunctionInfo::Attributes::ErrorOnNew) == 0);

        if (prototype == nullptr)
        {
            prototype = functionPrototype;
        }

        ConstructorCache* ctorCache = ((functionInfo->GetAttributes() & FunctionInfo::Attributes::SkipDefaultNewObject) != 0) ?
            this->builtInConstructorCache : &ConstructorCache::DefaultInstance;

        return RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, RuntimeFunction,
            DynamicType::New(scriptContext, TypeIds_Function, prototype, functionInfo->GetOriginalEntryPoint(), typeHandler),
            functionInfo, ctorCache);
    }

    JavascriptExternalFunction* JavascriptLibrary::CreateExternalConstructor(Js::ExternalMethod entryPoint, PropertyId nameId, RecyclableObject * prototype)
    {
        Assert(nameId >= Js::InternalPropertyIds::Count && scriptContext->IsTrackedPropertyId(nameId));
        JavascriptExternalFunction* function = this->CreateIdMappedExternalFunction(entryPoint, idMappedFunctionWithPrototypeType);
        function->SetFunctionNameId(TaggedInt::ToVarUnchecked(nameId));

        Js::RecyclableObject* objPrototype;
        if (prototype == nullptr)
        {
            objPrototype = CreateConstructorPrototypeObject(function);
            Assert(!objPrototype->IsEnumerable(Js::PropertyIds::constructor));
        }
        else
        {
            objPrototype = Js::RecyclableObject::FromVar(prototype);
            Js::JavascriptOperators::InitProperty(objPrototype, Js::PropertyIds::constructor, function);
            objPrototype->SetEnumerable(Js::PropertyIds::constructor, false);
        }

        Assert(!function->IsEnumerable(Js::PropertyIds::prototype));
        Assert(!function->IsConfigurable(Js::PropertyIds::prototype));
        Assert(!function->IsWritable(Js::PropertyIds::prototype));
        function->SetPropertyWithAttributes(Js::PropertyIds::prototype, objPrototype, PropertyNone, nullptr);

        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {
            JavascriptString * functionName = nullptr;
            DebugOnly(bool status =) function->GetFunctionName(&functionName);
            AssertMsg(status,"CreateExternalConstructor sets the functionNameId, status should always be true");
            function->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
        }

        return function;
    }

    JavascriptExternalFunction* JavascriptLibrary::CreateExternalConstructor(Js::ExternalMethod entryPoint, PropertyId nameId, InitializeMethod method, unsigned short deferredTypeSlots, bool hasAccessors)
    {
        Assert(nameId >= Js::InternalPropertyIds::Count && scriptContext->IsTrackedPropertyId(nameId));

        // Make sure the actual entry point is never null.
        if (entryPoint == nullptr)
        {
            entryPoint = Js::RecyclableObject::DefaultExternalEntryPoint;
        }

        JavascriptExternalFunction* function = RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptExternalFunction, entryPoint,
            externalConstructorFunctionWithDeferredPrototypeType, method, deferredTypeSlots, hasAccessors);

        function->SetFunctionNameId(TaggedInt::ToVarUnchecked(nameId));

        return function;
    }

    RuntimeFunction* JavascriptLibrary::DefaultCreateFunction(FunctionInfo * functionInfo, int length, DynamicObject * prototype, DynamicType * functionType, PropertyId nameId)
    {
        Assert(nameId >= Js::InternalPropertyIds::Count && scriptContext->IsTrackedPropertyId(nameId));
        return DefaultCreateFunction(functionInfo, length, prototype, functionType, TaggedInt::ToVarUnchecked((int)nameId));
    }

    RuntimeFunction* JavascriptLibrary::DefaultCreateFunction(FunctionInfo * functionInfo, int length, DynamicObject * prototype, DynamicType * functionType, Var nameId)
    {
        Assert(nameId != nullptr);
        RuntimeFunction * function;
        if (nullptr == functionType)
        {
            functionType = (nullptr == prototype) ?
                CreateFunctionWithLengthType(functionInfo) : CreateFunctionWithLengthAndPrototypeType(functionInfo);
        }

        function = RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, RuntimeFunction, functionType, functionInfo);

        if (prototype != nullptr)
        {
            // NOTE: Assume all built in function prototype doesn't contain valueOf or toString that has side effects
            function->SetPropertyWithAttributes(PropertyIds::prototype, prototype, PropertyNone, nullptr, PropertyOperation_None, SideEffects_None);
        }

        function->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(length), PropertyConfigurable, nullptr);
        function->SetFunctionNameId(nameId);
        if (function->GetScriptContext()->GetConfig()->IsES6FunctionNameEnabled())
        {
            JavascriptString * functionName = nullptr;
            DebugOnly(bool status = ) function->GetFunctionName(&functionName);
            AssertMsg(status, "DefaultCreateFunction sets the functionNameId, status should always be true");
            function->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
        }

#ifdef HEAP_ENUMERATION_VALIDATION
        if (prototype) prototype->SetHeapEnumValidationCookie(HEAP_ENUMERATION_LIBRARY_OBJECT_COOKIE);
        function->SetHeapEnumValidationCookie(HEAP_ENUMERATION_LIBRARY_OBJECT_COOKIE);
#endif
        return function;
    }

    JavascriptFunction* JavascriptLibrary::AddFunction(DynamicObject* object, PropertyId propertyId, RuntimeFunction* function)
    {

       AddMember(object, propertyId, function);
       function->SetFunctionNameId(TaggedInt::ToVarUnchecked((int)propertyId));
       return function;
    }

    JavascriptFunction * JavascriptLibrary::AddFunctionToLibraryObject(DynamicObject* object, PropertyId propertyId, FunctionInfo * functionInfo, int length, PropertyAttributes attributes)
    {
        RuntimeFunction* function = DefaultCreateFunction(functionInfo, length, nullptr, nullptr, propertyId);
        AddMember(object, propertyId, function, attributes);
        return function;
    }

    JavascriptFunction * JavascriptLibrary::AddFunctionToLibraryObjectWithPrototype(GlobalObject * object, PropertyId propertyId, FunctionInfo * functionInfo, int length, DynamicObject * prototype, DynamicType * functionType)
    {
        RuntimeFunction* function = DefaultCreateFunction(functionInfo, length, prototype, functionType, propertyId);
        AddMember(object, propertyId, function);
        return function;
    }

    JavascriptFunction * JavascriptLibrary::AddFunctionToLibraryObjectWithName(DynamicObject* object, PropertyId propertyId, PropertyId name, FunctionInfo * functionInfo, int length)
    {
        RuntimeFunction* function = DefaultCreateFunction(functionInfo, length, nullptr, nullptr, name);
        AddMember(object, propertyId, function);
        return function;
    }
    void JavascriptLibrary::AddAccessorsToLibraryObject(DynamicObject* object, PropertyId propertyId, FunctionInfo * getterFunctionInfo, FunctionInfo * setterFunctionInfo)
    {
        AddAccessorsToLibraryObjectWithName(object, propertyId, propertyId, getterFunctionInfo, setterFunctionInfo);
    }

    void JavascriptLibrary::AddAccessorsToLibraryObjectWithName(DynamicObject* object, PropertyId propertyId, PropertyId name, FunctionInfo * getterFunctionInfo, FunctionInfo * setterFunctionInfo)
    {
        Js::RecyclableObject * getter;
        Js::RecyclableObject * setter;

        if (getterFunctionInfo != nullptr)
        {
            Var name_withGetPrefix = LiteralString::Concat(LiteralString::NewCopySz(L"get ", scriptContext), scriptContext->GetPropertyString(name));
            RuntimeFunction* getterFunction = DefaultCreateFunction(getterFunctionInfo, 0, nullptr, nullptr, name_withGetPrefix);
            getterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(0), PropertyNone, nullptr);
            getter = getterFunction;
        }
        else
        {
            getter = GetUndefined();
        }

        if (setterFunctionInfo != nullptr)
        {
            Var name_withSetPrefix = LiteralString::Concat(LiteralString::NewCopySz(L"set ", scriptContext), scriptContext->GetPropertyString(name));
            RuntimeFunction* setterFunction = DefaultCreateFunction(setterFunctionInfo, 0, nullptr, nullptr, name_withSetPrefix);
            setterFunction->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyNone, nullptr);
            setter = setterFunction;
        }
        else
        {
            setter = GetUndefined();
        }
        object->SetAccessors(propertyId, getter, setter);
        object->SetAttributes(propertyId, PropertyConfigurable | PropertyWritable);
    }

    void JavascriptLibrary::AddMember(DynamicObject* object, PropertyId propertyId, Var value)
    {
        AddMember(object, propertyId, value, PropertyBuiltInMethodDefaults);
    }

    void JavascriptLibrary::AddMember(DynamicObject* object, PropertyId propertyId, Var value, PropertyAttributes attributes)
    {
        // NOTE: Assume all built in member doesn't have side effect
        object->SetPropertyWithAttributes(propertyId, value, attributes, nullptr, PropertyOperation_None, SideEffects_None);
    }

    void JavascriptLibrary::InitializeJSONObject(DynamicObject* JSONObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(JSONObject, mode, 3);
        JavascriptLibrary* library = JSONObject->GetLibrary();
        JSONObject->GetScriptContext()->SetBuiltInLibraryFunction(JSON::EntryInfo::Stringify.GetOriginalEntryPoint(),
        library->AddFunctionToLibraryObject(JSONObject, PropertyIds::stringify, &JSON::EntryInfo::Stringify, 3));
        library->AddFunctionToLibraryObject(JSONObject, PropertyIds::parse, &JSON::EntryInfo::Parse, 2);

        if (JSONObject->GetScriptContext()->GetConfig()->IsES6ToStringTagEnabled())
        {
            library->AddMember(JSONObject, PropertyIds::_symbolToStringTag, JSONObject->GetLibrary()->CreateStringFromCppLiteral(L"JSON"), PropertyConfigurable);
        }

        JSONObject->SetHasNoEnumerableProperties(true);
    }

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
    void JavascriptLibrary::InitializeEngineInterfaceObject(DynamicObject* engineInterface, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(engineInterface, mode, 3);

        EngineInterfaceObject::FromVar(engineInterface)->Initialize();

        engineInterface->SetHasNoEnumerableProperties(true);
    }
#endif

    JavascriptFunction* JavascriptLibrary::GetHostPromiseContinuationFunction()
    {
        if (this->hostPromiseContinuationFunction == nullptr)
        {
            this->hostPromiseContinuationFunction = scriptContext->GetHostScriptContext()->InitializeHostPromiseContinuationFunction();
        }

        return this->hostPromiseContinuationFunction;
    }

    void JavascriptLibrary::SetNativeHostPromiseContinuationFunction(PromiseContinuationCallback function, void *state)
    {
        this->nativeHostPromiseContinuationFunction = function;
        this->nativeHostPromiseContinuationFunctionState = state;
    }

    void JavascriptLibrary::PinJsrtContextObject(FinalizableObject* jsrtContext)
    {
        // With JsrtContext supporting cross context, ensure that it doesn't get GCed
        // prematurely. So pin the instance to javascriptLibrary so it will stay alive
        // until any object of it are alive.
        Assert(this->jsrtContextObject == nullptr);
        this->jsrtContextObject = jsrtContext;
    }

    FinalizableObject* JavascriptLibrary::GetPinnedJsrtContextObject()
    {
        return this->jsrtContextObject;
    }

    void JavascriptLibrary::EnqueueTask(Var taskVar)
    {
        Assert(JavascriptFunction::Is(taskVar));

        if (this->nativeHostPromiseContinuationFunction)
        {
            BEGIN_LEAVE_SCRIPT(scriptContext);
            try
            {
                this->nativeHostPromiseContinuationFunction(taskVar, this->nativeHostPromiseContinuationFunctionState);
            } catch (...)
            {
                // Hosts are required not to pass exceptions back across the callback boundary. If
                // this happens, it is a bug in the host, not something that we are expected to
                // handle gracefully.
                Js::Throw::FatalInternalError();
            }
            END_LEAVE_SCRIPT(scriptContext);
        }
        else
        {
            JavascriptFunction* hostPromiseContinuationFunction = this->GetHostPromiseContinuationFunction();

            hostPromiseContinuationFunction->GetEntryPoint()(
                hostPromiseContinuationFunction,
                Js::CallInfo(Js::CallFlags::CallFlags_Value, 3),
                this->GetUndefined(),
                taskVar,
                JavascriptNumber::ToVar(0, scriptContext));
        }
    }

#ifdef ENABLE_INTL_OBJECT
    void JavascriptLibrary::ResetIntlObject()
    {
        IntlObject = DynamicObject::New(
            recycler,
            DynamicType::New(scriptContext,
                             TypeIds_Object, objectPrototype, nullptr,
                             DeferredTypeHandler<InitializeIntlObject>::GetDefaultInstance()));
    }

    void JavascriptLibrary::EnsureIntlObjectReady()
    {
        Assert(this->IntlObject != nullptr);

        // For Intl builtins, we need to make sure the Intl object has been initialized before fetching the
        // builtins from the EngineInterfaceObject. This is because the builtins are actually created via
        // Intl.js and are registered into the EngineInterfaceObject as part of Intl object initialization.
        try
        {
            this->IntlObject->GetTypeHandler()->EnsureObjectReady(this->IntlObject);
        } catch (JavascriptExceptionObject *e)
        {
            // Propagate the OOM and SOE exceptions only
            if (e == ThreadContext::GetContextForCurrentThread()->GetPendingOOMErrorObject() ||
                e == ThreadContext::GetContextForCurrentThread()->GetPendingSOErrorObject())
            {
                e = e->CloneIfStaticExceptionObject(scriptContext);
                throw e;
            }
        }
    }

    void JavascriptLibrary::InitializeIntlObject(DynamicObject* IntlObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode)
    {
        typeHandler->Convert(IntlObject, mode,  /*initSlotCapacity*/ 2);

        auto intlInitializer = [&](IntlEngineInterfaceExtensionObject* intlExtension, ScriptContext * scriptContext, DynamicObject* intlObject) ->void
        {
            intlExtension->InjectIntlLibraryCode(scriptContext, intlObject, IntlInitializationType::Intl);
        };
        IntlObject->GetLibrary()->InitializeIntlForProtototypes(intlInitializer);
    }

    void JavascriptLibrary::InitializeIntlForStringPrototype()
    {
        auto stringPrototypeInitializer = [&](IntlEngineInterfaceExtensionObject* intlExtension, ScriptContext * scriptContext, DynamicObject* intlObject) ->void
        {
            intlExtension->InjectIntlLibraryCode(scriptContext, intlObject, IntlInitializationType::StringPrototype);
        };
        InitializeIntlForProtototypes(stringPrototypeInitializer);
    }

    void JavascriptLibrary::InitializeIntlForDatePrototype()
    {
        auto datePrototypeInitializer = [&](IntlEngineInterfaceExtensionObject* intlExtension, ScriptContext * scriptContext, DynamicObject* intlObject) ->void
        {
            intlExtension->InjectIntlLibraryCode(scriptContext, intlObject, IntlInitializationType::DatePrototype);
        };
        InitializeIntlForProtototypes(datePrototypeInitializer);
    }

    void JavascriptLibrary::InitializeIntlForNumberPrototype()
    {
        auto numberPrototypeInitializer = [&](IntlEngineInterfaceExtensionObject* intlExtension, ScriptContext * scriptContext, DynamicObject* intlObject) ->void
        {
            intlExtension->InjectIntlLibraryCode(scriptContext, intlObject, IntlInitializationType::NumberPrototype);
        };
        InitializeIntlForProtototypes(numberPrototypeInitializer);
    }

    template <class Fn>
    void JavascriptLibrary::InitializeIntlForProtototypes(Fn fn)
    {
        ScriptContext* scriptContext = this->IntlObject->GetScriptContext();
        if (scriptContext->VerifyAlive())  // Can't initialize if scriptContext closed, will need to run script
        {
            JavascriptLibrary* library = this->IntlObject->GetLibrary();
            Assert(library->engineInterfaceObject != nullptr);
            IntlEngineInterfaceExtensionObject* intlExtension = static_cast<IntlEngineInterfaceExtensionObject*>(library->GetEngineInterfaceObject()->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
            fn(intlExtension, scriptContext, IntlObject);
        }
    }
#endif

    void JavascriptLibrary::SetProfileMode(bool fSet)
    {
        inProfileMode = fSet;
    }

    void JavascriptLibrary::SetDispatchProfile(bool fSet, JavascriptMethod dispatchInvoke)
    {
        if (!fSet)
        {
            this->inDispatchProfileMode = false;
            this->GetScriptContext()->GetHostScriptContext()->SetDispatchInvoke(dispatchInvoke);
            idMappedFunctionWithPrototypeType->SetEntryPoint(JavascriptExternalFunction::ExternalFunctionThunk);
            externalFunctionWithDeferredPrototypeType->SetEntryPoint(JavascriptExternalFunction::ExternalFunctionThunk);
            stdCallFunctionWithDeferredPrototypeType->SetEntryPoint(JavascriptExternalFunction::StdCallExternalFunctionThunk);
        }
        else
        {
            this->inDispatchProfileMode = true;
            this->GetScriptContext()->GetHostScriptContext()->SetDispatchInvoke(dispatchInvoke);
            idMappedFunctionWithPrototypeType->SetEntryPoint(ProfileEntryThunk);
            externalFunctionWithDeferredPrototypeType->SetEntryPoint(ProfileEntryThunk);
            stdCallFunctionWithDeferredPrototypeType->SetEntryPoint(ProfileEntryThunk);
        }
    }
    JavascriptString* JavascriptLibrary::CreateEmptyString()
    {
        return LiteralString::CreateEmptyString(stringTypeStatic);
    }

    JavascriptRegExp* JavascriptLibrary::CreateEmptyRegExp()
    {
        return RecyclerNew(scriptContext->GetRecycler(), JavascriptRegExp, emptyRegexPattern,
                           this->GetRegexType());
    }

    void JavascriptLibrary::SetCrossSiteForSharedFunctionType(JavascriptFunction * function)
    {
        Assert(function->GetDynamicType()->GetIsShared());

        if (ScriptFunction::Is(function))
        {
#if DEBUG
            if (!function->GetFunctionProxy()->GetIsAnonymousFunction())
            {
                Assert(!function->GetFunctionInfo()->IsLambda() ?
                    function->GetDynamicType()->GetTypeHandler() == JavascriptLibrary::GetDeferredPrototypeFunctionTypeHandler(this->GetScriptContext()) :
                    function->GetDynamicType()->GetTypeHandler() == JavascriptLibrary::GetDeferredFunctionTypeHandler());
            }
            else
            {
                Assert(!function->GetFunctionInfo()->IsLambda() ?
                    function->GetDynamicType()->GetTypeHandler() == JavascriptLibrary::GetDeferredAnonymousPrototypeFunctionTypeHandler() :
                    function->GetDynamicType()->GetTypeHandler() == JavascriptLibrary::GetDeferredAnonymousFunctionTypeHandler());
            }
#endif
            function->ChangeType();
            function->SetEntryPoint(scriptContext->CurrentCrossSiteThunk);
        }
        else if (BoundFunction::Is(function))
        {
            function->ChangeType();
            function->SetEntryPoint(scriptContext->CurrentCrossSiteThunk);
        }
        else
        {
            DynamicTypeHandler * typeHandler = function->GetDynamicType()->GetTypeHandler();
            if (typeHandler == JavascriptLibrary::GetDeferredPrototypeFunctionTypeHandler(this->GetScriptContext()))
            {
                function->ReplaceType(crossSiteDeferredPrototypeFunctionType);
            }
            else if (typeHandler == Js::DeferredTypeHandler<Js::JavascriptExternalFunction::DeferredInitializer>::GetDefaultInstance())
            {
                function->ReplaceType(crossSiteExternalConstructFunctionWithPrototypeType);
            }
            else
            {
                Assert(typeHandler == &SharedIdMappedFunctionWithPrototypeTypeHandler);
                function->ReplaceType(crossSiteIdMappedFunctionWithPrototypeType);
            }
        }
    }

    JavascriptExternalFunction*
    JavascriptLibrary::CreateExternalFunction(ExternalMethod entryPoint, PropertyId nameId, Var signature, JavascriptTypeId prototypeTypeId, UINT64 flags)
    {
        Assert(nameId == 0 || scriptContext->IsTrackedPropertyId(nameId));
        return CreateExternalFunction(entryPoint, TaggedInt::ToVarUnchecked(nameId), signature, prototypeTypeId, flags);
    }

    JavascriptExternalFunction*
    JavascriptLibrary::CreateExternalFunction(ExternalMethod entryPoint, Var nameId, Var signature, JavascriptTypeId prototypeTypeId, UINT64 flags)
    {
        JavascriptExternalFunction* function = EnsureReadyIfHybridDebugging(this->CreateIdMappedExternalFunction(entryPoint, externalFunctionWithDeferredPrototypeType));
        function->SetPrototypeTypeId(prototypeTypeId);
        function->SetExternalFlags(flags);
        function->SetFunctionNameId(nameId);
        function->SetSignature(signature);
#ifdef HEAP_ENUMERATION_VALIDATION
        function->SetHeapEnumValidationCookie(HEAP_ENUMERATION_LIBRARY_OBJECT_COOKIE);
#endif

#ifdef ENABLE_JS_ETW
        JS_ETW(EventWriteJSCRIPT_BUILD_DIRECT_FUNCTION(scriptContext, function, TaggedInt::Is(nameId) ? scriptContext->GetThreadContext()->GetPropertyName(TaggedInt::ToInt32(nameId))->GetBuffer() : ((JavascriptString *)nameId)->GetString()));
#endif
#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostPhase))
        {
            Output::Print(L"Create new external function: methodAddr= %p, propertyRecord= %p, propertyName= %s\n",
                this, nameId,
                TaggedInt::Is(nameId) ? scriptContext->GetThreadContext()->GetPropertyName(TaggedInt::ToInt32(nameId))->GetBuffer() : ((JavascriptString *)nameId)->GetString());
        }
#endif
        return function;
    }

    void JavascriptLibrary::EnsureStringTemplateCallsiteObjectList()
    {
        if (this->stringTemplateCallsiteObjectList == nullptr)
        {
            this->stringTemplateCallsiteObjectList = RecyclerNew(GetRecycler(), StringTemplateCallsiteObjectList, GetRecycler());
        }
    }

    void JavascriptLibrary::AddStringTemplateCallsiteObject(RecyclableObject* callsite)
    {
        this->EnsureStringTemplateCallsiteObjectList();

        RecyclerWeakReference<RecyclableObject>* callsiteRef = this->GetRecycler()->CreateWeakReferenceHandle<RecyclableObject>(callsite);

        this->stringTemplateCallsiteObjectList->Item(callsiteRef);
    }

    RecyclableObject* JavascriptLibrary::TryGetStringTemplateCallsiteObject(ParseNodePtr pnode)
    {
        this->EnsureStringTemplateCallsiteObjectList();

        RecyclerWeakReference<RecyclableObject>* callsiteRef = this->stringTemplateCallsiteObjectList->LookupWithKey(pnode);

        if (callsiteRef)
        {
            RecyclableObject* callsite = callsiteRef->Get();

            if (callsite)
            {
                return callsite;
            }
        }

        return nullptr;
    }

    RecyclableObject* JavascriptLibrary::TryGetStringTemplateCallsiteObject(RecyclableObject* callsite)
    {
        this->EnsureStringTemplateCallsiteObjectList();

        RecyclerWeakReference<RecyclableObject>* callsiteRef = this->GetRecycler()->CreateWeakReferenceHandle<RecyclableObject>(callsite);
        RecyclerWeakReference<RecyclableObject>* existingCallsiteRef = this->stringTemplateCallsiteObjectList->LookupWithKey(callsiteRef);

        if (existingCallsiteRef)
        {
            RecyclableObject* existingCallsite = existingCallsiteRef->Get();

            if (existingCallsite)
            {
                return existingCallsite;
            }
        }

        return nullptr;
    }

#if DBG_DUMP
    const wchar_t* JavascriptLibrary::GetStringTemplateCallsiteObjectKey(Var callsite)
    {
        // Calculate the key for the string template callsite object.
        // Key is combination of the raw string literals delimited by '${}' since string template literals cannot include that symbol.
        // `str1${expr1}str2${expr2}str3` => "str1${}str2${}str3"

        ES5Array* callsiteObj = ES5Array::FromVar(callsite);
        ScriptContext* scriptContext = callsiteObj->GetScriptContext();

        Var var = JavascriptOperators::OP_GetProperty(callsiteObj, Js::PropertyIds::raw, scriptContext);
        ES5Array* rawArray = ES5Array::FromVar(var);
        uint32 arrayLength = rawArray->GetLength();
        uint32 totalStringLength = 0;
        JavascriptString* str;

        Assert(arrayLength != 0);

        // Count the size in characters of the raw strings
        for (uint32 i = 0; i < arrayLength; i++)
        {
            rawArray->DirectGetItemAt(i, &var);
            str = JavascriptString::FromVar(var);
            totalStringLength += str->GetLength();
        }

        uint32 keyLength = totalStringLength + (arrayLength - 1) * 3 + 1;
        wchar_t* key = RecyclerNewArray(scriptContext->GetRecycler(), wchar_t, keyLength);
        wchar_t* ptr = key;
        charcount_t remainingSpace = keyLength;

        // Get first item before loop - there always must be at least one item
        rawArray->DirectGetItemAt(0, &var);
        str = JavascriptString::FromVar(var);

        charcount_t len = str->GetLength();
        js_wmemcpy_s(ptr, remainingSpace, str->GetSz(), len);
        ptr += len;
        remainingSpace -= len;

        // Append a delimiter and the rest of the items
        for (uint32 i = 1; i < arrayLength; i++)
        {
            len = 3; // strlen(L"${}");
            js_wmemcpy_s(ptr, remainingSpace, L"${}", len);
            ptr += len;
            remainingSpace -= len;

            rawArray->DirectGetItemAt(i, &var);
            str = JavascriptString::FromVar(var);

            len = str->GetLength();
            js_wmemcpy_s(ptr, remainingSpace, str->GetSz(), len);
            ptr += len;
            remainingSpace -= len;
        }

        // Ensure string is terminated
        key[keyLength - 1] = L'\0';

        return key;
    }
#endif

    bool StringTemplateCallsiteObjectComparer<ParseNodePtr>::Equals(ParseNodePtr x, RecyclerWeakReference<Js::RecyclableObject>* y)
    {
        Assert(x != nullptr);
        Assert(x->nop == knopStrTemplate);

        Js::RecyclableObject* obj = y->Get();

        // If the weak reference is dead, we can't be equal.
        if (obj == nullptr)
        {
            return false;
        }

        Js::ES5Array* callsite = Js::ES5Array::FromVar(obj);
        uint32 length = callsite->GetLength();
        Js::Var element;
        Js::JavascriptString* str;
        IdentPtr pid;

        // If the length of string literals is different, these callsite objects are not equal.
        if (x->sxStrTemplate.countStringLiterals != length)
        {
            return false;
        }

        element = Js::JavascriptOperators::OP_GetProperty(callsite, Js::PropertyIds::raw, callsite->GetScriptContext());
        Js::ES5Array* rawArray = Js::ES5Array::FromVar(element);

        // Length of the raw strings should be the same as the cooked string literals.
        Assert(length == rawArray->GetLength());

        x = x->sxStrTemplate.pnodeStringRawLiterals;

        Assert(length != 0);

        for (uint32 i = 0; i < length - 1; i++)
        {
            rawArray->DirectGetItemAt(i, &element);
            str = Js::JavascriptString::FromVar(element);

            Assert(x->nop == knopList);
            Assert(x->sxBin.pnode1->nop == knopStr);

            pid = x->sxBin.pnode1->sxPid.pid;

            // If strings have different length, they aren't equal
            if (pid->Cch() != str->GetLength())
            {
                return false;
            }

            // If the strings at this index are not equal, the callsite objects are not equal.
            if (!JsUtil::CharacterBuffer<wchar_t>::StaticEquals(pid->Psz(), str->GetSz(), str->GetLength()))
            {
                return false;
            }

            x = x->sxBin.pnode2;
        }

        // There should be one more string in the callsite array - and the final string in the ParseNode

        rawArray->DirectGetItemAt(length - 1, &element);
        str = Js::JavascriptString::FromVar(element);

        Assert(x->nop == knopStr);
        pid = x->sxPid.pid;

        // If strings have different length, they aren't equal
        if (pid->Cch() != str->GetLength())
        {
            return false;
        }

        // If the strings at this index are not equal, the callsite objects are not equal.
        if (!JsUtil::CharacterBuffer<wchar_t>::StaticEquals(pid->Psz(), str->GetSz(), str->GetLength()))
        {
            return false;
        }

        return true;
    }

    bool StringTemplateCallsiteObjectComparer<ParseNodePtr>::Equals(ParseNodePtr x, ParseNodePtr y)
    {
        Assert(x != nullptr && y != nullptr);
        Assert(x->nop == knopStrTemplate && y->nop == knopStrTemplate);

        // If the ParseNode is the same, they are equal.
        if (x == y)
        {
            return true;
        }

        x = x->sxStrTemplate.pnodeStringRawLiterals;
        y = y->sxStrTemplate.pnodeStringRawLiterals;

        // If one of the templates only includes one string value, the raw literals ParseNode will
        // be a knopStr instead of knopList.
        if (x->nop != y->nop)
        {
            return false;
        }

        const wchar_t* pid_x;
        const wchar_t* pid_y;

        while (x->nop == knopList)
        {
            // If y is knopStr here, that means x has more strings in the list than y does.
            if (y->nop != knopList)
            {
                return false;
            }

            Assert(x->sxBin.pnode1->nop == knopStr);
            Assert(y->sxBin.pnode1->nop == knopStr);

            pid_x = x->sxBin.pnode1->sxPid.pid->Psz();
            pid_y = y->sxBin.pnode1->sxPid.pid->Psz();

            // If the pid values of each raw string don't match each other, these are different.
            if (!DefaultComparer<const wchar_t*>::Equals(pid_x, pid_y))
            {
                return false;
            }

            x = x->sxBin.pnode2;
            y = y->sxBin.pnode2;
        }

        // If y is still knopList here, that means y has more strings in the list than x does.
        if (y->nop != knopStr)
        {
            return false;
        }

        Assert(x->nop == knopStr);

        pid_x = x->sxPid.pid->Psz();
        pid_y = y->sxPid.pid->Psz();

        // This is the final string in the raw literals list. Return true if they are equal.
        return DefaultComparer<const wchar_t*>::Equals(pid_x, pid_y);
    }

    hash_t StringTemplateCallsiteObjectComparer<ParseNodePtr>::GetHashCode(ParseNodePtr i)
    {
        hash_t hash = 0;

        Assert(i != nullptr);
        Assert(i->nop == knopStrTemplate);

        i = i->sxStrTemplate.pnodeStringRawLiterals;

        const wchar_t* pid;

        while (i->nop == knopList)
        {
            Assert(i->sxBin.pnode1->nop == knopStr);

            pid = i->sxBin.pnode1->sxPid.pid->Psz();

            hash ^= DefaultComparer<const wchar_t*>::GetHashCode(pid);
            hash ^= DefaultComparer<const wchar_t*>::GetHashCode(L"${}");

            i = i->sxBin.pnode2;
        }

        Assert(i->nop == knopStr);

        pid = i->sxPid.pid->Psz();

        hash ^= DefaultComparer<const wchar_t*>::GetHashCode(pid);

        return hash;
    }

    bool StringTemplateCallsiteObjectComparer<RecyclerWeakReference<Js::RecyclableObject>*>::Equals(RecyclerWeakReference<Js::RecyclableObject>* x, ParseNodePtr y)
    {
        return StringTemplateCallsiteObjectComparer<ParseNodePtr>::Equals(y, x);
    }

    bool StringTemplateCallsiteObjectComparer<RecyclerWeakReference<Js::RecyclableObject>*>::Equals(RecyclerWeakReference<Js::RecyclableObject>* x, RecyclerWeakReference<Js::RecyclableObject>* y)
    {
        Js::RecyclableObject* objLeft = x->Get();
        Js::RecyclableObject* objRight = y->Get();

        // If either WeakReference is dead, we can't be equal to anything.
        if (objLeft == nullptr || objRight == nullptr)
        {
            return false;
        }

        // If the Var pointers are the same, they are equal.
        if (objLeft == objRight)
        {
            return true;
        }

        Js::ES5Array* arrayLeft = Js::ES5Array::FromVar(objLeft);
        Js::ES5Array* arrayRight = Js::ES5Array::FromVar(objRight);
        uint32 lengthLeft = arrayLeft->GetLength();
        uint32 lengthRight = arrayRight->GetLength();
        Js::Var varLeft;
        Js::Var varRight;

        // If the length of string literals is different, these callsite objects are not equal.
        if (lengthLeft != lengthRight)
        {
            return false;
        }

        Assert(lengthLeft != 0 && lengthRight != 0);

        // Change to the set of raw strings.
        varLeft = Js::JavascriptOperators::OP_GetProperty(arrayLeft, Js::PropertyIds::raw, arrayLeft->GetScriptContext());
        arrayLeft = Js::ES5Array::FromVar(varLeft);

        varRight = Js::JavascriptOperators::OP_GetProperty(arrayRight, Js::PropertyIds::raw, arrayRight->GetScriptContext());
        arrayRight = Js::ES5Array::FromVar(varRight);

        // Length of the raw strings should be the same as the cooked string literals.
        Assert(lengthLeft == arrayLeft->GetLength());
        Assert(lengthRight == arrayRight->GetLength());

        for (uint32 i = 0; i < lengthLeft; i++)
        {
            arrayLeft->DirectGetItemAt(i, &varLeft);
            arrayRight->DirectGetItemAt(i, &varRight);

            // If the strings at this index are not equal, the callsite objects are not equal.
            if (!Js::JavascriptString::Equals(varLeft, varRight))
            {
                return false;
            }
        }

        return true;
    }

    hash_t StringTemplateCallsiteObjectComparer<RecyclerWeakReference<Js::RecyclableObject>*>::GetHashCode(RecyclerWeakReference<Js::RecyclableObject>* o)
    {
        hash_t hash = 0;

        Js::RecyclableObject* obj = o->Get();

        if (obj == nullptr)
        {
            return hash;
        }

        Js::ES5Array* callsite = Js::ES5Array::FromVar(obj);
        Js::Var var = Js::JavascriptOperators::OP_GetProperty(callsite, Js::PropertyIds::raw, callsite->GetScriptContext());
        Js::ES5Array* rawArray = Js::ES5Array::FromVar(var);

        Assert(rawArray->GetLength() > 0);

        rawArray->DirectGetItemAt(0, &var);
        Js::JavascriptString* str = Js::JavascriptString::FromVar(var);
        hash ^= DefaultComparer<const wchar_t*>::GetHashCode(str->GetSz());

        for (uint32 i = 1; i < rawArray->GetLength(); i++)
        {
            hash ^= DefaultComparer<const wchar_t*>::GetHashCode(L"${}");

            rawArray->DirectGetItemAt(i, &var);
            str = Js::JavascriptString::FromVar(var);
            hash ^= DefaultComparer<const wchar_t*>::GetHashCode(str->GetSz());
        }

        return hash;
    }

    DynamicType * JavascriptLibrary::GetObjectLiteralType(uint16 requestedInlineSlotCapacity)
    {
        if (requestedInlineSlotCapacity <= MaxPreInitializedObjectTypeInlineSlotCount)
        {
            return objectTypes[DynamicTypeHandler::RoundUpInlineSlotCapacity(requestedInlineSlotCapacity) / InlineSlotCountIncrement];
        }
        else
        {
            return objectTypes[PreInitializedObjectTypeCount - 1];
        }
    }

    DynamicType * JavascriptLibrary::GetObjectHeaderInlinedLiteralType(uint16 requestedInlineSlotCapacity)
    {
        Assert(requestedInlineSlotCapacity <= MaxPreInitializedObjectHeaderInlinedTypeInlineSlotCount);

        return
            objectHeaderInlinedTypes[
                (
                    DynamicTypeHandler::RoundUpObjectHeaderInlinedInlineSlotCapacity(requestedInlineSlotCapacity) -
                    DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity()
                    ) / InlineSlotCountIncrement];
    }

    HeapArgumentsObject* JavascriptLibrary::CreateHeapArguments(Var frameObj, uint32 formalCount)
    {
        AssertMsg(heapArgumentsType, "Where's heapArgumentsType?");

        Recycler *recycler = this->GetRecycler();

        if (!this->arrayPrototypeValuesFunction) //InitializeArrayPrototype can be delay loaded, which could prevent us from access to array.prototype.values
        {
            this->arrayPrototypeValuesFunction = DefaultCreateFunction(&JavascriptArray::EntryInfo::Values, 0, nullptr, nullptr, PropertyIds::values);
        }
        return RecyclerNew(recycler, HeapArgumentsObject, recycler, (ActivationObject*)frameObj, formalCount, heapArgumentsType);
    }

    JavascriptArray* JavascriptLibrary::CreateArray()
    {
        AssertMsg(arrayType, "Where's arrayType?");
        return JavascriptArray::New<Var, JavascriptArray>(this->GetRecycler(), arrayType);
    }

    JavascriptArray* JavascriptLibrary::CreateArray(uint32 length)
    {
        AssertMsg(arrayType, "Where's arrayType?");
        JavascriptArray* arr = JavascriptArray::New<Var, JavascriptArray, 0>(length, arrayType, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif
        return arr;
    }

    JavascriptArray *JavascriptLibrary::CreateArrayOnStack(void *const stackAllocationPointer)
    {
        return JavascriptArray::New<JavascriptArray, 0>(stackAllocationPointer, 0, arrayType);
    }

    JavascriptNativeIntArray* JavascriptLibrary::CreateNativeIntArray()
    {
        AssertMsg(nativeIntArrayType, "Where's nativeIntArrayType?");
        return JavascriptArray::New<int32, JavascriptNativeIntArray>(this->GetRecycler(), nativeIntArrayType);
    }

    JavascriptNativeIntArray* JavascriptLibrary::CreateNativeIntArray(uint32 length)
    {
        AssertMsg(nativeIntArrayType, "Where's nativeIntArrayType?");
        JavascriptNativeIntArray* arr = JavascriptArray::New<int32, JavascriptNativeIntArray, 0>(length, nativeIntArrayType, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif
        return arr;
    }

    JavascriptNativeFloatArray* JavascriptLibrary::CreateNativeFloatArray()
    {
        AssertMsg(nativeFloatArrayType, "Where's nativeFloatArrayType?");
        return JavascriptArray::New<double, JavascriptNativeFloatArray>(this->GetRecycler(), nativeFloatArrayType);
    }

    JavascriptNativeFloatArray* JavascriptLibrary::CreateNativeFloatArray(uint32 length)
    {
        AssertMsg(nativeFloatArrayType, "Where's nativeIntArrayType?");
        JavascriptNativeFloatArray* arr = JavascriptArray::New<double, JavascriptNativeFloatArray, 0>(length, nativeFloatArrayType, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif
        return arr;
    }

    JavascriptArray* JavascriptLibrary::CreateArrayLiteral(uint32 length)
    {
        AssertMsg(arrayType, "Where's arrayType?");
        JavascriptArray* arr = JavascriptArray::NewLiteral<Var, JavascriptArray, 0>(length, arrayType, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif
        return arr;
    }

    JavascriptNativeIntArray* JavascriptLibrary::CreateNativeIntArrayLiteral(uint32 length)
    {
        AssertMsg(nativeIntArrayType, "Where's arrayType?");
        JavascriptNativeIntArray* arr = JavascriptArray::NewLiteral<int32, JavascriptNativeIntArray, 0>(length, nativeIntArrayType, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

        return arr;
    }

#if ENABLE_COPYONACCESS_ARRAY
    JavascriptNativeIntArray* JavascriptLibrary::CreateCopyOnAccessNativeIntArrayLiteral(ArrayCallSiteInfo *arrayInfo, FunctionBody *functionBody, const Js::AuxArray<int32> *ints)
    {
        AssertMsg(copyOnAccessNativeIntArrayType, "Where's arrayType?");
        JavascriptNativeIntArray* arr = JavascriptArray::NewCopyOnAccessLiteral<int32, JavascriptCopyOnAccessNativeIntArray, 0>(copyOnAccessNativeIntArrayType, arrayInfo, functionBody, ints, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

        return arr;
    }
#endif

    JavascriptNativeFloatArray* JavascriptLibrary::CreateNativeFloatArrayLiteral(uint32 length)
    {
        AssertMsg(nativeFloatArrayType, "Where's arrayType?");
        JavascriptNativeFloatArray* arr = JavascriptArray::NewLiteral<double, JavascriptNativeFloatArray, 0>(length, nativeFloatArrayType, this->GetRecycler());
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif
        return arr;
    }

    JavascriptArray* JavascriptLibrary::CreateArray(uint32 length, uint32 size)
    {
        AssertMsg(arrayType, "Where's arrayType?");
        JavascriptArray* arr = RecyclerNew(this->GetRecycler(), JavascriptArray, length, size, arrayType);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_ARRAY(arr));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif
        return arr;
    }

    ArrayBuffer* JavascriptLibrary::CreateArrayBuffer(uint32 length)
    {
        ArrayBuffer* arr = JavascriptArrayBuffer::Create(length, arrayBufferType);
        return arr;
    }

    ArrayBuffer* JavascriptLibrary::CreateArrayBuffer(byte* buffer, uint32 length)
    {
        ArrayBuffer* arr = JavascriptArrayBuffer::Create(buffer, length, arrayBufferType);
        return arr;
    }

    ArrayBuffer* JavascriptLibrary::CreateProjectionArraybuffer(uint32 length)
    {
        ArrayBuffer* arr = ProjectionArrayBuffer::Create(length, arrayBufferType);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(arr));
        return arr;
    }

    ArrayBuffer* JavascriptLibrary::CreateProjectionArraybuffer(byte* buffer, uint32 length)
    {
        ArrayBuffer* arr = ProjectionArrayBuffer::Create(buffer, length, arrayBufferType);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(arr));
        return arr;
    }

    DataView* JavascriptLibrary::CreateDataView(ArrayBuffer* arrayBuffer, uint32 offset, uint32 length)
    {
        DataView* dataView = RecyclerNew(this->GetRecycler(), DataView, arrayBuffer, offset, length, dataViewType);

        return dataView;
    }

    JavascriptBoolean* JavascriptLibrary::CreateBoolean(BOOL value)
    {
        AssertMsg(booleanTrue, "Where's booleanTrue?");
        AssertMsg(booleanFalse, "Where's booleanFalse?");
        return value ? booleanTrue : booleanFalse;
    }

    JavascriptDate* JavascriptLibrary::CreateDate()
    {
        AssertMsg(dateType, "Where's dateType?");
        return RecyclerNew(this->GetRecycler(), JavascriptDate, 0, dateType);
    }

    JavascriptDate* JavascriptLibrary::CreateDate(double value)
    {
        AssertMsg(dateType, "Where's dateType?");
        return RecyclerNew(this->GetRecycler(), JavascriptDate, value, dateType);
    }

    JavascriptDate* JavascriptLibrary::CreateDate(SYSTEMTIME* pst)
    {
        AssertMsg(dateType, "Where's dateType?");
        double value = DateImplementation::TimeFromSt(pst);
        return CreateDate(value);
    }

    JavascriptMap* JavascriptLibrary::CreateMap()
    {
        AssertMsg(mapType, "Where's mapType?");
        return RecyclerNew(this->GetRecycler(), JavascriptMap, mapType);
    }

    JavascriptSet* JavascriptLibrary::CreateSet()
    {
        AssertMsg(setType, "Where's setType?");
        return RecyclerNew(this->GetRecycler(), JavascriptSet, setType);
    }

    JavascriptWeakMap* JavascriptLibrary::CreateWeakMap()
    {
        AssertMsg(weakMapType, "Where's weakMapType?");
        return RecyclerNewFinalized(this->GetRecycler(), JavascriptWeakMap, weakMapType);
    }

    JavascriptWeakSet* JavascriptLibrary::CreateWeakSet()
    {
        AssertMsg(weakSetType, "Where's weakSetType?");
        return RecyclerNewFinalized(this->GetRecycler(), JavascriptWeakSet, weakSetType);
    }

    JavascriptPromise* JavascriptLibrary::CreatePromise()
    {
        AssertMsg(promiseType, "Where's promiseType?");
        return RecyclerNew(this->GetRecycler(), JavascriptPromise, promiseType);
    }

    JavascriptPromiseAsyncSpawnExecutorFunction* JavascriptLibrary::CreatePromiseAsyncSpawnExecutorFunction(JavascriptMethod entryPoint, JavascriptGenerator* generatorFunction, Var target)
    {
        FunctionInfo* functionInfo = RecyclerNew(this->GetRecycler(), FunctionInfo, entryPoint);
        DynamicType* type = CreateDeferredPrototypeFunctionType(this->inDispatchProfileMode ? ProfileEntryThunk : entryPoint);
        JavascriptPromiseAsyncSpawnExecutorFunction* function = EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseAsyncSpawnExecutorFunction, type, functionInfo, generatorFunction, target));

        return function;
    }

    JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* JavascriptLibrary::CreatePromiseAsyncSpawnStepArgumentExecutorFunction(JavascriptMethod entryPoint, JavascriptGenerator* generator, Var argument, JavascriptFunction* resolve, JavascriptFunction* reject, bool isReject)
    {
        FunctionInfo* functionInfo = RecyclerNew(this->GetRecycler(), FunctionInfo, entryPoint);
        DynamicType* type = CreateDeferredPrototypeFunctionType(this->inDispatchProfileMode ? ProfileEntryThunk : entryPoint);
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* function = EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction, type, functionInfo, generator, argument, resolve, reject, isReject));

        return function;
    }

    JavascriptGenerator* JavascriptLibrary::CreateGenerator(Arguments& args, ScriptFunction* scriptFunction, RecyclableObject* prototype)
    {
        Assert(scriptContext->GetConfig()->IsES6GeneratorsEnabled());
        DynamicType* generatorType = CreateGeneratorType(prototype);
        return RecyclerNew(this->GetRecycler(), JavascriptGenerator, generatorType, args, scriptFunction);
    }

    JavascriptError* JavascriptLibrary::CreateError()
    {
        AssertMsg(errorType, "Where's errorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, errorType);
        JavascriptError::SetErrorType(pError, kjstError);
        return pError;
    }

    JavascriptSymbol* JavascriptLibrary::CreateSymbol(JavascriptString* description)
    {
        return this->CreateSymbol(description->GetString(), (int)description->GetLength());
    }

    JavascriptSymbol* JavascriptLibrary::CreateSymbol(const wchar_t* description, int descriptionLength)
    {
        ENTER_PINNED_SCOPE(const Js::PropertyRecord, propertyRecord);

        propertyRecord = this->scriptContext->GetThreadContext()->UncheckedAddPropertyId(description, descriptionLength, /*bind*/false, /*isSymbol*/true);

        LEAVE_PINNED_SCOPE();

        return this->CreateSymbol(propertyRecord);
    }

    JavascriptSymbol* JavascriptLibrary::CreateSymbol(const PropertyRecord* propertyRecord)
    {
        AssertMsg(symbolTypeStatic, "Where's symbolTypeStatic?");
        return RecyclerNew(this->GetRecycler(), JavascriptSymbol, propertyRecord, symbolTypeStatic);
    }

    JavascriptError* JavascriptLibrary::CreateExternalError(ErrorTypeEnum errorTypeEnum)
    {
        DynamicType* baseErrorType = NULL;
        switch (errorTypeEnum)
        {
        case kjstError:
        default:
            baseErrorType = errorType;
            break;
        case kjstRangeError:
            baseErrorType = rangeErrorType;
            break;
        case kjstReferenceError:
            baseErrorType = referenceErrorType;
            break;
        case kjstSyntaxError:
            baseErrorType = syntaxErrorType;
            break;
        case kjstTypeError:
            baseErrorType = typeErrorType;
            break;
        case kjstURIError:
            baseErrorType = uriErrorType;
            break;
        }

        JavascriptError *pError = RecyclerNew(recycler, JavascriptError, baseErrorType, TRUE);
        JavascriptError::SetErrorType(pError, errorTypeEnum);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateEvalError()
    {
        AssertMsg(evalErrorType, "Where's evalErrorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, evalErrorType);
        JavascriptError::SetErrorType(pError, kjstEvalError);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateRangeError()
    {
        AssertMsg(rangeErrorType, "Where's rangeErrorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, rangeErrorType);
        JavascriptError::SetErrorType(pError, kjstRangeError);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateReferenceError()
    {
        AssertMsg(referenceErrorType, "Where's referenceErrorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, referenceErrorType);
        JavascriptError::SetErrorType(pError, kjstReferenceError);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateSyntaxError()
    {
        AssertMsg(syntaxErrorType, "Where's syntaxErrorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, syntaxErrorType);
        JavascriptError::SetErrorType(pError, kjstSyntaxError);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateTypeError()
    {
        AssertMsg(typeErrorType, "Where's typeErrorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, typeErrorType);
        JavascriptError::SetErrorType(pError, kjstTypeError);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateURIError()
    {
        AssertMsg(uriErrorType, "Where's uriErrorType?");
        JavascriptError *pError = RecyclerNew(this->GetRecycler(), JavascriptError, uriErrorType);
        JavascriptError::SetErrorType(pError, kjstURIError);
        return pError;
    }

    JavascriptError* JavascriptLibrary::CreateStackOverflowError()
    {
#if DBG
        // If we are doing a heap enum, we need to be able to allocate the error object.
        Recycler::AutoAllowAllocationDuringHeapEnum autoAllowAllocationDuringHeapEnum(this->GetRecycler());
#endif

        JavascriptError* stackOverflowError = scriptContext->GetLibrary()->CreateError();
        JavascriptError::SetErrorMessage(stackOverflowError, VBSERR_OutOfStack, NULL, scriptContext);
        return stackOverflowError;
    }

    JavascriptError* JavascriptLibrary::CreateOutOfMemoryError()
    {
        JavascriptError* outOfMemoryError = scriptContext->GetLibrary()->CreateError();
        JavascriptError::SetErrorMessage(outOfMemoryError, VBSERR_OutOfMemory, NULL, scriptContext);
        return outOfMemoryError;
    }

    JavascriptFunction* JavascriptLibrary::CreateNonProfiledFunction(FunctionInfo * functionInfo)
    {
        Assert(functionInfo->GetAttributes() & FunctionInfo::DoNotProfile);
        return EnsureReadyIfHybridDebugging(RecyclerNew(this->GetRecycler(), RuntimeFunction,
            CreateDeferredPrototypeFunctionTypeNoProfileThunk(functionInfo->GetOriginalEntryPoint()),
            functionInfo));
    }

    ScriptFunction* JavascriptLibrary::CreateScriptFunction(FunctionProxy * proxy)
    {
        ScriptFunctionType* deferredPrototypeType = proxy->EnsureDeferredPrototypeType();
        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, ScriptFunction, proxy, deferredPrototypeType));
    }

    AsmJsScriptFunction* JavascriptLibrary::CreateAsmJsScriptFunction(FunctionProxy * proxy)
    {
        ScriptFunctionType* deferredPrototypeType = proxy->EnsureDeferredPrototypeType();
        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, AsmJsScriptFunction, proxy, deferredPrototypeType));
    }

    ScriptFunctionWithInlineCache* JavascriptLibrary::CreateScriptFunctionWithInlineCache(FunctionProxy * proxy)
    {
        ScriptFunctionType* deferredPrototypeType = proxy->EnsureDeferredPrototypeType();
        return EnsureReadyIfHybridDebugging(RecyclerNewWithInfoBits(this->GetRecycler(), (Memory::ObjectInfoBits)(EnumFunctionClass | Memory::FinalizableObjectBits), ScriptFunctionWithInlineCache, proxy, deferredPrototypeType));
    }

    GeneratorVirtualScriptFunction* JavascriptLibrary::CreateGeneratorVirtualScriptFunction(FunctionProxy * proxy)
    {
        ScriptFunctionType* deferredPrototypeType = proxy->EnsureDeferredPrototypeType();
        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, GeneratorVirtualScriptFunction, proxy, deferredPrototypeType));
    }

    DynamicType * JavascriptLibrary::CreateGeneratorType(RecyclableObject* prototype)
    {
        return DynamicType::New(scriptContext, TypeIds_Generator, prototype, nullptr, NullTypeHandler<false>::GetDefaultInstance());
    }

    template <class MethodType>
    JavascriptExternalFunction* JavascriptLibrary::CreateIdMappedExternalFunction(MethodType entryPoint, DynamicType *pPrototypeType)
    {
        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptExternalFunction, entryPoint, pPrototypeType));
    }

    JavascriptGeneratorFunction* JavascriptLibrary::CreateGeneratorFunction(JavascriptMethod entryPoint, GeneratorVirtualScriptFunction* scriptFunction)
    {
        Assert(scriptContext->GetConfig()->IsES6GeneratorsEnabled());

        DynamicType* type = CreateDeferredPrototypeGeneratorFunctionType(entryPoint, scriptFunction->IsAnonymousFunction());

        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptGeneratorFunction, type, scriptFunction));
    }

    JavascriptExternalFunction* JavascriptLibrary::CreateStdCallExternalFunction(StdCallJavascriptMethod entryPoint, PropertyId nameId, void *callbackState)
    {
        Assert(nameId == 0 || scriptContext->IsTrackedPropertyId(nameId));
        return CreateStdCallExternalFunction(entryPoint, TaggedInt::ToVarUnchecked(nameId), callbackState);
    }

    JavascriptExternalFunction* JavascriptLibrary::CreateStdCallExternalFunction(StdCallJavascriptMethod entryPoint, Var nameId, void *callbackState)
    {
        JavascriptExternalFunction* function = EnsureReadyIfHybridDebugging(this->CreateIdMappedExternalFunction(entryPoint, stdCallFunctionWithDeferredPrototypeType));
        function->SetFunctionNameId(nameId);
        function->SetCallbackState(callbackState);

        return function;
    }

    JavascriptPromiseCapabilitiesExecutorFunction* JavascriptLibrary::CreatePromiseCapabilitiesExecutorFunction(JavascriptMethod entryPoint, JavascriptPromiseCapability* capability)
    {
        Assert(scriptContext->GetConfig()->IsES6PromiseEnabled());

        FunctionInfo* functionInfo = RecyclerNew(this->GetRecycler(), FunctionInfo, entryPoint);
        DynamicType* type = CreateDeferredPrototypeFunctionType(entryPoint);
        JavascriptPromiseCapabilitiesExecutorFunction* function = EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseCapabilitiesExecutorFunction, type, functionInfo, capability));

        function->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(2), PropertyConfigurable, nullptr);

        return function;
    }

    JavascriptPromiseResolveOrRejectFunction* JavascriptLibrary::CreatePromiseResolveOrRejectFunction(JavascriptMethod entryPoint, JavascriptPromise* promise, bool isReject, JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord)
    {
        Assert(scriptContext->GetConfig()->IsES6PromiseEnabled());

        FunctionInfo* functionInfo = &Js::JavascriptPromise::EntryInfo::ResolveOrRejectFunction;
        DynamicType* type = CreateDeferredPrototypeFunctionType(entryPoint);
        JavascriptPromiseResolveOrRejectFunction* function = EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseResolveOrRejectFunction, type, functionInfo, promise, isReject, alreadyResolvedRecord));

        function->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyConfigurable, nullptr);

        return function;
    }

    JavascriptPromiseReactionTaskFunction* JavascriptLibrary::CreatePromiseReactionTaskFunction(JavascriptMethod entryPoint, JavascriptPromiseReaction* reaction, Var argument)
    {
        Assert(scriptContext->GetConfig()->IsES6PromiseEnabled());

        FunctionInfo* functionInfo = RecyclerNew(this->GetRecycler(), FunctionInfo, entryPoint);
        DynamicType* type = CreateDeferredPrototypeFunctionType(entryPoint);

        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseReactionTaskFunction, type, functionInfo, reaction, argument));
    }

    JavascriptPromiseResolveThenableTaskFunction* JavascriptLibrary::CreatePromiseResolveThenableTaskFunction(JavascriptMethod entryPoint, JavascriptPromise* promise, RecyclableObject* thenable, RecyclableObject* thenFunction)
    {
        Assert(scriptContext->GetConfig()->IsES6PromiseEnabled());

        FunctionInfo* functionInfo = RecyclerNew(this->GetRecycler(), FunctionInfo, entryPoint);
        DynamicType* type = CreateDeferredPrototypeFunctionType(entryPoint);

        return EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseResolveThenableTaskFunction, type, functionInfo, promise, thenable, thenFunction));
    }

    JavascriptPromiseAllResolveElementFunction* JavascriptLibrary::CreatePromiseAllResolveElementFunction(JavascriptMethod entryPoint, uint32 index, JavascriptArray* values, JavascriptPromiseCapability* capabilities, JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElements)
    {
        Assert(scriptContext->GetConfig()->IsES6PromiseEnabled());

        FunctionInfo* functionInfo = &Js::JavascriptPromise::EntryInfo::AllResolveElementFunction;
        DynamicType* type = CreateDeferredPrototypeFunctionType(entryPoint);
        JavascriptPromiseAllResolveElementFunction* function = EnsureReadyIfHybridDebugging(RecyclerNewEnumClass(this->GetRecycler(), EnumFunctionClass, JavascriptPromiseAllResolveElementFunction, type, functionInfo, index, values, capabilities, remainingElements));

        function->SetPropertyWithAttributes(PropertyIds::length, TaggedInt::ToVarUnchecked(1), PropertyConfigurable, nullptr);

        return function;
    }

    JavascriptExternalFunction* JavascriptLibrary::CreateWrappedExternalFunction(JavascriptExternalFunction* wrappedFunction)
    {
        // The wrapped function will have profiling, so the wrapper function does not need it.
        JavascriptExternalFunction* function = EnsureReadyIfHybridDebugging(RecyclerNew(this->GetRecycler(), JavascriptExternalFunction, wrappedFunction, wrappedFunctionWithDeferredPrototypeType));
        function->SetFunctionNameId(wrappedFunction->GetSourceString());

        return function;
    }

#if !FLOATVAR
    JavascriptNumber * JavascriptLibrary::CreateNumber(double value, RecyclerJavascriptNumberAllocator * numberAllocator)
    {
        AssertMsg(numberTypeStatic, "Where's numberTypeStatic?");
        return AllocatorNew(RecyclerJavascriptNumberAllocator, numberAllocator, JavascriptNumber, value, numberTypeStatic);
    }

#if ENABLE_NATIVE_CODEGEN
    JavascriptNumber* JavascriptLibrary::CreateCodeGenNumber(CodeGenNumberAllocator * alloc, double value)
    {
        AssertMsg(numberTypeStatic, "Where's numberTypeStatic?");
        return new (alloc->Alloc()) JavascriptNumber(value, numberTypeStatic);
    }
#endif

#endif

    DynamicObject* JavascriptLibrary::CreateGeneratorConstructorPrototypeObject()
    {
        AssertMsg(generatorConstructorPrototypeObjectType, "Where's generatorConstructorPrototypeObjectType?");
        DynamicObject * prototype = DynamicObject::New(this->GetRecycler(), generatorConstructorPrototypeObjectType);
        // Generator functions' prototype objects are not created with a .constructor property
        return prototype;
    }

    DynamicObject* JavascriptLibrary::CreateConstructorPrototypeObject(JavascriptFunction * constructor)
    {
        AssertMsg(constructorPrototypeObjectType, "Where's constructorPrototypeObjectType?");
        DynamicObject * prototype = DynamicObject::New(this->GetRecycler(), constructorPrototypeObjectType);
        AddMember(prototype, PropertyIds::constructor, constructor);
        return prototype;
    }

    DynamicObject* JavascriptLibrary::CreateObject(
        const bool allowObjectHeaderInlining,
        const PropertyIndex requestedInlineSlotCapacity)
    {
        Assert(GetObjectType());
        Assert(GetObjectHeaderInlinedType());

        const bool useObjectHeaderInlining =
            allowObjectHeaderInlining && FunctionBody::DoObjectHeaderInliningForObjectLiteral(requestedInlineSlotCapacity);
        DynamicType *const type =
            useObjectHeaderInlining
            ? GetObjectHeaderInlinedLiteralType(requestedInlineSlotCapacity)
            : GetObjectLiteralType(requestedInlineSlotCapacity);
        return DynamicObject::New(GetRecycler(), type);
    }

    DynamicObject* JavascriptLibrary::CreateObject(DynamicTypeHandler * typeHandler)
    {
        return DynamicObject::New(this->GetRecycler(),
            Js::DynamicType::New(scriptContext, Js::TypeIds_Object, this->GetObjectPrototype(),
                RecyclableObject::DefaultEntryPoint, typeHandler, false, false));
    }

    DynamicType* JavascriptLibrary::CreateObjectType(RecyclableObject* prototype, Js::TypeId typeId, uint16 requestedInlineSlotCapacity)
    {
        const bool useObjectHeaderInlining = FunctionBody::DoObjectHeaderInliningForConstructor(requestedInlineSlotCapacity);
        const uint16 offsetOfInlineSlots =
            useObjectHeaderInlining
            ? DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots()
            : sizeof(DynamicObject);

        DynamicType* dynamicType = nullptr;
        const bool useCache = prototype->GetScriptContext() == this->scriptContext;
        if (useCache &&
            prototype->GetInternalProperty(prototype, Js::InternalPropertyIds::TypeOfPrototypObject, (Js::Var*) &dynamicType, nullptr, this->scriptContext))
        {
            //If the prototype is externalObject, then ExternalObject::Reinitialize can set all the properties to undefined in navigation scenario.
            //Check to make sure dynamicType which is stored as a Js::Var is not undefined.
            //See Blue 419324
            if (dynamicType != nullptr && (Js::Var)dynamicType != this->GetUndefined())
            {
                DynamicTypeHandler *const dynamicTypeHandler = dynamicType->GetTypeHandler();
                if (dynamicTypeHandler->IsObjectHeaderInlinedTypeHandler() == useObjectHeaderInlining &&
                    (
                        dynamicTypeHandler->GetInlineSlotCapacity() ==
                        (
                            useObjectHeaderInlining
                            ? DynamicTypeHandler::RoundUpObjectHeaderInlinedInlineSlotCapacity(requestedInlineSlotCapacity)
                            : DynamicTypeHandler::RoundUpInlineSlotCapacity(requestedInlineSlotCapacity)
                            )
                        ))
                {
                    Assert(dynamicType->GetIsShared());
                    return dynamicType;
                }
            }
        }

        SimplePathTypeHandler* typeHandler = SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, requestedInlineSlotCapacity, offsetOfInlineSlots, true, true);
        dynamicType = DynamicType::New(scriptContext, typeId, prototype, RecyclableObject::DefaultEntryPoint, typeHandler, true, true);

        if (useCache)
        {
            prototype->SetInternalProperty(Js::InternalPropertyIds::TypeOfPrototypObject, (Var)dynamicType, PropertyOperationFlags::PropertyOperation_Force, nullptr);
        }

        return dynamicType;
    }

    DynamicType* JavascriptLibrary::CreateObjectTypeNoCache(RecyclableObject* prototype, Js::TypeId typeId)
    {
        return DynamicType::New(scriptContext, typeId, prototype, RecyclableObject::DefaultEntryPoint,
            SimplePathTypeHandler::New(scriptContext, scriptContext->GetRootPath(), 0, 0, 0, true, true), true, true);
    }

    DynamicType* JavascriptLibrary::CreateObjectType(RecyclableObject* prototype, uint16 requestedInlineSlotCapacity)
    {
        // We can't reuse the type in objectType even if the prototype is the object prototype, because those has inline slot capacity fixed
        return CreateObjectType(prototype, TypeIds_Object, requestedInlineSlotCapacity);
    }

    DynamicObject* JavascriptLibrary::CreateObject(RecyclableObject* prototype, uint16 requestedInlineSlotCapacity)
    {
        Assert(JavascriptOperators::IsObject(prototype));

        DynamicType* dynamicType = CreateObjectType(prototype, requestedInlineSlotCapacity);
        return DynamicObject::New(this->GetRecycler(), dynamicType);
    }

    PropertyStringCacheMap* JavascriptLibrary::EnsurePropertyStringMap()
    {
        if (this->propertyStringMap == nullptr)
        {
            this->propertyStringMap = RecyclerNew(this->recycler, PropertyStringCacheMap, this->GetRecycler());
            this->scriptContext->RegisterWeakReferenceDictionary((JsUtil::IWeakReferenceDictionary*) this->propertyStringMap);
        }
        return this->propertyStringMap;
    }

    DynamicObject* JavascriptLibrary::CreateActivationObject()
    {
        AssertMsg(activationObjectType, "Where's activationObjectType?");
        return RecyclerNew(this->GetRecycler(), ActivationObject, activationObjectType);
    }

    DynamicObject* JavascriptLibrary::CreatePseudoActivationObject()
    {
        AssertMsg(activationObjectType, "Where's activationObjectType?");
        return RecyclerNew(this->GetRecycler(), PseudoActivationObject, activationObjectType);
    }

    DynamicObject* JavascriptLibrary::CreateBlockActivationObject()
    {
        AssertMsg(activationObjectType, "Where's activationObjectType?");
        return RecyclerNew(this->GetRecycler(), BlockActivationObject, activationObjectType);
    }

    DynamicObject* JavascriptLibrary::CreateConsoleScopeActivationObject()
    {
        AssertMsg(activationObjectType, "Where's activationObjectType?");
        return RecyclerNew(this->GetRecycler(), ConsoleScopeActivationObject, activationObjectType);
    }

    JavascriptString* JavascriptLibrary::GetEmptyString() const
    {
        AssertMsg(emptyString, "Where's emptyString?");
#ifdef PROFILE_STRINGS
        StringProfiler::RecordEmptyStringRequest(scriptContext);
#endif
        return emptyString;
    }

    PropertyString* JavascriptLibrary::CreatePropertyString(const Js::PropertyRecord* propertyRecord)
    {
        AssertMsg(stringTypeStatic, "Where's stringTypeStatic?");
        return PropertyString::New(stringTypeStatic, propertyRecord, this->GetRecycler());
    }

    PropertyString* JavascriptLibrary::CreatePropertyString(const Js::PropertyRecord* propertyRecord, ArenaAllocator *arena)
    {
        AssertMsg(stringTypeStatic, "Where's stringTypeStatic?");
        return PropertyString::New(stringTypeStatic, propertyRecord, arena);
    }

    JavascriptVariantDate* JavascriptLibrary::CreateVariantDate(const double value)
    {
        AssertMsg(variantDateType, "Where's variantDateType?");
        return RecyclerNewLeafZ(this->GetRecycler(), JavascriptVariantDate, value, variantDateType);
    }

    JavascriptBooleanObject* JavascriptLibrary::CreateBooleanObject()
    {
        AssertMsg(booleanTypeDynamic, "Where's booleanTypeDynamic?");
        return RecyclerNew(this->GetRecycler(), JavascriptBooleanObject, nullptr, booleanTypeDynamic);
    }

    JavascriptBooleanObject* JavascriptLibrary::CreateBooleanObject(BOOL value)
    {
        AssertMsg(booleanTypeDynamic, "Where's booleanTypeDynamic?");
        return RecyclerNew(this->GetRecycler(), JavascriptBooleanObject, CreateBoolean(value), booleanTypeDynamic);
    }

    JavascriptSymbolObject* JavascriptLibrary::CreateSymbolObject(JavascriptSymbol* value)
    {
        AssertMsg(symbolTypeDynamic, "Where's symbolTypeDynamic?");
        return RecyclerNew(this->GetRecycler(), JavascriptSymbolObject, value, symbolTypeDynamic);
    }

    JavascriptNumberObject* JavascriptLibrary::CreateNumberObject(Var number)
    {
        AssertMsg(numberTypeDynamic, "Where's numberTypeDynamic?");
        return RecyclerNew(this->GetRecycler(), JavascriptNumberObject, number, numberTypeDynamic);
    }

    JavascriptNumberObject* JavascriptLibrary::CreateNumberObjectWithCheck(double value)
    {
        return CreateNumberObject(JavascriptNumber::ToVarWithCheck(value, scriptContext));
    }

    JavascriptStringObject* JavascriptLibrary::CreateStringObject(JavascriptString* value)
    {
        AssertMsg(stringTypeDynamic, "Where's stringTypeDynamic?");
        return RecyclerNew(this->GetRecycler(), JavascriptStringObject, value, stringTypeDynamic);
    }

    JavascriptStringObject* JavascriptLibrary::CreateStringObject(const wchar_t* value, charcount_t length)
    {
        AssertMsg(stringTypeDynamic, "Where's stringTypeDynamic?");
        return RecyclerNew(this->GetRecycler(), JavascriptStringObject,
            Js::JavascriptString::NewWithBuffer(value, length, scriptContext), stringTypeDynamic);
    }

    JavascriptRegExp* JavascriptLibrary::CreateRegExp(UnifiedRegex::RegexPattern* pattern)
    {
        AssertMsg(regexType, "Where's regexType?");
        return RecyclerNew(this->GetRecycler(), JavascriptRegExp, pattern, regexType);
    }

    JavascriptArrayIterator* JavascriptLibrary::CreateArrayIterator(Var iterable, JavascriptArrayIteratorKind kind)
    {
        AssertMsg(arrayIteratorType, "Where's arrayIteratorType");
        return RecyclerNew(this->GetRecycler(), JavascriptArrayIterator, arrayIteratorType, iterable, kind);
    }

    JavascriptMapIterator* JavascriptLibrary::CreateMapIterator(JavascriptMap* map, JavascriptMapIteratorKind kind)
    {
        AssertMsg(mapIteratorType, "Where's mapIteratorType");
        return RecyclerNew(this->GetRecycler(), JavascriptMapIterator, mapIteratorType, map, kind);
    }

    JavascriptSetIterator* JavascriptLibrary::CreateSetIterator(JavascriptSet* set, JavascriptSetIteratorKind kind)
    {
        AssertMsg(setIteratorType, "Where's setIteratorType");
        return RecyclerNew(this->GetRecycler(), JavascriptSetIterator, setIteratorType, set, kind);
    }

    JavascriptStringIterator* JavascriptLibrary::CreateStringIterator(JavascriptString* string)
    {
        AssertMsg(stringIteratorType, "Where's stringIteratorType");
        return RecyclerNew(this->GetRecycler(), JavascriptStringIterator, stringIteratorType, string);
    }

    DynamicObject* JavascriptLibrary::CreateIteratorResultObject(Var value, Var done)
    {
        DynamicObject* iteratorResult = DynamicObject::New(this->GetRecycler(), iteratorResultType);

        iteratorResult->SetSlot(SetSlotArguments(Js::PropertyIds::value, 0, value));
        iteratorResult->SetSlot(SetSlotArguments(Js::PropertyIds::done, 1, done));

        return iteratorResult;
    }

    DynamicObject* JavascriptLibrary::CreateIteratorResultObjectValueFalse(Var value)
    {
        return CreateIteratorResultObject(value, GetFalse());
    }

    DynamicObject* JavascriptLibrary::CreateIteratorResultObjectUndefinedTrue()
    {
        return CreateIteratorResultObject(GetUndefined(), GetTrue());
    }

    RecyclableObject* JavascriptLibrary::CreateThrowErrorObject(JavascriptError* error)
    {
        return ThrowErrorObject::New(this->throwErrorObjectType, error, this->GetRecycler());
    }

    void JavascriptLibrary::SetForInEnumeratorCache(ForInObjectEnumerator* enumerator)
    {
        Assert(enumerator);
        this->cachedForInEnumerator = enumerator->GetWeakReference(this->recycler);
    }

    ForInObjectEnumerator* JavascriptLibrary::GetAndClearForInEnumeratorCache()
    {
        auto cachedEnumerator = this->cachedForInEnumerator;
        if (cachedEnumerator)
        {
            ForInObjectEnumerator * enumerator = cachedEnumerator->Get();
            this->cachedForInEnumerator = nullptr;
            return enumerator;
        }
        return nullptr;
    }

#if ENABLE_COPYONACCESS_ARRAY
    bool JavascriptLibrary::IsCopyOnAccessArrayCallSite(JavascriptLibrary *lib, ArrayCallSiteInfo *arrayInfo, uint32 length)
    {
        return
            lib->cacheForCopyOnAccessArraySegments
            && lib->cacheForCopyOnAccessArraySegments->IsNotOverHardLimit()
            && (
                PHASE_FORCE1(CopyOnAccessArrayPhase)  // -force:copyonaccessarray is only restricted by hard limit of the segment cache
                || (
                    !arrayInfo->isNotCopyOnAccessArray        // from profile
                    && !PHASE_OFF1(CopyOnAccessArrayPhase)
                    && lib->cacheForCopyOnAccessArraySegments->IsNotFull()  // cache size soft limit through -copyonaccessarraysegmentcachesize:<number>
                    && length <= (uint32)CONFIG_FLAG(MaxCopyOnAccessArrayLength)  // -maxcopyonaccessarraylength:<number>
                    && length >= (uint32)CONFIG_FLAG(MinCopyOnAccessArrayLength)  // -mincopyonaccessarraylength:<number>
                    )
                );
    }

    bool JavascriptLibrary::IsCachedCopyOnAccessArrayCallSite(const JavascriptLibrary *lib, ArrayCallSiteInfo *arrayInfo)
    {
        return lib->cacheForCopyOnAccessArraySegments
            && lib->cacheForCopyOnAccessArraySegments->IsValidIndex(arrayInfo->copyOnAccessArrayCacheIndex);
    }
#endif

    // static
    bool JavascriptLibrary::IsTypedArrayConstructor(Var constructor, ScriptContext* scriptContext)
    {
        JavascriptLibrary* library = scriptContext->GetLibrary();
        return constructor == library->GetInt8ArrayConstructor()
            || constructor == library->GetUint8ArrayConstructor()
            || constructor == library->GetUint8ClampedArrayConstructor()
            || constructor == library->GetInt16ArrayConstructor()
            || constructor == library->GetUint16ArrayConstructor()
            || constructor == library->GetInt32ArrayConstructor()
            || constructor == library->GetUint32ArrayConstructor()
            || constructor == library->GetFloat32ArrayConstructor()
            || constructor == library->GetFloat64ArrayConstructor();
    }

    JavascriptFunction ** JavascriptLibrary::GetBuiltinFunctions()
    {
        AssertMsg(this->builtinFunctions, "builtinFunctions table must've been initialized as part of library initialization!");
        return this->builtinFunctions;
    }

    INT_PTR* JavascriptLibrary::GetVTableAddresses()
    {
        AssertMsg(this->vtableAddresses, "vtableAddresses table must've been initialized as part of library initialization!");
        return this->vtableAddresses;
    }

#if ENABLE_NATIVE_CODEGEN
    //static
    BuiltinFunction JavascriptLibrary::GetBuiltInInlineCandidateId(OpCode opCode)
    {
        switch (opCode)
        {
        case OpCode::InlineMathAcos:
            return BuiltinFunction::Math_Acos;

        case OpCode::InlineMathAsin:
            return BuiltinFunction::Math_Asin;

        case OpCode::InlineMathAtan:
            return BuiltinFunction::Math_Atan;

        case OpCode::InlineMathAtan2:
            return BuiltinFunction::Math_Atan2;

        case OpCode::InlineMathCos:
            return BuiltinFunction::Math_Cos;

        case OpCode::InlineMathExp:
            return BuiltinFunction::Math_Exp;

        case OpCode::InlineMathLog:
            return BuiltinFunction::Math_Log;

        case OpCode::InlineMathPow:
            return BuiltinFunction::Math_Pow;

        case OpCode::InlineMathSin:
            return BuiltinFunction::Math_Sin;

        case OpCode::InlineMathSqrt:
            return BuiltinFunction::Math_Sqrt;

        case OpCode::InlineMathTan:
            return BuiltinFunction::Math_Tan;

        case OpCode::InlineMathAbs:
            return BuiltinFunction::Math_Abs;

        case OpCode::InlineMathClz32:
            return BuiltinFunction::Math_Clz32;

        case OpCode::InlineMathCeil:
            return BuiltinFunction::Math_Ceil;

        case OpCode::InlineMathFloor:
            return BuiltinFunction::Math_Floor;

        case OpCode::InlineMathMax:
            return BuiltinFunction::Math_Max;

        case OpCode::InlineMathMin:
            return BuiltinFunction::Math_Min;

        case OpCode::InlineMathImul:
            return BuiltinFunction::Math_Imul;

        case OpCode::InlineMathRandom:
            return BuiltinFunction::Math_Random;

        case OpCode::InlineMathRound:
            return BuiltinFunction::Math_Round;

        case OpCode::InlineMathFround:
            return BuiltinFunction::Math_Fround;

        case OpCode::InlineStringCharAt:
            return BuiltinFunction::String_CharAt;

        case OpCode::InlineStringCharCodeAt:
            return BuiltinFunction::String_CharCodeAt;

        case OpCode::InlineStringCodePointAt:
            return BuiltinFunction::String_CodePointAt;

        case OpCode::InlineArrayPop:
            return BuiltinFunction::Array_Pop;

        case OpCode::InlineArrayPush:
            return BuiltinFunction::Array_Push;

        case OpCode::InlineFunctionApply:
            return BuiltinFunction::Function_Apply;

        case OpCode::InlineFunctionCall:
            return BuiltinFunction::Function_Call;

        case OpCode::InlineRegExpExec:
            return BuiltinFunction::RegExp_Exec;

        }

        return BuiltinFunction::None;
    }
#endif

    // Parses given flags and arg kind (dst or src1, or src2) returns the type the arg must be type-specialized to.
    // static
    BuiltInArgSpecizationType JavascriptLibrary::GetBuiltInArgType(BuiltInFlags flags, BuiltInArgShift argKind)
    {
        Assert(argKind == BuiltInArgShift::BIAS_Dst || BuiltInArgShift::BIAS_Src1 || BuiltInArgShift::BIAS_Src2);

        BuiltInArgSpecizationType type = static_cast<BuiltInArgSpecizationType>(
            (flags >> argKind) &              // Shift-out everything to the right of start of interesting area.
            ((1 << Js::BIAS_ArgSize) - 1));   // Mask-out everything to the left of interesting area.

        return type;
    }

    // Register for profiler
#define DEFINE_OBJECT_NAME(object) const wchar_t *pwszObjectName = L#object;

#define REGISTER_OBJECT(object)\
    if (FAILED(hr = this->ProfilerRegister##object()))\
    {\
    return hr; \
    }\

#define REG_LIB_FUNC_CORE(pwszObjectName, pwszFunctionName, functionPropertyId, entryPoint)\
    if (FAILED(hr = this->GetScriptContext()->RegisterLibraryFunction(pwszObjectName, pwszFunctionName, functionPropertyId, entryPoint)))\
    {\
    return hr; \
    }\

#define REG_OBJECTS_DYNAMIC_LIB_FUNC(pwszFunctionName, nFuncNameLen, entryPoint) {\
    Js::PropertyRecord const * propRecord; \
    this->GetScriptContext()->GetOrAddPropertyRecord(pwszFunctionName, nFuncNameLen, &propRecord); \
    REG_LIB_FUNC_CORE(pwszObjectName, pwszFunctionName, propRecord->GetPropertyId(), entryPoint)\
}

#define REG_LIB_FUNC(pwszObjectName, functionPropertyId, entryPoint)\
    REG_LIB_FUNC_CORE(pwszObjectName, L#functionPropertyId, PropertyIds::##functionPropertyId, entryPoint)\

#define REG_OBJECTS_LIB_FUNC(functionPropertyId, entryPoint)\
    REG_LIB_FUNC(pwszObjectName, functionPropertyId, entryPoint)\

#define REG_OBJECTS_LIB_FUNC2(functionPropertyId, pwszFunctionPropertyName, entryPoint)\
    REG_LIB_FUNC_CORE(pwszObjectName, pwszFunctionPropertyName, PropertyIds::##functionPropertyId, entryPoint)\

#define REG_GLOBAL_LIB_FUNC(functionPropertyId, entryPoint)\
    REG_LIB_FUNC(NULL, functionPropertyId, entryPoint)\

#define REG_GLOBAL_CONSTRUCTOR(functionPropertyId)\
    REG_GLOBAL_LIB_FUNC(functionPropertyId, Javascript##functionPropertyId##::NewInstance)\

#define REGISTER_ERROR_OBJECT(functionPropertyId)\
    REG_GLOBAL_LIB_FUNC(functionPropertyId, JavascriptError::New##functionPropertyId##Instance)\
    REG_LIB_FUNC(L#functionPropertyId, toString, JavascriptError::EntryToString)\

    HRESULT JavascriptLibrary::ProfilerRegisterBuiltIns()
    {
        HRESULT hr = S_OK;

        // Register functions directly in global scope
        REG_GLOBAL_LIB_FUNC(eval, GlobalObject::EntryEval);
        REG_GLOBAL_LIB_FUNC(parseInt, GlobalObject::EntryParseInt);
        REG_GLOBAL_LIB_FUNC(parseFloat, GlobalObject::EntryParseFloat);
        REG_GLOBAL_LIB_FUNC(isNaN, GlobalObject::EntryIsNaN);
        REG_GLOBAL_LIB_FUNC(isFinite, GlobalObject::EntryIsFinite);
        REG_GLOBAL_LIB_FUNC(decodeURI, GlobalObject::EntryDecodeURI);
        REG_GLOBAL_LIB_FUNC(decodeURIComponent, GlobalObject::EntryDecodeURIComponent);
        REG_GLOBAL_LIB_FUNC(encodeURI, GlobalObject::EntryEncodeURI);
        REG_GLOBAL_LIB_FUNC(encodeURIComponent, GlobalObject::EntryEncodeURIComponent);
        REG_GLOBAL_LIB_FUNC(escape, GlobalObject::EntryEscape);
        REG_GLOBAL_LIB_FUNC(unescape, GlobalObject::EntryUnEscape);

        ScriptConfiguration const& config = *(scriptContext->GetConfig());
        if (config.SupportsCollectGarbage())
        {
            REG_GLOBAL_LIB_FUNC(CollectGarbage, GlobalObject::EntryCollectGarbage);
        }

        // Register constructors, prototypes and objects in global
        REGISTER_OBJECT(Object);
        REGISTER_OBJECT(Array);
        REGISTER_OBJECT(Boolean);
        REGISTER_OBJECT(Date);
        REGISTER_OBJECT(Function);
        REGISTER_OBJECT(Math);
        REGISTER_OBJECT(Number);
        REGISTER_OBJECT(String);
        REGISTER_OBJECT(RegExp);
        REGISTER_OBJECT(JSON);

        if (config.IsES6MapEnabled())
        {
            REGISTER_OBJECT(Map);
        }

        if (config.IsES6SetEnabled())
        {
            REGISTER_OBJECT(Set);
        }

        if (config.IsES6WeakMapEnabled())
        {
            REGISTER_OBJECT(WeakMap);
        }

        if (config.IsES6WeakSetEnabled())
        {
            REGISTER_OBJECT(WeakSet);
        }

        if (config.IsES6SymbolEnabled())
        {
            REGISTER_OBJECT(Symbol);
        }

        if (config.IsES6IteratorsEnabled())
        {
            REGISTER_OBJECT(Iterator);
            REGISTER_OBJECT(ArrayIterator);
            REGISTER_OBJECT(MapIterator);
            REGISTER_OBJECT(SetIterator);
            REGISTER_OBJECT(StringIterator);
            REGISTER_OBJECT(EnumeratorIterator);
        }

        if (config.IsES6TypedArrayExtensionsEnabled())
        {
            REGISTER_OBJECT(TypedArray);
        }

        if (config.IsES6PromiseEnabled())
        {
            REGISTER_OBJECT(Promise);
        }

        if (config.IsES6ProxyEnabled())
        {
            REGISTER_OBJECT(Proxy);
            REGISTER_OBJECT(Reflect);
        }

#ifdef IR_VIEWER
        if (Js::Configuration::Global.flags.IsEnabled(Js::IRViewerFlag))
        {
            REGISTER_OBJECT(IRViewer);
        }
#endif /* IR_VIEWER */

        // Error Constructors and prototypes
        REGISTER_ERROR_OBJECT(Error);
        REGISTER_ERROR_OBJECT(EvalError);
        REGISTER_ERROR_OBJECT(RangeError);
        REGISTER_ERROR_OBJECT(ReferenceError);
        REGISTER_ERROR_OBJECT(SyntaxError);
        REGISTER_ERROR_OBJECT(TypeError);
        REGISTER_ERROR_OBJECT(URIError);

#ifdef ENABLE_PROJECTION
        if (config.IsWinRTEnabled())
        {
            REGISTER_ERROR_OBJECT(WinRTError);
        }
#endif
        return hr;
    }

     HRESULT JavascriptLibrary::ProfilerRegisterObject()
    {
        HRESULT hr = S_OK;

        REG_GLOBAL_CONSTRUCTOR(Object);

        DEFINE_OBJECT_NAME(Object);

        REG_OBJECTS_LIB_FUNC(defineProperty, JavascriptObject::EntryDefineProperty);
        REG_OBJECTS_LIB_FUNC(getOwnPropertyDescriptor, JavascriptObject::EntryGetOwnPropertyDescriptor);

        REG_OBJECTS_LIB_FUNC(defineProperties, JavascriptObject::EntryDefineProperties);
        REG_OBJECTS_LIB_FUNC(create, JavascriptObject::EntryCreate);
        REG_OBJECTS_LIB_FUNC(seal, JavascriptObject::EntrySeal);
        REG_OBJECTS_LIB_FUNC(freeze, JavascriptObject::EntryFreeze);
        REG_OBJECTS_LIB_FUNC(preventExtensions, JavascriptObject::EntryPreventExtensions);
        REG_OBJECTS_LIB_FUNC(isSealed, JavascriptObject::EntryIsSealed);
        REG_OBJECTS_LIB_FUNC(isFrozen, JavascriptObject::EntryIsFrozen);
        REG_OBJECTS_LIB_FUNC(isExtensible, JavascriptObject::EntryIsExtensible);

        REG_OBJECTS_LIB_FUNC(getPrototypeOf, JavascriptObject::EntryGetPrototypeOf);
        REG_OBJECTS_LIB_FUNC(keys, JavascriptObject::EntryKeys);
        REG_OBJECTS_LIB_FUNC(getOwnPropertyNames, JavascriptObject::EntryGetOwnPropertyNames);

        REG_OBJECTS_LIB_FUNC(setPrototypeOf, JavascriptObject::EntrySetPrototypeOf);

        ScriptConfiguration const& config = *(scriptContext->GetConfig());
        if (config.IsES6SymbolEnabled())
        {
            REG_OBJECTS_LIB_FUNC(getOwnPropertySymbols, JavascriptObject::EntryGetOwnPropertySymbols);
        }

        REG_OBJECTS_LIB_FUNC(hasOwnProperty, JavascriptObject::EntryHasOwnProperty);
        REG_OBJECTS_LIB_FUNC(propertyIsEnumerable, JavascriptObject::EntryPropertyIsEnumerable);
        REG_OBJECTS_LIB_FUNC(isPrototypeOf, JavascriptObject::EntryIsPrototypeOf);
        REG_OBJECTS_LIB_FUNC(toLocaleString, JavascriptObject::EntryToLocaleString);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptObject::EntryToString);
        REG_OBJECTS_LIB_FUNC(valueOf, JavascriptObject::EntryValueOf);

        if (config.IsDefineGetterSetterEnabled())
        {
            REG_OBJECTS_LIB_FUNC(__defineGetter__, JavascriptObject::EntryDefineGetter);
            REG_OBJECTS_LIB_FUNC(__defineSetter__, JavascriptObject::EntryDefineSetter);
        }

        if (config.IsES6ObjectExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(is, JavascriptObject::EntryIs);
            REG_OBJECTS_LIB_FUNC(assign, JavascriptObject::EntryAssign);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterArray()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Array);

        DEFINE_OBJECT_NAME(Array);

        REG_OBJECTS_LIB_FUNC(isArray, JavascriptArray::EntryIsArray);
        REG_OBJECTS_LIB_FUNC(concat, JavascriptArray::EntryConcat);
        REG_OBJECTS_LIB_FUNC(join, JavascriptArray::EntryJoin);
        REG_OBJECTS_LIB_FUNC(pop, JavascriptArray::EntryPop);
        REG_OBJECTS_LIB_FUNC(push, JavascriptArray::EntryPush);
        REG_OBJECTS_LIB_FUNC(reverse, JavascriptArray::EntryReverse);
        REG_OBJECTS_LIB_FUNC(shift, JavascriptArray::EntryShift);
        REG_OBJECTS_LIB_FUNC(slice, JavascriptArray::EntrySlice);
        REG_OBJECTS_LIB_FUNC(sort, JavascriptArray::EntrySort);
        REG_OBJECTS_LIB_FUNC(splice, JavascriptArray::EntrySplice);
        REG_OBJECTS_LIB_FUNC(toLocaleString, JavascriptArray::EntryToLocaleString);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptArray::EntryToString);
        REG_OBJECTS_LIB_FUNC(unshift, JavascriptArray::EntryUnshift);
        REG_OBJECTS_LIB_FUNC(indexOf, JavascriptArray::EntryIndexOf);
        REG_OBJECTS_LIB_FUNC(every, JavascriptArray::EntryEvery);
        REG_OBJECTS_LIB_FUNC(filter, JavascriptArray::EntryFilter);
        REG_OBJECTS_LIB_FUNC(forEach, JavascriptArray::EntryForEach);
        REG_OBJECTS_LIB_FUNC(lastIndexOf, JavascriptArray::EntryLastIndexOf);
        REG_OBJECTS_LIB_FUNC(map, JavascriptArray::EntryMap);
        REG_OBJECTS_LIB_FUNC(reduce, JavascriptArray::EntryReduce);
        REG_OBJECTS_LIB_FUNC(reduceRight, JavascriptArray::EntryReduceRight);
        REG_OBJECTS_LIB_FUNC(some, JavascriptArray::EntrySome);

        ScriptConfiguration const& config = *(scriptContext->GetConfig());
        if (config.IsES6StringExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(find, JavascriptArray::EntryFind);
            REG_OBJECTS_LIB_FUNC(findIndex, JavascriptArray::EntryFindIndex);
        }

        if (config.IsES6IteratorsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(entries, JavascriptArray::EntryEntries)
            REG_OBJECTS_LIB_FUNC(keys, JavascriptArray::EntryKeys)
            REG_OBJECTS_LIB_FUNC(values, JavascriptArray::EntryValues)
            // _symbolIterator is just an alias for values on Array.prototype so do not register it as its own function
        }

        if (config.IsES6TypedArrayExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(fill, JavascriptArray::EntryFill)
            REG_OBJECTS_LIB_FUNC(copyWithin, JavascriptArray::EntryCopyWithin)
            REG_OBJECTS_LIB_FUNC(from, JavascriptArray::EntryFrom);
            REG_OBJECTS_LIB_FUNC(of, JavascriptArray::EntryOf);
        }

        if (config.IsES7BuiltinsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(includes, JavascriptArray::EntryIncludes);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterBoolean()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Boolean);

        DEFINE_OBJECT_NAME(Boolean);

        REG_OBJECTS_LIB_FUNC(valueOf, JavascriptBoolean::EntryValueOf);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptBoolean::EntryToString);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterDate()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Date);

        DEFINE_OBJECT_NAME(Date);

        REG_OBJECTS_LIB_FUNC(parse, JavascriptDate::EntryParse);
        REG_OBJECTS_LIB_FUNC(now, JavascriptDate::EntryNow);
        REG_OBJECTS_LIB_FUNC(UTC, JavascriptDate::EntryUTC);

        REG_OBJECTS_LIB_FUNC(getDate, JavascriptDate::EntryGetDate);
        REG_OBJECTS_LIB_FUNC(getDay, JavascriptDate::EntryGetDay);
        REG_OBJECTS_LIB_FUNC(getFullYear, JavascriptDate::EntryGetFullYear);
        REG_OBJECTS_LIB_FUNC(getHours, JavascriptDate::EntryGetHours);
        REG_OBJECTS_LIB_FUNC(getMilliseconds, JavascriptDate::EntryGetMilliseconds);
        REG_OBJECTS_LIB_FUNC(getMinutes, JavascriptDate::EntryGetMinutes);
        REG_OBJECTS_LIB_FUNC(getMonth, JavascriptDate::EntryGetMonth);
        REG_OBJECTS_LIB_FUNC(getSeconds, JavascriptDate::EntryGetSeconds);
        REG_OBJECTS_LIB_FUNC(getTime, JavascriptDate::EntryGetTime);
        REG_OBJECTS_LIB_FUNC(getTimezoneOffset, JavascriptDate::EntryGetTimezoneOffset);
        REG_OBJECTS_LIB_FUNC(getUTCDate, JavascriptDate::EntryGetUTCDate);
        REG_OBJECTS_LIB_FUNC(getUTCDay, JavascriptDate::EntryGetUTCDay);
        REG_OBJECTS_LIB_FUNC(getUTCFullYear, JavascriptDate::EntryGetUTCFullYear);
        REG_OBJECTS_LIB_FUNC(getUTCHours, JavascriptDate::EntryGetUTCHours);
        REG_OBJECTS_LIB_FUNC(getUTCMilliseconds, JavascriptDate::EntryGetUTCMilliseconds);
        REG_OBJECTS_LIB_FUNC(getUTCMinutes, JavascriptDate::EntryGetUTCMinutes);
        REG_OBJECTS_LIB_FUNC(getUTCMonth, JavascriptDate::EntryGetUTCMonth);
        REG_OBJECTS_LIB_FUNC(getUTCSeconds, JavascriptDate::EntryGetUTCSeconds);

        ScriptConfiguration const& config = *(scriptContext->GetConfig());
        if (config.SupportsES3Extensions())
        {
            REG_OBJECTS_LIB_FUNC(getVarDate, JavascriptDate::EntryGetVarDate);
        }
        REG_OBJECTS_LIB_FUNC(getYear, JavascriptDate::EntryGetYear);
        REG_OBJECTS_LIB_FUNC(setDate, JavascriptDate::EntrySetDate);
        REG_OBJECTS_LIB_FUNC(setFullYear, JavascriptDate::EntrySetFullYear);
        REG_OBJECTS_LIB_FUNC(setHours, JavascriptDate::EntrySetHours);
        REG_OBJECTS_LIB_FUNC(setMilliseconds, JavascriptDate::EntrySetMilliseconds);
        REG_OBJECTS_LIB_FUNC(setMinutes, JavascriptDate::EntrySetMinutes);
        REG_OBJECTS_LIB_FUNC(setMonth, JavascriptDate::EntrySetMonth);
        REG_OBJECTS_LIB_FUNC(setSeconds, JavascriptDate::EntrySetSeconds);
        REG_OBJECTS_LIB_FUNC(setTime, JavascriptDate::EntrySetTime);
        REG_OBJECTS_LIB_FUNC(setUTCDate, JavascriptDate::EntrySetUTCDate);
        REG_OBJECTS_LIB_FUNC(setUTCFullYear, JavascriptDate::EntrySetUTCFullYear);
        REG_OBJECTS_LIB_FUNC(setUTCHours, JavascriptDate::EntrySetUTCHours);
        REG_OBJECTS_LIB_FUNC(setUTCMilliseconds, JavascriptDate::EntrySetUTCMilliseconds);
        REG_OBJECTS_LIB_FUNC(setUTCMinutes, JavascriptDate::EntrySetUTCMinutes);
        REG_OBJECTS_LIB_FUNC(setUTCMonth, JavascriptDate::EntrySetUTCMonth);
        REG_OBJECTS_LIB_FUNC(setUTCSeconds, JavascriptDate::EntrySetUTCSeconds);
        REG_OBJECTS_LIB_FUNC(setYear, JavascriptDate::EntrySetYear);
        REG_OBJECTS_LIB_FUNC(toDateString, JavascriptDate::EntryToDateString);
        REG_OBJECTS_LIB_FUNC(toISOString, JavascriptDate::EntryToISOString);
        REG_OBJECTS_LIB_FUNC(toJSON, JavascriptDate::EntryToJSON);
        REG_OBJECTS_LIB_FUNC(toLocaleDateString, JavascriptDate::EntryToLocaleDateString);
        REG_OBJECTS_LIB_FUNC(toLocaleString, JavascriptDate::EntryToLocaleString);
        REG_OBJECTS_LIB_FUNC(toLocaleTimeString, JavascriptDate::EntryToLocaleTimeString);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptDate::EntryToString);
        REG_OBJECTS_LIB_FUNC(toTimeString, JavascriptDate::EntryToTimeString);
        REG_OBJECTS_LIB_FUNC(toUTCString, JavascriptDate::EntryToUTCString);
        REG_OBJECTS_LIB_FUNC(toGMTString, JavascriptDate::EntryToGMTString);
        REG_OBJECTS_LIB_FUNC(valueOf, JavascriptDate::EntryValueOf);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterFunction()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Function);

        DEFINE_OBJECT_NAME(Function);

        REG_OBJECTS_LIB_FUNC(apply, JavascriptFunction::EntryApply);
        REG_OBJECTS_LIB_FUNC(bind, JavascriptFunction::EntryBind);
        REG_OBJECTS_LIB_FUNC(call, JavascriptFunction::EntryCall);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptFunction::EntryToString);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterMath()
    {
        HRESULT hr = S_OK;

        DEFINE_OBJECT_NAME(Math);

        REG_OBJECTS_LIB_FUNC(abs, Math::Abs);
        REG_OBJECTS_LIB_FUNC(acos, Math::Acos);
        REG_OBJECTS_LIB_FUNC(asin, Math::Asin);
        REG_OBJECTS_LIB_FUNC(atan, Math::Atan);
        REG_OBJECTS_LIB_FUNC(atan2, Math::Atan2);
        REG_OBJECTS_LIB_FUNC(ceil, Math::Ceil);
        REG_OBJECTS_LIB_FUNC(cos, Math::Cos);
        REG_OBJECTS_LIB_FUNC(exp, Math::Exp);
        REG_OBJECTS_LIB_FUNC(floor, Math::Floor);
        REG_OBJECTS_LIB_FUNC(log, Math::Log);
        REG_OBJECTS_LIB_FUNC(max, Math::Max);
        REG_OBJECTS_LIB_FUNC(min, Math::Min);
        REG_OBJECTS_LIB_FUNC(pow, Math::Pow);
        REG_OBJECTS_LIB_FUNC(random, Math::Random);
        REG_OBJECTS_LIB_FUNC(round, Math::Round);
        REG_OBJECTS_LIB_FUNC(sin, Math::Sin);
        REG_OBJECTS_LIB_FUNC(sqrt, Math::Sqrt);
        REG_OBJECTS_LIB_FUNC(tan, Math::Tan);

        if (scriptContext->GetConfig()->IsES6MathExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(log10, Math::Log10);
            REG_OBJECTS_LIB_FUNC(log2, Math::Log2);
            REG_OBJECTS_LIB_FUNC(log1p, Math::Log1p);
            REG_OBJECTS_LIB_FUNC(expm1, Math::Expm1);
            REG_OBJECTS_LIB_FUNC(cosh, Math::Cosh);
            REG_OBJECTS_LIB_FUNC(sinh, Math::Sinh);
            REG_OBJECTS_LIB_FUNC(tanh, Math::Tanh);
            REG_OBJECTS_LIB_FUNC(acosh, Math::Acosh);
            REG_OBJECTS_LIB_FUNC(asinh, Math::Asinh);
            REG_OBJECTS_LIB_FUNC(atanh, Math::Atanh);
            REG_OBJECTS_LIB_FUNC(hypot, Math::Hypot);
            REG_OBJECTS_LIB_FUNC(trunc, Math::Trunc);
            REG_OBJECTS_LIB_FUNC(sign, Math::Sign);
            REG_OBJECTS_LIB_FUNC(cbrt, Math::Cbrt);
            REG_OBJECTS_LIB_FUNC(imul, Math::Imul);
            REG_OBJECTS_LIB_FUNC(clz32, Math::Clz32);
            REG_OBJECTS_LIB_FUNC(fround, Math::Fround);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterNumber()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Number);

        DEFINE_OBJECT_NAME(Number);

        if (scriptContext->GetConfig()->IsES6NumberExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(isNaN, JavascriptNumber::EntryIsNaN);
            REG_OBJECTS_LIB_FUNC(isFinite, JavascriptNumber::EntryIsFinite);
            REG_OBJECTS_LIB_FUNC(isInteger, JavascriptNumber::EntryIsInteger);
            REG_OBJECTS_LIB_FUNC(isSafeInteger, JavascriptNumber::EntryIsSafeInteger);
        }

        REG_OBJECTS_LIB_FUNC(toExponential, JavascriptNumber::EntryToExponential);
        REG_OBJECTS_LIB_FUNC(toFixed, JavascriptNumber::EntryToFixed);
        REG_OBJECTS_LIB_FUNC(toPrecision, JavascriptNumber::EntryToPrecision);
        REG_OBJECTS_LIB_FUNC(toLocaleString, JavascriptNumber::EntryToLocaleString);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptNumber::EntryToString);
        REG_OBJECTS_LIB_FUNC(valueOf, JavascriptNumber::EntryValueOf);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterString()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(String);

        DEFINE_OBJECT_NAME(String);

        REG_OBJECTS_LIB_FUNC(fromCharCode, JavascriptString::EntryFromCharCode);

        ScriptConfiguration const& config = *(scriptContext->GetConfig());
        if (config.IsES6UnicodeExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(fromCodePoint, JavascriptString::EntryFromCodePoint);
            REG_OBJECTS_LIB_FUNC(codePointAt, JavascriptString::EntryCodePointAt);
            REG_OBJECTS_LIB_FUNC(normalize, JavascriptString::EntryNormalize);
        }

        REG_OBJECTS_LIB_FUNC(indexOf, JavascriptString::EntryIndexOf);
        REG_OBJECTS_LIB_FUNC(lastIndexOf, JavascriptString::EntryLastIndexOf);
        REG_OBJECTS_LIB_FUNC(replace, JavascriptString::EntryReplace);
        REG_OBJECTS_LIB_FUNC(search, JavascriptString::EntrySearch);
        REG_OBJECTS_LIB_FUNC(slice, JavascriptString::EntrySlice);
        REG_OBJECTS_LIB_FUNC(charAt, JavascriptString::EntryCharAt);
        REG_OBJECTS_LIB_FUNC(charCodeAt, JavascriptString::EntryCharCodeAt);
        REG_OBJECTS_LIB_FUNC(concat, JavascriptString::EntryConcat);
        REG_OBJECTS_LIB_FUNC(localeCompare, JavascriptString::EntryLocaleCompare);
        REG_OBJECTS_LIB_FUNC(match, JavascriptString::EntryMatch);
        REG_OBJECTS_LIB_FUNC(split, JavascriptString::EntrySplit);
        REG_OBJECTS_LIB_FUNC(substring, JavascriptString::EntrySubstring);
        REG_OBJECTS_LIB_FUNC(substr, JavascriptString::EntrySubstr);
        REG_OBJECTS_LIB_FUNC(toLocaleLowerCase, JavascriptString::EntryToLocaleLowerCase);
        REG_OBJECTS_LIB_FUNC(toLocaleUpperCase, JavascriptString::EntryToLocaleUpperCase);
        REG_OBJECTS_LIB_FUNC(toLowerCase, JavascriptString::EntryToLowerCase);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptString::EntryToString);
        REG_OBJECTS_LIB_FUNC(toUpperCase, JavascriptString::EntryToUpperCase);
        REG_OBJECTS_LIB_FUNC(trim, JavascriptString::EntryTrim);
        REG_OBJECTS_LIB_FUNC(valueOf, JavascriptString::EntryValueOf);
        if (config.SupportsES3Extensions())
        {
            REG_OBJECTS_LIB_FUNC(anchor, JavascriptString::EntryAnchor);
            REG_OBJECTS_LIB_FUNC(big, JavascriptString::EntryBig);
            REG_OBJECTS_LIB_FUNC(blink, JavascriptString::EntryBlink);
            REG_OBJECTS_LIB_FUNC(bold, JavascriptString::EntryBold);
            REG_OBJECTS_LIB_FUNC(fixed, JavascriptString::EntryFixed);
            REG_OBJECTS_LIB_FUNC(fontcolor, JavascriptString::EntryFontColor);
            REG_OBJECTS_LIB_FUNC(fontsize, JavascriptString::EntryFontSize);
            REG_OBJECTS_LIB_FUNC(italics, JavascriptString::EntryItalics);
            REG_OBJECTS_LIB_FUNC(link, JavascriptString::EntryLink);
            REG_OBJECTS_DYNAMIC_LIB_FUNC(L"small", 5, JavascriptString::EntrySmall);
            REG_OBJECTS_LIB_FUNC(strike, JavascriptString::EntryStrike);
            REG_OBJECTS_LIB_FUNC(sub, JavascriptString::EntrySub);
            REG_OBJECTS_LIB_FUNC(sup, JavascriptString::EntrySup);
        }

        if (config.IsES6StringExtensionsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(repeat, JavascriptString::EntryRepeat);
            REG_OBJECTS_LIB_FUNC(startsWith, JavascriptString::EntryStartsWith);
            REG_OBJECTS_LIB_FUNC(endsWith, JavascriptString::EntryEndsWith);
            REG_OBJECTS_LIB_FUNC(includes, JavascriptString::EntryIncludes);
            REG_OBJECTS_LIB_FUNC(trimLeft, JavascriptString::EntryTrimLeft);
            REG_OBJECTS_LIB_FUNC(trimRight, JavascriptString::EntryTrimRight);

        }

        if (config.IsES6StringTemplateEnabled())
        {
            REG_OBJECTS_LIB_FUNC(raw, JavascriptString::EntryRaw);
        }

        if (config.IsES6IteratorsEnabled())
        {
            REG_OBJECTS_LIB_FUNC2(_symbolIterator, L"[Symbol.iterator]", JavascriptString::EntrySymbolIterator);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterRegExp()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(RegExp);

        DEFINE_OBJECT_NAME(RegExp);

        REG_OBJECTS_LIB_FUNC(exec, JavascriptRegExp::EntryExec);
        REG_OBJECTS_LIB_FUNC(test, JavascriptRegExp::EntryTest);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptRegExp::EntryToString);
        // Note: This is deprecated
        REG_OBJECTS_LIB_FUNC(compile, JavascriptRegExp::EntryCompile);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterJSON()
    {
        HRESULT hr = S_OK;

        DEFINE_OBJECT_NAME(JSON);

        REG_OBJECTS_LIB_FUNC(stringify, JSON::Stringify);
        REG_OBJECTS_LIB_FUNC(parse, JSON::Parse);
        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterWeakMap()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(WeakMap);

        DEFINE_OBJECT_NAME(WeakMap);

        REG_OBJECTS_LIB_FUNC2(delete_, L"delete", JavascriptWeakMap::EntryDelete);
        REG_OBJECTS_LIB_FUNC(get, JavascriptWeakMap::EntryGet);
        REG_OBJECTS_LIB_FUNC(has, JavascriptWeakMap::EntryHas);
        REG_OBJECTS_LIB_FUNC(set, JavascriptWeakMap::EntrySet);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterWeakSet()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(WeakSet);

        DEFINE_OBJECT_NAME(WeakSet);

        REG_OBJECTS_LIB_FUNC(add, JavascriptWeakSet::EntryAdd);
        REG_OBJECTS_LIB_FUNC2(delete_, L"delete", JavascriptWeakSet::EntryDelete);
        REG_OBJECTS_LIB_FUNC(has, JavascriptWeakSet::EntryHas);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterMap()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Map);

        DEFINE_OBJECT_NAME(Map);

        REG_OBJECTS_LIB_FUNC(clear, JavascriptMap::EntryClear);
        REG_OBJECTS_LIB_FUNC2(delete_, L"delete", JavascriptMap::EntryDelete);
        REG_OBJECTS_LIB_FUNC(forEach, JavascriptMap::EntryForEach);
        REG_OBJECTS_LIB_FUNC(get, JavascriptMap::EntryGet);
        REG_OBJECTS_LIB_FUNC(has, JavascriptMap::EntryHas);
        REG_OBJECTS_LIB_FUNC(set, JavascriptMap::EntrySet);

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(entries, JavascriptMap::EntryEntries);
            REG_OBJECTS_LIB_FUNC(keys, JavascriptMap::EntryKeys);
            REG_OBJECTS_LIB_FUNC(values, JavascriptMap::EntryValues);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterSet()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Set);

        DEFINE_OBJECT_NAME(Set);

        REG_OBJECTS_LIB_FUNC(add, JavascriptSet::EntryAdd);
        REG_OBJECTS_LIB_FUNC(clear, JavascriptSet::EntryClear);
        REG_OBJECTS_LIB_FUNC2(delete_, L"delete", JavascriptSet::EntryDelete);
        REG_OBJECTS_LIB_FUNC(forEach, JavascriptSet::EntryForEach);
        REG_OBJECTS_LIB_FUNC(has, JavascriptSet::EntryHas);

        if (scriptContext->GetConfig()->IsES6IteratorsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(entries, JavascriptSet::EntryEntries);
            REG_OBJECTS_LIB_FUNC(values, JavascriptSet::EntryValues);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterSymbol()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Symbol);

        DEFINE_OBJECT_NAME(Symbol);

        REG_OBJECTS_LIB_FUNC(valueOf, JavascriptSymbol::EntryValueOf);
        REG_OBJECTS_LIB_FUNC(toString, JavascriptSymbol::EntryToString);
        REG_OBJECTS_LIB_FUNC2(for_, L"for", JavascriptSymbol::EntryFor);
        REG_OBJECTS_LIB_FUNC(keyFor, JavascriptSymbol::EntryKeyFor);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterIterator()
    {
        HRESULT hr = S_OK;
        // Array Iterator has no global constructor

        DEFINE_OBJECT_NAME(Iterator);

        REG_OBJECTS_LIB_FUNC2(_symbolIterator, L"[Symbol.iterator]", JavascriptIterator::EntrySymbolIterator);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterArrayIterator()
    {
        HRESULT hr = S_OK;
        // Array Iterator has no global constructor

        DEFINE_OBJECT_NAME(Array Iterator);

        REG_OBJECTS_LIB_FUNC(next, JavascriptArrayIterator::EntryNext);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterMapIterator()
    {
        HRESULT hr = S_OK;
        // Map Iterator has no global constructor

        DEFINE_OBJECT_NAME(Map Iterator);

        REG_OBJECTS_LIB_FUNC(next, JavascriptMapIterator::EntryNext);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterSetIterator()
    {
        HRESULT hr = S_OK;
        // Set Iterator has no global constructor

        DEFINE_OBJECT_NAME(Set Iterator);

        REG_OBJECTS_LIB_FUNC(next, JavascriptSetIterator::EntryNext);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterStringIterator()
    {
        HRESULT hr = S_OK;
        // String Iterator has no global constructor

        DEFINE_OBJECT_NAME(String Iterator);

        REG_OBJECTS_LIB_FUNC(next, JavascriptStringIterator::EntryNext);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterEnumeratorIterator()
    {
        HRESULT hr = S_OK;
        // Enumerator Iterator has no global constructor

        DEFINE_OBJECT_NAME(Enumerator Iterator);

        REG_OBJECTS_LIB_FUNC(next, JavascriptEnumeratorIterator::EntryNext);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterTypedArray()
    {
        HRESULT hr = S_OK;
        // TypedArray has no named global constructor

        DEFINE_OBJECT_NAME(TypedArray);

        REG_OBJECTS_LIB_FUNC(from, TypedArrayBase::EntryFrom);
        REG_OBJECTS_LIB_FUNC(of, TypedArrayBase::EntryOf);
        REG_OBJECTS_LIB_FUNC(set, TypedArrayBase::EntrySet);
        REG_OBJECTS_LIB_FUNC(subarray, TypedArrayBase::EntrySubarray);
        REG_OBJECTS_LIB_FUNC(copyWithin, TypedArrayBase::EntryCopyWithin);
        REG_OBJECTS_LIB_FUNC(every, TypedArrayBase::EntryEvery);
        REG_OBJECTS_LIB_FUNC(fill, TypedArrayBase::EntryFill);
        REG_OBJECTS_LIB_FUNC(filter, TypedArrayBase::EntryFilter);
        REG_OBJECTS_LIB_FUNC(find, TypedArrayBase::EntryFind);
        REG_OBJECTS_LIB_FUNC(findIndex, TypedArrayBase::EntryFindIndex);
        REG_OBJECTS_LIB_FUNC(forEach, TypedArrayBase::EntryForEach);
        REG_OBJECTS_LIB_FUNC(indexOf, TypedArrayBase::EntryIndexOf);
        REG_OBJECTS_LIB_FUNC(join, TypedArrayBase::EntryJoin);
        REG_OBJECTS_LIB_FUNC(lastIndexOf, TypedArrayBase::EntryLastIndexOf);
        REG_OBJECTS_LIB_FUNC(map, TypedArrayBase::EntryMap);
        REG_OBJECTS_LIB_FUNC(reduce, TypedArrayBase::EntryReduce);
        REG_OBJECTS_LIB_FUNC(reduceRight, TypedArrayBase::EntryReduceRight);
        REG_OBJECTS_LIB_FUNC(reverse, TypedArrayBase::EntryReverse);
        REG_OBJECTS_LIB_FUNC(slice, TypedArrayBase::EntrySlice);
        REG_OBJECTS_LIB_FUNC(some, TypedArrayBase::EntrySome);
        REG_OBJECTS_LIB_FUNC(sort, TypedArrayBase::EntrySort);

        ScriptConfiguration const& config = *(scriptContext->GetConfig());
        if (config.IsES7BuiltinsEnabled())
        {
            REG_OBJECTS_LIB_FUNC(includes, TypedArrayBase::EntryIncludes);
        }

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterPromise()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Promise);

        DEFINE_OBJECT_NAME(Promise);

        REG_OBJECTS_LIB_FUNC(all, JavascriptPromise::EntryAll);
        REG_OBJECTS_LIB_FUNC2(catch_, L"catch", JavascriptPromise::EntryCatch);
        REG_OBJECTS_LIB_FUNC(race, JavascriptPromise::EntryRace);
        REG_OBJECTS_LIB_FUNC(resolve, JavascriptPromise::EntryResolve);
        REG_OBJECTS_LIB_FUNC(then, JavascriptPromise::EntryThen);

        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterProxy()
    {
        HRESULT hr = S_OK;
        REG_GLOBAL_CONSTRUCTOR(Proxy);

        DEFINE_OBJECT_NAME(Proxy);

        REG_OBJECTS_LIB_FUNC(revocable, JavascriptProxy::EntryRevocable);
        return hr;
    }

    HRESULT JavascriptLibrary::ProfilerRegisterReflect()
    {
        HRESULT hr = S_OK;
        DEFINE_OBJECT_NAME(Reflect);

        REG_OBJECTS_LIB_FUNC(defineProperty, JavascriptReflect::EntryDefineProperty);
        REG_OBJECTS_LIB_FUNC(deleteProperty, JavascriptReflect::EntryDeleteProperty);
        REG_OBJECTS_LIB_FUNC(enumerate, JavascriptReflect::EntryEnumerate);
        REG_OBJECTS_LIB_FUNC(get, JavascriptReflect::EntryGet);
        REG_OBJECTS_LIB_FUNC(getOwnPropertyDescriptor, JavascriptReflect::EntryGetOwnPropertyDescriptor);
        REG_OBJECTS_LIB_FUNC(getPrototypeOf, JavascriptReflect::EntryGetPrototypeOf);
        REG_OBJECTS_LIB_FUNC(has, JavascriptReflect::EntryHas);
        REG_OBJECTS_LIB_FUNC(isExtensible, JavascriptReflect::EntryIsExtensible);
        REG_OBJECTS_LIB_FUNC(ownKeys, JavascriptReflect::EntryOwnKeys);
        REG_OBJECTS_LIB_FUNC(preventExtensions, JavascriptReflect::EntryPreventExtensions);
        REG_OBJECTS_LIB_FUNC(set, JavascriptReflect::EntrySet);
        REG_OBJECTS_LIB_FUNC(setPrototypeOf, JavascriptReflect::EntrySetPrototypeOf);
        REG_OBJECTS_LIB_FUNC(apply, JavascriptReflect::EntryApply);
        REG_OBJECTS_LIB_FUNC(construct, JavascriptReflect::EntryConstruct);
        return hr;
    }


    HRESULT JavascriptLibrary::ProfilerRegisterGenerator()
    {
        HRESULT hr = S_OK;
        DEFINE_OBJECT_NAME(Generator);

        REG_OBJECTS_LIB_FUNC(next, JavascriptGenerator::EntryNext);
        REG_OBJECTS_LIB_FUNC(return_, JavascriptGenerator::EntryReturn);
        REG_OBJECTS_LIB_FUNC(throw_, JavascriptGenerator::EntryThrow);

        return hr;
    }

#if ENABLE_NATIVE_CODEGEN
    HRESULT JavascriptLibrary::ProfilerRegisterSIMD()
    {
        HRESULT hr = S_OK;

        DEFINE_OBJECT_NAME(SIMD);

        // Float32x4
        REG_OBJECTS_LIB_FUNC(Float32x4, SIMDFloat32x4Lib::EntryFloat32x4);
        REG_OBJECTS_LIB_FUNC(check, SIMDFloat32x4Lib::EntryCheck);
        REG_OBJECTS_LIB_FUNC(zero, SIMDFloat32x4Lib::EntryZero);
        REG_OBJECTS_LIB_FUNC(splat, SIMDFloat32x4Lib::EntrySplat);
        REG_OBJECTS_LIB_FUNC(extractLane, SIMDFloat32x4Lib::EntryExtractLane);
        REG_OBJECTS_LIB_FUNC(replaceLane, SIMDFloat32x4Lib::EntryReplaceLane);
        REG_OBJECTS_LIB_FUNC(fromFloat64x2, SIMDFloat32x4Lib::EntryFromFloat64x2);
        REG_OBJECTS_LIB_FUNC(fromFloat64x2Bits, SIMDFloat32x4Lib::EntryFromFloat64x2Bits);
        REG_OBJECTS_LIB_FUNC(fromInt32x4, SIMDFloat32x4Lib::EntryFromInt32x4);
        REG_OBJECTS_LIB_FUNC(fromInt32x4Bits, SIMDFloat32x4Lib::EntryFromInt32x4Bits);
        REG_OBJECTS_LIB_FUNC(add, SIMDFloat32x4Lib::EntryAdd);
        REG_OBJECTS_LIB_FUNC(sub, SIMDFloat32x4Lib::EntrySub);
        REG_OBJECTS_LIB_FUNC(mul, SIMDFloat32x4Lib::EntryMul);
        REG_OBJECTS_LIB_FUNC(div, SIMDFloat32x4Lib::EntryDiv);
        REG_OBJECTS_LIB_FUNC(and, SIMDFloat32x4Lib::EntryAnd);
        REG_OBJECTS_LIB_FUNC(or, SIMDFloat32x4Lib::EntryOr);
        REG_OBJECTS_LIB_FUNC(xor, SIMDFloat32x4Lib::EntryXor);
        REG_OBJECTS_LIB_FUNC(min, SIMDFloat32x4Lib::EntryMin);
        REG_OBJECTS_LIB_FUNC(max, SIMDFloat32x4Lib::EntryMax);
        REG_OBJECTS_LIB_FUNC(scale, SIMDFloat32x4Lib::EntryScale);
        REG_OBJECTS_LIB_FUNC(abs, SIMDFloat32x4Lib::EntryAbs);
        REG_OBJECTS_LIB_FUNC(neg, SIMDFloat32x4Lib::EntryNeg);
        REG_OBJECTS_LIB_FUNC(not, SIMDFloat32x4Lib::EntryNot);
        REG_OBJECTS_LIB_FUNC(sqrt, SIMDFloat32x4Lib::EntrySqrt);
        REG_OBJECTS_LIB_FUNC(reciprocal, SIMDFloat32x4Lib::EntryReciprocal);
        REG_OBJECTS_LIB_FUNC(reciprocalSqrt, SIMDFloat32x4Lib::EntryReciprocalSqrt);
        REG_OBJECTS_LIB_FUNC(lessThan, SIMDFloat32x4Lib::EntryLessThan);
        REG_OBJECTS_LIB_FUNC(lessThanOrEqual, SIMDFloat32x4Lib::EntryLessThanOrEqual);
        REG_OBJECTS_LIB_FUNC(equal, SIMDFloat32x4Lib::EntryEqual);
        REG_OBJECTS_LIB_FUNC(notEqual, SIMDFloat32x4Lib::EntryNotEqual);
        REG_OBJECTS_LIB_FUNC(greaterThan, SIMDFloat32x4Lib::EntryGreaterThan);
        REG_OBJECTS_LIB_FUNC(greaterThanOrEqual, SIMDFloat32x4Lib::EntryGreaterThanOrEqual);
        REG_OBJECTS_LIB_FUNC(swizzle, SIMDFloat32x4Lib::EntrySwizzle);
        REG_OBJECTS_LIB_FUNC(shuffle, SIMDFloat32x4Lib::EntryShuffle);
        REG_OBJECTS_LIB_FUNC(clamp, SIMDFloat32x4Lib::EntryClamp);
        REG_OBJECTS_LIB_FUNC(select, SIMDFloat32x4Lib::EntrySelect);
        // Float64x2
        REG_OBJECTS_LIB_FUNC(Float64x2, SIMDFloat64x2Lib::EntryFloat64x2);
        REG_OBJECTS_LIB_FUNC(check, SIMDFloat64x2Lib::EntryCheck);
        REG_OBJECTS_LIB_FUNC(zero, SIMDFloat64x2Lib::EntryZero);
        REG_OBJECTS_LIB_FUNC(splat, SIMDFloat64x2Lib::EntrySplat);
        REG_OBJECTS_LIB_FUNC(fromFloat32x4, SIMDFloat64x2Lib::EntryFromFloat32x4);
        REG_OBJECTS_LIB_FUNC(fromFloat32x4Bits, SIMDFloat64x2Lib::EntryFromFloat32x4Bits);
        REG_OBJECTS_LIB_FUNC(fromInt32x4, SIMDFloat64x2Lib::EntryFromInt32x4);
        REG_OBJECTS_LIB_FUNC(fromInt32x4Bits, SIMDFloat64x2Lib::EntryFromInt32x4Bits);
        REG_OBJECTS_LIB_FUNC(add, SIMDFloat64x2Lib::EntryAdd);
        REG_OBJECTS_LIB_FUNC(sub, SIMDFloat64x2Lib::EntrySub);
        REG_OBJECTS_LIB_FUNC(mul, SIMDFloat64x2Lib::EntryMul);
        REG_OBJECTS_LIB_FUNC(div, SIMDFloat64x2Lib::EntryDiv);
        REG_OBJECTS_LIB_FUNC(min, SIMDFloat64x2Lib::EntryMin);
        REG_OBJECTS_LIB_FUNC(max, SIMDFloat64x2Lib::EntryMax);
        REG_OBJECTS_LIB_FUNC(scale, SIMDFloat64x2Lib::EntryScale);
        REG_OBJECTS_LIB_FUNC(abs, SIMDFloat64x2Lib::EntryAbs);
        REG_OBJECTS_LIB_FUNC(neg, SIMDFloat64x2Lib::EntryNeg);
        REG_OBJECTS_LIB_FUNC(sqrt, SIMDFloat64x2Lib::EntrySqrt);
        REG_OBJECTS_LIB_FUNC(reciprocal, SIMDFloat64x2Lib::EntryReciprocal);
        REG_OBJECTS_LIB_FUNC(reciprocalSqrt, SIMDFloat64x2Lib::EntryReciprocalSqrt);
        REG_OBJECTS_LIB_FUNC(lessThan, SIMDFloat64x2Lib::EntryLessThan);
        REG_OBJECTS_LIB_FUNC(lessThanOrEqual, SIMDFloat64x2Lib::EntryLessThanOrEqual);
        REG_OBJECTS_LIB_FUNC(equal, SIMDFloat64x2Lib::EntryEqual);
        REG_OBJECTS_LIB_FUNC(notEqual, SIMDFloat64x2Lib::EntryNotEqual);
        REG_OBJECTS_LIB_FUNC(greaterThan, SIMDFloat64x2Lib::EntryGreaterThan);
        REG_OBJECTS_LIB_FUNC(greaterThanOrEqual, SIMDFloat64x2Lib::EntryGreaterThanOrEqual);
        REG_OBJECTS_LIB_FUNC(swizzle, SIMDFloat64x2Lib::EntrySwizzle);
        REG_OBJECTS_LIB_FUNC(shuffle, SIMDFloat64x2Lib::EntryShuffle);
        REG_OBJECTS_LIB_FUNC(clamp, SIMDFloat64x2Lib::EntryClamp);
        REG_OBJECTS_LIB_FUNC(select, SIMDFloat64x2Lib::EntrySelect);
        // Int32x4
        REG_OBJECTS_LIB_FUNC(Int32x4, SIMDInt32x4Lib::EntryInt32x4);
        REG_OBJECTS_LIB_FUNC(check, SIMDInt32x4Lib::EntryCheck);
        REG_OBJECTS_LIB_FUNC(zero, SIMDInt32x4Lib::EntryZero);
        REG_OBJECTS_LIB_FUNC(splat, SIMDInt32x4Lib::EntrySplat);
        REG_OBJECTS_LIB_FUNC(bool_, SIMDInt32x4Lib::EntryBool);
        REG_OBJECTS_LIB_FUNC(extractLane, SIMDInt32x4Lib::EntryExtractLane);
        REG_OBJECTS_LIB_FUNC(replaceLane, SIMDInt32x4Lib::EntryReplaceLane);
        REG_OBJECTS_LIB_FUNC(withFlagX, SIMDInt32x4Lib::EntryWithFlagX);
        REG_OBJECTS_LIB_FUNC(withFlagY, SIMDInt32x4Lib::EntryWithFlagY);
        REG_OBJECTS_LIB_FUNC(withFlagZ, SIMDInt32x4Lib::EntryWithFlagZ);
        REG_OBJECTS_LIB_FUNC(withFlagW, SIMDInt32x4Lib::EntryWithFlagW);
        REG_OBJECTS_LIB_FUNC(fromFloat64x2, SIMDInt32x4Lib::EntryFromFloat64x2);
        REG_OBJECTS_LIB_FUNC(fromFloat64x2Bits, SIMDInt32x4Lib::EntryFromFloat64x2Bits);
        REG_OBJECTS_LIB_FUNC(fromFloat32x4, SIMDInt32x4Lib::EntryFromFloat32x4);
        REG_OBJECTS_LIB_FUNC(fromFloat32x4Bits, SIMDInt32x4Lib::EntryFromFloat32x4Bits);
        REG_OBJECTS_LIB_FUNC(add, SIMDInt32x4Lib::EntryAdd);
        REG_OBJECTS_LIB_FUNC(sub, SIMDInt32x4Lib::EntrySub);
        REG_OBJECTS_LIB_FUNC(mul, SIMDInt32x4Lib::EntryMul);
        REG_OBJECTS_LIB_FUNC(and, SIMDInt32x4Lib::EntryAnd);
        REG_OBJECTS_LIB_FUNC(or,  SIMDInt32x4Lib::EntryOr);
        REG_OBJECTS_LIB_FUNC(xor, SIMDInt32x4Lib::EntryXor);
        REG_OBJECTS_LIB_FUNC(neg, SIMDInt32x4Lib::EntryNeg);
        REG_OBJECTS_LIB_FUNC(not, SIMDInt32x4Lib::EntryNot);
        REG_OBJECTS_LIB_FUNC(lessThan, SIMDInt32x4Lib::EntryLessThan);
        REG_OBJECTS_LIB_FUNC(equal, SIMDInt32x4Lib::EntryEqual);
        REG_OBJECTS_LIB_FUNC(greaterThan, SIMDInt32x4Lib::EntryGreaterThan);
        REG_OBJECTS_LIB_FUNC(swizzle, SIMDInt32x4Lib::EntrySwizzle);
        REG_OBJECTS_LIB_FUNC(shuffle, SIMDInt32x4Lib::EntryShuffle);
        REG_OBJECTS_LIB_FUNC(shiftLeft, SIMDInt32x4Lib::EntryShiftLeft);
        REG_OBJECTS_LIB_FUNC(shiftRightLogical, SIMDInt32x4Lib::EntryShiftRightLogical);
        REG_OBJECTS_LIB_FUNC(shiftRightArithmetic, SIMDInt32x4Lib::EntryShiftRightArithmetic);
        REG_OBJECTS_LIB_FUNC(select, SIMDInt32x4Lib::EntrySelect);

        // Int8x16
        REG_OBJECTS_LIB_FUNC(Int8x16, SIMDInt8x16Lib::EntryInt8x16);
        REG_OBJECTS_LIB_FUNC(check, SIMDInt8x16Lib::EntryCheck);
        REG_OBJECTS_LIB_FUNC(zero, SIMDInt8x16Lib::EntryZero);
        REG_OBJECTS_LIB_FUNC(splat, SIMDInt8x16Lib::EntrySplat);
        REG_OBJECTS_LIB_FUNC(fromFloat32x4Bits, SIMDInt8x16Lib::EntryFromFloat32x4Bits);
        REG_OBJECTS_LIB_FUNC(fromInt32x4Bits, SIMDInt8x16Lib::EntryFromInt32x4Bits);
        REG_OBJECTS_LIB_FUNC(neg, SIMDInt8x16Lib::EntryNeg);
        REG_OBJECTS_LIB_FUNC(not, SIMDInt8x16Lib::EntryNot);
        REG_OBJECTS_LIB_FUNC(add, SIMDInt8x16Lib::EntryAdd);
        REG_OBJECTS_LIB_FUNC(sub, SIMDInt8x16Lib::EntrySub);
        REG_OBJECTS_LIB_FUNC(mul, SIMDInt8x16Lib::EntryMul);
        REG_OBJECTS_LIB_FUNC(and, SIMDInt8x16Lib::EntryAnd);
        REG_OBJECTS_LIB_FUNC(or, SIMDInt8x16Lib::EntryOr);
        REG_OBJECTS_LIB_FUNC(xor, SIMDInt8x16Lib::EntryXor);
        REG_OBJECTS_LIB_FUNC(lessThan, SIMDInt8x16Lib::EntryLessThan);
        REG_OBJECTS_LIB_FUNC(equal, SIMDInt8x16Lib::EntryEqual);
        REG_OBJECTS_LIB_FUNC(greaterThan, SIMDInt8x16Lib::EntryGreaterThan);
        REG_OBJECTS_LIB_FUNC(shiftLeftByScalar, SIMDInt8x16Lib::EntryShiftLeftByScalar);
        REG_OBJECTS_LIB_FUNC(shiftRightLogicalByScalar, SIMDInt8x16Lib::EntryShiftRightLogicalByScalar);
        REG_OBJECTS_LIB_FUNC(shiftRightArithmeticByScalar, SIMDInt8x16Lib::EntryShiftRightArithmeticByScalar);
        REG_OBJECTS_LIB_FUNC(extractLane, SIMDInt8x16Lib::EntryExtractLane);
        REG_OBJECTS_LIB_FUNC(replaceLane, SIMDInt8x16Lib::EntryReplaceLane);

        return hr;
    }
#endif

#if DBG
    void JavascriptLibrary::DumpLibraryByteCode()
    {
#ifdef ENABLE_INTL_OBJECT
        // We aren't going to be passing in a number to check range of -dump:LibInit, that will be done by Intl/Promise
        // This is just to force init Intl code if dump:LibInit has been passed
        if (CONFIG_ISENABLED(DumpFlag) && Js::Configuration::Global.flags.Dump.IsEnabled(Js::JsLibInitPhase))
        {
            for (uint i = 0; i <= MaxEngineInterfaceExtensionKind; i++)
            {
                EngineExtensionObjectBase* engineExtension = this->GetEngineInterfaceObject()->GetEngineExtension((Js::EngineInterfaceExtensionKind)i);
                if (engineExtension != nullptr)
                {
                    engineExtension->DumpByteCode();
                }
            }
        }
#endif
    }
#endif
#ifdef IR_VIEWER
    HRESULT JavascriptLibrary::ProfilerRegisterIRViewer()
    {
        HRESULT hr = S_OK;

        DEFINE_OBJECT_NAME(IRViewer);

        // TODO (t-doilij) move GlobalObject::EntryParseIR to JavascriptIRViewer::EntryParseIR
        REG_LIB_FUNC_CORE(pwszObjectName, L"parseIR", scriptContext->GetOrAddPropertyIdTracked(L"parseIR"), GlobalObject::EntryParseIR);
        REG_LIB_FUNC_CORE(pwszObjectName, L"functionList", scriptContext->GetOrAddPropertyIdTracked(L"functionList"), GlobalObject::EntryFunctionList);
        REG_LIB_FUNC_CORE(pwszObjectName, L"rejitFunction", scriptContext->GetOrAddPropertyIdTracked(L"rejitFunction"), GlobalObject::EntryRejitFunction);

        return hr;
    }
#endif /* IR_VIEWER */
}
