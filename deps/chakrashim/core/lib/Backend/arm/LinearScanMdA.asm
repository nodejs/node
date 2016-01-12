;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

    OPT 2 ; disable listing
#include "ksarm.h"
    OPT 1 ; re-enable listing

    TTL Lib\Backend\arm\LinearScanMdA.asm

    EXPORT |?SaveAllRegistersAndBailOut@LinearScanMD@@SAXQAVBailOutRecord@@@Z|
    EXPORT |?SaveAllRegistersAndBranchBailOut@LinearScanMD@@SAXQAVBranchBailOutRecord@@H@Z|

    ; BailOutRecord::BailOut(BailOutRecord const * bailOutRecord)
    IMPORT |?BailOut@BailOutRecord@@SAPAXPBV1@@Z|

    ; BranchBailOutRecord::BailOut(BranchBailOutRecord const * bailOutRecord, BOOL cond)
    IMPORT |?BailOut@BranchBailOutRecord@@SAPAXPBV1@H@Z|

    TEXTAREA

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; LinearScanMD::SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord)

    NESTED_ENTRY ?SaveAllRegistersAndBailOut@LinearScanMD@@SAXQAVBailOutRecord@@@Z

    ; r0 == bailOutRecord
    ; lr == return address

    ; Save all registers except the above, which would have already been saved by jitted code if necessary
    ldr r12, [r0] ; bailOutRecord->globalBailOutRecordDataTable
    ldr r12, [r12] ; bailOutRecord->globalBailOutRecordDataTable->registerSaveSpace
    add r12, r12, #192 ; &reinterpret_cast<byte *>(bailOutRecord->globalBailOutRecordDataTable->registerSaveSpace)[(RegD0 - 1) * 4 + VFP_REGCOUNT * 8]
    vstmdb r12!, {d0 - d15}
    sub r12, r12, #16 ; skip r12-r15
    stmdb r12!, {r1 - r11}

    b |?BailOut@BailOutRecord@@SAPAXPBV1@@Z|

    NESTED_END

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; LinearScanMD::SaveAllRegistersAndBranchBailOut(BranchBailOutRecord *const bailOutRecord, const BOOL condition)

    NESTED_ENTRY ?SaveAllRegistersAndBranchBailOut@LinearScanMD@@SAXQAVBranchBailOutRecord@@H@Z

    ; r0 == bailOutRecord
    ; r1 == condition
    ; lr == return address

    ; Save all registers except the above, which would have already been saved by jitted code if necessary
    ldr r12, [r0] ; bailOutRecord->globalBailOutRecordDataTable
    ldr r12, [r12] ; bailOutRecord->globalBailOutRecordDataTable->registerSaveSpace
    add r12, r12, #192 ; &bailOutRecord->globalBailOutRecordDataTable->registerSaveSpace[(RegD0 - 1) * 4 + VFP_REGCOUNT * 8]
    vstmdb r12!, {d0 - d15}
    sub r12, r12, #16 ; skip r12-r15
    stmdb r12!, {r2 - r11}

    b |?BailOut@BranchBailOutRecord@@SAPAXPBV1@H@Z|

    NESTED_END

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    END
