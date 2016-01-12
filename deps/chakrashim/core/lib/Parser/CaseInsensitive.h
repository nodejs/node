//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace UnifiedRegex
{
    namespace CaseInsensitive
    {
        // It turns out there are many upper-case characters with three lower-case variants, thus
        // the maximum size of an equivalence class is four.
        static const int EquivClassSize = 4;

        enum class MappingSource : uint8
        {
            UnicodeData,
            CaseFolding
        };

        // Following two functions return equivalents from UnicodeData (for wchar_t) and CaseFolding
        // (for codepoint_t) files. Their names don't have anything distinguishing them so that
        // they can be called easily from template functions.
        bool RangeToEquivClass(uint& tblidx, uint l, uint h, uint& acth, __out_ecount(EquivClassSize) wchar_t equivl[EquivClassSize]);
        bool RangeToEquivClass(uint& tblidx, uint l, uint h, uint& acth, __out_ecount(EquivClassSize) codepoint_t equivl[EquivClassSize]);

        // Returns equivalents only from the given source. Some case-folding mappings already exist in
        // UnicodeData, so this function doesn't return them when CaseFolding is passed as the source.
        bool RangeToEquivClassOnlyInSource(MappingSource mappingSource, uint& tblidx, uint l, uint h, uint& acth, __out_ecount(EquivClassSize) wchar_t equivl[EquivClassSize]);
    }
}
