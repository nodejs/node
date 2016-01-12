//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef ENABLE_MUTATION_BREAKPOINT
namespace Js
{
    class MutationBreakpoint;

    class MutationBreakpointDelegate : public IMutationBreakpoint
    {
        MutationBreakpointDelegate(MutationBreakpoint *bp);

        ULONG m_refCount;
        MutationBreakpoint *m_breakpoint;
        bool m_didCauseBreak;
        PropertyRecord *m_propertyRecord;
        MutationType m_type;
    public:
        // Creates a new delegate object on the heap
        static MutationBreakpointDelegate * New(MutationBreakpoint *bp);
        // Get the breakpoint which the the delegate object associate with
        MutationBreakpoint * GetBreakpoint() const;

        /* IMutationBreakpoint methods */

        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();
        STDMETHODIMP QueryInterface(REFIID iid, void ** ppv);

        STDMETHODIMP Delete(void);
        STDMETHODIMP DidCauseBreak(
            /* [out] */ __RPC__out BOOL *didCauseBreak);
    };

    struct PropertyMutation
    {
        const PropertyRecord *pr;
        MutationType mFlag;
    };

    class MutationBreakpoint : FinalizableObject
    {
        bool isValid;
        bool didCauseBreak;
        MutationType mFlag;
        RecyclerWeakReference<DynamicObject> * obj;
        Var newValue;
        PropertyId parentPropertyId;
        const PropertyRecord *propertyRecord;
        typedef JsUtil::List<PropertyMutation, Recycler> PropertyMutationList;
        PropertyMutationList *properties;
        MutationBreakpointDelegate *mutationBreakpointDelegate;
        MutationType breakMutationType;

        void Break(ScriptContext *scriptContext, MutationType mutationType, const PropertyRecord *pr);

        MutationBreakpointDelegate * GetDelegate();
        bool DeleteProperty(PropertyRecord *pr);
        static MutationBreakpoint * New(ScriptContext *scriptContext, DynamicObject *obj, const Js::PropertyRecord *pr, MutationType type, Js::PropertyId parentPropertyId);
    public:
        MutationBreakpoint(ScriptContext *scriptContext, DynamicObject *obj, const PropertyRecord *pr, MutationType type, Js::PropertyId parentPropertyId);
        ~MutationBreakpoint();

        bool IsValid() const;
        void Invalidate();

        // Invalidate(), release delegate, and remove strong reference from DynamicObject
        bool Reset();

        void SetBreak(MutationType type, const PropertyRecord *pr);

        bool ShouldBreak(MutationType type);
        bool ShouldBreak(MutationType type, PropertyId pid);

        bool GetDidCauseBreak() const;
        const PropertyId GetBreakPropertyId() const;
        const wchar_t * GetBreakPropertyName() const;
        const PropertyId GetParentPropertyId() const;
        const wchar_t * GetParentPropertyName() const;
        MutationType GetBreakMutationType() const;

        const Var GetMutationObjectVar() const;
        const Var GetBreakNewValueVar() const;

        /* Static methods */
        // Whether mutation breakpoint is enabled (and if debug is enabled on scriptContext)
        static bool IsFeatureEnabled();
        static bool IsFeatureEnabled(ScriptContext *scriptContext);

        // Whether a mutation breakpoint could be set on an object
        static bool CanSet(Var object);

        // Setting a mutation breakpoint on an object/property of an object
        static MutationBreakpointDelegate * Set(ScriptContext *scriptContext, Var obj, BOOL setOnObject, MutationType type, PropertyId parentPropertyId, PropertyId propertyId);

        // Mutation handlers
        static bool HandleSetProperty(ScriptContext *scriptContext, RecyclableObject *object, PropertyId propertyId, Var newValue);
        static void HandleDeleteProperty(ScriptContext *scriptContext, Var instance, PropertyId propertyId);

        static const wchar_t * GetBreakMutationTypeName(MutationType mutationType);
        static const wchar_t * GetMutationTypeForConditionalEval(MutationType mutationType);

        /* Override methods - FinalizableObject */
        virtual void Finalize(bool isShutdown);
        virtual void Dispose(bool isShutdown);
        virtual void Mark(Recycler * recycler);
    };
}
#endif
