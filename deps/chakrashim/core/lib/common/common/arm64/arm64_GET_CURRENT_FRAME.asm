;-------------------------------------------------------------------------------------------------------
; Copyright (C) Microsoft. All rights reserved.
; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;-------------------------------------------------------------------------------------------------------
;Var arm64_GET_CURRENT_FRAME()
;
;   This method returns the current value of the frame pointer.
;
    OPT 2       ; disable listing

#include "ksarm64.h"

    OPT 1       ; re-enable listing

    TTL Lib\Common\arm64\arm64_GET_CURRENT_FRAME.asm


    EXPORT  arm64_GET_CURRENT_FRAME

    TEXTAREA

    LEAF_ENTRY arm64_GET_CURRENT_FRAME

    mov     x0,x29

    br      lr

    LEAF_END arm64_GET_CURRENT_FRAME

    END
