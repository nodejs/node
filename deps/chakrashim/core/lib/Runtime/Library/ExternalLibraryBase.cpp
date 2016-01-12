//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ExternalLibraryBase::ExternalLibraryBase() :
        scriptContext(nullptr),
        javascriptLibrary(nullptr),
        next(nullptr)
    {

    }

    void ExternalLibraryBase::Initialize(JavascriptLibrary* library)
    {
        Assert(this->javascriptLibrary == nullptr);
        this->javascriptLibrary = library;
        this->scriptContext = library->GetScriptContext();
#if DBG
        ExternalLibraryBase* current = library->externalLibraryList;
        while (current != nullptr)
        {
            Assert(current != this);
            current = current->next;
        }
#endif
        this->next = library->externalLibraryList;
        library->externalLibraryList = this;
    }

    void ExternalLibraryBase::Close()
    {
        ExternalLibraryBase* current = javascriptLibrary->externalLibraryList;
#if DBG
        bool found = false;
#endif
        if (current == this)
        {
            javascriptLibrary->externalLibraryList = this->next;
#if DBG
            found = true;
#endif
        }
        else
        {
            while (current != nullptr)
            {
                if (current->next == this)
                {
                    current->next = this->next;
#if DBG
                    found = true;
#endif
                    break;
                }
            }
        }
        Assert(found);
        this->javascriptLibrary = nullptr;
    }
}
