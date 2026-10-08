/* Copyright 2021 - 2026 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_MSG_H
#define LIEF_MSG_H

#if !defined(LIEF_DOC_PREFIX)
  #define LIEF_DOC_PREFIX "https://lief.re/doc"
#endif

#define SUBMISSION_MSG                                                            \
  "This file can be interesting for improving LIEF. "                             \
  "If you have the rights to do, please consider submitting this file "           \
  "through a GitHub issue: https://github.com/lief-project/LIEF"

#if !defined(DEBUG_FMT_NOT_SUPPORTED)
  #define DEBUG_FMT_NOT_SUPPORTED                                                 \
    "Debug info not available for this build.\n"                                  \
    "Please checkout " LIEF_DOC_PREFIX                                            \
    "/latest/extended/intro.html for the details"
#endif

#if !defined(OBJC_NOT_SUPPORTED)
  #define OBJC_NOT_SUPPORTED                                                      \
    "ObjC metadata not available for this build.\n"                               \
    "Please checkout " LIEF_DOC_PREFIX                                            \
    "/latest/extended/intro.html for the details"
#endif

#if !defined(DSC_NOT_SUPPORTED)
  #define DSC_NOT_SUPPORTED                                                       \
    "Dyld shared cache not available for this build.\n"                           \
    "Please checkout " LIEF_DOC_PREFIX                                            \
    "/latest/extended/intro.html for the details"
#endif

#if !defined(ASSEMBLY_NOT_SUPPORTED)
  #define ASSEMBLY_NOT_SUPPORTED                                                  \
    "Assembler/disassembler not available for this build.\n"                      \
    "Please checkout " LIEF_DOC_PREFIX                                            \
    "/latest/extended/intro.html for the details"
#endif

#if !defined(NEEDS_EXTENDED_MSG)
  #define NEEDS_EXTENDED_MSG                                                      \
    "This function requires the extended version of LIEF.\n"                      \
    "Please checkout " LIEF_DOC_PREFIX                                            \
    "/latest/extended/intro.html for the details"
#endif

#if !defined(LIEF_NEEDS_RUNTIME_MSG)
  #define LIEF_NEEDS_RUNTIME_MSG                                                  \
    "This version of LIEF does not contain runtime features."
#endif


#endif
