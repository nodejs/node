#ifndef V8_GEN_TORQUE_GENERATED_BUILTIN_DEFINITIONS_H_
#define V8_GEN_TORQUE_GENERATED_BUILTIN_DEFINITIONS_H_


#define BUILTIN_LIST_FROM_TORQUE(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, ASM) \
TFJ(AggregateErrorConstructor, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeAt, JSParameterCount(1), kReceiver, kIndex) \
TFJ(ArrayPrototypeConcat, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeCopyWithin, kDontAdaptArgumentsSentinel) \
TFJ(ArrayEveryLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kThisArg, kInitialK, kLength) \
TFJ(ArrayEveryLoopLazyDeoptContinuation, JSParameterCount(5), kReceiver, kCallback, kThisArg, kInitialK, kLength, kResult) \
TFC(ArrayEveryLoopContinuation, ArrayEveryLoopContinuation) \
TFJ(ArrayEvery, kDontAdaptArgumentsSentinel) \
TFJ(ArrayFilterLoopEagerDeoptContinuation, JSParameterCount(6), kReceiver, kCallback, kThisArg, kArray, kInitialK, kLength, kInitialTo) \
TFJ(ArrayFilterLoopLazyDeoptContinuation, JSParameterCount(8), kReceiver, kCallback, kThisArg, kArray, kInitialK, kLength, kValueK, kInitialTo, kResult) \
TFC(ArrayFilterLoopContinuation, ArrayFilterLoopContinuation) \
TFJ(ArrayFilter, kDontAdaptArgumentsSentinel) \
TFJ(ArrayFindLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kThisArg, kInitialK, kLength) \
TFJ(ArrayFindLoopLazyDeoptContinuation, JSParameterCount(5), kReceiver, kCallback, kThisArg, kInitialK, kLength, kResult) \
TFJ(ArrayFindLoopAfterCallbackLazyDeoptContinuation, JSParameterCount(6), kReceiver, kCallback, kThisArg, kInitialK, kLength, kFoundValue, kIsFound) \
TFC(ArrayFindLoopContinuation, ArrayFindLoopContinuation) \
TFJ(ArrayPrototypeFind, kDontAdaptArgumentsSentinel) \
TFJ(ArrayFindIndexLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kThisArg, kInitialK, kLength) \
TFJ(ArrayFindIndexLoopLazyDeoptContinuation, JSParameterCount(5), kReceiver, kCallback, kThisArg, kInitialK, kLength, kResult) \
TFJ(ArrayFindIndexLoopAfterCallbackLazyDeoptContinuation, JSParameterCount(6), kReceiver, kCallback, kThisArg, kInitialK, kLength, kFoundValue, kIsFound) \
TFC(ArrayFindIndexLoopContinuation, ArrayFindIndexLoopContinuation) \
TFJ(ArrayPrototypeFindIndex, kDontAdaptArgumentsSentinel) \
TFC(ArrayFindLastLoopContinuation, ArrayFindLastLoopContinuation) \
TFJ(ArrayPrototypeFindLast, kDontAdaptArgumentsSentinel) \
TFC(ArrayFindLastIndexLoopContinuation, ArrayFindLastIndexLoopContinuation) \
TFJ(ArrayPrototypeFindLastIndex, kDontAdaptArgumentsSentinel) \
TFC(FlattenIntoArrayWithoutMapFn, FlattenIntoArrayWithoutMapFn) \
TFC(FlattenIntoArrayWithMapFn, FlattenIntoArrayWithMapFn) \
TFJ(ArrayPrototypeFlat, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeFlatMap, kDontAdaptArgumentsSentinel) \
TFJ(ArrayForEachLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kThisArg, kInitialK, kLength) \
TFJ(ArrayForEachLoopLazyDeoptContinuation, JSParameterCount(5), kReceiver, kCallback, kThisArg, kInitialK, kLength, kResult) \
TFC(ArrayForEachLoopContinuation, ArrayForEachLoopContinuation) \
TFJ(ArrayForEach, kDontAdaptArgumentsSentinel) \
TFJ(ArrayFromAsyncIterableOnFulfilled, JSParameterCount(1), kReceiver, kResult) \
TFJ(ArrayFromAsyncIterableOnRejected, JSParameterCount(1), kReceiver, kResult) \
TFJ(ArrayFromAsyncArrayLikeOnFulfilled, JSParameterCount(1), kReceiver, kResult) \
TFJ(ArrayFromAsyncArrayLikeOnRejected, JSParameterCount(1), kReceiver, kResult) \
TFJ(ArrayFromAsync, kDontAdaptArgumentsSentinel) \
TFJ(ArrayFrom, kDontAdaptArgumentsSentinel) \
TFJ(ArrayIsArray, JSParameterCount(1), kReceiver, kArg) \
TFC(LoadJoinElement_DictionaryElements_0, LoadJoinElement_DictionaryElements_0) \
TFC(LoadJoinElement_FastSmiOrObjectElements_0, LoadJoinElement_FastSmiOrObjectElements_0) \
TFC(LoadJoinElement_FastDoubleElements_0, LoadJoinElement_FastDoubleElements_0) \
TFC(ConvertToLocaleString, ConvertToLocaleString) \
TFC(JoinStackPush, JoinStackPush) \
TFC(JoinStackPop, JoinStackPop) \
TFJ(ArrayPrototypeJoin, kDontAdaptArgumentsSentinel) \
TFC(ArrayPrototypeJoinImpl, ArrayPrototypeJoinImpl) \
TFJ(ArrayPrototypeToLocaleString, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeToString, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeJoin, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeToLocaleString, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeLastIndexOf, kDontAdaptArgumentsSentinel) \
TFJ(ArrayMapPreLoopLazyDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kThisArg, kLength, kResult) \
TFJ(ArrayMapLoopEagerDeoptContinuation, JSParameterCount(5), kReceiver, kCallback, kThisArg, kArray, kInitialK, kLength) \
TFJ(ArrayMapLoopLazyDeoptContinuation, JSParameterCount(6), kReceiver, kCallback, kThisArg, kArray, kInitialK, kLength, kResult) \
TFC(ArrayMapLoopContinuation, ArrayMapLoopContinuation) \
TFJ(ArrayMap, kDontAdaptArgumentsSentinel) \
TFJ(ArrayOf, kDontAdaptArgumentsSentinel) \
TFJ(ArrayReduceRightPreLoopEagerDeoptContinuation, JSParameterCount(2), kReceiver, kCallback, kLength) \
TFJ(ArrayReduceRightLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kInitialK, kLength, kAccumulator) \
TFJ(ArrayReduceRightLoopLazyDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kInitialK, kLength, kResult) \
TFC(ArrayReduceRightLoopContinuation, ArrayReduceRightLoopContinuation) \
TFJ(ArrayReduceRight, kDontAdaptArgumentsSentinel) \
TFJ(ArrayReducePreLoopEagerDeoptContinuation, JSParameterCount(2), kReceiver, kCallback, kLength) \
TFJ(ArrayReduceLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kInitialK, kLength, kAccumulator) \
TFJ(ArrayReduceLoopLazyDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kInitialK, kLength, kResult) \
TFC(ArrayReduceLoopContinuation, ArrayReduceLoopContinuation) \
TFJ(ArrayReduce, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeReverse, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeShift, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeSlice, kDontAdaptArgumentsSentinel) \
TFJ(ArraySomeLoopEagerDeoptContinuation, JSParameterCount(4), kReceiver, kCallback, kThisArg, kInitialK, kLength) \
TFJ(ArraySomeLoopLazyDeoptContinuation, JSParameterCount(5), kReceiver, kCallback, kThisArg, kInitialK, kLength, kResult) \
TFC(ArraySomeLoopContinuation, ArraySomeLoopContinuation) \
TFJ(ArraySome, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeSplice, kDontAdaptArgumentsSentinel) \
TFC(GenericArrayToReversed, GenericArrayToReversed) \
TFJ(ArrayPrototypeToReversed, kDontAdaptArgumentsSentinel) \
TFC(ArrayPowerSortIntoCopy, ArrayPowerSortIntoCopy) \
TFJ(ArrayPrototypeToSorted, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeToSpliced, kDontAdaptArgumentsSentinel) \
TFJ(ArrayPrototypeUnshift, kDontAdaptArgumentsSentinel) \
TFC(GenericArrayWith, GenericArrayWith) \
TFJ(ArrayPrototypeWith, JSParameterCount(2), kReceiver, kIndex, kValue) \
TFJ(ArraySortContinueFromSnapshotEagerDeoptContinuation, JSParameterCount(3), kReceiver, kComparefn, kTempArray, kOriginalLength) \
TFJ(ArraySortContinueFromSnapshotLazyDeoptContinuation, JSParameterCount(4), kReceiver, kComparefn, kTempArray, kOriginalLength, kResult) \
TFJ(ArrayBufferPrototypeGetByteLength, JSParameterCount(0), kReceiver) \
TFJ(ArrayBufferPrototypeGetMaxByteLength, JSParameterCount(0), kReceiver) \
TFJ(ArrayBufferPrototypeGetResizable, JSParameterCount(0), kReceiver) \
TFJ(ArrayBufferPrototypeGetDetached, JSParameterCount(0), kReceiver) \
TFJ(ArrayBufferPrototypeGetImmutable, JSParameterCount(0), kReceiver) \
TFJ(SharedArrayBufferPrototypeGetMaxByteLength, JSParameterCount(0), kReceiver) \
TFJ(SharedArrayBufferPrototypeGetGrowable, JSParameterCount(0), kReceiver) \
TFJ(ArrayBufferIsView, JSParameterCount(1), kReceiver, kArg) \
TFC(ToInteger, ToInteger) \
TFC(FastCreateDataProperty, FastCreateDataProperty) \
TFJ(BooleanConstructor, kDontAdaptArgumentsSentinel) \
TFJ(BooleanPrototypeToString, JSParameterCount(0), kReceiver) \
TFJ(BooleanPrototypeValueOf, JSParameterCount(0), kReceiver) \
TFC(BigIntAddNoThrow, BigIntAddNoThrow) \
TFC(BigIntAdd, BigIntAdd) \
TFC(BigIntSubtractNoThrow, BigIntSubtractNoThrow) \
TFC(BigIntSubtract, BigIntSubtract) \
TFC(BigIntMultiplyNoThrow, BigIntMultiplyNoThrow) \
TFC(BigIntMultiply, BigIntMultiply) \
TFC(BigIntDivideNoThrow, BigIntDivideNoThrow) \
TFC(BigIntDivide, BigIntDivide) \
TFC(BigIntModulusNoThrow, BigIntModulusNoThrow) \
TFC(BigIntModulus, BigIntModulus) \
TFC(BigIntBitwiseAndNoThrow, BigIntBitwiseAndNoThrow) \
TFC(BigIntBitwiseAnd, BigIntBitwiseAnd) \
TFC(BigIntBitwiseOrNoThrow, BigIntBitwiseOrNoThrow) \
TFC(BigIntBitwiseOr, BigIntBitwiseOr) \
TFC(BigIntBitwiseXorNoThrow, BigIntBitwiseXorNoThrow) \
TFC(BigIntBitwiseXor, BigIntBitwiseXor) \
TFC(BigIntShiftLeftNoThrow, BigIntShiftLeftNoThrow) \
TFC(BigIntShiftLeft, BigIntShiftLeft) \
TFC(BigIntShiftRightNoThrow, BigIntShiftRightNoThrow) \
TFC(BigIntShiftRight, BigIntShiftRight) \
TFC(BigIntEqual, BigIntEqual) \
TFC(BigIntLessThan, BigIntLessThan) \
TFC(BigIntGreaterThan, BigIntGreaterThan) \
TFC(BigIntLessThanOrEqual, BigIntLessThanOrEqual) \
TFC(BigIntGreaterThanOrEqual, BigIntGreaterThanOrEqual) \
TFC(BigIntUnaryMinus, BigIntUnaryMinus) \
TFC(ToString, ToString) \
TFC(ToStringConvertSymbol, ToStringConvertSymbol) \
TFJ(StringPrototypeToString, JSParameterCount(0), kReceiver) \
TFJ(StringPrototypeValueOf, JSParameterCount(0), kReceiver) \
TFC(StringToList, StringToList) \
TFJ(StringPrototypeCharAt, JSParameterCount(1), kReceiver, kPosition) \
TFJ(StringPrototypeCharCodeAt, JSParameterCount(1), kReceiver, kPosition) \
TFJ(StringPrototypeCodePointAt, JSParameterCount(1), kReceiver, kPosition) \
TFJ(StringPrototypeConcat, kDontAdaptArgumentsSentinel) \
TFJ(StringConstructor, kDontAdaptArgumentsSentinel) \
TFJ(StringCreateLazyDeoptContinuation, JSParameterCount(1), kReceiver, kValue) \
TFC(StringAddConvertLeft, StringAddConvertLeft) \
TFC(StringAddConvertRight, StringAddConvertRight) \
TFC(StringCharAt, StringCharAt) \
TFC(FastNewClosureBaseline, FastNewClosureBaseline) \
TFC(FastNewFunctionContextEval, FastNewFunctionContextEval) \
TFC(FastNewFunctionContextFunction, FastNewFunctionContextFunction) \
TFC(FastNewFunctionContextFunctionWithCells, FastNewFunctionContextFunctionWithCells) \
TFC(CreateRegExpLiteral, CreateRegExpLiteral) \
TFC(CreateShallowArrayLiteral, CreateShallowArrayLiteral) \
TFC(CreateEmptyArrayLiteral, CreateEmptyArrayLiteral) \
TFC(CreateShallowObjectLiteral, CreateShallowObjectLiteral) \
TFJ(ObjectConstructor, kDontAdaptArgumentsSentinel) \
TFC(CreateEmptyLiteralObject, CreateEmptyLiteralObject) \
TFJ(NumberConstructor, kDontAdaptArgumentsSentinel) \
TFJ(GenericLazyDeoptContinuation, JSParameterCount(1), kReceiver, kResult) \
TFC(StringToNumber, StringToNumber) \
TFC(NonNumberToNumber, NonNumberToNumber) \
TFC(NonNumberToNumeric, NonNumberToNumeric) \
TFC(ToNumeric, ToNumeric) \
TFC(NumberToString, NumberToString) \
TFC(SmiToString, SmiToString) \
TFC(Int32ToString, Int32ToString) \
TFC(Float64ToString, Float64ToString) \
TFC(ToBoolean, ToBoolean) \
TFC(ToBooleanForBaselineJump, ToBooleanForBaselineJump) \
TFC(ToLength, ToLength) \
TFC(ToName, ToName) \
TFC(ToObject, ToObject) \
TFC(NonPrimitiveToPrimitive_Default, NonPrimitiveToPrimitive_Default) \
TFC(NonPrimitiveToPrimitive_Number, NonPrimitiveToPrimitive_Number) \
TFC(NonPrimitiveToPrimitive_String, NonPrimitiveToPrimitive_String) \
TFC(OrdinaryToPrimitive_Number, OrdinaryToPrimitive_Number) \
TFC(OrdinaryToPrimitive_Number_Inline, OrdinaryToPrimitive_Number_Inline) \
TFC(OrdinaryToPrimitive_String, OrdinaryToPrimitive_String) \
TFJ(FastConsoleAssert, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetBuffer, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetByteLength, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetByteOffset, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetUint8, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetInt8, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetUint16, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetInt16, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetUint32, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetInt32, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetFloat16, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetFloat32, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetFloat64, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetBigUint64, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeGetBigInt64, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetUint8, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetInt8, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetUint16, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetInt16, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetUint32, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetInt32, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetFloat16, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetFloat32, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetFloat64, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetBigUint64, kDontAdaptArgumentsSentinel) \
TFJ(DataViewPrototypeSetBigInt64, kDontAdaptArgumentsSentinel) \
TFJ(FinalizationRegistryConstructor, kDontAdaptArgumentsSentinel) \
TFJ(FinalizationRegistryRegister, kDontAdaptArgumentsSentinel) \
TFJ(FunctionPrototypeHasInstance, JSParameterCount(1), kReceiver, kValue) \
TFJ(FastFunctionPrototypeBind, kDontAdaptArgumentsSentinel) \
TFJ(GeneratorPrototypeNextLazyDeoptContinuation, JSParameterCount(3), kReceiver, kGenerator, kException, kResult) \
TFC(IncBlockCounter, IncBlockCounter) \
TFC(GetTemplateObject, GetTemplateObject) \
TFC(ForInNext, ForInNext) \
TFC(GetImportMetaObjectBaseline, GetImportMetaObjectBaseline) \
TFC(GetIteratorWithFeedback, GetIteratorWithFeedback) \
TFC(GetIteratorBaseline, GetIteratorBaseline) \
TFC(CreateAsyncFromSyncIteratorBaseline, CreateAsyncFromSyncIteratorBaseline) \
TFC(ForOfNext, ForOfNext) \
TFC(ForOfNextBaseline, ForOfNextBaseline) \
TFC(CallIteratorWithFeedback, CallIteratorWithFeedback) \
TFC(IteratorClose, IteratorClose) \
TFJ(IteratorConstructor, JSParameterCount(0), kReceiver) \
TFJ(IteratorPrototypeGetToStringTag, JSParameterCount(0), kReceiver) \
TFJ(IteratorPrototypeSetToStringTag, JSParameterCount(1), kReceiver, kValue) \
TFJ(IteratorPrototypeGetConstructor, JSParameterCount(0), kReceiver) \
TFJ(IteratorPrototypeSetConstructor, JSParameterCount(1), kReceiver, kValue) \
TFJ(IteratorPrototypeDispose, JSParameterCount(0), kReceiver) \
TFJ(AsyncIteratorPrototypeAsyncDisposeResolveClosure, JSParameterCount(0), kReceiver) \
TFJ(AsyncIteratorPrototypeAsyncDispose, JSParameterCount(0), kReceiver) \
TFC(ArrayDestructure, ArrayDestructure) \
TFC(ArrayDestructureLazyDeoptContinuation, ArrayDestructureLazyDeoptContinuation) \
TFJ(IteratorFrom, JSParameterCount(1), kReceiver, kObjArg) \
TFJ(WrapForValidIteratorPrototypeNext, JSParameterCount(0), kReceiver) \
TFJ(WrapForValidIteratorPrototypeReturn, JSParameterCount(0), kReceiver) \
TFJ(IteratorHelperPrototypeNext, JSParameterCount(0), kReceiver) \
TFJ(IteratorHelperPrototypeReturn, JSParameterCount(0), kReceiver) \
TFJ(IteratorPrototypeMap, JSParameterCount(1), kReceiver, kMapper) \
TFC(IteratorMapHelperNext, IteratorMapHelperNext) \
TFJ(IteratorPrototypeFilter, JSParameterCount(1), kReceiver, kPredicate) \
TFC(IteratorFilterHelperNext, IteratorFilterHelperNext) \
TFJ(IteratorPrototypeTake, JSParameterCount(1), kReceiver, kLimit) \
TFC(IteratorTakeHelperNext, IteratorTakeHelperNext) \
TFJ(IteratorPrototypeDrop, JSParameterCount(1), kReceiver, kLimit) \
TFC(IteratorDropHelperNext, IteratorDropHelperNext) \
TFJ(IteratorPrototypeFlatMap, JSParameterCount(1), kReceiver, kMapper) \
TFC(IteratorFlatMapHelperNext, IteratorFlatMapHelperNext) \
TFJ(IteratorPrototypeReduce, kDontAdaptArgumentsSentinel) \
TFJ(IteratorPrototypeToArray, JSParameterCount(0), kReceiver) \
TFJ(IteratorPrototypeForEach, JSParameterCount(1), kReceiver, kFn) \
TFJ(IteratorPrototypeSome, JSParameterCount(1), kReceiver, kPredicate) \
TFJ(IteratorPrototypeEvery, JSParameterCount(1), kReceiver, kPredicate) \
TFJ(IteratorPrototypeFind, JSParameterCount(1), kReceiver, kPredicate) \
TFJ(IteratorConcat, kDontAdaptArgumentsSentinel) \
TFC(IteratorConcatHelperNext, IteratorConcatHelperNext) \
TFJ(IteratorPrototypeJoin, JSParameterCount(1), kReceiver, kSeparator) \
TFJ(IteratorZip, kDontAdaptArgumentsSentinel) \
TFC(IteratorZipHelperNext, IteratorZipHelperNext) \
TFJ(IteratorZipKeyed, kDontAdaptArgumentsSentinel) \
TFC(IteratorZipKeyedHelperNext, IteratorZipKeyedHelperNext) \
TFJ(IteratorPrototypeIncludes, kDontAdaptArgumentsSentinel) \
TFJ(MapGroupBy, JSParameterCount(2), kReceiver, kItems, kCallback) \
TFJ(MathAbs, JSParameterCount(1), kReceiver, kX) \
TFJ(MathCeil, JSParameterCount(1), kReceiver, kX) \
TFJ(MathFloor, JSParameterCount(1), kReceiver, kX) \
TFJ(MathRound, JSParameterCount(1), kReceiver, kX) \
TFJ(MathTrunc, JSParameterCount(1), kReceiver, kX) \
TFJ(MathPow, JSParameterCount(2), kReceiver, kBase, kExponent) \
TFJ(MathMax, kDontAdaptArgumentsSentinel) \
TFJ(MathMin, kDontAdaptArgumentsSentinel) \
TFJ(MathAcos, JSParameterCount(1), kReceiver, kX) \
TFJ(MathAcosh, JSParameterCount(1), kReceiver, kX) \
TFJ(MathAsin, JSParameterCount(1), kReceiver, kX) \
TFJ(MathAsinh, JSParameterCount(1), kReceiver, kX) \
TFJ(MathAtan, JSParameterCount(1), kReceiver, kX) \
TFJ(MathAtan2, JSParameterCount(2), kReceiver, kY, kX) \
TFJ(MathAtanh, JSParameterCount(1), kReceiver, kX) \
TFJ(MathCbrt, JSParameterCount(1), kReceiver, kX) \
TFJ(MathClz32, JSParameterCount(1), kReceiver, kX) \
TFJ(MathCos, JSParameterCount(1), kReceiver, kX) \
TFJ(MathCosh, JSParameterCount(1), kReceiver, kX) \
TFJ(MathExp, JSParameterCount(1), kReceiver, kX) \
TFJ(MathExpm1, JSParameterCount(1), kReceiver, kX) \
TFJ(MathFround, JSParameterCount(1), kReceiver, kX) \
TFJ(MathF16round, JSParameterCount(1), kReceiver, kX) \
TFJ(MathImul, JSParameterCount(2), kReceiver, kX, kY) \
TFJ(MathLog, JSParameterCount(1), kReceiver, kX) \
TFJ(MathLog1p, JSParameterCount(1), kReceiver, kX) \
TFJ(MathLog10, JSParameterCount(1), kReceiver, kX) \
TFJ(MathLog2, JSParameterCount(1), kReceiver, kX) \
TFJ(MathSin, JSParameterCount(1), kReceiver, kX) \
TFJ(MathSign, JSParameterCount(1), kReceiver, kX) \
TFJ(MathSinh, JSParameterCount(1), kReceiver, kX) \
TFJ(MathSqrt, JSParameterCount(1), kReceiver, kX) \
TFJ(MathTan, JSParameterCount(1), kReceiver, kX) \
TFJ(MathTanh, JSParameterCount(1), kReceiver, kX) \
TFJ(MathHypot, kDontAdaptArgumentsSentinel) \
TFJ(MathRandom, JSParameterCount(0), kReceiver) \
TFJ(NumberPrototypeToString, kDontAdaptArgumentsSentinel) \
TFJ(NumberIsFinite, JSParameterCount(1), kReceiver, kValue) \
TFJ(NumberIsInteger, JSParameterCount(1), kReceiver, kValue) \
TFJ(NumberIsNaN, JSParameterCount(1), kReceiver, kValue) \
TFJ(NumberIsSafeInteger, JSParameterCount(1), kReceiver, kValue) \
TFJ(NumberPrototypeValueOf, JSParameterCount(0), kReceiver) \
TFJ(NumberParseFloat, JSParameterCount(1), kReceiver, kValue) \
TFC(ParseInt, ParseInt) \
TFJ(NumberParseInt, JSParameterCount(2), kReceiver, kValue, kRadix) \
TFC(Add, Add) \
TFC(Subtract, Subtract) \
TFC(Multiply, Multiply) \
TFC(Divide, Divide) \
TFC(Modulus, Modulus) \
TFC(Exponentiate, Exponentiate) \
TFC(Negate, Negate) \
TFC(BitwiseNot, BitwiseNot) \
TFC(Decrement, Decrement) \
TFC(Increment, Increment) \
TFC(ShiftLeft, ShiftLeft) \
TFC(ShiftRight, ShiftRight) \
TFC(ShiftRightLogical, ShiftRightLogical) \
TFC(BitwiseAnd, BitwiseAnd) \
TFC(BitwiseOr, BitwiseOr) \
TFC(BitwiseXor, BitwiseXor) \
TFC(LessThan, LessThan) \
TFC(LessThanOrEqual, LessThanOrEqual) \
TFC(GreaterThan, GreaterThan) \
TFC(GreaterThanOrEqual, GreaterThanOrEqual) \
TFC(Equal, Equal) \
TFC(StrictEqual, StrictEqual) \
TFJ(ObjectFromEntries, kDontAdaptArgumentsSentinel) \
TFC(GroupByGeneric, GroupByGeneric) \
TFJ(ObjectGroupBy, JSParameterCount(2), kReceiver, kItems, kCallback) \
TFC(CreateObjectWithoutProperties, CreateObjectWithoutProperties) \
TFJ(ObjectIsExtensible, JSParameterCount(1), kReceiver, kObject) \
TFJ(ObjectPreventExtensions, JSParameterCount(1), kReceiver, kObject) \
TFJ(ObjectGetPrototypeOf, JSParameterCount(1), kReceiver, kObject) \
TFJ(ObjectSetPrototypeOf, JSParameterCount(2), kReceiver, kObject, kProto) \
TFJ(ObjectPrototypeToString, JSParameterCount(0), kReceiver) \
TFJ(ObjectPrototypeValueOf, JSParameterCount(0), kReceiver) \
TFJ(ObjectPrototypeToLocaleString, JSParameterCount(0), kReceiver) \
TFC(FulfillPromise, FulfillPromise) \
TFC(RejectPromise, RejectPromise) \
TFC(NewPromiseCapability, NewPromiseCapability) \
TFJ(PromiseCapabilityDefaultReject, JSParameterCount(1), kReceiver, kReason) \
TFJ(PromiseCapabilityDefaultResolve, JSParameterCount(1), kReceiver, kResolution) \
TFJ(PerformPromiseThenFunction, JSParameterCount(3), kReceiver, kOnFulfilled, kOnRejected, kResultCapability) \
TFC(PerformPromiseThen, PerformPromiseThen) \
TFJ(PromiseReject, JSParameterCount(1), kReceiver, kReason) \
TFJ(PromiseGetCapabilitiesExecutor, JSParameterCount(2), kReceiver, kResolve, kReject) \
TFJ(PromiseConstructorLazyDeoptContinuation, JSParameterCount(4), kReceiver, kPromise, kReject, kException, kResult) \
TFJ(PromiseAll, JSParameterCount(1), kReceiver, kIterable) \
TFJ(PromiseAllSettled, JSParameterCount(1), kReceiver, kIterable) \
TFJ(PromiseAllResolveElementClosure, JSParameterCount(1), kReceiver, kValue) \
TFJ(PromiseAllSettledResolveElementClosure, JSParameterCount(1), kReceiver, kValue) \
TFJ(PromiseAllSettledRejectElementClosure, JSParameterCount(1), kReceiver, kValue) \
TFJ(PromiseAnyRejectElementClosure, JSParameterCount(1), kReceiver, kValue) \
TFJ(PromiseAny, JSParameterCount(1), kReceiver, kIterable) \
TFJ(PromiseConstructor, JSParameterCount(1), kReceiver, kExecutor) \
TFJ(PromisePrototypeCatch, JSParameterCount(1), kReceiver, kOnRejected) \
TFJ(PromiseValueThunkFinally, JSParameterCount(0), kReceiver) \
TFJ(PromiseThrowerFinally, JSParameterCount(0), kReceiver) \
TFJ(PromiseCatchFinally, JSParameterCount(1), kReceiver, kReason) \
TFJ(PromiseThenFinally, JSParameterCount(1), kReceiver, kValue) \
TFJ(PromisePrototypeFinally, JSParameterCount(1), kReceiver, kOnFinally) \
TFC(PromiseResolveThenableJob, PromiseResolveThenableJob) \
TFJ(GetContinuationPreservedEmbedderData, JSParameterCount(0), kReceiver) \
TFJ(SetContinuationPreservedEmbedderData, JSParameterCount(1), kReceiver, kData) \
TFC(AsyncAwaitNonThenableFastPath, AsyncAwaitNonThenableFastPath) \
TFJ(PromiseRace, JSParameterCount(1), kReceiver, kIterable) \
TFC(PromiseFulfillReactionJob, PromiseFulfillReactionJob) \
TFC(PromiseRejectReactionJob, PromiseRejectReactionJob) \
TFJ(PromiseResolveTrampoline, JSParameterCount(1), kReceiver, kValue) \
TFC(PromiseResolve, PromiseResolve) \
TFC(ResolvePromise, ResolvePromise) \
TFJ(PromisePrototypeThen, JSParameterCount(2), kReceiver, kOnFulfilled, kOnRejected) \
TFJ(PromiseTry, kDontAdaptArgumentsSentinel) \
TFJ(PromiseWithResolvers, JSParameterCount(0), kReceiver) \
TFJ(ProxyConstructor, JSParameterCount(2), kReceiver, kTarget, kHandler) \
TFC(ProxyDeleteProperty, ProxyDeleteProperty) \
TFC(ProxyGetProperty, ProxyGetProperty) \
TFC(ProxyGetPrototypeOf, ProxyGetPrototypeOf) \
TFC(ProxyHasProperty, ProxyHasProperty) \
TFC(ProxyIsExtensible, ProxyIsExtensible) \
TFC(ProxyPreventExtensions, ProxyPreventExtensions) \
TFJ(ProxyRevocable, JSParameterCount(2), kReceiver, kTarget, kHandler) \
TFJ(ProxyRevoke, JSParameterCount(0), kReceiver) \
TFC(ProxySetProperty, ProxySetProperty) \
TFC(ProxySetPrototypeOf, ProxySetPrototypeOf) \
TFJ(ReflectIsExtensible, JSParameterCount(1), kReceiver, kObject) \
TFJ(ReflectPreventExtensions, JSParameterCount(1), kReceiver, kObject) \
TFJ(ReflectGetPrototypeOf, JSParameterCount(1), kReceiver, kObject) \
TFJ(ReflectSetPrototypeOf, JSParameterCount(2), kReceiver, kObject, kProto) \
TFJ(ReflectGet, kDontAdaptArgumentsSentinel) \
TFJ(ReflectDeleteProperty, JSParameterCount(2), kReceiver, kObject, kKey) \
TFJ(ReflectHas, JSParameterCount(2), kReceiver, kObject, kKey) \
TFJ(ReflectGetOwnPropertyDescriptor, JSParameterCount(2), kReceiver, kTarget, kPropertyKey) \
TFC(RegExpPrototypeExecSlow, RegExpPrototypeExecSlow) \
TFJ(RegExpPrototypeExec, JSParameterCount(1), kReceiver, kString) \
TFJ(RegExpPrototypeMatchAll, JSParameterCount(1), kReceiver, kString) \
TFJ(RegExpStringIteratorPrototypeNext, JSParameterCount(0), kReceiver) \
TFC(RegExpMatchFast, RegExpMatchFast) \
TFJ(RegExpPrototypeMatch, JSParameterCount(1), kReceiver, kString) \
TFC(RegExpReplace, RegExpReplace) \
TFJ(RegExpPrototypeReplace, kDontAdaptArgumentsSentinel) \
TFC(RegExpSearchFast, RegExpSearchFast) \
TFJ(RegExpPrototypeSearch, JSParameterCount(1), kReceiver, kString) \
TFJ(RegExpPrototypeSourceGetter, JSParameterCount(0), kReceiver) \
TFC(RegExpSplit, RegExpSplit) \
TFJ(RegExpPrototypeSplit, kDontAdaptArgumentsSentinel) \
TFJ(RegExpPrototypeTest, JSParameterCount(1), kReceiver, kString) \
TFC(RegExpPrototypeTestFast, RegExpPrototypeTestFast) \
TFJ(RegExpPrototypeGlobalGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeIgnoreCaseGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeMultilineGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeHasIndicesGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeLinearGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeDotAllGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeStickyGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeUnicodeGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeUnicodeSetsGetter, JSParameterCount(0), kReceiver) \
TFJ(RegExpPrototypeFlagsGetter, JSParameterCount(0), kReceiver) \
TFJ(SetPrototypeDifference, JSParameterCount(1), kReceiver, kOther) \
TFJ(SetPrototypeIntersection, JSParameterCount(1), kReceiver, kOther) \
TFJ(SetPrototypeIsDisjointFrom, JSParameterCount(1), kReceiver, kOther) \
TFJ(SetPrototypeIsSubsetOf, JSParameterCount(1), kReceiver, kOther) \
TFJ(SetPrototypeIsSupersetOf, JSParameterCount(1), kReceiver, kOther) \
TFJ(SetPrototypeSymmetricDifference, JSParameterCount(1), kReceiver, kOther) \
TFJ(SetPrototypeUnion, JSParameterCount(1), kReceiver, kOther) \
TFJ(StringPrototypeAt, JSParameterCount(1), kReceiver, kIndex) \
TFJ(StringPrototypeEndsWith, kDontAdaptArgumentsSentinel) \
TFC(CreateHTML, CreateHTML) \
TFJ(StringPrototypeAnchor, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeBig, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeBlink, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeBold, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeFontcolor, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeFontsize, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeFixed, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeItalics, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeLink, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeSmall, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeStrike, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeSub, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeSup, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeIncludes, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeIndexOf, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeIsWellFormed, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeIterator, JSParameterCount(0), kReceiver) \
TFJ(StringIteratorPrototypeNext, JSParameterCount(0), kReceiver) \
TFJ(StringPrototypeMatch, JSParameterCount(1), kReceiver, kRegexp) \
TFJ(StringPrototypeSearch, JSParameterCount(1), kReceiver, kRegexp) \
TFJ(StringPrototypePadStart, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypePadEnd, kDontAdaptArgumentsSentinel) \
TFC(StringRepeat, StringRepeat) \
TFJ(StringPrototypeRepeat, JSParameterCount(1), kReceiver, kCount) \
TFJ(StringPrototypeReplaceAll, JSParameterCount(2), kReceiver, kSearchValue, kReplaceValue) \
TFJ(StringPrototypeSlice, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeStartsWith, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeSubstr, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeSubstring, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeToWellFormed, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeTrim, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeTrimStart, kDontAdaptArgumentsSentinel) \
TFJ(StringPrototypeTrimEnd, kDontAdaptArgumentsSentinel) \
TFJ(SuppressedErrorConstructor, kDontAdaptArgumentsSentinel) \
TFJ(SymbolPrototypeDescriptionGetter, JSParameterCount(0), kReceiver) \
TFJ(SymbolPrototypeToPrimitive, JSParameterCount(1), kReceiver, kHint) \
TFJ(SymbolPrototypeToString, JSParameterCount(0), kReceiver) \
TFJ(SymbolPrototypeValueOf, JSParameterCount(0), kReceiver) \
TFJ(TypedArrayPrototypeAt, JSParameterCount(1), kReceiver, kIndex) \
TFC(CreateTypedArray, CreateTypedArray) \
TFJ(TypedArrayPrototypeEvery, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeEntries, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeFilter, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeFind, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeFindIndex, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeFindLast, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeFindLastIndex, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeForEach, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayFrom, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeKeys, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayOf, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeReduce, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeReduceRight, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeSet, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeSlice, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeSome, kDontAdaptArgumentsSentinel) \
TFC(TypedArrayMergeSort, TypedArrayMergeSort) \
TFJ(TypedArrayPrototypeSort, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeSubArray, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeToReversed, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeToSorted, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeValues, kDontAdaptArgumentsSentinel) \
TFJ(TypedArrayPrototypeWith, JSParameterCount(2), kReceiver, kIndex, kValueArg) \
TFJ(WeakRefConstructor, kDontAdaptArgumentsSentinel) \
TFJ(WeakRefDeref, JSParameterCount(0), kReceiver) \
TFC(NewSloppyArgumentsElements, NewSloppyArgumentsElements) \
TFC(NewStrictArgumentsElements, NewStrictArgumentsElements) \
TFC(NewRestArgumentsElements, NewRestArgumentsElements) \
TFC(FastNewSloppyArguments, FastNewSloppyArguments) \
TFC(FastNewStrictArguments, FastNewStrictArguments) \
TFC(FastNewRestArguments, FastNewRestArguments) \
TFC(LoadFromContextCell, LoadFromContextCell) \
TFC(StoreCurrentContextElementBaseline, StoreCurrentContextElementBaseline) \
TFC(StoreContextElementBaseline, StoreContextElementBaseline) \
TFC(DetachContextCell, DetachContextCell) \
TFC(StringSlowFlatten, StringSlowFlatten) \
TFC(StringIndexOf, StringIndexOf) \
TFC(TestTurbofanType, TestTurbofanType) \
TFC(CheckTurbofanType, CheckTurbofanType) \
TFC(CheckTurboshaftWord32Type, CheckTurboshaftWord32Type) \
TFC(CheckTurboshaftWord64Type, CheckTurboshaftWord64Type) \
TFC(CheckTurboshaftFloat32Type, CheckTurboshaftFloat32Type) \
TFC(CheckTurboshaftFloat64Type, CheckTurboshaftFloat64Type) \
TFC(DebugPrintWord32, DebugPrintWord32) \
TFC(DebugPrintWord64, DebugPrintWord64) \
TFC(DebugPrintFloat32, DebugPrintFloat32) \
TFC(DebugPrintFloat64, DebugPrintFloat64) \
TFC(DebugPrintObject, DebugPrintObject) \
TFC(GenericBuiltinTest_JSAny_0, GenericBuiltinTest_JSAny_0) \
TFC(TestHelperPlus1, TestHelperPlus1) \
TFC(TestHelperPlus2, TestHelperPlus2) \
TFC(ReturnTwoValues, ReturnTwoValues) \
TFC(ThrowAsBuiltin, ThrowAsBuiltin) \
TFC(TestCallNever, TestCallNever) \
TFC(TestIncrementArraySpeciesModified, TestIncrementArraySpeciesModified) \
TFC(Load_FastSmiElements_0, Load_FastSmiElements_0) \
TFC(Load_FastObjectElements_0, Load_FastObjectElements_0) \
TFC(Load_FastDoubleElements_0, Load_FastDoubleElements_0) \
TFC(Store_FastSmiElements_0, Store_FastSmiElements_0) \
TFC(Store_FastObjectElements_0, Store_FastObjectElements_0) \
TFC(Store_FastDoubleElements_0, Store_FastDoubleElements_0) \
TFC(Delete_FastSmiElements_0, Delete_FastSmiElements_0) \
TFC(Delete_FastObjectElements_0, Delete_FastObjectElements_0) \
TFC(Delete_FastDoubleElements_0, Delete_FastDoubleElements_0) \
TFC(SortCompareDefault, SortCompareDefault) \
TFC(SortCompareUserFn, SortCompareUserFn) \
TFC(CanUseSameAccessor_GenericElementsAccessor_0, CanUseSameAccessor_GenericElementsAccessor_0) \
TFC(Copy, Copy) \
TFC(MergeAt, MergeAt) \
TFC(GallopLeft, GallopLeft) \
TFC(GallopRight, GallopRight) \
TFC(ArrayPowerSort, ArrayPowerSort) \
TFJ(ArrayPrototypeSort, kDontAdaptArgumentsSentinel) \
TFC(StringFastLocaleCompare, StringFastLocaleCompare) \
TFJ(JSToWasmWrapper, kDontAdaptArgumentsSentinel) \
TFJ(WasmPromising, kDontAdaptArgumentsSentinel) \
TFJ(WasmStressSwitch, kDontAdaptArgumentsSentinel) \
TFC(JSToWasmHandleReturns, JSToWasmHandleReturns) \
TFC(WasmInt32ToHeapNumber, WasmInt32ToHeapNumber) \
TFC(WasmInt32ToSharedHeapNumber, WasmInt32ToSharedHeapNumber) \
TFC(WasmFuncRefToJS, WasmFuncRefToJS) \
TFC(WasmTaggedNonSmiToInt32, WasmTaggedNonSmiToInt32) \
TFC(WasmTaggedToFloat64, WasmTaggedToFloat64) \
TFC(WasmTaggedToFloat32, WasmTaggedToFloat32) \
TFC(WasmMemoryGrow, WasmMemoryGrow) \
TFC(WasmTableInit, WasmTableInit) \
TFC(WasmTableCopy, WasmTableCopy) \
TFC(WasmTableFill, WasmTableFill) \
TFC(WasmTableGrow, WasmTableGrow) \
TFC(WasmTableGet, WasmTableGet) \
TFC(WasmTableSet, WasmTableSet) \
TFC(WasmTableGetFuncRef, WasmTableGetFuncRef) \
TFC(WasmFunctionTableGet, WasmFunctionTableGet) \
TFC(WasmTableSetFuncRef, WasmTableSetFuncRef) \
TFC(WasmRefFunc, WasmRefFunc) \
TFC(WasmInternalFunctionCreateExternal, WasmInternalFunctionCreateExternal) \
TFC(WasmAllocateZeroedFixedArray, WasmAllocateZeroedFixedArray) \
TFC(WasmAllocateFixedArray, WasmAllocateFixedArray) \
TFC(WasmLiftoffDeoptFinish, WasmLiftoffDeoptFinish) \
TFC(WasmThrow, WasmThrow) \
TFC(WasmRethrow, WasmRethrow) \
TFC(WasmThrowRef, WasmThrowRef) \
TFC(WasmRethrowExplicitContext, WasmRethrowExplicitContext) \
TFC(WasmTriggerTierUp, WasmTriggerTierUp) \
TFC(WasmGrowableStackGuard, WasmGrowableStackGuard) \
TFC(WasmStackGuard, WasmStackGuard) \
TFC(WasmStackGuardLoop, WasmStackGuardLoop) \
TFC(WasmStackOverflow, WasmStackOverflow) \
TFC(WasmTraceGlobal, WasmTraceGlobal) \
TFC(WasmTraceMemory, WasmTraceMemory) \
TFC(WasmTraceEnter, WasmTraceEnter) \
TFC(WasmTraceExit, WasmTraceExit) \
TFC(WasmConfigureAllPrototypesOpt, WasmConfigureAllPrototypesOpt) \
TFC(WasmAllocateJSArray, WasmAllocateJSArray) \
TFC(WasmAllocateStructWithRtt, WasmAllocateStructWithRtt) \
TFC(WasmAllocateDescriptorStruct, WasmAllocateDescriptorStruct) \
TFC(WasmAllocateSharedStructWithRtt, WasmAllocateSharedStructWithRtt) \
TFC(WasmAllocateArray_Uninitialized, WasmAllocateArray_Uninitialized) \
TFC(WasmAllocateSharedArray_Uninitialized, WasmAllocateSharedArray_Uninitialized) \
TFC(WasmArrayNewSegment, WasmArrayNewSegment) \
TFC(WasmArrayInitSegment, WasmArrayInitSegment) \
TFC(WasmArrayCopy, WasmArrayCopy) \
TFC(WasmUint32ToNumber, WasmUint32ToNumber) \
TFC(UintPtr53ToNumber, UintPtr53ToNumber) \
TFC(WasmI32AtomicWait, WasmI32AtomicWait) \
TFC(WasmI64AtomicWait, WasmI64AtomicWait) \
TFC(WasmManagedObjectWait32, WasmManagedObjectWait32) \
TFC(WasmManagedObjectWait64, WasmManagedObjectWait64) \
TFC(WasmManagedObjectWaitRef, WasmManagedObjectWaitRef) \
TFC(WasmWaitqueueNew, WasmWaitqueueNew) \
TFC(CallRefIC, CallRefIC) \
TFC(CallIndirectIC, CallIndirectIC) \
TFC(WasmTrap, WasmTrap) \
TFC(ThrowWasmTrapUnreachable, ThrowWasmTrapUnreachable) \
TFC(WasmPropagateException, WasmPropagateException) \
TFC(ThrowWasmTrapMemOutOfBounds, ThrowWasmTrapMemOutOfBounds) \
TFC(ThrowWasmTrapUnalignedAccess, ThrowWasmTrapUnalignedAccess) \
TFC(ThrowWasmTrapDivByZero, ThrowWasmTrapDivByZero) \
TFC(ThrowWasmTrapDivUnrepresentable, ThrowWasmTrapDivUnrepresentable) \
TFC(ThrowWasmTrapRemByZero, ThrowWasmTrapRemByZero) \
TFC(ThrowWasmTrapFloatUnrepresentable, ThrowWasmTrapFloatUnrepresentable) \
TFC(ThrowWasmTrapFuncSigMismatch, ThrowWasmTrapFuncSigMismatch) \
TFC(ThrowWasmTrapNullFunc, ThrowWasmTrapNullFunc) \
TFC(ThrowWasmTrapDataSegmentOutOfBounds, ThrowWasmTrapDataSegmentOutOfBounds) \
TFC(ThrowWasmTrapElementSegmentOutOfBounds, ThrowWasmTrapElementSegmentOutOfBounds) \
TFC(ThrowWasmTrapTableOutOfBounds, ThrowWasmTrapTableOutOfBounds) \
TFC(ThrowWasmTrapRethrowNull, ThrowWasmTrapRethrowNull) \
TFC(ThrowWasmTrapNullDereference, ThrowWasmTrapNullDereference) \
TFC(ThrowWasmTrapIllegalCast, ThrowWasmTrapIllegalCast) \
TFC(ThrowWasmTrapArrayOutOfBounds, ThrowWasmTrapArrayOutOfBounds) \
TFC(ThrowWasmTrapArrayTooLarge, ThrowWasmTrapArrayTooLarge) \
TFC(ThrowWasmTrapStringOffsetOutOfBounds, ThrowWasmTrapStringOffsetOutOfBounds) \
TFC(WasmTypeAssertionFailed, WasmTypeAssertionFailed) \
TFC(ThrowWasmTrapResume, ThrowWasmTrapResume) \
TFC(ThrowWasmTrapSuspend, ThrowWasmTrapSuspend) \
TFC(ThrowWasmTrapSwitch, ThrowWasmTrapSwitch) \
TFC(WasmStringNewWtf8, WasmStringNewWtf8) \
TFC(WasmStringNewWtf8Array, WasmStringNewWtf8Array) \
TFC(WasmStringNewWtf16, WasmStringNewWtf16) \
TFC(WasmStringNewWtf16Array, WasmStringNewWtf16Array) \
TFC(WasmStringFromDataSegment, WasmStringFromDataSegment) \
TFC(WasmStringAsWtf16, WasmStringAsWtf16) \
TFC(WasmStringConst, WasmStringConst) \
TFC(WasmStringMeasureUtf8, WasmStringMeasureUtf8) \
TFC(WasmStringMeasureWtf8, WasmStringMeasureWtf8) \
TFC(WasmStringEncodeWtf8, WasmStringEncodeWtf8) \
TFC(WasmStringEncodeWtf8Array, WasmStringEncodeWtf8Array) \
TFC(WasmStringToUtf8Array, WasmStringToUtf8Array) \
TFC(WasmStringEncodeWtf16, WasmStringEncodeWtf16) \
TFC(WasmStringEncodeWtf16Array, WasmStringEncodeWtf16Array) \
TFC(ThrowToLowerCaseCalledOnNull, ThrowToLowerCaseCalledOnNull) \
TFC(ThrowIndexOfCalledOnNull, ThrowIndexOfCalledOnNull) \
TFC(ThrowDataViewTypeError, ThrowDataViewTypeError) \
TFC(ThrowDataViewDetachedError, ThrowDataViewDetachedError) \
TFC(ThrowDataViewOutOfBounds, ThrowDataViewOutOfBounds) \
TFC(WasmStringConcat, WasmStringConcat) \
TFC(WasmStringAdd_NoMapCheck_Shared, WasmStringAdd_NoMapCheck_Shared) \
TFC(WasmStringEqual, WasmStringEqual) \
TFC(WasmStringIsUSVSequence, WasmStringIsUSVSequence) \
TFC(WasmStringAsWtf8, WasmStringAsWtf8) \
TFC(WasmStringViewWtf8Advance, WasmStringViewWtf8Advance) \
TFC(WasmStringViewWtf8Encode, WasmStringViewWtf8Encode) \
TFC(WasmStringViewWtf8Slice, WasmStringViewWtf8Slice) \
TFC(WasmStringViewWtf16GetCodeUnit, WasmStringViewWtf16GetCodeUnit) \
TFC(WasmStringViewWtf16Encode, WasmStringViewWtf16Encode) \
TFC(WasmStringViewWtf16Slice, WasmStringViewWtf16Slice) \
TFC(WasmStringSliceShared, WasmStringSliceShared) \
TFC(WasmStringAsIter, WasmStringAsIter) \
TFC(WasmStringCodePointAt, WasmStringCodePointAt) \
TFC(WasmStringViewIterNext, WasmStringViewIterNext) \
TFC(WasmStringViewIterAdvance, WasmStringViewIterAdvance) \
TFC(WasmStringViewIterRewind, WasmStringViewIterRewind) \
TFC(WasmStringViewIterSlice, WasmStringViewIterSlice) \
TFC(WasmIntToString, WasmIntToString) \
TFC(WasmStringToDouble, WasmStringToDouble) \
TFC(WasmStringFromCodePoint, WasmStringFromCodePoint) \
TFC(WasmStringHash, WasmStringHash) \
TFC(WasmAnyConvertExtern, WasmAnyConvertExtern) \
TFC(WasmAnyConvertExternShared, WasmAnyConvertExternShared) \
TFC(WasmExternConvertAny, WasmExternConvertAny) \
TFC(WasmFastApiCallTypeCheckAndUpdateIC, WasmFastApiCallTypeCheckAndUpdateIC) \
TFC(WasmStringIndexOf, WasmStringIndexOf) \
TFC(WasmLiftoffIsEqRefUnshared, WasmLiftoffIsEqRefUnshared) \
TFC(WasmLiftoffIsArrayRefUnshared, WasmLiftoffIsArrayRefUnshared) \
TFC(WasmLiftoffIsStructRefUnshared, WasmLiftoffIsStructRefUnshared) \
TFC(WasmLiftoffCastEqRefUnshared, WasmLiftoffCastEqRefUnshared) \
TFC(WasmLiftoffCastArrayRefUnshared, WasmLiftoffCastArrayRefUnshared) \
TFC(WasmLiftoffCastStructRefUnshared, WasmLiftoffCastStructRefUnshared) \
TFJ(WebAssemblyStringCast, JSParameterCount(1), kReceiver, kArg) \
TFJ(WebAssemblyStringTest, JSParameterCount(1), kReceiver, kArg) \
TFJ(WebAssemblyStringFromWtf16Array, JSParameterCount(3), kReceiver, kArrayArg, kStartArg, kEndArg) \
TFJ(WebAssemblyStringFromWtf16ArrayShared, JSParameterCount(3), kReceiver, kArrayArg, kStartArg, kEndArg) \
TFJ(WebAssemblyStringFromUtf8Array, JSParameterCount(3), kReceiver, kArrayArg, kStartArg, kEndArg) \
TFJ(WebAssemblyStringFromUtf8ArrayShared, JSParameterCount(3), kReceiver, kArrayArg, kStartArg, kEndArg) \
TFJ(WebAssemblyStringIntoUtf8Array, JSParameterCount(3), kReceiver, kStringArg, kArrayArg, kStartArg) \
TFJ(WebAssemblyStringIntoUtf8ArrayShared, JSParameterCount(3), kReceiver, kStringArg, kArrayArg, kStartArg) \
TFJ(WebAssemblyStringToUtf8Array, JSParameterCount(1), kReceiver, kStringArg) \
TFJ(WebAssemblyStringToUtf8ArrayShared, JSParameterCount(1), kReceiver, kStringArg) \
TFJ(WebAssemblyStringToWtf16Array, JSParameterCount(3), kReceiver, kStringArg, kArrayArg, kStartArg) \
TFJ(WebAssemblyStringToWtf16ArrayShared, JSParameterCount(3), kReceiver, kStringArg, kArrayArg, kStartArg) \
TFJ(WebAssemblyStringFromCharCode, JSParameterCount(1), kReceiver, kCodeArg) \
TFJ(WebAssemblyStringFromCharCodeShared, JSParameterCount(1), kReceiver, kCodeArg) \
TFJ(WebAssemblyStringFromCodePoint, JSParameterCount(1), kReceiver, kCodeArg) \
TFJ(WebAssemblyStringFromCodePointShared, JSParameterCount(1), kReceiver, kCodeArg) \
TFJ(WebAssemblyStringCodePointAt, JSParameterCount(2), kReceiver, kStringArg, kIndexArg) \
TFJ(WebAssemblyStringCharCodeAt, JSParameterCount(2), kReceiver, kStringArg, kIndexArg) \
TFJ(WebAssemblyStringLength, JSParameterCount(1), kReceiver, kStringArg) \
TFJ(WebAssemblyStringMeasureUtf8, JSParameterCount(1), kReceiver, kStringArg) \
TFJ(WebAssemblyStringConcat, JSParameterCount(2), kReceiver, kFirstArg, kSecondArg) \
TFJ(WebAssemblyStringSubstring, JSParameterCount(3), kReceiver, kStringArg, kStartArg, kEndArg) \
TFJ(WebAssemblyStringEquals, JSParameterCount(2), kReceiver, kA, kB) \
TFJ(WebAssemblyStringCompare, JSParameterCount(2), kReceiver, kFirstArg, kSecondArg) \
TFJ(WebAssemblyConfigureAllPrototypes, JSParameterCount(4), kReceiver, kPrototypes, kFunctions, kData, kConstructors) \
TFC(LoadJoinElement_GenericElementsAccessor_0, LoadJoinElement_GenericElementsAccessor_0) \
TFC(LoadJoinTypedElement_Int32Elements_0, LoadJoinTypedElement_Int32Elements_0) \
TFC(LoadJoinTypedElement_Float16Elements_0, LoadJoinTypedElement_Float16Elements_0) \
TFC(LoadJoinTypedElement_Float32Elements_0, LoadJoinTypedElement_Float32Elements_0) \
TFC(LoadJoinTypedElement_Float64Elements_0, LoadJoinTypedElement_Float64Elements_0) \
TFC(LoadJoinTypedElement_Uint8ClampedElements_0, LoadJoinTypedElement_Uint8ClampedElements_0) \
TFC(LoadJoinTypedElement_BigUint64Elements_0, LoadJoinTypedElement_BigUint64Elements_0) \
TFC(LoadJoinTypedElement_BigInt64Elements_0, LoadJoinTypedElement_BigInt64Elements_0) \
TFC(LoadJoinTypedElement_Uint8Elements_0, LoadJoinTypedElement_Uint8Elements_0) \
TFC(LoadJoinTypedElement_Int8Elements_0, LoadJoinTypedElement_Int8Elements_0) \
TFC(LoadJoinTypedElement_Uint16Elements_0, LoadJoinTypedElement_Uint16Elements_0) \
TFC(LoadJoinTypedElement_Int16Elements_0, LoadJoinTypedElement_Int16Elements_0) \
TFC(LoadJoinTypedElement_Uint32Elements_0, LoadJoinTypedElement_Uint32Elements_0) \
TFC(GenericBuiltinTest_Smi_0, GenericBuiltinTest_Smi_0) \
TFC(CanUseSameAccessor_FastDoubleElements_0, CanUseSameAccessor_FastDoubleElements_0) \
TFC(CanUseSameAccessor_FastSmiElements_0, CanUseSameAccessor_FastSmiElements_0) \
TFC(CanUseSameAccessor_FastObjectElements_0, CanUseSameAccessor_FastObjectElements_0) \
TFC(LoadNoHasPropertyCheck_GenericElementsAccessor_0, LoadNoHasPropertyCheck_GenericElementsAccessor_0) \
TFC(Load_GenericElementsAccessor_0, Load_GenericElementsAccessor_0) \
TFC(Store_GenericElementsAccessor_0, Store_GenericElementsAccessor_0) \
TFC(Delete_GenericElementsAccessor_0, Delete_GenericElementsAccessor_0) \
TFC(LoadTypedElement_Int32Elements_0, LoadTypedElement_Int32Elements_0) \
TFC(StoreTypedElementNumeric_Int32Elements_0, StoreTypedElementNumeric_Int32Elements_0) \
TFC(StoreTypedElementJSAny_Int32Elements_0, StoreTypedElementJSAny_Int32Elements_0) \
TFC(LoadTypedElement_Float16Elements_0, LoadTypedElement_Float16Elements_0) \
TFC(StoreTypedElementNumeric_Float16Elements_0, StoreTypedElementNumeric_Float16Elements_0) \
TFC(StoreTypedElementJSAny_Float16Elements_0, StoreTypedElementJSAny_Float16Elements_0) \
TFC(LoadTypedElement_Float32Elements_0, LoadTypedElement_Float32Elements_0) \
TFC(StoreTypedElementNumeric_Float32Elements_0, StoreTypedElementNumeric_Float32Elements_0) \
TFC(StoreTypedElementJSAny_Float32Elements_0, StoreTypedElementJSAny_Float32Elements_0) \
TFC(LoadTypedElement_Float64Elements_0, LoadTypedElement_Float64Elements_0) \
TFC(StoreTypedElementNumeric_Float64Elements_0, StoreTypedElementNumeric_Float64Elements_0) \
TFC(StoreTypedElementJSAny_Float64Elements_0, StoreTypedElementJSAny_Float64Elements_0) \
TFC(LoadTypedElement_Uint8ClampedElements_0, LoadTypedElement_Uint8ClampedElements_0) \
TFC(StoreTypedElementNumeric_Uint8ClampedElements_0, StoreTypedElementNumeric_Uint8ClampedElements_0) \
TFC(StoreTypedElementJSAny_Uint8ClampedElements_0, StoreTypedElementJSAny_Uint8ClampedElements_0) \
TFC(LoadTypedElement_BigUint64Elements_0, LoadTypedElement_BigUint64Elements_0) \
TFC(StoreTypedElementNumeric_BigUint64Elements_0, StoreTypedElementNumeric_BigUint64Elements_0) \
TFC(StoreTypedElementJSAny_BigUint64Elements_0, StoreTypedElementJSAny_BigUint64Elements_0) \
TFC(LoadTypedElement_BigInt64Elements_0, LoadTypedElement_BigInt64Elements_0) \
TFC(StoreTypedElementNumeric_BigInt64Elements_0, StoreTypedElementNumeric_BigInt64Elements_0) \
TFC(StoreTypedElementJSAny_BigInt64Elements_0, StoreTypedElementJSAny_BigInt64Elements_0) \
TFC(LoadTypedElement_Uint8Elements_0, LoadTypedElement_Uint8Elements_0) \
TFC(StoreTypedElementNumeric_Uint8Elements_0, StoreTypedElementNumeric_Uint8Elements_0) \
TFC(StoreTypedElementJSAny_Uint8Elements_0, StoreTypedElementJSAny_Uint8Elements_0) \
TFC(LoadTypedElement_Int8Elements_0, LoadTypedElement_Int8Elements_0) \
TFC(StoreTypedElementNumeric_Int8Elements_0, StoreTypedElementNumeric_Int8Elements_0) \
TFC(StoreTypedElementJSAny_Int8Elements_0, StoreTypedElementJSAny_Int8Elements_0) \
TFC(LoadTypedElement_Uint16Elements_0, LoadTypedElement_Uint16Elements_0) \
TFC(StoreTypedElementNumeric_Uint16Elements_0, StoreTypedElementNumeric_Uint16Elements_0) \
TFC(StoreTypedElementJSAny_Uint16Elements_0, StoreTypedElementJSAny_Uint16Elements_0) \
TFC(LoadTypedElement_Int16Elements_0, LoadTypedElement_Int16Elements_0) \
TFC(StoreTypedElementNumeric_Int16Elements_0, StoreTypedElementNumeric_Int16Elements_0) \
TFC(StoreTypedElementJSAny_Int16Elements_0, StoreTypedElementJSAny_Int16Elements_0) \
TFC(LoadTypedElement_Uint32Elements_0, LoadTypedElement_Uint32Elements_0) \
TFC(StoreTypedElementNumeric_Uint32Elements_0, StoreTypedElementNumeric_Uint32Elements_0) \
TFC(StoreTypedElementJSAny_Uint32Elements_0, StoreTypedElementJSAny_Uint32Elements_0) \

#define TORQUE_FUNCTION_POINTER_TYPE_TO_BUILTIN_MAP(V) \
  V(0,LoadJoinElement_DictionaryElements_0)\
  V(1,LoadTypedElement_Int32Elements_0)\
  V(2,StoreTypedElementNumeric_Int32Elements_0)\
  V(3,StoreTypedElementJSAny_Int32Elements_0)\
  V(4,GenericArrayToReversed)\
  V(5,TestHelperPlus1)\
  V(6,WasmTraceGlobal)\
  V(7,CreateAsyncFromSyncIteratorBaseline)\
  V(8,Load_FastSmiElements_0)\
  V(9,Store_FastSmiElements_0)\
  V(10,Delete_FastSmiElements_0)\

#endif  // V8_GEN_TORQUE_GENERATED_BUILTIN_DEFINITIONS_H_
