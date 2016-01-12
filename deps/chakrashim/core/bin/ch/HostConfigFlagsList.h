//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifdef FLAG
FLAG(BSTR, Serialized,                    "If source is UTF8, deserializes from bytecode file", NULL)
FLAG(BSTR, GenerateLibraryByteCodeHeader, "Generate bytecode header file from library code", NULL)
#undef FLAG
#endif
