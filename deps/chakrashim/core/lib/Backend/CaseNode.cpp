//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

int
DefaultComparer<CaseNode *>::Compare(CaseNode* caseNode1, CaseNode* caseNode2)
{
    int caseVal1 = caseNode1->GetSrc2IntConst();
    int caseVal2 = caseNode2->GetSrc2IntConst();
    uint32 caseOffset1 = caseNode1->GetOffset();
    uint32 caseOffset2 = caseNode2->GetOffset();

    if (caseVal1 == caseVal2)
    {
        return caseOffset1 - caseOffset2;
    }

    if (caseVal1 > caseVal2) return 1;
    return -1;
}

bool
DefaultComparer<CaseNode *>::Equals(CaseNode * caseNode1, CaseNode* caseNode2)
{
    if(caseNode1->IsSrc2IntConst() && caseNode2->IsSrc2IntConst())
    {
        int caseVal1 = caseNode1->GetSrc2IntConst();
        int caseVal2 = caseNode2->GetSrc2IntConst();
        return caseVal1 == caseVal2;
    }
    else if(caseNode1->IsSrc2StrConst() && caseNode2->IsSrc2StrConst())
    {
        Js::JavascriptString * caseVal1 = caseNode1->GetSrc2StringConst();
        Js::JavascriptString * caseVal2 = caseNode2->GetSrc2StringConst();
        return Js::JavascriptString::Equals(caseVal1, caseVal2);
    }
    else
    {
        AssertMsg(false, "Should not reach here. CaseNodes should store only string or integer case values");
        return false;
    }
}

uint
DefaultComparer<CaseNode *>::GetHashCode(CaseNode* caseNode)
{
    return (uint)caseNode->GetSrc2IntConst();
}
