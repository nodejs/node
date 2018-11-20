----------------------------------------------------------------
--  ZLib for Ada thick binding.                               --
--                                                            --
--  Copyright (C) 2002-2003 Dmitriy Anisimkov                 --
--                                                            --
--  Open source license information is in the zlib.ads file.  --
----------------------------------------------------------------

--  $Id: zlib-streams.ads,v 1.12 2004/05/31 10:53:40 vagul Exp $

package ZLib.Streams is

   type Stream_Mode is (In_Stream, Out_Stream, Duplex);

   type Stream_Access is access all Ada.Streams.Root_Stream_Type'Class;

   type Stream_Type is
      new Ada.Streams.Root_Stream_Type with private;

   procedure Read
     (Stream : in out Stream_Type;
      Item   :    out Ada.Streams.Stream_Element_Array;
      Last   :    out Ada.Streams.Stream_Element_Offset);

   procedure Write
     (Stream : in out Stream_Type;
      Item   : in     Ada.Streams.Stream_Element_Array);

   procedure Flush
     (Stream : in out Stream_Type;
      Mode   : in     Flush_Mode := Sync_Flush);
   --  Flush the written data to the back stream,
   --  all data placed to the compressor is flushing to the Back stream.
   --  Should not be used until necessary, because it is decreasing
   --  compression.

   function Read_Total_In (Stream : in Stream_Type) return Count;
   pragma Inline (Read_Total_In);
   --  Return total number of bytes read from back stream so far.

   function Read_Total_Out (Stream : in Stream_Type) return Count;
   pragma Inline (Read_Total_Out);
   --  Return total number of bytes read so far.

   function Write_Total_In (Stream : in Stream_Type) return Count;
   pragma Inline (Write_Total_In);
   --  Return total number of bytes written so far.

   function Write_Total_Out (Stream : in Stream_Type) return Count;
   pragma Inline (Write_Total_Out);
   --  Return total number of bytes written to the back stream.

   procedure Create
     (Stream            :    out Stream_Type;
      Mode              : in     Stream_Mode;
      Back              : in     Stream_Access;
      Back_Compressed   : in     Boolean;
      Level             : in     Compression_Level := Default_Compression;
      Strategy          : in     Strategy_Type     := Default_Strategy;
      Header            : in     Header_Type       := Default;
      Read_Buffer_Size  : in     Ada.Streams.Stream_Element_Offset
                                    := Default_Buffer_Size;
      Write_Buffer_Size : in     Ada.Streams.Stream_Element_Offset
                                    := Default_Buffer_Size);
   --  Create the Comression/Decompression stream.
   --  If mode is In_Stream then Write operation is disabled.
   --  If mode is Out_Stream then Read operation is disabled.

   --  If Back_Compressed is true then
   --  Data written to the Stream is compressing to the Back stream
   --  and data read from the Stream is decompressed data from the Back stream.

   --  If Back_Compressed is false then
   --  Data written to the Stream is decompressing to the Back stream
   --  and data read from the Stream is compressed data from the Back stream.

   --  !!! When the Need_Header is False ZLib-Ada is using undocumented
   --  ZLib 1.1.4 functionality to do not create/wait for ZLib headers.

   function Is_Open (Stream : Stream_Type) return Boolean;

   procedure Close (Stream : in out Stream_Type);

private

   use Ada.Streams;

   type Buffer_Access is access all Stream_Element_Array;

   type Stream_Type
     is new Root_Stream_Type with
   record
      Mode       : Stream_Mode;

      Buffer     : Buffer_Access;
      Rest_First : Stream_Element_Offset;
      Rest_Last  : Stream_Element_Offset;
      --  Buffer for Read operation.
      --  We need to have this buffer in the record
      --  because not all read data from back stream
      --  could be processed during the read operation.

      Buffer_Size : Stream_Element_Offset;
      --  Buffer size for write operation.
      --  We do not need to have this buffer
      --  in the record because all data could be
      --  processed in the write operation.

      Back       : Stream_Access;
      Reader     : Filter_Type;
      Writer     : Filter_Type;
   end record;

end ZLib.Streams;
