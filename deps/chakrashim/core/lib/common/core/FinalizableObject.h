//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class FinalizableObject
{
public:
    // Called right after finish marking and this object is determined to be dead.
    // Should contain only simple clean up code.
    // Can't run another script
    // Can't cause a re-entrant collection

    virtual void Finalize(bool isShutdown) = 0;

    // Call after sweeping is done.
    // Can call other script or cause another collection.

    virtual void Dispose(bool isShutdown) = 0;

    // Used only by TrackableObjects (created with TrackedBit on by RecyclerNew*Tracked)
    virtual void Mark(Recycler * recycler) = 0;
};
