//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class LinearScan;
class LinearScanMDShared
{
public:
    void Init(LinearScan * linearScan) { this->linearScan = linearScan; }

protected:
    LinearScan * linearScan;
};
