//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#ifdef ENABLE_MUTATION_BREAKPOINT
Js::MutationBreakpoint::MutationBreakpoint(ScriptContext *scriptContext, DynamicObject *obj, const PropertyRecord *pr, MutationType type, Js::PropertyId parentPropertyId)
    : isValid(true)
    , didCauseBreak(false)
    , mFlag(MutationTypeNone)
    , obj(nullptr)
    , properties(nullptr)
    , mutationBreakpointDelegate(nullptr)
    , breakMutationType(MutationTypeNone)
    , propertyRecord(nullptr)
    , newValue(nullptr)
    , parentPropertyId(Constants::NoProperty)
{
    // create weak reference to object
    this->obj = scriptContext->GetRecycler()->CreateWeakReferenceHandle(obj);

    // initialize property mutation list
    this->properties = RecyclerNew(scriptContext->GetRecycler(), PropertyMutationList, scriptContext->GetRecycler());

    // Save the property id of parent object
    this->parentPropertyId = parentPropertyId;

    // set breakpoint
    this->SetBreak(type, pr);
}

Js::MutationBreakpoint::~MutationBreakpoint()
{}

bool Js::MutationBreakpoint::HandleSetProperty(Js::ScriptContext *scriptContext, RecyclableObject *object, Js::PropertyId propertyId, Var newValue)
{
    Assert(scriptContext);
    Assert(object);
    ScriptContext *objectContext = object->GetScriptContext();
    if (IsFeatureEnabled(scriptContext)
        && objectContext->HasMutationBreakpoints())
    {
        MutationBreakpoint *bp = nullptr;
        DynamicObject *dynObj = DynamicObject::FromVar(object);

        if (dynObj->GetInternalProperty(object, InternalPropertyIds::MutationBp, reinterpret_cast<Var*>(&bp), nullptr, objectContext)
            && bp)
        {
            if (!bp->IsValid())
            {
                bp->Reset();
            }
            else
            {
                MutationType mutationType = MutationTypeUpdate;
                if (!object->HasProperty(propertyId))
                {
                    mutationType = MutationTypeAdd;
                }
                if (bp->ShouldBreak(mutationType, propertyId))
                {
                    const PropertyRecord *pr = scriptContext->GetPropertyName(propertyId);
                    bp->newValue = newValue;
                    bp->Break(scriptContext, mutationType, pr);
                    bp->newValue = nullptr;
                    return true;
                }
                else
                {
                    // Mutation breakpoint exists; do not update cache
                    return true;
                }
            }
        }
    }
    return false;
}

void Js::MutationBreakpoint::HandleDeleteProperty(ScriptContext *scriptContext, Var instance, PropertyId propertyId)
{
    Assert(scriptContext);
    Assert(instance);
    if (MutationBreakpoint::CanSet(instance))
    {
        DynamicObject *obj = DynamicObject::FromVar(instance);
        if (obj->GetScriptContext()->HasMutationBreakpoints())
        {
            MutationBreakpoint *bp = nullptr;
            if (obj->GetInternalProperty(obj, InternalPropertyIds::MutationBp, reinterpret_cast<Var *>(&bp), nullptr, scriptContext)
                && bp)
            {
                if (!bp->IsValid())
                {
                    bp->Reset();
                }
                else if (bp->ShouldBreak(MutationTypeDelete, propertyId))
                {
                    const PropertyRecord *pr = scriptContext->GetPropertyName(propertyId);
                    bp->Break(scriptContext, MutationTypeDelete, pr);
                }
            }
        }
    }
}

bool Js::MutationBreakpoint::DeleteProperty(PropertyRecord *pr)
{
    Assert(pr != nullptr);

    for (int i = 0; i < this->properties->Count(); i++)
    {
        PropertyMutation pm = this->properties->Item(i);
        if (pm.pr == pr)
        {
            this->properties->RemoveAt(i);
            return true;
        }
    }
    return false;
}

const Js::Var Js::MutationBreakpoint::GetMutationObjectVar() const
{
    Var retVar = nullptr;
    Assert(this->didCauseBreak);
    if (this->obj != nullptr)
    {
        DynamicObject *dynObj = this->obj->Get();
        if (dynObj != nullptr)
        {
            retVar = static_cast<Js::Var>(dynObj);
        }
    }
    return retVar;
}

const Js::Var Js::MutationBreakpoint::GetBreakNewValueVar() const
{

    Assert(this->didCauseBreak);
    return this->newValue;
}

bool Js::MutationBreakpoint::IsFeatureEnabled(ScriptContext *scriptContext)
{
    Assert(scriptContext != nullptr);
    return scriptContext->IsInDebugMode() && !PHASE_OFF1(Js::ObjectMutationBreakpointPhase);
}

bool Js::MutationBreakpoint::CanSet(Var object)
{
    if (!object)
    {
        return false;
    }

    TypeId id = JavascriptOperators::GetTypeId(object);
    return JavascriptOperators::IsObjectType(id) && !JavascriptOperators::IsSpecialObjectType(id);
}

Js::MutationBreakpoint * Js::MutationBreakpoint::New(ScriptContext *scriptContext, DynamicObject *obj, const PropertyRecord *pr, MutationType type, Js::PropertyId propertyId)
{
    return RecyclerNewFinalized(scriptContext->GetRecycler(), MutationBreakpoint, scriptContext, obj, pr, type, propertyId);
}

Js::MutationBreakpointDelegate * Js::MutationBreakpoint::GetDelegate()
{
    // Create a new breakpoint object if needed
    if (!mutationBreakpointDelegate)
    {
        mutationBreakpointDelegate = Js::MutationBreakpointDelegate::New(this);
        // NOTE: no need to add ref here, as a new MutationBreakpointDelegate is initialized with 1 ref count
    }
    mutationBreakpointDelegate->AddRef();
    return mutationBreakpointDelegate;
}

bool Js::MutationBreakpoint::IsValid() const
{
    return isValid;
}

void Js::MutationBreakpoint::Invalidate()
{
    AssertMsg(isValid, "Breakpoint already invalid");
    isValid = false;
}

// Return true if breakpoint should break on object with a specific mutation type
bool Js::MutationBreakpoint::ShouldBreak(MutationType type)
{
    return ShouldBreak(type, Constants::NoProperty);
}

// Return true if breakpoint should break on object, or a property pid, with
// a specific mutation type
bool Js::MutationBreakpoint::ShouldBreak(MutationType type, PropertyId pid)
{
    Assert(isValid);
    if (mFlag == MutationTypeNone && pid == Constants::NoProperty)
    {
        return false;
    }
    else if (type != MutationTypeNone && (type & mFlag) == type)
    {
        // Break on object
        return true;
    }

    // search properties vector
    for (int i = 0; i < properties->Count(); i++)
    {
        PropertyMutation pm = properties->Item(i);

        if (pm.pr->GetPropertyId() == pid)
        {
            if (pm.mFlag == MutationTypeNone)
            {
                return false;
            }
            else if (type != MutationTypeNone && (pm.mFlag & type) == type)
            {
                return true;
            }
            break;
        }
    }
    return false;
}

bool Js::MutationBreakpoint::Reset()
{
    // Invalidate breakpoint
    if (isValid)
    {
        this->Invalidate();
    }

    // Release existing delegate object
    if (mutationBreakpointDelegate)
    {
        mutationBreakpointDelegate->Release();
        mutationBreakpointDelegate = nullptr;
    }

    // Clear all property records
    if (properties)
    {
        properties->ClearAndZero();
    }

    // Get object and remove strong ref
    DynamicObject *obj = this->obj->Get();
    return obj && obj->SetInternalProperty(InternalPropertyIds::MutationBp, nullptr, PropertyOperation_SpecialValue, NULL);
}

void Js::MutationBreakpoint::SetBreak(MutationType type, const PropertyRecord *pr)
{
    // Break on entire object if pid is NoProperty
    if (!pr)
    {
        if (type == MutationTypeNone)
        {
            mFlag = MutationTypeNone;
        }
        else
        {
            mFlag = static_cast<MutationType>(mFlag | type);
        }
        return;
    }

    // Check if property is already added
    for (int i = 0; i < properties->Count(); i++)
    {
        PropertyMutation& pm = properties->Item(i);
        if (pm.pr == pr)
        {
            // added to existing property mutation struct
            if (type == MutationTypeNone)
            {
                pm.mFlag = MutationTypeNone;
            }
            else
            {
                pm.mFlag = static_cast<MutationType>(pm.mFlag | type);
            }
            return;
        }
    }
    // if not in list, add new property mutation
    PropertyMutation pm = {
        pr,
        type
    };
    properties->Add(pm);
}

void Js::MutationBreakpoint::Break(ScriptContext *scriptContext, MutationType mutationType, const PropertyRecord *pr)
{
    this->didCauseBreak = true;
    this->breakMutationType = mutationType;
    this->propertyRecord = pr;

    InterpreterHaltState haltState(STOP_MUTATIONBREAKPOINT, /*executingFunction*/nullptr, this);
    scriptContext->GetDebugContext()->GetProbeContainer()->DispatchMutationBreakpoint(&haltState);

    this->didCauseBreak = false;
}

bool Js::MutationBreakpoint::GetDidCauseBreak() const
{
    return this->didCauseBreak;
}

const wchar_t * Js::MutationBreakpoint::GetBreakPropertyName() const
{
    Assert(this->didCauseBreak);
    Assert(this->propertyRecord);
    return this->propertyRecord->GetBuffer();
}

const Js::PropertyId Js::MutationBreakpoint::GetParentPropertyId() const
{
    Assert(this->didCauseBreak);
    return this->parentPropertyId;
}

const wchar_t * Js::MutationBreakpoint::GetParentPropertyName() const
{
    Assert(this->didCauseBreak);
    const PropertyRecord *pr = nullptr;
    if ((this->parentPropertyId != Constants::NoProperty) && (this->obj != nullptr))
    {
        DynamicObject *dynObj = this->obj->Get();
        if (dynObj != nullptr)
        {
            pr = dynObj->GetScriptContext()->GetPropertyName(this->parentPropertyId);
            return pr->GetBuffer();
        }
    }
    return L"";
}

MutationType Js::MutationBreakpoint::GetBreakMutationType() const
{
    Assert(this->didCauseBreak);
    return this->breakMutationType;
}

const Js::PropertyId Js::MutationBreakpoint::GetBreakPropertyId() const
{
    Assert(this->didCauseBreak);
    Assert(this->propertyRecord);
    return this->propertyRecord->GetPropertyId();
}

const wchar_t * Js::MutationBreakpoint::GetBreakMutationTypeName(MutationType mutationType)
{
    switch (mutationType)
    {
    case MutationTypeUpdate: return L"Changing";
    case MutationTypeDelete: return L"Deleting";
    case MutationTypeAdd: return L"Adding";
    default: AssertMsg(false, "Unhandled break reason mutation type. Did we add a new mutation type?"); return L"";
    }
}

const wchar_t * Js::MutationBreakpoint::GetMutationTypeForConditionalEval(MutationType mutationType)
{
    switch (mutationType)
    {
    case MutationTypeUpdate: return L"update";
    case MutationTypeDelete: return L"delete";
    case MutationTypeAdd: return L"add";
    default: AssertMsg(false, "Unhandled mutation type in conditional object mutation breakpoint."); return L"";
    }
}

// setOnObject - Is true if we are watching the parent object
// parentPropertyId - Property ID of object on which Mutation is set
// propertyId - Property ID of property. If setOnObject is false we are watching a specific property (propertyId)

Js::MutationBreakpointDelegate * Js::MutationBreakpoint::Set(ScriptContext *scriptContext, Var obj, BOOL setOnObject, MutationType type, PropertyId parentPropertyId, PropertyId propertyId)
{
    Assert(obj);
    Assert(scriptContext);

    if (!CanSet(obj))
    {
        return nullptr;
    }
    DynamicObject *dynObj = static_cast<DynamicObject*>(obj);

    const PropertyRecord *pr = nullptr;

    if (!setOnObject && (propertyId != Constants::NoProperty))
    {
        pr = scriptContext->GetPropertyName(propertyId);
        Assert(pr);
    }

    MutationBreakpoint *bp = nullptr;

    // Breakpoint exists; update it.
    if (dynObj->GetInternalProperty(dynObj, InternalPropertyIds::MutationBp, reinterpret_cast<Var*>(&bp), nullptr, scriptContext)
        && bp)
    {
        // Valid bp; update it.
        if (bp->IsValid())
        {
            Assert(bp->mutationBreakpointDelegate); // Delegate must already exist
            // set breakpoint
            bp->SetBreak(type, pr);

            return bp->GetDelegate();
        }
        // bp invalidated by haven't got cleaned up yet, so reset it right here
        // and then create new bp
        else
        {
            bp->Reset();
        }
    }

    // Create new breakpoint
    bp = MutationBreakpoint::New(scriptContext, dynObj, const_cast<PropertyRecord *>(pr), type, parentPropertyId);

    // Adding reference to dynamic object
    dynObj->SetInternalProperty(InternalPropertyIds::MutationBp, bp, PropertyOperation_SpecialValue, nullptr);

    // Track in object's own script context
    dynObj->GetScriptContext()->InsertMutationBreakpoint(bp);

    return bp->GetDelegate();
}

void Js::MutationBreakpoint::Finalize(bool isShutdown) {}
void Js::MutationBreakpoint::Dispose(bool isShutdown)
{
    // TODO (t-shchan): If removed due to detaching debugger, do not fire event
    // TODO (t-shchan): Fire debugger event for breakpoint removal
    if (mutationBreakpointDelegate)
    {
        mutationBreakpointDelegate->Release();
    }
}
void Js::MutationBreakpoint::Mark(Recycler * recycler) {}


/*
    MutationBreakpointDelegate definition
*/

Js::MutationBreakpointDelegate::MutationBreakpointDelegate(Js::MutationBreakpoint *bp)
    : m_refCount(1), m_breakpoint(bp), m_didCauseBreak(false), m_propertyRecord(nullptr), m_type(MutationTypeNone)
{
    Assert(bp != nullptr);
}

Js::MutationBreakpointDelegate * Js::MutationBreakpointDelegate::New(Js::MutationBreakpoint *bp)
{
    return HeapNew(Js::MutationBreakpointDelegate, bp);
}

/*
    IMutationBreakpoint interface definition
*/

STDMETHODIMP_(ULONG) Js::MutationBreakpointDelegate::AddRef()
{
    return (ulong)InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) Js::MutationBreakpointDelegate::Release()
{
    ulong refCount = (ulong)InterlockedDecrement(&m_refCount);

    if (0 == refCount)
    {
        HeapDelete(this);
    }

    return refCount;
}

STDMETHODIMP Js::MutationBreakpointDelegate::QueryInterface(REFIID iid, void ** ppv)
{
    if (!ppv)
    {
        return E_INVALIDARG;
    }

    if (__uuidof(IUnknown) == iid || __uuidof(IMutationBreakpoint) == iid)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMutationBreakpoint*>(this));
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP Js::MutationBreakpointDelegate::Delete(void)
{
    this->m_breakpoint->Invalidate();
    return S_OK;
}

STDMETHODIMP Js::MutationBreakpointDelegate::DidCauseBreak(
    /* [out] */ __RPC__out BOOL *didCauseBreak)
{
    if (!didCauseBreak)
    {
        return E_INVALIDARG;
    }
    *didCauseBreak = this->m_breakpoint->GetDidCauseBreak();
    return S_OK;
}
#endif
