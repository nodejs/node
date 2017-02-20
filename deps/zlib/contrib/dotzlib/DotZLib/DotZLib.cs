//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;


namespace DotZLib
{

    #region Internal types

    /// <summary>
    /// Defines constants for the various flush types used with zlib
    /// </summary>
    internal enum FlushTypes
    {
        None,  Partial,  Sync,  Full,  Finish,  Block
    }

    #region ZStream structure
    // internal mapping of the zlib zstream structure for marshalling
    [StructLayoutAttribute(LayoutKind.Sequential, Pack=4, Size=0, CharSet=CharSet.Ansi)]
    internal struct ZStream
    {
        public IntPtr next_in;
        public uint avail_in;
        public uint total_in;

        public IntPtr next_out;
        public uint avail_out;
        public uint total_out;

        [MarshalAs(UnmanagedType.LPStr)]
        string msg;
        uint state;

        uint zalloc;
        uint zfree;
        uint opaque;

        int data_type;
        public uint adler;
        uint reserved;
    }

    #endregion

    #endregion

    #region Public enums
    /// <summary>
    /// Defines constants for the available compression levels in zlib
    /// </summary>
    public enum CompressLevel : int
    {
        /// <summary>
        /// The default compression level with a reasonable compromise between compression and speed
        /// </summary>
        Default = -1,
        /// <summary>
        /// No compression at all. The data are passed straight through.
        /// </summary>
        None = 0,
        /// <summary>
        /// The maximum compression rate available.
        /// </summary>
        Best = 9,
        /// <summary>
        /// The fastest available compression level.
        /// </summary>
        Fastest = 1
    }
    #endregion

    #region Exception classes
    /// <summary>
    /// The exception that is thrown when an error occurs on the zlib dll
    /// </summary>
    public class ZLibException : ApplicationException
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ZLibException"/> class with a specified
        /// error message and error code
        /// </summary>
        /// <param name="errorCode">The zlib error code that caused the exception</param>
        /// <param name="msg">A message that (hopefully) describes the error</param>
        public ZLibException(int errorCode, string msg) : base(String.Format("ZLib error {0} {1}", errorCode, msg))
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ZLibException"/> class with a specified
        /// error code
        /// </summary>
        /// <param name="errorCode">The zlib error code that caused the exception</param>
        public ZLibException(int errorCode) : base(String.Format("ZLib error {0}", errorCode))
        {
        }
    }
    #endregion

    #region Interfaces

    /// <summary>
    /// Declares methods and properties that enables a running checksum to be calculated
    /// </summary>
    public interface ChecksumGenerator
    {
        /// <summary>
        /// Gets the current value of the checksum
        /// </summary>
        uint Value { get; }

        /// <summary>
        /// Clears the current checksum to 0
        /// </summary>
        void Reset();

        /// <summary>
        /// Updates the current checksum with an array of bytes
        /// </summary>
        /// <param name="data">The data to update the checksum with</param>
        void Update(byte[] data);

        /// <summary>
        /// Updates the current checksum with part of an array of bytes
        /// </summary>
        /// <param name="data">The data to update the checksum with</param>
        /// <param name="offset">Where in <c>data</c> to start updating</param>
        /// <param name="count">The number of bytes from <c>data</c> to use</param>
        /// <exception cref="ArgumentException">The sum of offset and count is larger than the length of <c>data</c></exception>
        /// <exception cref="ArgumentNullException"><c>data</c> is a null reference</exception>
        /// <exception cref="ArgumentOutOfRangeException">Offset or count is negative.</exception>
        void Update(byte[] data, int offset, int count);

        /// <summary>
        /// Updates the current checksum with the data from a string
        /// </summary>
        /// <param name="data">The string to update the checksum with</param>
        /// <remarks>The characters in the string are converted by the UTF-8 encoding</remarks>
        void Update(string data);

        /// <summary>
        /// Updates the current checksum with the data from a string, using a specific encoding
        /// </summary>
        /// <param name="data">The string to update the checksum with</param>
        /// <param name="encoding">The encoding to use</param>
        void Update(string data, Encoding encoding);
    }


    /// <summary>
    /// Represents the method that will be called from a codec when new data
    /// are available.
    /// </summary>
    /// <paramref name="data">The byte array containing the processed data</paramref>
    /// <paramref name="startIndex">The index of the first processed byte in <c>data</c></paramref>
    /// <paramref name="count">The number of processed bytes available</paramref>
    /// <remarks>On return from this method, the data may be overwritten, so grab it while you can.
    /// You cannot assume that startIndex will be zero.
    /// </remarks>
    public delegate void DataAvailableHandler(byte[] data, int startIndex, int count);

    /// <summary>
    /// Declares methods and events for implementing compressors/decompressors
    /// </summary>
    public interface Codec
    {
        /// <summary>
        /// Occurs when more processed data are available.
        /// </summary>
        event DataAvailableHandler DataAvailable;

        /// <summary>
        /// Adds more data to the codec to be processed.
        /// </summary>
        /// <param name="data">Byte array containing the data to be added to the codec</param>
        /// <remarks>Adding data may, or may not, raise the <c>DataAvailable</c> event</remarks>
        void Add(byte[] data);

        /// <summary>
        /// Adds more data to the codec to be processed.
        /// </summary>
        /// <param name="data">Byte array containing the data to be added to the codec</param>
        /// <param name="offset">The index of the first byte to add from <c>data</c></param>
        /// <param name="count">The number of bytes to add</param>
        /// <remarks>Adding data may, or may not, raise the <c>DataAvailable</c> event</remarks>
        void Add(byte[] data, int offset, int count);

        /// <summary>
        /// Finishes up any pending data that needs to be processed and handled.
        /// </summary>
        void Finish();

        /// <summary>
        /// Gets the checksum of the data that has been added so far
        /// </summary>
        uint Checksum { get; }


    }

    #endregion

    #region Classes
    /// <summary>
    /// Encapsulates general information about the ZLib library
    /// </summary>
    public class Info
    {
        #region DLL imports
        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern uint zlibCompileFlags();

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern string zlibVersion();
        #endregion

        #region Private stuff
        private uint _flags;

        // helper function that unpacks a bitsize mask
        private static int bitSize(uint bits)
        {
            switch (bits)
            {
                case 0: return 16;
                case 1: return 32;
                case 2: return 64;
            }
            return -1;
        }
        #endregion

        /// <summary>
        /// Constructs an instance of the <c>Info</c> class.
        /// </summary>
        public Info()
        {
            _flags = zlibCompileFlags();
        }

        /// <summary>
        /// True if the library is compiled with debug info
        /// </summary>
        public bool HasDebugInfo { get { return 0 != (_flags & 0x100); } }

        /// <summary>
        /// True if the library is compiled with assembly optimizations
        /// </summary>
        public bool UsesAssemblyCode { get { return 0 != (_flags & 0x200); } }

        /// <summary>
        /// Gets the size of the unsigned int that was compiled into Zlib
        /// </summary>
        public int SizeOfUInt { get { return bitSize(_flags & 3); } }

        /// <summary>
        /// Gets the size of the unsigned long that was compiled into Zlib
        /// </summary>
        public int SizeOfULong { get { return bitSize((_flags >> 2) & 3); } }

        /// <summary>
        /// Gets the size of the pointers that were compiled into Zlib
        /// </summary>
        public int SizeOfPointer { get { return bitSize((_flags >> 4) & 3); } }

        /// <summary>
        /// Gets the size of the z_off_t type that was compiled into Zlib
        /// </summary>
        public int SizeOfOffset { get { return bitSize((_flags >> 6) & 3); } }

        /// <summary>
        /// Gets the version of ZLib as a string, e.g. "1.2.1"
        /// </summary>
        public static string Version { get { return zlibVersion(); } }
    }

    #endregion

}
