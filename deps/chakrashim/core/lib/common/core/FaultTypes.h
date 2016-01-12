//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef FAULT_TYPE
#error FAULT_TYPE not defined
#endif

FAULT_TYPE(Throw)
FAULT_TYPE(NoThrow)
FAULT_TYPE(MarkThrow)
FAULT_TYPE(MarkNoThrow)
FAULT_TYPE(StackProbe)
FAULT_TYPE(ScriptTermination)
FAULT_TYPE(ScriptTerminationOnDispose)
FAULT_TYPE(FaultInjectioSelfTest)

// custom fault types
// e.g. FaultInterpretThunk,

FAULT_TYPE(EnumFields_Fail)

