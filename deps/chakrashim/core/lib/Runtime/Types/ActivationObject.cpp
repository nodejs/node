//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "cmperr.h"
#include "Language\JavascriptStackWalker.h"

namespace Js
{
    bool ActivationObject::Is(void* instance)
    {
        return VirtualTableInfo<Js::ActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::ActivationObjectEx>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::PseudoActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::BlockActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::ConsoleScopeActivationObject>::HasVirtualTable(instance);
    }

    BOOL ActivationObject::HasOwnPropertyCheckNoRedecl(PropertyId propertyId)
    {
        bool noRedecl = false;
        if (!GetTypeHandler()->HasProperty(this, propertyId, &noRedecl))
        {
            return FALSE;
        }
        else if (noRedecl)
        {
            JavascriptError::ThrowReferenceError(GetScriptContext(), ERRRedeclaration);
        }
        return TRUE;
    }

    BOOL ActivationObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return DynamicObject::SetProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return DynamicObject::SetProperty(propertyNameString, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return DynamicObject::SetProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, PropertyWritable|PropertyEnumerable, info, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue));
    }

    BOOL ActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {
        DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        return true;
    }

    BOOL ActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {
        // Var binding of functions declared in eval are elided when conflicting
        // with function scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasProperty(this, propertyId, &noRedecl) || !noRedecl)
        {
            DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ActivationObject::EnsureProperty(PropertyId propertyId)
    {
        if (!DynamicObject::HasOwnProperty(propertyId))
        {
            DynamicObject::SetPropertyWithAttributes(propertyId, this->GetLibrary()->GetUndefined(), PropertyDynamicTypeDefaults, nullptr, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {
        ActivationObject::HasOwnPropertyCheckNoRedecl(propertyId);
        return true;
    }

    BOOL ActivationObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {
        return false;
    }

    BOOL ActivationObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"{ActivationObject}");
        return TRUE;
    }

    BOOL ActivationObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Object, (ActivationObject)");
        return TRUE;
    }

    BOOL BlockActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {
        // eval, etc., should not create var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::EnsureProperty(PropertyId propertyId)
    {
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BlockActivationObject* BlockActivationObject::Clone(ScriptContext *scriptContext)
    {
        DynamicType* type = this->GetDynamicType();
        BlockActivationObject* blockScopeClone = DynamicObject::NewObject<BlockActivationObject>(scriptContext->GetRecycler(), type);
        int slotCapacity = this->GetTypeHandler()->GetSlotCapacity();

        for (int i = 0; i < slotCapacity; i += 1)
        {
            DebugOnly(PropertyId propId = this->GetPropertyId(i));
            Var value = this->GetSlot(i);
            blockScopeClone->SetSlot(SetSlotArguments(propId, i, value));
        }

        return blockScopeClone;
    }

    BOOL PseudoActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {
        // eval, etc., should not create function properties on something like a "catch" scope
        return false;
    }

    BOOL PseudoActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {
        // eval, etc., should not create function properties on something like a "catch" scope
        return false;
    }

    BOOL PseudoActivationObject::EnsureProperty(PropertyId propertyId)
    {
        return false;
    }

    BOOL PseudoActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {
        return false;
    }

    /* static */
    const PropertyId * ActivationObjectEx::GetCachedScopeInfo(const PropertyIdArray *propIds)
    {
        // Cached scope info is appended to the "normal" prop ID array elements.
        return &propIds->elements[propIds->count];
    }

    BOOL ActivationObjectEx::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {
        // No need to invalidate the cached scope even if the property is a cached function object.
        // The caller won't be using the object itself.
        return __super::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    void ActivationObjectEx::GetPropertyCore(PropertyValueInfo *info, ScriptContext *requestContext)
    {
        if (info)
        {
            PropertyIndex slot = info->GetPropertyIndex();
            if (slot >= this->firstFuncSlot && slot <= this->lastFuncSlot)
            {
                this->parentFunc->InvalidateCachedScopeChain();

                // If the caller is an eval, then each time we execute the eval we need to invalidate the
                // cached scope chain. We can't rely on detecting the escape each time, because inline
                // cache hits may keep us from entering the runtime. So set a flag to make sure the
                // invalidation always happens.
                JavascriptFunction *currentFunc = nullptr;
                JavascriptStackWalker walker(requestContext);
                while (walker.GetCaller(&currentFunc))
                {
                    if (walker.IsEvalCaller())
                    {
                        //We are walking the stack, so the function body must have been deserialized by this point.
                        currentFunc->GetFunctionBody()->SetFuncEscapes(true);
                        break;
                    }
                }
            }
        }
    }

    BOOL ActivationObjectEx::GetProperty(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {
        if (__super::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {
            GetPropertyCore(info, requestContext);
            return TRUE;
        }
        return FALSE;
    }

    BOOL ActivationObjectEx::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {
        if (__super::GetProperty(originalInstance, propertyNameString, value, info, requestContext))
        {
            GetPropertyCore(info, requestContext);
            return TRUE;
        }
        return FALSE;
    }

    void ActivationObjectEx::InvalidateCachedScope()
    {
        if (this->cachedFuncCount != 0)
        {
            // Clearing the cached functions and types isn't strictly necessary for correctness,
            // but we want those objects to be collected even if the scope object is part of someone's
            // closure environment.
            memset(this->cache, 0, this->cachedFuncCount * sizeof(FuncCacheEntry));
        }
        this->parentFunc->SetCachedScope(nullptr);
    }

    void ActivationObjectEx::SetCachedFunc(uint i, ScriptFunction *func)
    {
        Assert(i < cachedFuncCount);
        cache[i].func = func;
        cache[i].type = (DynamicType*)func->GetType();
    }
};
