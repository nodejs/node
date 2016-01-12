;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------

;void arm_SAVE_REGISTERS(void*)
;
;   This method pushes sp, r1-r12 onto an array of 13 DWORDs at r0.
;   By convention, the stack pointer is at offset 0
;
;       DWORD registers[13];
;       arm_SAVE_REGISTERS(registers);
;
    OPT 2   ; disable listing

#include "ksarm.h"

    OPT 1   ; re-enable listing

    TTL Lib\Common\Memory\arm\arm_SAVE_REGISTERS.asm


    EXPORT  arm_SAVE_REGISTERS

    TEXTAREA

    LEAF_ENTRY arm_SAVE_REGISTERS

    str     sp, [r0], #+4
    stmia   r0, {r1-r12}

    bx      lr

    LEAF_END arm_SAVE_REGISTERS

    END
