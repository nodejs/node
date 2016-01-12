//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Memory
{
    /*
    * Telemetry timestamp macros
    *
    * To record a particular timestamp, use RECORD_TIMESTAMP. This overwrites the previous timestamp.
    * To have auto-start/end events logged, use the AUTO_TIMESTAMP macro.
    */
    class AutoTimestamp
    {
    public:
        AutoTimestamp(FILETIME * startTimestamp, FILETIME * endTimestamp) : endTimestamp(endTimestamp)
        {
            ::GetSystemTimeAsFileTime(startTimestamp);
        }
        ~AutoTimestamp()
        {
            ::GetSystemTimeAsFileTime(endTimestamp);
        }
    private:
        FILETIME * endTimestamp;
    };

#define RECORD_TIMESTAMP(Field) ::GetSystemTimeAsFileTime(&telemetryBlock->Field);
#define INC_TIMESTAMP_FIELD(Field) telemetryBlock->Field++;
#define AUTO_TIMESTAMP(Field) Memory::AutoTimestamp timestamp_##Field(&telemetryBlock->Field##StartTime, &telemetryBlock->Field##EndTime);


    struct RecyclerWatsonTelemetryBlock
    {
        FILETIME initialCollectionStartTime;
        DWORDLONG initialCollectionStartProcessUsedBytes;
        FILETIME currentCollectionStartTime;
        DWORDLONG currentCollectionStartProcessUsedBytes;
        FILETIME concurrentMarkFinishTime;
        FILETIME disposeStartTime;
        FILETIME disposeEndTime;
        FILETIME externalWeakReferenceObjectResolveStartTime;
        FILETIME externalWeakReferenceObjectResolveEndTime;
        FILETIME currentCollectionEndTime;
        FILETIME lastCollectionEndTime;
        DWORD exhaustiveRepeatedCount;
    };
};
