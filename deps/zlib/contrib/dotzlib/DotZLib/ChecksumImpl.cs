//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.Runtime.InteropServices;
using System.Text;


namespace DotZLib
{
    #region ChecksumGeneratorBase
    /// <summary>
    /// Implements the common functionality needed for all <see cref="ChecksumGenerator"/>s
    /// </summary>
    /// <example></example>
    public abstract class ChecksumGeneratorBase : ChecksumGenerator
    {
        /// <summary>
        /// The value of the current checksum
        /// </summary>
        protected uint _current;

        /// <summary>
        /// Initializes a new instance of the checksum generator base - the current checksum is
        /// set to zero
        /// </summary>
        public ChecksumGeneratorBase()
        {
            _current = 0;
        }

        /// <summary>
        /// Initializes a new instance of the checksum generator basewith a specified value
        /// </summary>
        /// <param name="initialValue">The value to set the current checksum to</param>
        public ChecksumGeneratorBase(uint initialValue)
        {
            _current = initialValue;
        }

        /// <summary>
        /// Resets the current checksum to zero
        /// </summary>
        public void Reset() { _current = 0; }

        /// <summary>
        /// Gets the current checksum value
        /// </summary>
        public uint Value { get { return _current; } }

        /// <summary>
        /// Updates the current checksum with part of an array of bytes
        /// </summary>
        /// <param name="data">The data to update the checksum with</param>
        /// <param name="offset">Where in <c>data</c> to start updating</param>
        /// <param name="count">The number of bytes from <c>data</c> to use</param>
        /// <exception cref="ArgumentException">The sum of offset and count is larger than the length of <c>data</c></exception>
        /// <exception cref="NullReferenceException"><c>data</c> is a null reference</exception>
        /// <exception cref="ArgumentOutOfRangeException">Offset or count is negative.</exception>
        /// <remarks>All the other <c>Update</c> methods are implmeneted in terms of this one.
        /// This is therefore the only method a derived class has to implement</remarks>
        public abstract void Update(byte[] data, int offset, int count);

        /// <summary>
        /// Updates the current checksum with an array of bytes.
        /// </summary>
        /// <param name="data">The data to update the checksum with</param>
        public void Update(byte[] data)
        {
            Update(data, 0, data.Length);
        }

        /// <summary>
        /// Updates the current checksum with the data from a string
        /// </summary>
        /// <param name="data">The string to update the checksum with</param>
        /// <remarks>The characters in the string are converted by the UTF-8 encoding</remarks>
        public void Update(string data)
        {
			Update(Encoding.UTF8.GetBytes(data));
        }

        /// <summary>
        /// Updates the current checksum with the data from a string, using a specific encoding
        /// </summary>
        /// <param name="data">The string to update the checksum with</param>
        /// <param name="encoding">The encoding to use</param>
        public void Update(string data, Encoding encoding)
        {
            Update(encoding.GetBytes(data));
        }

    }
    #endregion

    #region CRC32
    /// <summary>
    /// Implements a CRC32 checksum generator
    /// </summary>
    public sealed class CRC32Checksum : ChecksumGeneratorBase
    {
        #region DLL imports

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern uint crc32(uint crc, int data, uint length);

        #endregion

        /// <summary>
        /// Initializes a new instance of the CRC32 checksum generator
        /// </summary>
        public CRC32Checksum() : base() {}

        /// <summary>
        /// Initializes a new instance of the CRC32 checksum generator with a specified value
        /// </summary>
        /// <param name="initialValue">The value to set the current checksum to</param>
        public CRC32Checksum(uint initialValue) : base(initialValue) {}

        /// <summary>
        /// Updates the current checksum with part of an array of bytes
        /// </summary>
        /// <param name="data">The data to update the checksum with</param>
        /// <param name="offset">Where in <c>data</c> to start updating</param>
        /// <param name="count">The number of bytes from <c>data</c> to use</param>
        /// <exception cref="ArgumentException">The sum of offset and count is larger than the length of <c>data</c></exception>
        /// <exception cref="NullReferenceException"><c>data</c> is a null reference</exception>
        /// <exception cref="ArgumentOutOfRangeException">Offset or count is negative.</exception>
        public override void Update(byte[] data, int offset, int count)
        {
            if (offset < 0 || count < 0) throw new ArgumentOutOfRangeException();
            if ((offset+count) > data.Length) throw new ArgumentException();
            GCHandle hData = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                _current = crc32(_current, hData.AddrOfPinnedObject().ToInt32()+offset, (uint)count);
            }
            finally
            {
                hData.Free();
            }
        }

    }
    #endregion

    #region Adler
    /// <summary>
    /// Implements a checksum generator that computes the Adler checksum on data
    /// </summary>
    public sealed class AdlerChecksum : ChecksumGeneratorBase
    {
        #region DLL imports

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern uint adler32(uint adler, int data, uint length);

        #endregion

        /// <summary>
        /// Initializes a new instance of the Adler checksum generator
        /// </summary>
        public AdlerChecksum() : base() {}

        /// <summary>
        /// Initializes a new instance of the Adler checksum generator with a specified value
        /// </summary>
        /// <param name="initialValue">The value to set the current checksum to</param>
        public AdlerChecksum(uint initialValue) : base(initialValue) {}

        /// <summary>
        /// Updates the current checksum with part of an array of bytes
        /// </summary>
        /// <param name="data">The data to update the checksum with</param>
        /// <param name="offset">Where in <c>data</c> to start updating</param>
        /// <param name="count">The number of bytes from <c>data</c> to use</param>
        /// <exception cref="ArgumentException">The sum of offset and count is larger than the length of <c>data</c></exception>
        /// <exception cref="NullReferenceException"><c>data</c> is a null reference</exception>
        /// <exception cref="ArgumentOutOfRangeException">Offset or count is negative.</exception>
        public override void Update(byte[] data, int offset, int count)
        {
            if (offset < 0 || count < 0) throw new ArgumentOutOfRangeException();
            if ((offset+count) > data.Length) throw new ArgumentException();
            GCHandle hData = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                _current = adler32(_current, hData.AddrOfPinnedObject().ToInt32()+offset, (uint)count);
            }
            finally
            {
                hData.Free();
            }
        }

    }
    #endregion

}