//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#if DBG

class DbCheckPostLower
{
private:
    Func *func;

    void        Check(IR::Opnd *opnd);
    void        Check(IR::RegOpnd *regOpnd);

public:
    DbCheckPostLower(Func *func) : func(func) { }
    void        Check();
};

#endif  // DBG
