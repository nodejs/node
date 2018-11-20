//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.IO;
using System.Runtime.InteropServices;

namespace DotZLib
{
	/// <summary>
	/// Implements a compressed <see cref="Stream"/>, in GZip (.gz) format.
	/// </summary>
	public class GZipStream : Stream, IDisposable
	{
        #region Dll Imports
        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl, CharSet=CharSet.Ansi)]
        private static extern IntPtr gzopen(string name, string mode);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int gzclose(IntPtr gzFile);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int gzwrite(IntPtr gzFile, int data, int length);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int gzread(IntPtr gzFile, int data, int length);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int gzgetc(IntPtr gzFile);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int gzputc(IntPtr gzFile, int c);

        #endregion

        #region Private data
        private IntPtr _gzFile;
        private bool _isDisposed = false;
        private bool _isWriting;
        #endregion

        #region Constructors
        /// <summary>
        /// Creates a new file as a writeable GZipStream
        /// </summary>
        /// <param name="fileName">The name of the compressed file to create</param>
        /// <param name="level">The compression level to use when adding data</param>
        /// <exception cref="ZLibException">If an error occurred in the internal zlib function</exception>
		public GZipStream(string fileName, CompressLevel level)
		{
            _isWriting = true;
            _gzFile = gzopen(fileName, String.Format("wb{0}", (int)level));
            if (_gzFile == IntPtr.Zero)
                throw new ZLibException(-1, "Could not open " + fileName);
		}

        /// <summary>
        /// Opens an existing file as a readable GZipStream
        /// </summary>
        /// <param name="fileName">The name of the file to open</param>
        /// <exception cref="ZLibException">If an error occurred in the internal zlib function</exception>
        public GZipStream(string fileName)
        {
            _isWriting = false;
            _gzFile = gzopen(fileName, "rb");
            if (_gzFile == IntPtr.Zero)
                throw new ZLibException(-1, "Could not open " + fileName);

        }
        #endregion

        #region Access properties
        /// <summary>
        /// Returns true of this stream can be read from, false otherwise
        /// </summary>
        public override bool CanRead
        {
            get
            {
                return !_isWriting;
            }
        }


        /// <summary>
        /// Returns false.
        /// </summary>
        public override bool CanSeek
        {
            get
            {
                return false;
            }
        }

        /// <summary>
        /// Returns true if this tsream is writeable, false otherwise
        /// </summary>
        public override bool CanWrite
        {
            get
            {
                return _isWriting;
            }
        }
        #endregion

        #region Destructor & IDispose stuff

        /// <summary>
        /// Destroys this instance
        /// </summary>
        ~GZipStream()
        {
            cleanUp(false);
        }

        /// <summary>
        /// Closes the external file handle
        /// </summary>
        public void Dispose()
        {
            cleanUp(true);
        }

        // Does the actual closing of the file handle.
        private void cleanUp(bool isDisposing)
        {
            if (!_isDisposed)
            {
                gzclose(_gzFile);
                _isDisposed = true;
            }
        }
        #endregion

        #region Basic reading and writing
        /// <summary>
        /// Attempts to read a number of bytes from the stream.
        /// </summary>
        /// <param name="buffer">The destination data buffer</param>
        /// <param name="offset">The index of the first destination byte in <c>buffer</c></param>
        /// <param name="count">The number of bytes requested</param>
        /// <returns>The number of bytes read</returns>
        /// <exception cref="ArgumentNullException">If <c>buffer</c> is null</exception>
        /// <exception cref="ArgumentOutOfRangeException">If <c>count</c> or <c>offset</c> are negative</exception>
        /// <exception cref="ArgumentException">If <c>offset</c>  + <c>count</c> is &gt; buffer.Length</exception>
        /// <exception cref="NotSupportedException">If this stream is not readable.</exception>
        /// <exception cref="ObjectDisposedException">If this stream has been disposed.</exception>
        public override int Read(byte[] buffer, int offset, int count)
        {
            if (!CanRead) throw new NotSupportedException();
            if (buffer == null) throw new ArgumentNullException();
            if (offset < 0 || count < 0) throw new ArgumentOutOfRangeException();
            if ((offset+count) > buffer.Length) throw new ArgumentException();
            if (_isDisposed) throw new ObjectDisposedException("GZipStream");

            GCHandle h = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            int result;
            try
            {
                result = gzread(_gzFile, h.AddrOfPinnedObject().ToInt32() + offset, count);
                if (result < 0)
                    throw new IOException();
            }
            finally
            {
                h.Free();
            }
            return result;
        }

        /// <summary>
        /// Attempts to read a single byte from the stream.
        /// </summary>
        /// <returns>The byte that was read, or -1 in case of error or End-Of-File</returns>
        public override int ReadByte()
        {
            if (!CanRead) throw new NotSupportedException();
            if (_isDisposed) throw new ObjectDisposedException("GZipStream");
            return gzgetc(_gzFile);
        }

        /// <summary>
        /// Writes a number of bytes to the stream
        /// </summary>
        /// <param name="buffer"></param>
        /// <param name="offset"></param>
        /// <param name="count"></param>
        /// <exception cref="ArgumentNullException">If <c>buffer</c> is null</exception>
        /// <exception cref="ArgumentOutOfRangeException">If <c>count</c> or <c>offset</c> are negative</exception>
        /// <exception cref="ArgumentException">If <c>offset</c>  + <c>count</c> is &gt; buffer.Length</exception>
        /// <exception cref="NotSupportedException">If this stream is not writeable.</exception>
        /// <exception cref="ObjectDisposedException">If this stream has been disposed.</exception>
        public override void Write(byte[] buffer, int offset, int count)
        {
            if (!CanWrite) throw new NotSupportedException();
            if (buffer == null) throw new ArgumentNullException();
            if (offset < 0 || count < 0) throw new ArgumentOutOfRangeException();
            if ((offset+count) > buffer.Length) throw new ArgumentException();
            if (_isDisposed) throw new ObjectDisposedException("GZipStream");

            GCHandle h = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                int result = gzwrite(_gzFile, h.AddrOfPinnedObject().ToInt32() + offset, count);
                if (result < 0)
                    throw new IOException();
            }
            finally
            {
                h.Free();
            }
        }

        /// <summary>
        /// Writes a single byte to the stream
        /// </summary>
        /// <param name="value">The byte to add to the stream.</param>
        /// <exception cref="NotSupportedException">If this stream is not writeable.</exception>
        /// <exception cref="ObjectDisposedException">If this stream has been disposed.</exception>
        public override void WriteByte(byte value)
        {
            if (!CanWrite) throw new NotSupportedException();
            if (_isDisposed) throw new ObjectDisposedException("GZipStream");

            int result = gzputc(_gzFile, (int)value);
            if (result < 0)
                throw new IOException();
        }
        #endregion

        #region Position & length stuff
        /// <summary>
        /// Not supported.
        /// </summary>
        /// <param name="value"></param>
        /// <exception cref="NotSupportedException">Always thrown</exception>
        public override void SetLength(long value)
        {
            throw new NotSupportedException();
        }

        /// <summary>
        ///  Not suppported.
        /// </summary>
        /// <param name="offset"></param>
        /// <param name="origin"></param>
        /// <returns></returns>
        /// <exception cref="NotSupportedException">Always thrown</exception>
        public override long Seek(long offset, SeekOrigin origin)
        {
            throw new NotSupportedException();
        }

        /// <summary>
        /// Flushes the <c>GZipStream</c>.
        /// </summary>
        /// <remarks>In this implementation, this method does nothing. This is because excessive
        /// flushing may degrade the achievable compression rates.</remarks>
        public override void Flush()
        {
            // left empty on purpose
        }

        /// <summary>
        /// Gets/sets the current position in the <c>GZipStream</c>. Not suppported.
        /// </summary>
        /// <remarks>In this implementation this property is not supported</remarks>
        /// <exception cref="NotSupportedException">Always thrown</exception>
        public override long Position
        {
            get
            {
                throw new NotSupportedException();
            }
            set
            {
                throw new NotSupportedException();
            }
        }

        /// <summary>
        /// Gets the size of the stream. Not suppported.
        /// </summary>
        /// <remarks>In this implementation this property is not supported</remarks>
        /// <exception cref="NotSupportedException">Always thrown</exception>
        public override long Length
        {
            get
            {
                throw new NotSupportedException();
            }
        }
        #endregion
    }
}
