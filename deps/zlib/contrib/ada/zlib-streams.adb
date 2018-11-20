----------------------------------------------------------------
--  ZLib for Ada thick binding.                               --
--                                                            --
--  Copyright (C) 2002-2003 Dmitriy Anisimkov                 --
--                                                            --
--  Open source license information is in the zlib.ads file.  --
----------------------------------------------------------------

--  $Id: zlib-streams.adb,v 1.10 2004/05/31 10:53:40 vagul Exp $

with Ada.Unchecked_Deallocation;

package body ZLib.Streams is

   -----------
   -- Close --
   -----------

   procedure Close (Stream : in out Stream_Type) is
      procedure Free is new Ada.Unchecked_Deallocation
         (Stream_Element_Array, Buffer_Access);
   begin
      if Stream.Mode = Out_Stream or Stream.Mode = Duplex then
         --  We should flush the data written by the writer.

         Flush (Stream, Finish);

         Close (Stream.Writer);
      end if;

      if Stream.Mode = In_Stream or Stream.Mode = Duplex then
         Close (Stream.Reader);
         Free (Stream.Buffer);
      end if;
   end Close;

   ------------
   -- Create --
   ------------

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
                                    := Default_Buffer_Size)
   is

      subtype Buffer_Subtype is Stream_Element_Array (1 .. Read_Buffer_Size);

      procedure Init_Filter
         (Filter   : in out Filter_Type;
          Compress : in     Boolean);

      -----------------
      -- Init_Filter --
      -----------------

      procedure Init_Filter
         (Filter   : in out Filter_Type;
          Compress : in     Boolean) is
      begin
         if Compress then
            Deflate_Init
              (Filter, Level, Strategy, Header => Header);
         else
            Inflate_Init (Filter, Header => Header);
         end if;
      end Init_Filter;

   begin
      Stream.Back := Back;
      Stream.Mode := Mode;

      if Mode = Out_Stream or Mode = Duplex then
         Init_Filter (Stream.Writer, Back_Compressed);
         Stream.Buffer_Size := Write_Buffer_Size;
      else
         Stream.Buffer_Size := 0;
      end if;

      if Mode = In_Stream or Mode = Duplex then
         Init_Filter (Stream.Reader, not Back_Compressed);

         Stream.Buffer     := new Buffer_Subtype;
         Stream.Rest_First := Stream.Buffer'Last + 1;
         Stream.Rest_Last  := Stream.Buffer'Last;
      end if;
   end Create;

   -----------
   -- Flush --
   -----------

   procedure Flush
     (Stream : in out Stream_Type;
      Mode   : in     Flush_Mode := Sync_Flush)
   is
      Buffer : Stream_Element_Array (1 .. Stream.Buffer_Size);
      Last   : Stream_Element_Offset;
   begin
      loop
         Flush (Stream.Writer, Buffer, Last, Mode);

         Ada.Streams.Write (Stream.Back.all, Buffer (1 .. Last));

         exit when Last < Buffer'Last;
      end loop;
   end Flush;

   -------------
   -- Is_Open --
   -------------

   function Is_Open (Stream : Stream_Type) return Boolean is
   begin
      return Is_Open (Stream.Reader) or else Is_Open (Stream.Writer);
   end Is_Open;

   ----------
   -- Read --
   ----------

   procedure Read
     (Stream : in out Stream_Type;
      Item   :    out Stream_Element_Array;
      Last   :    out Stream_Element_Offset)
   is

      procedure Read
        (Item : out Stream_Element_Array;
         Last : out Stream_Element_Offset);

      ----------
      -- Read --
      ----------

      procedure Read
        (Item : out Stream_Element_Array;
         Last : out Stream_Element_Offset) is
      begin
         Ada.Streams.Read (Stream.Back.all, Item, Last);
      end Read;

      procedure Read is new ZLib.Read
         (Read       => Read,
          Buffer     => Stream.Buffer.all,
          Rest_First => Stream.Rest_First,
          Rest_Last  => Stream.Rest_Last);

   begin
      Read (Stream.Reader, Item, Last);
   end Read;

   -------------------
   -- Read_Total_In --
   -------------------

   function Read_Total_In (Stream : in Stream_Type) return Count is
   begin
      return Total_In (Stream.Reader);
   end Read_Total_In;

   --------------------
   -- Read_Total_Out --
   --------------------

   function Read_Total_Out (Stream : in Stream_Type) return Count is
   begin
      return Total_Out (Stream.Reader);
   end Read_Total_Out;

   -----------
   -- Write --
   -----------

   procedure Write
     (Stream : in out Stream_Type;
      Item   : in     Stream_Element_Array)
   is

      procedure Write (Item : in Stream_Element_Array);

      -----------
      -- Write --
      -----------

      procedure Write (Item : in Stream_Element_Array) is
      begin
         Ada.Streams.Write (Stream.Back.all, Item);
      end Write;

      procedure Write is new ZLib.Write
         (Write       => Write,
          Buffer_Size => Stream.Buffer_Size);

   begin
      Write (Stream.Writer, Item, No_Flush);
   end Write;

   --------------------
   -- Write_Total_In --
   --------------------

   function Write_Total_In (Stream : in Stream_Type) return Count is
   begin
      return Total_In (Stream.Writer);
   end Write_Total_In;

   ---------------------
   -- Write_Total_Out --
   ---------------------

   function Write_Total_Out (Stream : in Stream_Type) return Count is
   begin
      return Total_Out (Stream.Writer);
   end Write_Total_Out;

end ZLib.Streams;
