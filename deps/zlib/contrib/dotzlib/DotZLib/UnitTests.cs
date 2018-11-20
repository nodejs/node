//
// © Copyright Henrik Ravn 2004
//
// Use, modification and distribution are subject to the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

using System;
using System.Collections;
using System.IO;

// uncomment the define below to include unit tests
//#define nunit
#if nunit
using NUnit.Framework;

// Unit tests for the DotZLib class library
// ----------------------------------------
//
// Use this with NUnit 2 from http://www.nunit.org
//

namespace DotZLibTests
{
    using DotZLib;

    // helper methods
    internal class Utils
    {
        public static bool byteArrEqual( byte[] lhs, byte[] rhs )
        {
            if (lhs.Length != rhs.Length)
                return false;
            for (int i = lhs.Length-1; i >= 0; --i)
                if (lhs[i] != rhs[i])
                    return false;
            return true;
        }

    }


    [TestFixture]
    public class CircBufferTests
    {
        #region Circular buffer tests
        [Test]
        public void SinglePutGet()
        {
            CircularBuffer buf = new CircularBuffer(10);
            Assert.AreEqual( 0, buf.Size );
            Assert.AreEqual( -1, buf.Get() );

            Assert.IsTrue(buf.Put( 1 ));
            Assert.AreEqual( 1, buf.Size );
            Assert.AreEqual( 1, buf.Get() );
            Assert.AreEqual( 0, buf.Size );
            Assert.AreEqual( -1, buf.Get() );
        }

        [Test]
        public void BlockPutGet()
        {
            CircularBuffer buf = new CircularBuffer(10);
            byte[] arr = {1,2,3,4,5,6,7,8,9,10};
            Assert.AreEqual( 10, buf.Put(arr,0,10) );
            Assert.AreEqual( 10, buf.Size );
            Assert.IsFalse( buf.Put(11) );
            Assert.AreEqual( 1, buf.Get() );
            Assert.IsTrue( buf.Put(11) );

            byte[] arr2 = (byte[])arr.Clone();
            Assert.AreEqual( 9, buf.Get(arr2,1,9) );
            Assert.IsTrue( Utils.byteArrEqual(arr,arr2) );
        }

        #endregion
    }

    [TestFixture]
    public class ChecksumTests
    {
        #region CRC32 Tests
        [Test]
        public void CRC32_Null()
        {
            CRC32Checksum crc32 = new CRC32Checksum();
            Assert.AreEqual( 0, crc32.Value );

            crc32 = new CRC32Checksum(1);
            Assert.AreEqual( 1, crc32.Value );

            crc32 = new CRC32Checksum(556);
            Assert.AreEqual( 556, crc32.Value );
        }

        [Test]
        public void CRC32_Data()
        {
            CRC32Checksum crc32 = new CRC32Checksum();
            byte[] data = { 1,2,3,4,5,6,7 };
            crc32.Update(data);
            Assert.AreEqual( 0x70e46888, crc32.Value  );

            crc32 = new CRC32Checksum();
            crc32.Update("penguin");
            Assert.AreEqual( 0x0e5c1a120, crc32.Value );

            crc32 = new CRC32Checksum(1);
            crc32.Update("penguin");
            Assert.AreEqual(0x43b6aa94, crc32.Value);

        }
        #endregion

        #region Adler tests

        [Test]
        public void Adler_Null()
        {
            AdlerChecksum adler = new AdlerChecksum();
            Assert.AreEqual(0, adler.Value);

            adler = new AdlerChecksum(1);
            Assert.AreEqual( 1, adler.Value );

            adler = new AdlerChecksum(556);
            Assert.AreEqual( 556, adler.Value );
        }

        [Test]
        public void Adler_Data()
        {
            AdlerChecksum adler = new AdlerChecksum(1);
            byte[] data = { 1,2,3,4,5,6,7 };
            adler.Update(data);
            Assert.AreEqual( 0x5b001d, adler.Value  );

            adler = new AdlerChecksum();
            adler.Update("penguin");
            Assert.AreEqual(0x0bcf02f6, adler.Value );

            adler = new AdlerChecksum(1);
            adler.Update("penguin");
            Assert.AreEqual(0x0bd602f7, adler.Value);

        }
        #endregion
    }

    [TestFixture]
    public class InfoTests
    {
        #region Info tests
        [Test]
        public void Info_Version()
        {
            Info info = new Info();
            Assert.AreEqual("1.2.8", Info.Version);
            Assert.AreEqual(32, info.SizeOfUInt);
            Assert.AreEqual(32, info.SizeOfULong);
            Assert.AreEqual(32, info.SizeOfPointer);
            Assert.AreEqual(32, info.SizeOfOffset);
        }
        #endregion
    }

    [TestFixture]
    public class DeflateInflateTests
    {
        #region Deflate tests
        [Test]
        public void Deflate_Init()
        {
            using (Deflater def = new Deflater(CompressLevel.Default))
            {
            }
        }

        private ArrayList compressedData = new ArrayList();
        private uint adler1;

        private ArrayList uncompressedData = new ArrayList();
        private uint adler2;

        public void CDataAvail(byte[] data, int startIndex, int count)
        {
            for (int i = 0; i < count; ++i)
                compressedData.Add(data[i+startIndex]);
        }

        [Test]
        public void Deflate_Compress()
        {
            compressedData.Clear();

            byte[] testData = new byte[35000];
            for (int i = 0; i < testData.Length; ++i)
                testData[i] = 5;

            using (Deflater def = new Deflater((CompressLevel)5))
            {
                def.DataAvailable += new DataAvailableHandler(CDataAvail);
                def.Add(testData);
                def.Finish();
                adler1 = def.Checksum;
            }
        }
        #endregion

        #region Inflate tests
        [Test]
        public void Inflate_Init()
        {
            using (Inflater inf = new Inflater())
            {
            }
        }

        private void DDataAvail(byte[] data, int startIndex, int count)
        {
            for (int i = 0; i < count; ++i)
                uncompressedData.Add(data[i+startIndex]);
        }

        [Test]
        public void Inflate_Expand()
        {
            uncompressedData.Clear();

            using (Inflater inf = new Inflater())
            {
                inf.DataAvailable += new DataAvailableHandler(DDataAvail);
                inf.Add((byte[])compressedData.ToArray(typeof(byte)));
                inf.Finish();
                adler2 = inf.Checksum;
            }
            Assert.AreEqual( adler1, adler2 );
        }
        #endregion
    }

    [TestFixture]
    public class GZipStreamTests
    {
        #region GZipStream test
        [Test]
        public void GZipStream_WriteRead()
        {
            using (GZipStream gzOut = new GZipStream("gzstream.gz", CompressLevel.Best))
            {
                BinaryWriter writer = new BinaryWriter(gzOut);
                writer.Write("hi there");
                writer.Write(Math.PI);
                writer.Write(42);
            }

            using (GZipStream gzIn = new GZipStream("gzstream.gz"))
            {
                BinaryReader reader = new BinaryReader(gzIn);
                string s = reader.ReadString();
                Assert.AreEqual("hi there",s);
                double d = reader.ReadDouble();
                Assert.AreEqual(Math.PI, d);
                int i = reader.ReadInt32();
                Assert.AreEqual(42,i);
            }

        }
        #endregion
	}
}

#endif
