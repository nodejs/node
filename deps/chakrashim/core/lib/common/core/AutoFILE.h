//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class AutoFILE : public BasePtr<FILE>
{
public:
    AutoFILE(FILE * file = nullptr) : BasePtr<FILE>(file) {};
    ~AutoFILE()
    {
        Close();
    }
    AutoFILE& operator=(FILE * file)
    {
        Close();
        this->ptr = file;
        return *this;
    }
    void Close()
    {
        if (ptr != nullptr)
        {
            fclose(ptr);
        }
    }
};
