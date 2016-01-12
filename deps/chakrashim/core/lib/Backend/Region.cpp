//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

Region *
Region::New(RegionType type, Region * parent, Func * func)
{
    Region * region = JitAnew(func->m_alloc, Region);

    region->type = type;
    region->parent = parent;
    region->bailoutReturnThunkLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    if (type == RegionTypeTry)
    {
        region->selfOrFirstTryAncestor = region;
    }
    return region;
}

void
Region::AllocateEHBailoutData(Func * func, IR::Instr * tryInstr)
{
    if (this->GetType() == RegionTypeRoot)
    {
        this->ehBailoutData = NativeCodeDataNew(func->GetNativeCodeDataAllocator(), Js::EHBailoutData, -1 /*nestingDepth*/, 0 /*catchOffset*/, nullptr /*parent*/);
    }
    else
    {
        this->ehBailoutData = NativeCodeDataNew(func->GetNativeCodeDataAllocator(), Js::EHBailoutData, this->GetParent()->ehBailoutData->nestingDepth + 1, 0, this->GetParent()->ehBailoutData);
        if (this->GetType() == RegionTypeTry)
        {
            Assert(tryInstr);
            if (tryInstr->m_opcode == Js::OpCode::TryCatch)
            {
                this->ehBailoutData->catchOffset = tryInstr->AsBranchInstr()->GetTarget()->GetByteCodeOffset(); // ByteCode offset of the Catch
            }
        }
    }
}

Region *
Region::GetSelfOrFirstTryAncestor()
{
    if (!this->selfOrFirstTryAncestor)
    {
        Region* region = this;
        while (region && region->GetType() != RegionTypeTry)
        {
            region = region->GetParent();
        }
        this->selfOrFirstTryAncestor = region;
    }
    return this->selfOrFirstTryAncestor;
}
