//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.Runtime.InteropServices;

namespace DotZLib
{
	/// <summary>
	/// Implements the common functionality needed for all <see cref="Codec"/>s
	/// </summary>
	public abstract class CodecBase : Codec, IDisposable
	{

        #region Data members

        /// <summary>
        /// Instance of the internal zlib buffer structure that is
        /// passed to all functions in the zlib dll
        /// </summary>
        internal ZStream _ztream = new ZStream();

        /// <summary>
        /// True if the object instance has been disposed, false otherwise
        /// </summary>
        protected bool _isDisposed = false;

        /// <summary>
        /// The size of the internal buffers
        /// </summary>
        protected const int kBufferSize = 16384;

        private byte[] _outBuffer = new byte[kBufferSize];
        private byte[] _inBuffer = new byte[kBufferSize];

        private GCHandle _hInput;
        private GCHandle _hOutput;

        private uint _checksum = 0;

        #endregion

        /// <summary>
        /// Initializes a new instance of the <c>CodeBase</c> class.
        /// </summary>
		public CodecBase()
		{
            try
            {
                _hInput = GCHandle.Alloc(_inBuffer, GCHandleType.Pinned);
                _hOutput = GCHandle.Alloc(_outBuffer, GCHandleType.Pinned);
            }
            catch (Exception)
            {
                CleanUp(false);
                throw;
            }
        }


        #region Codec Members

        /// <summary>
        /// Occurs when more processed data are available.
        /// </summary>
        public event DataAvailableHandler DataAvailable;

        /// <summary>
        /// Fires the <see cref="DataAvailable"/> event
        /// </summary>
        protected void OnDataAvailable()
        {
            if (_ztream.total_out > 0)
            {
                if (DataAvailable != null)
                    DataAvailable( _outBuffer, 0, (int)_ztream.total_out);
                resetOutput();
            }
        }

        /// <summary>
        /// Adds more data to the codec to be processed.
        /// </summary>
        /// <param name="data">Byte array containing the data to be added to the codec</param>
        /// <remarks>Adding data may, or may not, raise the <c>DataAvailable</c> event</remarks>
        public void Add(byte[] data)
        {
            Add(data,0,data.Length);
        }

        /// <summary>
        /// Adds more data to the codec to be processed.
        /// </summary>
        /// <param name="data">Byte array containing the data to be added to the codec</param>
        /// <param name="offset">The index of the first byte to add from <c>data</c></param>
        /// <param name="count">The number of bytes to add</param>
        /// <remarks>Adding data may, or may not, raise the <c>DataAvailable</c> event</remarks>
        /// <remarks>This must be implemented by a derived class</remarks>
        public abstract void Add(byte[] data, int offset, int count);

        /// <summary>
        /// Finishes up any pending data that needs to be processed and handled.
        /// </summary>
        /// <remarks>This must be implemented by a derived class</remarks>
        public abstract void Finish();

        /// <summary>
        /// Gets the checksum of the data that has been added so far
        /// </summary>
        public uint Checksum { get { return _checksum; } }

        #endregion

        #region Destructor & IDisposable stuff

        /// <summary>
        /// Destroys this instance
        /// </summary>
        ~CodecBase()
        {
            CleanUp(false);
        }

        /// <summary>
        /// Releases any unmanaged resources and calls the <see cref="CleanUp()"/> method of the derived class
        /// </summary>
        public void Dispose()
        {
            CleanUp(true);
        }

        /// <summary>
        /// Performs any codec specific cleanup
        /// </summary>
        /// <remarks>This must be implemented by a derived class</remarks>
        protected abstract void CleanUp();

        // performs the release of the handles and calls the dereived CleanUp()
        private void CleanUp(bool isDisposing)
        {
            if (!_isDisposed)
            {
                CleanUp();
                if (_hInput.IsAllocated)
                    _hInput.Free();
                if (_hOutput.IsAllocated)
                    _hOutput.Free();

                _isDisposed = true;
            }
        }


        #endregion

        #region Helper methods

        /// <summary>
        /// Copies a number of bytes to the internal codec buffer - ready for proccesing
        /// </summary>
        /// <param name="data">The byte array that contains the data to copy</param>
        /// <param name="startIndex">The index of the first byte to copy</param>
        /// <param name="count">The number of bytes to copy from <c>data</c></param>
        protected void copyInput(byte[] data, int startIndex, int count)
        {
            Array.Copy(data, startIndex, _inBuffer,0, count);
            _ztream.next_in = _hInput.AddrOfPinnedObject();
            _ztream.total_in = 0;
            _ztream.avail_in = (uint)count;

        }

        /// <summary>
        /// Resets the internal output buffers to a known state - ready for processing
        /// </summary>
        protected void resetOutput()
        {
            _ztream.total_out = 0;
            _ztream.avail_out = kBufferSize;
            _ztream.next_out = _hOutput.AddrOfPinnedObject();
        }

        /// <summary>
        /// Updates the running checksum property
        /// </summary>
        /// <param name="newSum">The new checksum value</param>
        protected void setChecksum(uint newSum)
        {
            _checksum = newSum;
        }
        #endregion

    }
}
