------------------------------------------------------------------------------
--                      ZLib for Ada thick binding.                         --
--                                                                          --
--              Copyright (C) 2002-2004 Dmitriy Anisimkov                   --
--                                                                          --
--  This library is free software; you can redistribute it and/or modify    --
--  it under the terms of the GNU General Public License as published by    --
--  the Free Software Foundation; either version 2 of the License, or (at   --
--  your option) any later version.                                         --
--                                                                          --
--  This library is distributed in the hope that it will be useful, but     --
--  WITHOUT ANY WARRANTY; without even the implied warranty of              --
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       --
--  General Public License for more details.                                --
--                                                                          --
--  You should have received a copy of the GNU General Public License       --
--  along with this library; if not, write to the Free Software Foundation, --
--  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.          --
--                                                                          --
--  As a special exception, if other files instantiate generics from this   --
--  unit, or you link this unit with other files to produce an executable,  --
--  this  unit  does not  by itself cause  the resulting executable to be   --
--  covered by the GNU General Public License. This exception does not      --
--  however invalidate any other reasons why the executable file  might be  --
--  covered by the  GNU Public License.                                     --
------------------------------------------------------------------------------

--  $Id: zlib.ads,v 1.26 2004/09/06 06:53:19 vagul Exp $

with Ada.Streams;

with Interfaces;

package ZLib is

   ZLib_Error   : exception;
   Status_Error : exception;

   type Compression_Level is new Integer range -1 .. 9;

   type Flush_Mode is private;

   type Compression_Method is private;

   type Window_Bits_Type is new Integer range 8 .. 15;

   type Memory_Level_Type is new Integer range 1 .. 9;

   type Unsigned_32 is new Interfaces.Unsigned_32;

   type Strategy_Type is private;

   type Header_Type is (None, Auto, Default, GZip);
   --  Header type usage have a some limitation for inflate.
   --  See comment for Inflate_Init.

   subtype Count is Ada.Streams.Stream_Element_Count;

   Default_Memory_Level : constant Memory_Level_Type := 8;
   Default_Window_Bits  : constant Window_Bits_Type  := 15;

   ----------------------------------
   -- Compression method constants --
   ----------------------------------

   Deflated : constant Compression_Method;
   --  Only one method allowed in this ZLib version

   ---------------------------------
   -- Compression level constants --
   ---------------------------------

   No_Compression      : constant Compression_Level := 0;
   Best_Speed          : constant Compression_Level := 1;
   Best_Compression    : constant Compression_Level := 9;
   Default_Compression : constant Compression_Level := -1;

   --------------------------
   -- Flush mode constants --
   --------------------------

   No_Flush      : constant Flush_Mode;
   --  Regular way for compression, no flush

   Partial_Flush : constant Flush_Mode;
   --  Will be removed, use Z_SYNC_FLUSH instead

   Sync_Flush    : constant Flush_Mode;
   --  All pending output is flushed to the output buffer and the output
   --  is aligned on a byte boundary, so that the decompressor can get all
   --  input data available so far. (In particular avail_in is zero after the
   --  call if enough output space has been provided  before the call.)
   --  Flushing may degrade compression for some compression algorithms and so
   --  it should be used only when necessary.

   Block_Flush   : constant Flush_Mode;
   --  Z_BLOCK requests that inflate() stop
   --  if and when it get to the next deflate block boundary. When decoding the
   --  zlib or gzip format, this will cause inflate() to return immediately
   --  after the header and before the first block. When doing a raw inflate,
   --  inflate() will go ahead and process the first block, and will return
   --  when it gets to the end of that block, or when it runs out of data.

   Full_Flush    : constant Flush_Mode;
   --  All output is flushed as with SYNC_FLUSH, and the compression state
   --  is reset so that decompression can restart from this point if previous
   --  compressed data has been damaged or if random access is desired. Using
   --  Full_Flush too often can seriously degrade the compression.

   Finish        : constant Flush_Mode;
   --  Just for tell the compressor that input data is complete.

   ------------------------------------
   -- Compression strategy constants --
   ------------------------------------

   --  RLE stategy could be used only in version 1.2.0 and later.

   Filtered         : constant Strategy_Type;
   Huffman_Only     : constant Strategy_Type;
   RLE              : constant Strategy_Type;
   Default_Strategy : constant Strategy_Type;

   Default_Buffer_Size : constant := 4096;

   type Filter_Type is tagged limited private;
   --  The filter is for compression and for decompression.
   --  The usage of the type is depend of its initialization.

   function Version return String;
   pragma Inline (Version);
   --  Return string representation of the ZLib version.

   procedure Deflate_Init
     (Filter       : in out Filter_Type;
      Level        : in     Compression_Level  := Default_Compression;
      Strategy     : in     Strategy_Type      := Default_Strategy;
      Method       : in     Compression_Method := Deflated;
      Window_Bits  : in     Window_Bits_Type   := Default_Window_Bits;
      Memory_Level : in     Memory_Level_Type  := Default_Memory_Level;
      Header       : in     Header_Type        := Default);
   --  Compressor initialization.
   --  When Header parameter is Auto or Default, then default zlib header
   --  would be provided for compressed data.
   --  When Header is GZip, then gzip header would be set instead of
   --  default header.
   --  When Header is None, no header would be set for compressed data.

   procedure Inflate_Init
     (Filter      : in out Filter_Type;
      Window_Bits : in     Window_Bits_Type := Default_Window_Bits;
      Header      : in     Header_Type      := Default);
   --  Decompressor initialization.
   --  Default header type mean that ZLib default header is expecting in the
   --  input compressed stream.
   --  Header type None mean that no header is expecting in the input stream.
   --  GZip header type mean that GZip header is expecting in the
   --  input compressed stream.
   --  Auto header type mean that header type (GZip or Native) would be
   --  detected automatically in the input stream.
   --  Note that header types parameter values None, GZip and Auto are
   --  supported for inflate routine only in ZLib versions 1.2.0.2 and later.
   --  Deflate_Init is supporting all header types.

   function Is_Open (Filter : in Filter_Type) return Boolean;
   pragma Inline (Is_Open);
   --  Is the filter opened for compression or decompression.

   procedure Close
     (Filter       : in out Filter_Type;
      Ignore_Error : in     Boolean := False);
   --  Closing the compression or decompressor.
   --  If stream is closing before the complete and Ignore_Error is False,
   --  The exception would be raised.

   generic
      with procedure Data_In
        (Item : out Ada.Streams.Stream_Element_Array;
         Last : out Ada.Streams.Stream_Element_Offset);
      with procedure Data_Out
        (Item : in Ada.Streams.Stream_Element_Array);
   procedure Generic_Translate
     (Filter          : in out Filter_Type;
      In_Buffer_Size  : in     Integer := Default_Buffer_Size;
      Out_Buffer_Size : in     Integer := Default_Buffer_Size);
   --  Compress/decompress data fetch from Data_In routine and pass the result
   --  to the Data_Out routine. User should provide Data_In and Data_Out
   --  for compression/decompression data flow.
   --  Compression or decompression depend on Filter initialization.

   function Total_In (Filter : in Filter_Type) return Count;
   pragma Inline (Total_In);
   --  Returns total number of input bytes read so far

   function Total_Out (Filter : in Filter_Type) return Count;
   pragma Inline (Total_Out);
   --  Returns total number of bytes output so far

   function CRC32
     (CRC    : in Unsigned_32;
      Data   : in Ada.Streams.Stream_Element_Array)
      return Unsigned_32;
   pragma Inline (CRC32);
   --  Compute CRC32, it could be necessary for make gzip format

   procedure CRC32
     (CRC  : in out Unsigned_32;
      Data : in     Ada.Streams.Stream_Element_Array);
   pragma Inline (CRC32);
   --  Compute CRC32, it could be necessary for make gzip format

   -------------------------------------------------
   --  Below is more complex low level routines.  --
   -------------------------------------------------

   procedure Translate
     (Filter    : in out Filter_Type;
      In_Data   : in     Ada.Streams.Stream_Element_Array;
      In_Last   :    out Ada.Streams.Stream_Element_Offset;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode);
   --  Compress/decompress the In_Data buffer and place the result into
   --  Out_Data. In_Last is the index of last element from In_Data accepted by
   --  the Filter. Out_Last is the last element of the received data from
   --  Filter. To tell the filter that incoming data are complete put the
   --  Flush parameter to Finish.

   function Stream_End (Filter : in Filter_Type) return Boolean;
   pragma Inline (Stream_End);
   --  Return the true when the stream is complete.

   procedure Flush
     (Filter    : in out Filter_Type;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode);
   pragma Inline (Flush);
   --  Flushing the data from the compressor.

   generic
      with procedure Write
        (Item : in Ada.Streams.Stream_Element_Array);
      --  User should provide this routine for accept
      --  compressed/decompressed data.

      Buffer_Size : in Ada.Streams.Stream_Element_Offset
         := Default_Buffer_Size;
      --  Buffer size for Write user routine.

   procedure Write
     (Filter  : in out Filter_Type;
      Item    : in     Ada.Streams.Stream_Element_Array;
      Flush   : in     Flush_Mode := No_Flush);
   --  Compress/Decompress data from Item to the generic parameter procedure
   --  Write. Output buffer size could be set in Buffer_Size generic parameter.

   generic
      with procedure Read
        (Item : out Ada.Streams.Stream_Element_Array;
         Last : out Ada.Streams.Stream_Element_Offset);
      --  User should provide data for compression/decompression
      --  thru this routine.

      Buffer : in out Ada.Streams.Stream_Element_Array;
      --  Buffer for keep remaining data from the previous
      --  back read.

      Rest_First, Rest_Last : in out Ada.Streams.Stream_Element_Offset;
      --  Rest_First have to be initialized to Buffer'Last + 1
      --  Rest_Last have to be initialized to Buffer'Last
      --  before usage.

      Allow_Read_Some : in Boolean := False;
      --  Is it allowed to return Last < Item'Last before end of data.

   procedure Read
     (Filter : in out Filter_Type;
      Item   :    out Ada.Streams.Stream_Element_Array;
      Last   :    out Ada.Streams.Stream_Element_Offset;
      Flush  : in     Flush_Mode := No_Flush);
   --  Compress/Decompress data from generic parameter procedure Read to the
   --  Item. User should provide Buffer and initialized Rest_First, Rest_Last
   --  indicators. If Allow_Read_Some is True, Read routines could return
   --  Last < Item'Last only at end of stream.

private

   use Ada.Streams;

   pragma Assert (Ada.Streams.Stream_Element'Size    =    8);
   pragma Assert (Ada.Streams.Stream_Element'Modulus = 2**8);

   type Flush_Mode is new Integer range 0 .. 5;

   type Compression_Method is new Integer range 8 .. 8;

   type Strategy_Type is new Integer range 0 .. 3;

   No_Flush      : constant Flush_Mode := 0;
   Partial_Flush : constant Flush_Mode := 1;
   Sync_Flush    : constant Flush_Mode := 2;
   Full_Flush    : constant Flush_Mode := 3;
   Finish        : constant Flush_Mode := 4;
   Block_Flush   : constant Flush_Mode := 5;

   Filtered         : constant Strategy_Type := 1;
   Huffman_Only     : constant Strategy_Type := 2;
   RLE              : constant Strategy_Type := 3;
   Default_Strategy : constant Strategy_Type := 0;

   Deflated : constant Compression_Method := 8;

   type Z_Stream;

   type Z_Stream_Access is access all Z_Stream;

   type Filter_Type is tagged limited record
      Strm        : Z_Stream_Access;
      Compression : Boolean;
      Stream_End  : Boolean;
      Header      : Header_Type;
      CRC         : Unsigned_32;
      Offset      : Stream_Element_Offset;
      --  Offset for gzip header/footer output.
   end record;

end ZLib;
