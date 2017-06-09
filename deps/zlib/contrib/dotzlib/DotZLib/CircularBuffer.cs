//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.Diagnostics;

namespace DotZLib
{

	/// <summary>
	/// This class implements a circular buffer
	/// </summary>
	internal class CircularBuffer
	{
        #region Private data
        private int _capacity;
        private int _head;
        private int _tail;
        private int _size;
        private byte[] _buffer;
        #endregion

        public CircularBuffer(int capacity)
        {
            Debug.Assert( capacity > 0 );
            _buffer = new byte[capacity];
            _capacity = capacity;
            _head = 0;
            _tail = 0;
            _size = 0;
        }

        public int Size { get { return _size; } }

        public int Put(byte[] source, int offset, int count)
        {
            Debug.Assert( count > 0 );
            int trueCount = Math.Min(count, _capacity - Size);
            for (int i = 0; i < trueCount; ++i)
                _buffer[(_tail+i) % _capacity] = source[offset+i];
            _tail += trueCount;
            _tail %= _capacity;
            _size += trueCount;
            return trueCount;
        }

        public bool Put(byte b)
        {
            if (Size == _capacity) // no room
                return false;
            _buffer[_tail++] = b;
            _tail %= _capacity;
            ++_size;
            return true;
        }

        public int Get(byte[] destination, int offset, int count)
        {
            int trueCount = Math.Min(count,Size);
            for (int i = 0; i < trueCount; ++i)
                destination[offset + i] = _buffer[(_head+i) % _capacity];
            _head += trueCount;
            _head %= _capacity;
            _size -= trueCount;
            return trueCount;
        }

        public int Get()
        {
            if (Size == 0)
                return -1;

            int result = (int)_buffer[_head++ % _capacity];
            --_size;
            return result;
        }

    }
}
