//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class JsrtExceptionBase : public Js::ExceptionBase
{
public:
    virtual JsErrorCode GetJsErrorCode() = 0;
};
