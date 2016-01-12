//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_PROFILE_INFO
namespace Js
{
    // Provides a thread-safe, read-only view of a profile. This is the view used
    // by codegen. We copy settings that are no thread-safe and we pass-through
    // the rest.
    // Note that this class' ctor is called on the codegen thread, so if the
    // copying itself (as opposed to accesses) ever becomes thread-unsafe then a locking
    // scheme would need to be added.
    class ReadOnlyDynamicProfileInfo
    {
    public:
        ReadOnlyDynamicProfileInfo(const DynamicProfileInfo * profileInfo, ArenaAllocator *const backgroundAllocator) :
            profileInfo(profileInfo),
            backgroundAllocator(backgroundAllocator),
            isAggressiveIntTypeSpecDisabled(false),
            isAggressiveIntTypeSpecDisabled_jitLoopBody(false),
            isAggressiveMulIntTypeSpecDisabled(false),
            isAggressiveMulIntTypeSpecDisabled_jitLoopBody(false),
            isDivIntTypeSpecDisabled(false),
            isDivIntTypeSpecDisabled_jitLoopBody(false),
            isLossyIntTypeSpecDisabled(false),
            isTrackCompoundedIntOverflowDisabled(false),
            isFloatTypeSpecDisabled(false),
            isArrayCheckHoistDisabled(false),
            isArrayCheckHoistDisabled_jitLoopBody(false),
            isArrayMissingValueCheckHoistDisabled(false),
            isArrayMissingValueCheckHoistDisabled_jitLoopBody(false),
            isJsArraySegmentHoistDisabled(false),
            isJsArraySegmentHoistDisabled_jitLoopBody(false),
            isArrayLengthHoistDisabled(false),
            isArrayLengthHoistDisabled_jitLoopBody(false),
            isTypedArrayTypeSpecDisabled(false),
            isTypedArrayTypeSpecDisabled_jitLoopBody(false),
            isLdLenIntSpecDisabled(false),
            isBoundCheckHoistDisabled(false),
            isBoundCheckHoistDisabled_jitLoopBody(false),
            isLoopCountBasedBoundCheckHoistDisabled(false),
            isLoopCountBasedBoundCheckHoistDisabled_jitLoopBody(false),
            isFloorInliningDisabled(false),
            isNoProfileBailoutsDisabled(false),
            isSwitchOptDisabled(false),
            isEquivalentObjTypeSpecDisabled(false),
            isObjTypeSpecDisabled_jitLoopBody(false),
            ldElemInfo(nullptr),
            stElemInfo(nullptr)
        {
            if (profileInfo == nullptr)
            {
                return;
            }

            this->isAggressiveIntTypeSpecDisabled = profileInfo->IsAggressiveIntTypeSpecDisabled(false);
            this->isAggressiveIntTypeSpecDisabled_jitLoopBody = profileInfo->IsAggressiveIntTypeSpecDisabled(true);
            this->isAggressiveMulIntTypeSpecDisabled = profileInfo->IsAggressiveMulIntTypeSpecDisabled(false);
            this->isAggressiveMulIntTypeSpecDisabled_jitLoopBody = profileInfo->IsAggressiveMulIntTypeSpecDisabled(true);
            this->isDivIntTypeSpecDisabled = profileInfo->IsDivIntTypeSpecDisabled(false);
            this->isDivIntTypeSpecDisabled_jitLoopBody = profileInfo->IsDivIntTypeSpecDisabled(true);
            this->isLossyIntTypeSpecDisabled = profileInfo->IsLossyIntTypeSpecDisabled();
            this->isTrackCompoundedIntOverflowDisabled = profileInfo->IsTrackCompoundedIntOverflowDisabled();
            this->isFloatTypeSpecDisabled = profileInfo->IsFloatTypeSpecDisabled();
            this->isArrayCheckHoistDisabled = profileInfo->IsArrayCheckHoistDisabled(false);
            this->isArrayCheckHoistDisabled_jitLoopBody = profileInfo->IsArrayCheckHoistDisabled(true);
            this->isArrayMissingValueCheckHoistDisabled = profileInfo->IsArrayMissingValueCheckHoistDisabled(false);
            this->isArrayMissingValueCheckHoistDisabled_jitLoopBody = profileInfo->IsArrayMissingValueCheckHoistDisabled(true);
            this->isJsArraySegmentHoistDisabled = profileInfo->IsJsArraySegmentHoistDisabled(false);
            this->isJsArraySegmentHoistDisabled_jitLoopBody = profileInfo->IsJsArraySegmentHoistDisabled(true);
            this->isArrayLengthHoistDisabled = profileInfo->IsArrayLengthHoistDisabled(false);
            this->isArrayLengthHoistDisabled_jitLoopBody = profileInfo->IsArrayLengthHoistDisabled(true);
            this->isTypedArrayTypeSpecDisabled = profileInfo->IsTypedArrayTypeSpecDisabled(false);
            this->isTypedArrayTypeSpecDisabled_jitLoopBody = profileInfo->IsTypedArrayTypeSpecDisabled(true);
            this->isLdLenIntSpecDisabled = profileInfo->IsLdLenIntSpecDisabled();
            this->isBoundCheckHoistDisabled = profileInfo->IsBoundCheckHoistDisabled(false);
            this->isBoundCheckHoistDisabled_jitLoopBody = profileInfo->IsBoundCheckHoistDisabled(true);
            this->isLoopCountBasedBoundCheckHoistDisabled = profileInfo->IsLoopCountBasedBoundCheckHoistDisabled(false);
            this->isLoopCountBasedBoundCheckHoistDisabled_jitLoopBody = profileInfo->IsLoopCountBasedBoundCheckHoistDisabled(true);
            this->isFloorInliningDisabled = profileInfo->IsFloorInliningDisabled();
            this->isNoProfileBailoutsDisabled = profileInfo->IsNoProfileBailoutsDisabled();
            this->isSwitchOptDisabled = profileInfo->IsSwitchOptDisabled();
            this->isEquivalentObjTypeSpecDisabled = profileInfo->IsEquivalentObjTypeSpecDisabled();
            this->isObjTypeSpecDisabled_jitLoopBody = profileInfo->IsObjTypeSpecDisabledInJitLoopBody();
        }

        void OnBackgroundAllocatorReset()
        {
            // The background allocator was reset, so need to clear any references to data cloned using that allocator
            Assert(backgroundAllocator);
            ldElemInfo = nullptr;
            stElemInfo = nullptr;
        }

        bool HasProfileInfo() const
        {
            return this->profileInfo != NULL;
        }

        const LdElemInfo *GetLdElemInfo(FunctionBody *functionBody, ProfileId ldElemId);
        const StElemInfo *GetStElemInfo(FunctionBody *functionBody, ProfileId stElemId);

        ArrayCallSiteInfo *GetArrayCallSiteInfo(FunctionBody *functionBody, ProfileId index)
        {
            return this->profileInfo->GetArrayCallSiteInfo(functionBody, index);
        }

        FldInfo * GetFldInfo(FunctionBody* functionBody, uint fieldAccessId) const
        {
            return this->profileInfo->GetFldInfo(functionBody, fieldAccessId);
        }

        ThisInfo GetThisInfo() const
        {
            return this->profileInfo->GetThisInfo();
        }

        ValueType GetReturnType(FunctionBody* functionBody, Js::OpCode opcode, ProfileId callSiteId) const
        {
            return this->profileInfo->GetReturnType(functionBody, opcode, callSiteId);
        }

        ValueType GetDivProfileInfo(FunctionBody* functionBody, ProfileId divideId) const
        {
            return this->profileInfo->GetDivideResultType(functionBody, divideId);
        }

        bool IsModulusOpByPowerOf2(FunctionBody* functionBody, ProfileId moduleId) const
        {
            return this->profileInfo->IsModulusOpByPowerOf2(functionBody, moduleId);
        }

        ValueType GetSwitchProfileInfo(FunctionBody* functionBody, ProfileId switchId) const
        {
            return this->profileInfo->GetSwitchType(functionBody, switchId);
        }

        ValueType GetParameterInfo(FunctionBody* functionBody, ArgSlot index) const
        {
            return this->profileInfo->GetParameterInfo(functionBody, index);
        }

        ImplicitCallFlags GetLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum) const
        {
            return this->profileInfo->GetLoopImplicitCallFlags(functionBody, loopNum);
        }

        ImplicitCallFlags GetImplicitCallFlags() const
        {
            return this->profileInfo->GetImplicitCallFlags();
        }

        LoopFlags GetLoopFlags(uint loopNum) const
        {
            return this->profileInfo->GetLoopFlags(loopNum);
        }

        bool IsAggressiveIntTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isAggressiveIntTypeSpecDisabled_jitLoopBody
                    : this->isAggressiveIntTypeSpecDisabled;
        }

        void DisableAggressiveIntTypeSpec(const bool isJitLoopBody)
        {
            this->isAggressiveIntTypeSpecDisabled_jitLoopBody = true;
            if(!isJitLoopBody)
            {
                this->isAggressiveIntTypeSpecDisabled = true;
            }
        }

        bool IsSwitchOptDisabled() const
        {
            return this->isSwitchOptDisabled;
        }

        void DisableSwitchOpt()
        {
            this->isSwitchOptDisabled = true;
        }

        bool IsEquivalentObjTypeSpecDisabled() const
        {
            return this->isEquivalentObjTypeSpecDisabled;
        }

        void DisableEquivalentObjTypeSpec()
        {
            this->isEquivalentObjTypeSpecDisabled = true;
        }

        bool IsObjTypeSpecDisabledInJitLoopBody() const
        {
            return this->isObjTypeSpecDisabled_jitLoopBody;
        }

        void DisableObjTypeSpecInJitLoopBody()
        {
            this->isObjTypeSpecDisabled_jitLoopBody = true;
        }

        bool IsAggressiveMulIntTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isAggressiveMulIntTypeSpecDisabled_jitLoopBody
                    : this->isAggressiveMulIntTypeSpecDisabled;
        }

        void DisableAggressiveMulIntTypeSpec(const bool isJitLoopBody)
        {
            this->isAggressiveMulIntTypeSpecDisabled_jitLoopBody = true;
            if(!isJitLoopBody)
            {
                this->isAggressiveMulIntTypeSpecDisabled = true;
            }
        }

        bool IsDivIntTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                ? this->isDivIntTypeSpecDisabled_jitLoopBody
                : this->isDivIntTypeSpecDisabled;
        }

        void DisableDivIntTypeSpec(const bool isJitLoopBody)
        {
            this->isDivIntTypeSpecDisabled_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->isDivIntTypeSpecDisabled = true;
            }
        }

        bool IsLossyIntTypeSpecDisabled() const
        {
            return this->isLossyIntTypeSpecDisabled;
        }

        bool IsMemOpDisabled() const
        {
            return this->profileInfo->IsMemOpDisabled();
        }

        bool IsTrackCompoundedIntOverflowDisabled() const
        {
            return this->isTrackCompoundedIntOverflowDisabled;
        }

        void DisableTrackCompoundedIntOverflow()
        {
            this->isTrackCompoundedIntOverflowDisabled = true;
        }

        bool IsFloatTypeSpecDisabled() const
        {
            return this->isFloatTypeSpecDisabled;
        }

        bool IsCheckThisDisabled() const
        {
            return this->profileInfo->IsCheckThisDisabled();
        }

        bool IsArrayCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isArrayCheckHoistDisabled_jitLoopBody
                    : this->isArrayCheckHoistDisabled;
        }

        bool IsArrayMissingValueCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isArrayMissingValueCheckHoistDisabled_jitLoopBody
                    : this->isArrayMissingValueCheckHoistDisabled;
        }

        bool IsJsArraySegmentHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isJsArraySegmentHoistDisabled_jitLoopBody
                    : this->isJsArraySegmentHoistDisabled;
        }

        bool IsArrayLengthHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isArrayLengthHoistDisabled_jitLoopBody
                    : this->isArrayLengthHoistDisabled;
        }

        bool IsTypedArrayTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isTypedArrayTypeSpecDisabled_jitLoopBody
                    : this->isTypedArrayTypeSpecDisabled;
        }

        bool IsLdLenIntSpecDisabled() const
        {
            return this->isLdLenIntSpecDisabled;
        }

        bool IsBoundCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isBoundCheckHoistDisabled_jitLoopBody
                    : this->isBoundCheckHoistDisabled;
        }

        bool IsLoopCountBasedBoundCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->isLoopCountBasedBoundCheckHoistDisabled_jitLoopBody
                    : this->isLoopCountBasedBoundCheckHoistDisabled;
        }

        bool IsFloorInliningDisabled() const
        {
            return this->isFloorInliningDisabled;
        }

        bool IsNoProfileBailoutsDisabled() const
        {
            return this->isNoProfileBailoutsDisabled;
        }

    private:
        const DynamicProfileInfo * profileInfo;
        ArenaAllocator *const backgroundAllocator; // null if the work item is being jitted in the foreground

        // These settings need to be copied because they are used at multiple points in the globopt,
        // and the readings need to be consistent.
        bool isAggressiveIntTypeSpecDisabled : 1;
        bool isAggressiveIntTypeSpecDisabled_jitLoopBody : 1;
        bool isAggressiveMulIntTypeSpecDisabled : 1;
        bool isAggressiveMulIntTypeSpecDisabled_jitLoopBody : 1;
        bool isDivIntTypeSpecDisabled : 1;
        bool isDivIntTypeSpecDisabled_jitLoopBody : 1;
        bool isLossyIntTypeSpecDisabled : 1;
        bool isTrackCompoundedIntOverflowDisabled : 1;
        bool isFloatTypeSpecDisabled : 1;
        bool isArrayCheckHoistDisabled : 1;
        bool isArrayCheckHoistDisabled_jitLoopBody : 1;
        bool isArrayMissingValueCheckHoistDisabled : 1;
        bool isArrayMissingValueCheckHoistDisabled_jitLoopBody : 1;
        bool isJsArraySegmentHoistDisabled : 1;
        bool isJsArraySegmentHoistDisabled_jitLoopBody : 1;
        bool isArrayLengthHoistDisabled : 1;
        bool isArrayLengthHoistDisabled_jitLoopBody : 1;
        bool isTypedArrayTypeSpecDisabled : 1;
        bool isTypedArrayTypeSpecDisabled_jitLoopBody : 1;
        bool isLdLenIntSpecDisabled : 1;
        bool isBoundCheckHoistDisabled : 1;
        bool isBoundCheckHoistDisabled_jitLoopBody : 1;
        bool isLoopCountBasedBoundCheckHoistDisabled : 1;
        bool isLoopCountBasedBoundCheckHoistDisabled_jitLoopBody : 1;
        bool isFloorInliningDisabled : 1;
        bool isNoProfileBailoutsDisabled : 1;
        bool isSwitchOptDisabled : 1;
        bool isEquivalentObjTypeSpecDisabled : 1;
        bool isObjTypeSpecDisabled_jitLoopBody : 1;
        const LdElemInfo *ldElemInfo;
        const StElemInfo *stElemInfo;

        // Other settings are safe to be accessed concurrently. If that changes then they need
        // to be copied.
    };
};
#endif
