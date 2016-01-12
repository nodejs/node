//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct StatementReader
    {
    private:
        const byte* m_startLocation;
        SmallSpanSequence* m_statementMap;
        SmallSpanSequenceIter m_statementMapIter;

        FunctionBody::StatementMapList* m_fullstatementMap;
        const byte* m_nextStatementBoundary;
        int m_statementIndex;
        bool m_startOfStatement;

    public:
        void Create(FunctionBody* functionRead, uint startOffset = 0);
        void Create(FunctionBody* functionRead, uint startOffset, bool useOriginalByteCode);

        inline bool AtStatementBoundary(ByteCodeReader * reader) { return m_nextStatementBoundary == reader->GetIP(); }
        inline uint32 MoveNextStatementBoundary();
        inline uint32 GetStatementIndex() const { return m_statementIndex; }
    };
} // namespace Js
