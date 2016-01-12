//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#define ARRAY_CROSSOVER_FOR_VALIDATE 0

namespace Js
{

    class SegmentBTree
    {
        // This is an auxiliary data structure to speed finding the correct array segment for sparse arrays.

        // Rather than implement remove we only implement SwapSegment which requires the segment to be
        // swapped is in the same relative order as the segment it replaces.

        // The B-tree algorithm used is adapted from the pseudo-code in
        // Introduction to Algorithms by Corman, Leiserson, and Rivest.

    protected:
        uint32*              keys;           // keys[i] == segments[i]->left
        SparseArraySegmentBase** segments;   // Length of segmentCount.
        SegmentBTree*        children;       // Length of segmentCount+1.
        uint32               segmentCount;   // number of sparseArray segments in the Node

    public:
        static const uint MinDegree = 20; // Degree is the minimum branching factor. (If non-root, and non-leaf.)
                                          // non-root nodes are between MinDegree and MinDegree*2-1 in size.
                                          // e.g. For MinDegree == 32 ->  this is 31 to 62 keys
                                          // and 32 to 63 children (every key is surrounded by before and after children).
                                          //
                                          // Allocations are simply the max possible sizes of nodes
                                          // We may do something more clever in the future.

        static const uint32 MinKeys   = MinDegree - 1;  // Minimum number of keys in any non-root node.
        static const uint32 MaxKeys   = MinDegree*2 - 1;// Max number of keys in any node
        static const uint32 MaxDegree = MinDegree*2;    // Max number of children

        static uint32 GetLazyCrossOverLimit(); // = MinDegree*3;  // This is the crossover point for using the segmentBTee in our Arrays
                                                    // Ideally this doesn't belong here.
                                                    // Putting it here simply acknowledges that this BTree is not generic.
                                                    // The implementation is tightly coupled with it's use in arrays.

                                                    // The segment BTree adds memory overhead, we only want to incur it if
                                                    // it is needed to prevent O(n) effects from using large sparse arrays
        // the BtreeNode is implicit:
        // btreenode := (children[0], segments[0], children[1], segments[1], ... segments[segmentCount-1], children[segmentCount])
        // Children pointers to the left contain segments strictly less than the segment to the right
        // Children points to the right contain segments strictly greater than the segment to the left.
        // Segments do not overlap, so the left index in a segment is sufficient to determine ordering.

        // keys are replicated in another array so that we do not incur the overhead of touching the memory for segments
        // that are uninteresting.


    public:
        SegmentBTree();


        void SwapSegment(uint32 originalKey, SparseArraySegmentBase* oldSeg, SparseArraySegmentBase* newSeg);

        template<typename Func>
        void Walk(Func& func) const;

    protected:

        BOOL IsLeaf() const;
        BOOL IsFullNode() const;

        static void InternalFind(SegmentBTree* node, uint32 itemIndex, SparseArraySegmentBase*& prev, SparseArraySegmentBase*& matchOrNext);
        static void SplitChild(Recycler* recycler, SegmentBTree* tree, uint32 count, SegmentBTree* root);
        static void InsertNonFullNode(Recycler* recycler, SegmentBTree* tree, SparseArraySegmentBase* newSeg);
    };

    class SegmentBTreeRoot : public SegmentBTree
    {
    public:
        void Add(Recycler* recycler, SparseArraySegmentBase* newSeg);
        void Find(uint itemIndex, SparseArraySegmentBase*& prevOrMatch, SparseArraySegmentBase*& matchOrNext);

        SparseArraySegmentBase * lastUsedSegment;
    };

    class JavascriptArray : public ArrayObject
    {
        template <class TPropertyIndex>
        friend class ES5ArrayTypeHandlerBase;

    public:
        static const size_t StackAllocationSize;

    private:
        static PropertyId const specialPropertyIds[];

    protected:
        DEFINE_VTABLE_CTOR(JavascriptArray, ArrayObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptArray);
    private:
        bool isInitialized;
    protected:
        SparseArraySegmentBase* head;
        union
        {
            SparseArraySegmentBase* lastUsedSegment;
            SegmentBTreeRoot* segmentBTreeRoot;
        }segmentUnion;
    public:
        typedef Var TElement;

        static const SparseArraySegmentBase *EmptySegment;

        static uint32 const InvalidIndex = 0xFFFFFFFF;
        static uint32 const MaxArrayLength = InvalidIndex;
        static uint32 const MaxInitialDenseLength=1<<18;
        static ushort const MergeSegmentsLengthHeuristics = 128; // If the length is less than MergeSegmentsLengthHeuristics then try to merge the segments

        static const Var MissingItem;
        template<typename T> static T GetMissingItem();

        SparseArraySegmentBase * GetHead() const { return head; }
        SparseArraySegmentBase * GetLastUsedSegment() const;
    public:
        JavascriptArray(DynamicType * type);
        JavascriptArray(uint32 length, uint32 size, DynamicType * type);
        JavascriptArray(DynamicType * type, uint32 size);

        static Var OP_NewScArray(uint32 argLength, ScriptContext* scriptContext);
        static Var OP_NewScArrayWithElements(uint32 argLength, Var *elements, ScriptContext* scriptContext);
        static Var OP_NewScArrayWithMissingValues(uint32 argLength, ScriptContext* scriptContext);
        static Var OP_NewScIntArray(AuxArray<int32> *ints, ScriptContext* scriptContext);
        static Var OP_NewScFltArray(AuxArray<double> *doubles, ScriptContext* scriptContext);

#if ENABLE_PROFILE_INFO
        static Var ProfiledNewScArray(uint32 argLength, ScriptContext *scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef);
        static Var ProfiledNewScIntArray(AuxArray<int32> *ints, ScriptContext* scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef);
        static Var ProfiledNewScFltArray(AuxArray<double> *doubles, ScriptContext* scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef);
        static Var ProfiledNewInstanceNoArg(RecyclableObject *function, ScriptContext *scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef);
#endif

        static TypeId OP_SetNativeIntElementC(JavascriptNativeIntArray *arr, uint32 index, Var value, ScriptContext *scriptContext);
        static TypeId OP_SetNativeFloatElementC(JavascriptNativeFloatArray *arr, uint32 index, Var value, ScriptContext *scriptContext);
        template<typename T> void SetArrayLiteralItem(uint32 index, T value);

        void Sort(RecyclableObject* compFn);

        template<typename NativeArrayType, typename T> NativeArrayType * ConvertToNativeArrayInPlace(JavascriptArray *varArray);

        template <typename T> T GetNativeValue(Var iVal, ScriptContext * scriptContext);
        template <> int32 GetNativeValue<int32>(Var iVal, ScriptContext * scriptContext);
        template <> double GetNativeValue<double>(Var iVal, ScriptContext * scriptContext);

        template<typename T> void ChangeArrayTypeToNativeArray(JavascriptArray * varArray, ScriptContext * scriptContext);
        template<> void ChangeArrayTypeToNativeArray<double>(JavascriptArray * varArray, ScriptContext * scriptContext);
        template<> void ChangeArrayTypeToNativeArray<int32>(JavascriptArray * varArray, ScriptContext * scriptContext);

        template<typename T> inline BOOL DirectGetItemAt(uint32 index, T* outVal);
        virtual BOOL DirectGetVarItemAt(uint index, Var* outval, ScriptContext *scriptContext);
        virtual BOOL DirectGetItemAtFull(uint index, Var* outVal);
        virtual Var DirectGetItem(uint32 index);

        Var DirectGetItem(JavascriptString *propName, ScriptContext* scriptContext);

        template<typename T> inline void DirectSetItemAt(uint32 itemIndex, T newValue);
        template<typename T> inline void DirectSetItemInLastUsedSegmentAt(const uint32 offset, const T newValue);
#if ENABLE_PROFILE_INFO
        template<typename T> inline void DirectProfiledSetItemInHeadSegmentAt(const uint32 offset, const T newValue, StElemInfo *const stElemInfo);
#endif
        template<typename T> void DirectSetItem_Full(uint32 itemIndex, T newValue);
        template<typename T> SparseArraySegment<T>* PrepareSegmentForMemOp(uint32 startIndex, uint32 length);
        template<typename T> void DirectSetItemAtRange(uint32 startIndex, uint32 length, T newValue);
        template<typename T> void DirectSetItemAtRangeFull(uint32 startIndex, uint32 length, T newValue);
        template<typename T> void DirectSetItemAtRangeFromArray(uint32 startIndex, uint32 length, JavascriptArray *fromArray, uint32 fromStartIndex);
#if DBG
        template <typename T> void VerifyNotNeedMarshal(T value) {};
        template <> void VerifyNotNeedMarshal<Var>(Var value) { Assert(value == JavascriptArray::MissingItem || !CrossSite::NeedMarshalVar(value, this->GetScriptContext())); }
#endif
        void DirectSetItemIfNotExist(uint32 index, Var newValue);

        template<typename T> BOOL DirectDeleteItemAt(uint32 itemIndex);

        virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) override
        {
            Var value = nullptr;
            return this->DirectGetItemAt(index, &value) ? WritableData : None;
        }

        static bool Is(Var aValue);
        static bool Is(TypeId typeId);
        static JavascriptArray* FromVar(Var aValue);

        static bool IsVarArray(Var aValue);
        static bool IsVarArray(TypeId typeId);

        static JavascriptArray* FromAnyArray(Var aValue);
        static bool IsDirectAccessArray(Var aValue);

        void SetLength(uint32 newLength);
        BOOL SetLength(Var newLength);
        virtual void ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength);

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo Concat;
            static FunctionInfo Every;
            static FunctionInfo Filter;
            static FunctionInfo ForEach;
            static FunctionInfo IndexOf;
            static FunctionInfo Includes;
            static FunctionInfo Join;
            static FunctionInfo LastIndexOf;
            static FunctionInfo Map;
            static FunctionInfo Pop;
            static FunctionInfo Push;
            static FunctionInfo Reduce;
            static FunctionInfo ReduceRight;
            static FunctionInfo Reverse;
            static FunctionInfo Shift;
            static FunctionInfo Slice;
            static FunctionInfo Some;
            static FunctionInfo Sort;
            static FunctionInfo Splice;
            static FunctionInfo ToString;
            static FunctionInfo ToLocaleString;
            static FunctionInfo Unshift;
            static FunctionInfo IsArray;
            static FunctionInfo Find;
            static FunctionInfo FindIndex;
            static FunctionInfo Entries;
            static FunctionInfo Keys;
            static FunctionInfo Values;
            static FunctionInfo CopyWithin;
            static FunctionInfo Fill;
            static FunctionInfo From;
            static FunctionInfo Of;
            static FunctionInfo GetterSymbolSpecies;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewInstance(RecyclableObject* function, Arguments args);
        static Var ProfiledNewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryConcat(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEvery(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFilter(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryForEach(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryJoin(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryMap(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryPop(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryPush(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReduce(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReduceRight(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReverse(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryShift(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySlice(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySome(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySort(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySplice(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryUnshift(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIsArray(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFind(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFindIndex(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEntries(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryKeys(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValues(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryCopyWithin(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFill(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFrom(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...);

        static Var Push(ScriptContext * scriptContext, Var object, Var value);
        static Var EntryPushNonJavascriptArray(ScriptContext * scriptContext, Var * args, uint argCount);
        static Var EntryPushJavascriptArray(ScriptContext * scriptContext, Var * args, uint argCount);
        static Var EntryPushJavascriptArrayNoFastPath(ScriptContext * scriptContext, Var * args, uint argCount);

        static Var Pop(ScriptContext * scriptContext, Var object);


        static Var EntryPopJavascriptArray(ScriptContext * scriptContext, Var object);
        static Var EntryPopNonJavascriptArray(ScriptContext * scriptContext, Var object);

#if DEBUG
        static BOOL GetIndex(const wchar_t* propName, ulong *pIndex);
#endif

        uint32 GetNextIndex(uint32 index) const;
        template<typename T> uint32 GetNextIndexHelper(uint32 index) const;
#ifdef VALIDATE_ARRAY
        virtual void ValidateArray();
        void ValidateArrayCommon();
        template<typename T> static void ValidateSegment(SparseArraySegment<T>* seg);
        static void ValidateVarSegment(SparseArraySegment<Var>* seg);
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        void CheckForceES5Array();
#endif

        virtual BOOL HasProperty(PropertyId propertyId) override;
        virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags) override;
        virtual BOOL IsEnumerable(PropertyId propertyId) override;
        virtual BOOL IsConfigurable(PropertyId propertyId) override;
        virtual BOOL SetEnumerable(PropertyId propertyId, BOOL value) override;
        virtual BOOL SetWritable(PropertyId propertyId, BOOL value) override;
        virtual BOOL SetConfigurable(PropertyId propertyId, BOOL value) override;
        virtual BOOL SetAttributes(PropertyId propertyId, PropertyAttributes attributes) override;

        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) override;

        virtual BOOL HasItem(uint32 index) override;
        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags) override;
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags) override;

        virtual BOOL SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags) override;
        virtual BOOL PreventExtensions() override;
        virtual BOOL Seal() override;
        virtual BOOL Freeze() override;

        virtual BOOL GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics = true, bool enumSymbols = false) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext) override;
        virtual uint GetSpecialPropertyCount() const override;
        virtual PropertyId const * GetSpecialPropertyIds() const override;
        virtual DescriptorFlags GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;

        // objectArray support
        virtual BOOL SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes) override;
        virtual BOOL SetItemAttributes(uint32 index, PropertyAttributes attributes) override;
        virtual BOOL SetItemAccessors(uint32 index, Var getter, Var setter) override;
        virtual BOOL IsObjectArrayFrozen() override;
        virtual BOOL GetEnumerator(Var originalInstance, BOOL enumNonEnumerable, Var* enumerator, ScriptContext* requestContext, bool preferSnapshotSemantics = true, bool enumSymbols = false) override;

        // Get non-index enumerator for SCA
        virtual BOOL GetNonIndexEnumerator(Var* enumerator, ScriptContext* requestContext);
        virtual BOOL IsItemEnumerable(uint32 index);

        template<typename Func>
        void WalkExisting(Func func)
        {
            Assert(!JavascriptNativeIntArray::Is(this) && !JavascriptNativeFloatArray::Is(this));
            ArrayElementEnumerator e(this, 0);
            while(e.MoveNext<Var>())
            {
                func(e.GetIndex(), e.GetItem<Var>());
            }
        }

        static JavascriptArray* CreateArrayFromConstructor(RecyclableObject* constructor, uint32 length, ScriptContext* scriptContext);

        template<typename unitType, typename className>
        static className* New(Recycler* recycler, DynamicType* arrayType);
        template<typename unitType, typename className, uint inlineSlots>
        static className* New(uint32 length, DynamicType* arrayType, Recycler* recycler);
        template<typename unitType, typename className, uint inlineSlots>
        static className* NewLiteral(uint32 length, DynamicType* arrayType, Recycler* recycler);
#if ENABLE_COPYONACCESS_ARRAY
        template<typename unitType, typename className, uint inlineSlots>
        static className* NewCopyOnAccessLiteral(DynamicType* arrayType, ArrayCallSiteInfo *arrayInfo, FunctionBody *functionBody, const Js::AuxArray<int32> *ints, Recycler* recycler);
#endif
        static bool HasInlineHeadSegment(uint32 length);

        template<class T, uint InlinePropertySlots>
        static T *New(void *const stackAllocationPointer, const uint32 length, DynamicType *const arrayType);
        template<class T, uint InlinePropertySlots>
        static T *NewLiteral(void *const stackAllocationPointer, const uint32 length, DynamicType *const arrayType);

        static JavascriptArray *EnsureNonNativeArray(JavascriptArray *arr);

#if ENABLE_PROFILE_INFO
        virtual JavascriptArray *FillFromArgs(uint length, uint start, Var *args, ArrayCallSiteInfo *info = nullptr, bool dontCreateNewArray = false);
#else
        virtual JavascriptArray *FillFromArgs(uint length, uint start, Var *args, bool dontCreateNewArray = false);
#endif

    protected:
        // Use static New methods to create array.
        JavascriptArray(uint32 length, DynamicType * type);

        // For BoxStackInstance
        JavascriptArray(JavascriptArray * instance, bool boxHead);

        template<typename T> inline void LinkSegments(SparseArraySegment<T>* prev, SparseArraySegment<T>* current);
        template<typename T> inline SparseArraySegment<T>* ReallocNonLeafSegment(SparseArraySegment<T>* seg, SparseArraySegmentBase* nextSeg);
        void TryAddToSegmentMap(Recycler* recycler, SparseArraySegmentBase* seg);

    private:
        DynamicObjectFlags GetFlags() const;
        DynamicObjectFlags GetFlags_Unchecked() const; // do not use except in extreme circumstances
        void SetFlags(const DynamicObjectFlags flags);
        void LinkSegmentsCommon(SparseArraySegmentBase* prev, SparseArraySegmentBase* current);

    public:
        static JavascriptArray *GetArrayForArrayOrObjectWithArray(const Var var);
        static JavascriptArray *GetArrayForArrayOrObjectWithArray(const Var var, bool *const isObjectWithArrayRef, TypeId *const arrayTypeIdRef);
        static const SparseArraySegmentBase *Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(const Var var);
        static uint32 Jit_GetArrayHeadSegmentLength(const SparseArraySegmentBase *const headSegment);
        static bool Jit_OperationInvalidatedArrayHeadSegment(const SparseArraySegmentBase *const headSegmentBeforeOperation, const uint32 headSegmentLengthBeforeOperation, const Var varAfterOperation);
        static uint32 Jit_GetArrayLength(const Var var);
        static bool Jit_OperationInvalidatedArrayLength(const uint32 lengthBeforeOperation, const Var varAfterOperation);
        static DynamicObjectFlags Jit_GetArrayFlagsForArrayOrObjectWithArray(const Var var);
        static bool Jit_OperationCreatedFirstMissingValue(const DynamicObjectFlags flagsBeforeOperation, const Var varAfterOperation);

    public:
        bool HasNoMissingValues() const; // if true, the head segment has no missing values
        bool HasNoMissingValues_Unchecked() const; // do not use except in extreme circumstances
        void SetHasNoMissingValues(const bool hasNoMissingValues = true);

        virtual bool IsMissingHeadSegmentItem(const uint32 index) const;

        static VTableValue VtableHelper()
        {
            return VTableValue::VtableJavascriptArray;
        }
        static LibraryValue InitialTypeHelper()
        {
            return LibraryValue::ValueJavascriptArrayType;
        }
        static DynamicType * GetInitialType(ScriptContext * scriptContext);
    public:
        static uint32 defaultSmallSegmentAlignedSize;
        template<typename unitType, typename classname>
        inline BOOL TryGrowHeadSegmentAndSetItem(uint32 indexInt, unitType iValue);

        static int64 GetIndexFromVar(Js::Var arg, int64 length, ScriptContext* scriptContext);
        template <typename T>
        static Var MapHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext);
        static Var FillHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, int64 length, Arguments& args, ScriptContext* scriptContext);
        static Var CopyWithinHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, int64 length, Arguments& args, ScriptContext* scriptContext);
        template <typename T>
        static BOOL GetParamForIndexOf(T length, Arguments const & args, Var& search, T& fromIndex, ScriptContext * scriptContext);
        static BOOL GetParamForLastIndexOf(int64 length, Arguments const & args, Var& search, int64& fromIndex, ScriptContext * scriptContext);

        template <bool includesAlgorithm, typename T, typename P = uint32>
        static Var TemplatedIndexOfHelper(T* pArr, Var search, P fromIndex, P toIndex, ScriptContext * scriptContext);
        template <typename T>
        static Var LastIndexOfHelper(T* pArr, Var search, int64 fromIndex, ScriptContext * scriptContext);
        template <typename T>
        static BOOL TemplatedGetItem(T *pArr, uint32 index, Var * element, ScriptContext * scriptContext);
        template <typename T>
        static BOOL TemplatedGetItem(T *pArr, uint64 index, Var * element, ScriptContext * scriptContext);
        template <typename T = uint32>
        static Var ReverseHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, ScriptContext* scriptContext);
        template <typename T = uint32>
        static Var SliceHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext);
        template <typename T = uint32>
        static Var EveryHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext);
        template <typename T = uint32>
        static Var SomeHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext);
        template <bool findIndex>
        static Var FindHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, int64 length, Arguments& args, ScriptContext* scriptContext);
        template <typename T = uint32>
        static Var ReduceHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext);
        template <typename T = uint32>
        static Var ReduceRightHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext);
        static Var OfHelper(bool isTypedArrayEntryPoint, Arguments& args, ScriptContext* scriptContext);

    protected:
        template<class T> bool IsMissingHeadSegmentItemImpl(const uint32 index) const;
        SegmentBTreeRoot * GetSegmentMap() const;
        void SetHeadAndLastUsedSegment(SparseArraySegmentBase * segment);
        void SetLastUsedSegment(SparseArraySegmentBase * segment);
        bool HasSegmentMap() const;

    private:
        void SetSegmentMap(SegmentBTreeRoot * segmentMap);
        void ClearSegmentMap();

        template <typename Fn> SparseArraySegmentBase * ForEachSegment(Fn fn) const;
        template <typename Fn> static SparseArraySegmentBase * ForEachSegment(SparseArraySegmentBase * segment, Fn fn);

        template<typename T> bool NeedScanForMissingValuesUponSetItem(SparseArraySegment<T> *const segment, const uint32 offset) const;
        template<typename T> void ScanForMissingValues(const uint startIndex = 0);
        template<typename T> bool ScanForMissingValues(const uint startIndex, const uint endIndex);
        template<typename T, uint InlinePropertySlots> static SparseArraySegment<typename T::TElement> *InitArrayAndHeadSegment(T *const array, const uint32 length, const uint32 size, const bool wasZeroAllocated);
        template<typename T> static void SliceHelper(JavascriptArray*pArr, JavascriptArray* pNewArr, uint32 start, uint32 newLen);

        template<typename T>
        static void ShiftHelper(JavascriptArray* pArr, ScriptContext * scriptContext);

        template<typename T>
        static void UnshiftHelper(JavascriptArray* pArr, uint32 unshiftElements, Js::Var * elements);

        template<typename T>
        static void GrowArrayHeadHelperForUnshift(JavascriptArray* pArr, uint32 unshiftElements, ScriptContext * scriptContext);

        static uint32 GetFromIndex(Var arg, uint32 length, ScriptContext *scriptContext);
        static uint64 GetFromIndex(Var arg, uint64 length, ScriptContext *scriptContext);
        static int64 GetFromLastIndex(Var arg, int64 length, ScriptContext *scriptContext);
        static JavascriptString* JoinToString(Var value, ScriptContext* scriptContext);
        static JavascriptString* JoinHelper(Var thisArg, JavascriptString* separatorStr, ScriptContext* scriptContext);
        template <typename T>
        static JavascriptString* JoinArrayHelper(T * arr, JavascriptString* separatorStr, ScriptContext* scriptContext);
        static JavascriptString* JoinOtherHelper(RecyclableObject *object, JavascriptString* separatorStr, ScriptContext* scriptContext);

        template <bool includesAlgorithm>
        static Var IndexOfHelper(Arguments const & args, ScriptContext *scriptContext);

        virtual int32 HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext);


        template<typename T>
        static void ArraySpliceHelper(JavascriptArray* pNewArr, JavascriptArray* pArr, uint32 start, uint32 deleteLen,
                                                    Var* insertArgs, uint32 insertLen, ScriptContext *scriptContext);
        template<typename T>
        static void ArraySegmentSpliceHelper(JavascriptArray *pnewArr, SparseArraySegment<T> *seg, SparseArraySegment<T> **prev, uint32 start, uint32 deleteLen,
                                                    Var* insertArgs, uint32 insertLen, Recycler *recycler);
        template<typename T>
        static RecyclableObject* ObjectSpliceHelper(RecyclableObject* pObj, uint32 len, uint32 start, uint32 deleteLen,
                                                    Var* insertArgs, uint32 insertLen, ScriptContext *scriptContext, RecyclableObject* pNewObj = nullptr);
        static JavascriptString* ToLocaleStringHelper(Var value, ScriptContext* scriptContext);
        static Js::JavascriptArray* CreateNewArrayHelper(uint32 len, bool isIntArray, bool isFloatArray, Js::JavascriptArray *baseArray, ScriptContext* scriptContext);

        void FillFromPrototypes(uint32 startIndex, uint32 endIndex);
        bool IsFillFromPrototypes();
        void GetArrayTypeAndConvert(bool* isIntArray, bool* isFloatArray);
        template<typename T> void EnsureHeadStartsFromZero(Recycler * recycler);

        SparseArraySegmentBase * GetBeginLookupSegment(uint32 index, const bool useSegmentMap = true) const;
        SegmentBTreeRoot *  BuildSegmentMap();
        void InvalidateLastUsedSegment();
        inline BOOL IsFullArray() const; // no missing elements till array length
        inline BOOL IsSingleSegmentArray() const;

        template<typename T> void AllocateHead();
        template<typename T> void EnsureHead();

        uint32 sort(__inout_ecount(*length) Var *orig, uint32 *length, ScriptContext *scriptContext);

        BOOL GetPropertyBuiltIns(PropertyId propertyId, Var* value);
        bool GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* descriptorFlags);
    private:
        struct Element {
            Var Value;
            JavascriptString* StringValue;
        };

        static int __cdecl CompareElements(void* context, const void* elem1, const void* elem2);
        void SortElements(Element* elements, uint32 left, uint32 right);

        template <typename Fn>
        static void ForEachOwnArrayIndexOfObject(RecyclableObject* obj, uint32 startIndex, uint32 limitIndex, Fn fn);

        template <typename T, typename Fn>
        static void ForEachOwnMissingArrayIndexOfObject(JavascriptArray *baseArr, JavascriptArray *destArray, RecyclableObject* obj, uint32 startIndex, uint32 limitIndex, T destIndex, Fn fn);

        // NativeArrays may change it's content type, but not others
        template <typename T> static bool MayChangeType() { return false; }
        template <> static bool MayChangeType<JavascriptNativeIntArray>() { return true; }
        template <> static bool MayChangeType<JavascriptNativeFloatArray>() { return true; }

        template <bool hasSideEffect, typename T, typename Fn>
        static void TemplatedForEachItemInRange(T * arr, uint32 startIndex, uint32 limitIndex, Var missingItem, ScriptContext * scriptContext, Fn fn)
        {
            for (uint32 i = startIndex; i < limitIndex; i++)
            {
                Var element;
                fn(i, TemplatedGetItem(arr, i, &element, scriptContext) ? element : missingItem);

                if (hasSideEffect && MayChangeType<T>() && !T::Is(arr))
                {
                    // The function has changed, go to another ForEachItemInRange
                    JavascriptArray::FromVar(arr)->ForEachItemInRange<true>(i + 1, limitIndex, missingItem, scriptContext, fn);
                    return;
                }
            }
        }

        template <bool hasSideEffect, typename T, typename P, typename Fn>
        static void TemplatedForEachItemInRange(T * arr, P startIndex, P limitIndex, ScriptContext * scriptContext, Fn fn)
        {
            for (P i = startIndex; i < limitIndex; i++)
            {
                Var element;
                if (TemplatedGetItem(arr, i, &element, scriptContext))
                {
                    fn(i, element);

                    if (hasSideEffect && MayChangeType<T>() && !T::Is(arr))
                    {
                        // The function has changed, go to another ForEachItemInRange
                        JavascriptArray::FromVar(arr)->ForEachItemInRange<true>(i + 1, limitIndex, scriptContext, fn);
                        return;
                    }
                }
            }
        }
    public:

        template <bool hasSideEffect, typename Fn>
        void ForEachItemInRange(uint64 startIndex, uint64 limitIndex, ScriptContext * scriptContext, Fn fn)
        {
            Assert(false);
            Throw::InternalError();
        }
        template <bool hasSideEffect, typename Fn>
        void ForEachItemInRange(uint32 startIndex, uint32 limitIndex, ScriptContext * scriptContext, Fn fn)
        {
            switch (this->GetTypeId())
            {
            case TypeIds_Array:
                TemplatedForEachItemInRange<hasSideEffect>(this, startIndex, limitIndex, scriptContext, fn);
                break;
            case TypeIds_NativeIntArray:
                TemplatedForEachItemInRange<hasSideEffect>(JavascriptNativeIntArray::FromVar(this), startIndex, limitIndex, scriptContext, fn);
                break;
            case TypeIds_NativeFloatArray:
                TemplatedForEachItemInRange<hasSideEffect>(JavascriptNativeFloatArray::FromVar(this), startIndex, limitIndex, scriptContext, fn);
                break;
            default:
                Assert(false);
                break;
            }
        }

        template <bool hasSideEffect, typename Fn>
        void ForEachItemInRange(uint32 startIndex, uint32 limitIndex, Var missingItem, ScriptContext * scriptContext, Fn fn)
        {
            switch (this->GetTypeId())
            {
            case TypeIds_Array:
                TemplatedForEachItemInRange<hasSideEffect>(this, startIndex, limitIndex, missingItem, scriptContext, fn);
                break;
            case TypeIds_NativeIntArray:
                TemplatedForEachItemInRange<hasSideEffect>(JavascriptNativeIntArray::FromVar(this), startIndex, limitIndex, missingItem, scriptContext, fn);
                break;
            case TypeIds_NativeFloatArray:
                TemplatedForEachItemInRange<hasSideEffect>(JavascriptNativeFloatArray::FromVar(this), startIndex, limitIndex, missingItem, scriptContext, fn);
                break;
            default:
                Assert(false);
                break;
            }
        }

        // ArrayElementEnumerator walks an array's segments and enumerates the elements in order.
        class ArrayElementEnumerator
        {
        private:
            SparseArraySegmentBase* seg;
            uint32 index, endIndex;
            const uint32 start, end;

        public:
            ArrayElementEnumerator(JavascriptArray* arr, uint32 start = 0, uint32 end = MaxArrayLength);

            template<typename T> bool MoveNext();
            uint32 GetIndex() const;
            template<typename T> T GetItem() const;

        private:
            void Init(JavascriptArray* arr);
        };

        template <typename T>
        class IndexTrace
        {
        public:
            static Var ToNumber(const T& index, ScriptContext* scriptContext);

            // index on JavascriptArray
            static BOOL GetItem(JavascriptArray* arr, const T& index, Var* outVal);
            static BOOL SetItem(JavascriptArray* arr, const T& index, Var newValue);
            static void SetItemIfNotExist(JavascriptArray* arr, const T& index, Var newValue);
            static BOOL DeleteItem(JavascriptArray* arr, const T& index);

            // index on RecyclableObject
            static BOOL SetItem(RecyclableObject* obj, const T& index, Var newValue, PropertyOperationFlags flags = PropertyOperation_None);
            static BOOL DeleteItem(RecyclableObject* obj, const T& index, PropertyOperationFlags flags = PropertyOperation_None);
        };

        // BigIndex represents a general index which may grow larger than uint32.
        class BigIndex
        {
        private:
            uint32 index;
            uint64 bigIndex;

            typedef IndexTrace<uint32> small_index;

        public:
            BigIndex(uint32 initIndex = 0);
            BigIndex(uint64 initIndex);

            bool IsSmallIndex() const;
            bool IsUint32Max() const;
            uint32 GetSmallIndex() const;
            uint64 GetBigIndex() const;
            Var ToNumber(ScriptContext* scriptContext) const;

            const BigIndex& operator++();
            const BigIndex& operator--();
            BigIndex operator+(const BigIndex& delta) const;
            BigIndex operator+(uint32 delta) const;
            bool operator==(const BigIndex& rhs)   const;
            bool operator> (const BigIndex& rhs)   const;
            bool operator< (const BigIndex& rhs)   const;
            bool operator<=(const BigIndex& rhs)   const;
            bool operator>=(const BigIndex& rhs)   const;

            BOOL GetItem(JavascriptArray* arr, Var* outVal) const;
            BOOL SetItem(JavascriptArray* arr, Var newValue) const;
            void SetItemIfNotExist(JavascriptArray* arr, Var newValue) const;
            BOOL DeleteItem(JavascriptArray* arr) const;

            BOOL SetItem(RecyclableObject* obj, Var newValue, PropertyOperationFlags flags = PropertyOperation_None) const;
            BOOL DeleteItem(RecyclableObject* obj, PropertyOperationFlags flags = PropertyOperation_None) const;
        };

        BOOL DirectGetItemAt(const BigIndex& index, Var* outVal) { return index.GetItem(this, outVal); }
        void DirectSetItemAt(const BigIndex& index, Var newValue) { index.SetItem(this, newValue); }
        void DirectSetItemIfNotExist(const BigIndex& index, Var newValue) { index.SetItemIfNotExist(this, newValue); }
        void TruncateToProperties(const BigIndex& index, uint32 start);

        template<typename T>
        static void InternalCopyArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptArray* srcArray, uint32 start, uint32 end);
        template<typename T>
        static void InternalCopyNativeFloatArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end);
        template<typename T>
        static void InternalCopyNativeIntArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end);
        template<typename T>
        static void InternalFillFromPrototype(JavascriptArray *dstArray, const T& dstIndex, JavascriptArray *srcArray, uint32 start, uint32 end, uint32 count);

        static void CopyArrayElements(JavascriptArray* dstArray, uint32 dstIndex, JavascriptArray* srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static void CopyArrayElements(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptArray* srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        template <typename T>
        static void CopyAnyArrayElementsToVar(JavascriptArray* dstArray, T dstIndex, JavascriptArray* srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static bool CopyNativeIntArrayElements(JavascriptNativeIntArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static bool CopyNativeIntArrayElementsToFloat(JavascriptNativeFloatArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static void CopyNativeIntArrayElementsToVar(JavascriptArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static void CopyNativeIntArrayElementsToVar(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptNativeIntArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static bool CopyNativeFloatArrayElements(JavascriptNativeFloatArray* dstArray, uint32 dstIndex, JavascriptNativeFloatArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static void CopyNativeFloatArrayElementsToVar(JavascriptArray* dstArray, uint32 dstIndex, JavascriptNativeFloatArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);
        static void CopyNativeFloatArrayElementsToVar(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptNativeFloatArray *srcArray, uint32 start = 0, uint32 end = MaxArrayLength);

        static bool BoxConcatItem(Var aItem, uint idxArg, ScriptContext *scriptContext);

        template<typename T>
        static void SetConcatItem(Var aItem, uint idxArg, JavascriptArray* pDestArray, RecyclableObject* pDestObj, T idxDest, ScriptContext *scriptContext);

        template<typename T>
        static void ConcatArgs(RecyclableObject* pDestObj, TypeId* remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext, uint start, BigIndex startIdxDest, BOOL firstPromotedItemIsSpreadable, BigIndex firstPromotedItemLength);
        template<typename T>
        static void ConcatArgs(RecyclableObject* pDestObj, TypeId* remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext, uint start = 0, uint startIdxDest = 0u, BOOL FirstPromotedItemIsSpreadable = false, BigIndex FirstPromotedItemLength = 0u);
        static void ConcatIntArgs(JavascriptNativeIntArray* pDestArray, TypeId* remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext);
        static bool PromoteToBigIndex(BigIndex lhs, BigIndex rhs);
        static bool PromoteToBigIndex(BigIndex lhs, uint32 rhs);
        static void ConcatFloatArgs(JavascriptNativeFloatArray* pDestArray, TypeId* remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext);
    private:
        template<typename T=uint32>
        static RecyclableObject* ArraySpeciesCreate(Var pThisArray, T length, ScriptContext* scriptContext, bool* pIsIntArray = nullptr, bool* pIsFloatArray = nullptr);
        template <typename T, typename R> static R ConvertToIndex(T idxDest, ScriptContext* scriptContext) { Throw::InternalError(); return 0; }
        template <> static Var ConvertToIndex<uint32, Var>(uint32 idxDest, ScriptContext* scriptContext);
        template <> static uint32 ConvertToIndex<uint32, uint32>(uint32 idxDest, ScriptContext* scriptContext) { return idxDest; }
        template <> static Var ConvertToIndex<BigIndex, Var>(BigIndex idxDest, ScriptContext* scriptContext);
        template <> static uint32 ConvertToIndex<BigIndex, uint32>(BigIndex idxDest, ScriptContext* scriptContext);
        static BOOL SetArrayLikeObjects(RecyclableObject* pDestObj, uint32 idxDest, Var aItem);
        static BOOL SetArrayLikeObjects(RecyclableObject* pDestObj, BigIndex idxDest, Var aItem);
        static void ConcatArgsCallingHelper(RecyclableObject* pDestObj, TypeId* remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext, ::Math::RecordOverflowPolicy &destLengthOverflow);
    public:
        template<typename T, typename P = uint32>
        static void Unshift(RecyclableObject* obj, const T& toIndex, uint32 start, P end, ScriptContext* scriptContext);

        template <typename T>
        class ItemTrace
        {
        public:
            static uint32 GetLength(T* obj, ScriptContext* scriptContext);
            static BOOL GetItem(T* obj, uint32 index, Var* outVal, ScriptContext* scriptContext);
        };

        template <typename T>
        static JavascriptString* ToLocaleString(T* obj, ScriptContext* scriptContext);
        static JavascriptString* GetLocaleSeparator(ScriptContext* scriptContext);

    public:
        static uint32 GetOffsetOfArrayFlags() { return offsetof(JavascriptArray, arrayFlags); }
        static uint32 GetOffsetOfHead() { return offsetof(JavascriptArray, head); }
        static uint32 GetOffsetOfLastUsedSegmentOrSegmentMap() { return offsetof(JavascriptArray, segmentUnion.lastUsedSegment); }
        static Var SpreadArrayArgs(Var arrayToSpread, const Js::AuxArray<uint32> *spreadIndices, ScriptContext *scriptContext);
        static uint32 GetSpreadArgLen(Var spreadArg, ScriptContext *scriptContext);

        static JavascriptArray * BoxStackInstance(JavascriptArray * instance);
    protected:
        template <typename T> void InitBoxedInlineHeadSegment(SparseArraySegment<T> * dst, SparseArraySegment<T> * src);

        template <typename T> static T * BoxStackInstance(T * instance);

    public:
        template<class T, uint InlinePropertySlots> static size_t DetermineAllocationSize(const uint inlineElementSlots, size_t *const allocationPlusSizeRef = nullptr, uint *const alignedInlineElementSlotsRef = nullptr);
        template<class T, uint InlinePropertySlots> static uint DetermineAvailableInlineElementSlots(const size_t allocationSize, bool *const isSufficientSpaceForInlinePropertySlotsRef);
        template<class T, uint ConstInlinePropertySlots, bool UseDynamicInlinePropertySlots> static SparseArraySegment<typename T::TElement> *DetermineInlineHeadSegmentPointer(T *const array);
    };

    // Ideally we would propagate the throw flag setting of true from the array operations down to the [[Delete]]/[[Put]]/... methods. But that is a big change
    // so we are checking for failure on DeleteProperty/DeleteItem/... etc instead. This helper makes that checking a little less intrusive.
    class ThrowTypeErrorOnFailureHelper
    {
        ScriptContext *m_scriptContext;
        PCWSTR m_functionName;

    public:
        ThrowTypeErrorOnFailureHelper(ScriptContext *scriptContext, PCWSTR functionName) : m_scriptContext(scriptContext), m_functionName(functionName) {}
        inline void ThrowTypeErrorOnFailure(BOOL operationSucceeded);
        inline void ThrowTypeErrorOnFailure();
        inline BOOL IsThrowTypeError(BOOL operationSucceeded);
    };

    class JavascriptNativeArray : public JavascriptArray
    {
        friend class JavascriptArray;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptNativeArray, JavascriptArray);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptNativeArray);

    public:
        JavascriptNativeArray(DynamicType * type) :
            JavascriptArray(type), weakRefToFuncBody(nullptr)
        {
        }

    protected:
        JavascriptNativeArray(uint32 length, DynamicType * type) :
            JavascriptArray(length, type), weakRefToFuncBody(nullptr) {}

        // For BoxStackInstance
        JavascriptNativeArray(JavascriptNativeArray * instance);

        RecyclerWeakReference<FunctionBody> *weakRefToFuncBody;

    public:
        static bool Is(Var aValue);
        static bool Is(TypeId typeId);
        static JavascriptNativeArray* FromVar(Var aValue);

        void SetArrayCallSite(ProfileId index, RecyclerWeakReference<FunctionBody> *weakRef)
        {
            Assert(weakRef);
            Assert(!weakRefToFuncBody);
            SetArrayCallSiteIndex(index);
            weakRefToFuncBody = weakRef;
        }
        void ClearArrayCallSiteIndex()
        {
            weakRefToFuncBody = nullptr;
        }

#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *GetArrayCallSiteInfo();
#endif

        static uint32 GetOffsetOfArrayCallSiteIndex() { return offsetof(JavascriptNativeArray, arrayCallSiteIndex); }
        static uint32 GetOffsetOfWeakFuncRef() { return offsetof(JavascriptNativeArray, weakRefToFuncBody); }

#if ENABLE_PROFILE_INFO
        void SetArrayProfileInfo(RecyclerWeakReference<FunctionBody> *weakRef, ArrayCallSiteInfo *arrayInfo);
        void CopyArrayProfileInfo(Js::JavascriptNativeArray* baseArray);
#endif

        Var FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax);
        template<typename T, bool checkNaNAndNegZero> Var FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax); // NativeInt arrays can't have NaNs or -0

        static void PopWithNoDst(Var nativeArray);
    };

    class JavascriptNativeFloatArray;
    class JavascriptNativeIntArray : public JavascriptNativeArray
    {
        friend class JavascriptArray;

    public:
        static const size_t StackAllocationSize;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptNativeIntArray, JavascriptNativeArray);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptNativeIntArray);
    public:
        JavascriptNativeIntArray(DynamicType * type);
        JavascriptNativeIntArray(uint32 length, uint32 size, DynamicType * type);
        JavascriptNativeIntArray(DynamicType * type, uint32 size);

    protected:
        JavascriptNativeIntArray(uint32 length, DynamicType * type) :
            JavascriptNativeArray(length, type) {}

        // For BoxStackInstance
        JavascriptNativeIntArray(JavascriptNativeIntArray * instance, bool boxHead);
    public:
        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewInstance(RecyclableObject* function, Arguments args);

        static bool Is(Var aValue);
        static bool Is(TypeId typeId);
        static JavascriptNativeIntArray* FromVar(Var aValue);
        static bool IsNonCrossSite(Var aValue);

        typedef int32 TElement;

        static const int32 MissingItem;

        virtual BOOL HasItem(uint32 index) override;
        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL DirectGetVarItemAt(uint index, Var* outval, ScriptContext *scriptContext);
        virtual BOOL DirectGetItemAtFull(uint index, Var* outVal);
        virtual Var DirectGetItem(uint32 index);
        virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) override
        {
            int32 value = 0;
            return this->DirectGetItemAt(index, &value) ? WritableData : None;
        }

        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags) override;
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags) override;
#ifdef VALIDATE_ARRAY
        virtual void ValidateArray() override;
#endif
        BOOL SetItem(uint32 index, int32 iValue);

        static JavascriptNativeFloatArray * ToNativeFloatArray(JavascriptNativeIntArray *intArray);
        static JavascriptArray * ToVarArray(JavascriptNativeIntArray *intArray);
        static JavascriptArray * ConvertToVarArray(JavascriptNativeIntArray *intArray);
        static Var Push(ScriptContext * scriptContext, Var array, int value);
        static int32 Pop(ScriptContext * scriptContext, Var nativeIntArray);

#if ENABLE_PROFILE_INFO
        virtual JavascriptArray *FillFromArgs(uint length, uint start, Var *args, ArrayCallSiteInfo *info = nullptr, bool dontCreateNewArray = false) override;
#else
        virtual JavascriptArray *FillFromArgs(uint length, uint start, Var *args, bool dontCreateNewArray = false) override;
#endif
        virtual void ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength) override;
        virtual void SetIsPrototype() override;

        TypeId TrySetNativeIntArrayItem(Var value, int32 *iValue, double *dValue);

        virtual bool IsMissingHeadSegmentItem(const uint32 index) const override;

        static VTableValue VtableHelper()
        {
            return VTableValue::VtableNativeIntArray;
        }
        static LibraryValue InitialTypeHelper()
        {
            return LibraryValue::ValueNativeIntArrayType;
        }
        static DynamicType * GetInitialType(ScriptContext * scriptContext);
        static JavascriptNativeIntArray * BoxStackInstance(JavascriptNativeIntArray * instance);
    private:
        virtual int32 HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext) override;
    };

#if ENABLE_COPYONACCESS_ARRAY
    class JavascriptCopyOnAccessNativeIntArray : public JavascriptNativeIntArray
    {
        friend class JavascriptArray;

    public:
        static const size_t StackAllocationSize;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptCopyOnAccessNativeIntArray, JavascriptNativeIntArray);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptCopyOnAccessNativeIntArray);

    public:
        JavascriptCopyOnAccessNativeIntArray(uint32 length, DynamicType * type) :
            JavascriptNativeIntArray(length, type) {}

        virtual BOOL IsCopyOnAccessArray() { return TRUE; }

        static bool Is(Var aValue);
        static bool Is(TypeId typeId);
        static JavascriptCopyOnAccessNativeIntArray* FromVar(Var aValue);

        static DynamicType * GetInitialType(ScriptContext * scriptContext);
        void ConvertCopyOnAccessSegment();

        uint32 GetNextIndex(uint32 index) const;
        BOOL DirectGetItemAt(uint32 index, int* outVal);

        static VTableValue VtableHelper()
        {
            return VTableValue::VtableCopyOnAccessNativeIntArray;
        }
    };
#endif

    class JavascriptNativeFloatArray : public JavascriptNativeArray
    {
        friend class JavascriptArray;

    public:
        static const size_t StackAllocationSize;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptNativeFloatArray, JavascriptNativeArray);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptNativeFloatArray);
    public:
        JavascriptNativeFloatArray(DynamicType * type);
        JavascriptNativeFloatArray(uint32 length, uint32 size, DynamicType * type);
        JavascriptNativeFloatArray(DynamicType * type, uint32 size);

    private:
        JavascriptNativeFloatArray(uint32 length, DynamicType * type) :
            JavascriptNativeArray(length, type) {}

        // For BoxStackInstance
        JavascriptNativeFloatArray(JavascriptNativeFloatArray * instance, bool boxHead);

    public:
        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewInstance(RecyclableObject* function, Arguments args);

        static bool Is(Var aValue);
        static bool Is(TypeId typeId);
        static JavascriptNativeFloatArray* FromVar(Var aValue);
        static bool IsNonCrossSite(Var aValue);

        typedef double TElement;

        static const double MissingItem;

        virtual BOOL HasItem(uint32 index) override;
        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL DirectGetVarItemAt(uint index, Var* outval, ScriptContext *scriptContext);
        virtual BOOL DirectGetItemAtFull(uint index, Var* outVal);
        virtual Var DirectGetItem(uint32 index);
        virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) override
        {
            double value = 0;
            return this->DirectGetItemAt(index, &value) ? WritableData : None;
        }

        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags) override;
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags) override;
#ifdef VALIDATE_ARRAY
        virtual void ValidateArray() override;
#endif
        BOOL SetItem(uint32 index, double dValue);

        static JavascriptArray * ToVarArray(JavascriptNativeFloatArray *fArray);
        static JavascriptArray * ConvertToVarArray(JavascriptNativeFloatArray *fArray);

#if ENABLE_PROFILE_INFO
        virtual JavascriptArray *FillFromArgs(uint length, uint start, Var *args, ArrayCallSiteInfo *info = nullptr, bool dontCreateNewArray = false) override;
#else
        virtual JavascriptArray *FillFromArgs(uint length, uint start, Var *args, bool dontCreateNewArray = false) override;
#endif
        virtual void ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength) override;
        virtual void SetIsPrototype() override;

        TypeId TrySetNativeFloatArrayItem(Var value, double *dValue);

        virtual bool IsMissingHeadSegmentItem(const uint32 index) const override;

        static VTableValue VtableHelper()
        {
            return VTableValue::VtableNativeFloatArray;
        }
        static LibraryValue InitialTypeHelper()
        {
            return LibraryValue::ValueNativeFloatArrayType;
        }
        static DynamicType * GetInitialType(ScriptContext * scriptContext);

        static Var Push(ScriptContext * scriptContext, Var * nativeFloatArray, double value);
        static JavascriptNativeFloatArray * BoxStackInstance(JavascriptNativeFloatArray * instance);
        static double Pop(ScriptContext * scriptContext, Var nativeFloatArray);
    private:
        virtual int32 HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext) override;
    };
} // namespace Js
