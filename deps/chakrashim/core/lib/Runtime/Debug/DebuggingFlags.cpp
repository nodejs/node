//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

//static
DebuggingFlags::DebuggingFlags() :
    m_forceInterpreter(false),
    m_isIgnoringException(false),
    m_byteCodeOffsetAfterIgnoreException(InvalidByteCodeOffset),
    m_funcNumberAfterIgnoreException(InvalidFuncNumber),
    m_isBuiltInWrapperPresent(false)
{
    // In Lowerer::LowerBailForDebugger we rely on the following:
    CompileAssert(offsetof(DebuggingFlags, m_isIgnoringException) == offsetof(DebuggingFlags, m_forceInterpreter) + 1);
}

bool DebuggingFlags::GetForceInterpreter() const
{
    return this->m_forceInterpreter;
}

void DebuggingFlags::SetForceInterpreter(bool value)
{
    this->m_forceInterpreter = value;
}

//static
size_t DebuggingFlags::GetForceInterpreterOffset()
{
    return offsetof(DebuggingFlags, m_forceInterpreter);
}

int DebuggingFlags::GetByteCodeOffsetAfterIgnoreException() const
{
    return this->m_byteCodeOffsetAfterIgnoreException;
}

uint DebuggingFlags::GetFuncNumberAfterIgnoreException() const
{
    return this->m_funcNumberAfterIgnoreException;
}

void DebuggingFlags::SetByteCodeOffsetAfterIgnoreException(int offset)
{
    this->m_byteCodeOffsetAfterIgnoreException = offset;
    this->m_isIgnoringException = offset != InvalidByteCodeOffset;
}

void DebuggingFlags::SetByteCodeOffsetAndFuncAfterIgnoreException(int offset, uint functionNumber)
{
    this->SetByteCodeOffsetAfterIgnoreException(offset);
    this->m_funcNumberAfterIgnoreException = functionNumber;
}

void DebuggingFlags::ResetByteCodeOffsetAndFuncAfterIgnoreException()
{
    this->SetByteCodeOffsetAfterIgnoreException(InvalidByteCodeOffset);
    this->m_funcNumberAfterIgnoreException = InvalidFuncNumber;
}

size_t DebuggingFlags::GetByteCodeOffsetAfterIgnoreExceptionOffset() const
{
    return offsetof(DebuggingFlags, m_byteCodeOffsetAfterIgnoreException);
}

bool DebuggingFlags::IsBuiltInWrapperPresent() const
{
    return m_isBuiltInWrapperPresent;
}

void DebuggingFlags::SetIsBuiltInWrapperPresent(bool value /* = true */)
{
    m_isBuiltInWrapperPresent = value;
}
