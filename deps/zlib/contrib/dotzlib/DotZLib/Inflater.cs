//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace DotZLib
{

    /// <summary>
    /// Implements a data decompressor, using the inflate algorithm in the ZLib dll
    /// </summary>
    public class Inflater : CodecBase
	{
        #region Dll imports
        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl, CharSet=CharSet.Ansi)]
        private static extern int inflateInit_(ref ZStream sz, string vs, int size);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int inflate(ref ZStream sz, int flush);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int inflateReset(ref ZStream sz);

        [DllImport("ZLIB1.dll", CallingConvention=CallingConvention.Cdecl)]
        private static extern int inflateEnd(ref ZStream sz);
        #endregion

        /// <summary>
        /// Constructs an new instance of the <c>Inflater</c>
        /// </summary>
        public Inflater() : base()
		{
            int retval = inflateInit_(ref _ztream, Info.Version, Marshal.SizeOf(_ztream));
            if (retval != 0)
                throw new ZLibException(retval, "Could not initialize inflater");

            resetOutput();
        }


        /// <summary>
        /// Adds more data to the codec to be processed.
        /// </summary>
        /// <param name="data">Byte array containing the data to be added to the codec</param>
        /// <param name="offset">The index of the first byte to add from <c>data</c></param>
        /// <param name="count">The number of bytes to add</param>
        /// <remarks>Adding data may, or may not, raise the <c>DataAvailable</c> event</remarks>
        public override void Add(byte[] data, int offset, int count)
        {
            if (data == null) throw new ArgumentNullException();
            if (offset < 0 || count < 0) throw new ArgumentOutOfRangeException();
            if ((offset+count) > data.Length) throw new ArgumentException();

            int total = count;
            int inputIndex = offset;
            int err = 0;

            while (err >= 0 && inputIndex < total)
            {
                copyInput(data, inputIndex, Math.Min(total - inputIndex, kBufferSize));
                err = inflate(ref _ztream, (int)FlushTypes.None);
                if (err == 0)
                    while (_ztream.avail_out == 0)
                    {
                        OnDataAvailable();
                        err = inflate(ref _ztream, (int)FlushTypes.None);
                    }

                inputIndex += (int)_ztream.total_in;
            }
            setChecksum( _ztream.adler );
        }


        /// <summary>
        /// Finishes up any pending data that needs to be processed and handled.
        /// </summary>
        public override void Finish()
        {
            int err;
            do
            {
                err = inflate(ref _ztream, (int)FlushTypes.Finish);
                OnDataAvailable();
            }
            while (err == 0);
            setChecksum( _ztream.adler );
            inflateReset(ref _ztream);
            resetOutput();
        }

        /// <summary>
        /// Closes the internal zlib inflate stream
        /// </summary>
        protected override void CleanUp() { inflateEnd(ref _ztream); }


	}
}
