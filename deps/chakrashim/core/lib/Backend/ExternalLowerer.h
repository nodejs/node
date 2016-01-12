//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

class ExternalLowerer
{
public:
    static bool TryGenerateFastExternalEqTest(IR::Opnd * src1, IR::Opnd * src2, IR::BranchInstr * instrBranch,
        IR::LabelInstr * labelHelper, IR::LabelInstr * labelBooleanCmp, Lowerer * lowerer, bool isStrictBr);
};

#if !NTBUILD
// ChakraCore default implementation doesn't have anything external type to check
inline bool
ExternalLowerer::TryGenerateFastExternalEqTest(IR::Opnd * src1, IR::Opnd * src2, IR::BranchInstr * instrBranch,
    IR::LabelInstr * labelHelper, IR::LabelInstr * labelBooleanCmp, Lowerer * lowerer, bool isStrictBr)
{
    return false;
}
#endif
