# Copyright (C) The c-ares project and its contributors
# SPDX-License-Identifier: MIT
# -----------------------------------------------
# Detect NMAKE version deducing old MSVC versions
# -----------------------------------------------

!IFNDEF _NMAKE_VER
!  MESSAGE Macro _NMAKE_VER not defined.
!  MESSAGE Use MSVC's NMAKE to process this makefile.
!  ERROR   See previous message.
!ENDIF

!IF     "$(_NMAKE_VER)" == "6.00.8168.0"
CC_VERS_NUM = 60
!ELSEIF "$(_NMAKE_VER)" == "6.00.9782.0"
CC_VERS_NUM = 60
!ELSEIF "$(_NMAKE_VER)" == "7.00.8882"
CC_VERS_NUM = 70
!ELSEIF "$(_NMAKE_VER)" == "7.00.9466"
CC_VERS_NUM = 70
!ELSEIF "$(_NMAKE_VER)" == "7.00.9955"
CC_VERS_NUM = 70
!ELSE
# Pick an arbitrary bigger number for all later versions
CC_VERS_NUM = 199
!ENDIF
