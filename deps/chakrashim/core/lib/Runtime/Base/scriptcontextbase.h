//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//////////////////////////////////////////////////////////
// Should we remove this??
// ScriptContextBase.h is used by static lib shared between host and chakra. We need to keep
// the size consistent and try not to change its size. We need to have matching mshtml.dll
// if the size changed here.
/////////////////////////////////////////////////////////
#pragma once
interface IActiveScriptDirect;

namespace Js
{
    class GlobalObject;
    class JavascriptLibrary;

    class ScriptContextBase
    {
    public:
        ScriptContextBase():
            javascriptLibrary(nullptr),
            globalObject(nullptr),
            isClosed(false),
            pActiveScriptDirect(nullptr) {}
        JavascriptLibrary* GetLibrary() const { return javascriptLibrary; }
        void SetLibrary(JavascriptLibrary* library) { javascriptLibrary = library;}
        void ClearGlobalObject();
        void SetGlobalObject(GlobalObject *globalObject);
        GlobalObject* GetGlobalObject() const { return globalObject; }
        IActiveScriptDirect* GetActiveScriptDirect()
        {
            return this->pActiveScriptDirect;
        }

        void SetActiveScriptDirect(IActiveScriptDirect* pActiveScriptDirect)
        {
            this->pActiveScriptDirect = pActiveScriptDirect;
        }


    protected:
        JavascriptLibrary* javascriptLibrary;
        GlobalObject* globalObject;
        bool isClosed;
        IActiveScriptDirect* pActiveScriptDirect;
    };
}
