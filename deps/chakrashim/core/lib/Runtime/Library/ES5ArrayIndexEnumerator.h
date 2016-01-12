//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    //
    // An enumerator to enumerate ES5Array index property names as uint32 indices.
    //
    template <bool enumNonEnumerable = false>
    class ES5ArrayIndexEnumerator
    {
    public:
        typedef ES5Array ArrayType;

    private:
        ES5Array* m_array;                      // The ES5Array to enumerate on

        uint32 m_initialLength;                 // Initial length of the array, for snapshot
        uint32 m_index;                         // Current index
        uint32 m_dataIndex;                     // Current data item index
        uint32 m_descriptorIndex;               // Current descriptor item index
        IndexPropertyDescriptor* m_descriptor;  // Current descriptor associated with m_descriptorIndex
        void * m_descriptorValidationToken;
    public:
        ES5ArrayIndexEnumerator(ES5Array* array)
            : m_array(array)
        {
            Reset();
        }

        //
        // Reset to enumerate from beginning.
        //
        void Reset()
        {
            m_initialLength = m_array->GetLength();
            m_index = JavascriptArray::InvalidIndex;
            m_dataIndex = JavascriptArray::InvalidIndex;
            m_descriptorIndex = JavascriptArray::InvalidIndex;
            m_descriptor = NULL;
            m_descriptorValidationToken = nullptr;
        }

        //
        // Get the current index. Valid only when MoveNext() returns true.
        //
        uint32 GetIndex() const
        {
            return m_index;
        }

        //
        // Move to next index. If successful, use GetIndex() to get the index.
        //
        bool MoveNext(PropertyAttributes* attributes = nullptr)
        {
            while (true)
            {
                Assert(m_index == min(m_dataIndex, m_descriptorIndex));
                if (m_index == m_dataIndex)
                {
                    m_dataIndex = m_array->GetNextIndex(m_dataIndex);
                }
                if (m_index == m_descriptorIndex || !m_array->IsValidDescriptorToken(m_descriptorValidationToken))
                {
                    m_descriptorIndex = m_array->GetNextDescriptor(m_index, &m_descriptor, &m_descriptorValidationToken);
                }

                m_index = min(m_dataIndex, m_descriptorIndex);
                if (m_index >= m_initialLength) // End of array
                {
                    break;
                }

                if (enumNonEnumerable
                    || m_index < m_descriptorIndex
                    || (m_descriptor->Attributes & PropertyEnumerable))
                {
                    if (attributes != nullptr)
                    {
                        if (m_index < m_descriptorIndex)
                        {
                            *attributes = PropertyEnumerable;
                        }
                        else
                        {
                            *attributes = m_descriptor->Attributes;
                        }
                    }

                    return true;
                }
            }

            return false;
        }
    };
}
