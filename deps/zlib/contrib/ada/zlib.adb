----------------------------------------------------------------
--  ZLib for Ada thick binding.                               --
--                                                            --
--  Copyright (C) 2002-2004 Dmitriy Anisimkov                 --
--                                                            --
--  Open source license information is in the zlib.ads file.  --
----------------------------------------------------------------

--  $Id: zlib.adb,v 1.31 2004/09/06 06:53:19 vagul Exp $

with Ada.Exceptions;
with Ada.Unchecked_Conversion;
with Ada.Unchecked_Deallocation;

with Interfaces.C.Strings;

with ZLib.Thin;

package body ZLib is

   use type Thin.Int;

   type Z_Stream is new Thin.Z_Stream;

   type Return_Code_Enum is
      (OK,
       STREAM_END,
       NEED_DICT,
       ERRNO,
       STREAM_ERROR,
       DATA_ERROR,
       MEM_ERROR,
       BUF_ERROR,
       VERSION_ERROR);

   type Flate_Step_Function is access
     function (Strm : in Thin.Z_Streamp; Flush : in Thin.Int) return Thin.Int;
   pragma Convention (C, Flate_Step_Function);

   type Flate_End_Function is access
      function (Ctrm : in Thin.Z_Streamp) return Thin.Int;
   pragma Convention (C, Flate_End_Function);

   type Flate_Type is record
      Step : Flate_Step_Function;
      Done : Flate_End_Function;
   end record;

   subtype Footer_Array is Stream_Element_Array (1 .. 8);

   Simple_GZip_Header : constant Stream_Element_Array (1 .. 10)
     := (16#1f#, 16#8b#,                 --  Magic header
         16#08#,                         --  Z_DEFLATED
         16#00#,                         --  Flags
         16#00#, 16#00#, 16#00#, 16#00#, --  Time
         16#00#,                         --  XFlags
         16#03#                          --  OS code
        );
   --  The simplest gzip header is not for informational, but just for
   --  gzip format compatibility.
   --  Note that some code below is using assumption
   --  Simple_GZip_Header'Last > Footer_Array'Last, so do not make
   --  Simple_GZip_Header'Last <= Footer_Array'Last.

   Return_Code : constant array (Thin.Int range <>) of Return_Code_Enum
     := (0 => OK,
         1 => STREAM_END,
         2 => NEED_DICT,
        -1 => ERRNO,
        -2 => STREAM_ERROR,
        -3 => DATA_ERROR,
        -4 => MEM_ERROR,
        -5 => BUF_ERROR,
        -6 => VERSION_ERROR);

   Flate : constant array (Boolean) of Flate_Type
     := (True  => (Step => Thin.Deflate'Access,
                   Done => Thin.DeflateEnd'Access),
         False => (Step => Thin.Inflate'Access,
                   Done => Thin.InflateEnd'Access));

   Flush_Finish : constant array (Boolean) of Flush_Mode
     := (True => Finish, False => No_Flush);

   procedure Raise_Error (Stream : in Z_Stream);
   pragma Inline (Raise_Error);

   procedure Raise_Error (Message : in String);
   pragma Inline (Raise_Error);

   procedure Check_Error (Stream : in Z_Stream; Code : in Thin.Int);

   procedure Free is new Ada.Unchecked_Deallocation
      (Z_Stream, Z_Stream_Access);

   function To_Thin_Access is new Ada.Unchecked_Conversion
     (Z_Stream_Access, Thin.Z_Streamp);

   procedure Translate_GZip
     (Filter    : in out Filter_Type;
      In_Data   : in     Ada.Streams.Stream_Element_Array;
      In_Last   :    out Ada.Streams.Stream_Element_Offset;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode);
   --  Separate translate routine for make gzip header.

   procedure Translate_Auto
     (Filter    : in out Filter_Type;
      In_Data   : in     Ada.Streams.Stream_Element_Array;
      In_Last   :    out Ada.Streams.Stream_Element_Offset;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode);
   --  translate routine without additional headers.

   -----------------
   -- Check_Error --
   -----------------

   procedure Check_Error (Stream : in Z_Stream; Code : in Thin.Int) is
      use type Thin.Int;
   begin
      if Code /= Thin.Z_OK then
         Raise_Error
            (Return_Code_Enum'Image (Return_Code (Code))
              & ": " & Last_Error_Message (Stream));
      end if;
   end Check_Error;

   -----------
   -- Close --
   -----------

   procedure Close
     (Filter       : in out Filter_Type;
      Ignore_Error : in     Boolean := False)
   is
      Code : Thin.Int;
   begin
      if not Ignore_Error and then not Is_Open (Filter) then
         raise Status_Error;
      end if;

      Code := Flate (Filter.Compression).Done (To_Thin_Access (Filter.Strm));

      if Ignore_Error or else Code = Thin.Z_OK then
         Free (Filter.Strm);
      else
         declare
            Error_Message : constant String
              := Last_Error_Message (Filter.Strm.all);
         begin
            Free (Filter.Strm);
            Ada.Exceptions.Raise_Exception
               (ZLib_Error'Identity,
                Return_Code_Enum'Image (Return_Code (Code))
                  & ": " & Error_Message);
         end;
      end if;
   end Close;

   -----------
   -- CRC32 --
   -----------

   function CRC32
     (CRC  : in Unsigned_32;
      Data : in Ada.Streams.Stream_Element_Array)
      return Unsigned_32
   is
      use Thin;
   begin
      return Unsigned_32 (crc32 (ULong (CRC),
                                 Data'Address,
                                 Data'Length));
   end CRC32;

   procedure CRC32
     (CRC  : in out Unsigned_32;
      Data : in     Ada.Streams.Stream_Element_Array) is
   begin
      CRC := CRC32 (CRC, Data);
   end CRC32;

   ------------------
   -- Deflate_Init --
   ------------------

   procedure Deflate_Init
     (Filter       : in out Filter_Type;
      Level        : in     Compression_Level  := Default_Compression;
      Strategy     : in     Strategy_Type      := Default_Strategy;
      Method       : in     Compression_Method := Deflated;
      Window_Bits  : in     Window_Bits_Type   := Default_Window_Bits;
      Memory_Level : in     Memory_Level_Type  := Default_Memory_Level;
      Header       : in     Header_Type        := Default)
   is
      use type Thin.Int;
      Win_Bits : Thin.Int := Thin.Int (Window_Bits);
   begin
      if Is_Open (Filter) then
         raise Status_Error;
      end if;

      --  We allow ZLib to make header only in case of default header type.
      --  Otherwise we would either do header by ourselfs, or do not do
      --  header at all.

      if Header = None or else Header = GZip then
         Win_Bits := -Win_Bits;
      end if;

      --  For the GZip CRC calculation and make headers.

      if Header = GZip then
         Filter.CRC    := 0;
         Filter.Offset := Simple_GZip_Header'First;
      else
         Filter.Offset := Simple_GZip_Header'Last + 1;
      end if;

      Filter.Strm        := new Z_Stream;
      Filter.Compression := True;
      Filter.Stream_End  := False;
      Filter.Header      := Header;

      if Thin.Deflate_Init
           (To_Thin_Access (Filter.Strm),
            Level      => Thin.Int (Level),
            method     => Thin.Int (Method),
            windowBits => Win_Bits,
            memLevel   => Thin.Int (Memory_Level),
            strategy   => Thin.Int (Strategy)) /= Thin.Z_OK
      then
         Raise_Error (Filter.Strm.all);
      end if;
   end Deflate_Init;

   -----------
   -- Flush --
   -----------

   procedure Flush
     (Filter    : in out Filter_Type;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode)
   is
      No_Data : Stream_Element_Array := (1 .. 0 => 0);
      Last    : Stream_Element_Offset;
   begin
      Translate (Filter, No_Data, Last, Out_Data, Out_Last, Flush);
   end Flush;

   -----------------------
   -- Generic_Translate --
   -----------------------

   procedure Generic_Translate
     (Filter          : in out ZLib.Filter_Type;
      In_Buffer_Size  : in     Integer := Default_Buffer_Size;
      Out_Buffer_Size : in     Integer := Default_Buffer_Size)
   is
      In_Buffer  : Stream_Element_Array
                     (1 .. Stream_Element_Offset (In_Buffer_Size));
      Out_Buffer : Stream_Element_Array
                     (1 .. Stream_Element_Offset (Out_Buffer_Size));
      Last       : Stream_Element_Offset;
      In_Last    : Stream_Element_Offset;
      In_First   : Stream_Element_Offset;
      Out_Last   : Stream_Element_Offset;
   begin
      Main : loop
         Data_In (In_Buffer, Last);

         In_First := In_Buffer'First;

         loop
            Translate
              (Filter   => Filter,
               In_Data  => In_Buffer (In_First .. Last),
               In_Last  => In_Last,
               Out_Data => Out_Buffer,
               Out_Last => Out_Last,
               Flush    => Flush_Finish (Last < In_Buffer'First));

            if Out_Buffer'First <= Out_Last then
               Data_Out (Out_Buffer (Out_Buffer'First .. Out_Last));
            end if;

            exit Main when Stream_End (Filter);

            --  The end of in buffer.

            exit when In_Last = Last;

            In_First := In_Last + 1;
         end loop;
      end loop Main;

   end Generic_Translate;

   ------------------
   -- Inflate_Init --
   ------------------

   procedure Inflate_Init
     (Filter      : in out Filter_Type;
      Window_Bits : in     Window_Bits_Type := Default_Window_Bits;
      Header      : in     Header_Type      := Default)
   is
      use type Thin.Int;
      Win_Bits : Thin.Int := Thin.Int (Window_Bits);

      procedure Check_Version;
      --  Check the latest header types compatibility.

      procedure Check_Version is
      begin
         if Version <= "1.1.4" then
            Raise_Error
              ("Inflate header type " & Header_Type'Image (Header)
               & " incompatible with ZLib version " & Version);
         end if;
      end Check_Version;

   begin
      if Is_Open (Filter) then
         raise Status_Error;
      end if;

      case Header is
         when None =>
            Check_Version;

            --  Inflate data without headers determined
            --  by negative Win_Bits.

            Win_Bits := -Win_Bits;
         when GZip =>
            Check_Version;

            --  Inflate gzip data defined by flag 16.

            Win_Bits := Win_Bits + 16;
         when Auto =>
            Check_Version;

            --  Inflate with automatic detection
            --  of gzip or native header defined by flag 32.

            Win_Bits := Win_Bits + 32;
         when Default => null;
      end case;

      Filter.Strm        := new Z_Stream;
      Filter.Compression := False;
      Filter.Stream_End  := False;
      Filter.Header      := Header;

      if Thin.Inflate_Init
         (To_Thin_Access (Filter.Strm), Win_Bits) /= Thin.Z_OK
      then
         Raise_Error (Filter.Strm.all);
      end if;
   end Inflate_Init;

   -------------
   -- Is_Open --
   -------------

   function Is_Open (Filter : in Filter_Type) return Boolean is
   begin
      return Filter.Strm /= null;
   end Is_Open;

   -----------------
   -- Raise_Error --
   -----------------

   procedure Raise_Error (Message : in String) is
   begin
      Ada.Exceptions.Raise_Exception (ZLib_Error'Identity, Message);
   end Raise_Error;

   procedure Raise_Error (Stream : in Z_Stream) is
   begin
      Raise_Error (Last_Error_Message (Stream));
   end Raise_Error;

   ----------
   -- Read --
   ----------

   procedure Read
     (Filter : in out Filter_Type;
      Item   :    out Ada.Streams.Stream_Element_Array;
      Last   :    out Ada.Streams.Stream_Element_Offset;
      Flush  : in     Flush_Mode := No_Flush)
   is
      In_Last    : Stream_Element_Offset;
      Item_First : Ada.Streams.Stream_Element_Offset := Item'First;
      V_Flush    : Flush_Mode := Flush;

   begin
      pragma Assert (Rest_First in Buffer'First .. Buffer'Last + 1);
      pragma Assert (Rest_Last in Buffer'First - 1 .. Buffer'Last);

      loop
         if Rest_Last = Buffer'First - 1 then
            V_Flush := Finish;

         elsif Rest_First > Rest_Last then
            Read (Buffer, Rest_Last);
            Rest_First := Buffer'First;

            if Rest_Last < Buffer'First then
               V_Flush := Finish;
            end if;
         end if;

         Translate
           (Filter   => Filter,
            In_Data  => Buffer (Rest_First .. Rest_Last),
            In_Last  => In_Last,
            Out_Data => Item (Item_First .. Item'Last),
            Out_Last => Last,
            Flush    => V_Flush);

         Rest_First := In_Last + 1;

         exit when Stream_End (Filter)
           or else Last = Item'Last
           or else (Last >= Item'First and then Allow_Read_Some);

         Item_First := Last + 1;
      end loop;
   end Read;

   ----------------
   -- Stream_End --
   ----------------

   function Stream_End (Filter : in Filter_Type) return Boolean is
   begin
      if Filter.Header = GZip and Filter.Compression then
         return Filter.Stream_End
            and then Filter.Offset = Footer_Array'Last + 1;
      else
         return Filter.Stream_End;
      end if;
   end Stream_End;

   --------------
   -- Total_In --
   --------------

   function Total_In (Filter : in Filter_Type) return Count is
   begin
      return Count (Thin.Total_In (To_Thin_Access (Filter.Strm).all));
   end Total_In;

   ---------------
   -- Total_Out --
   ---------------

   function Total_Out (Filter : in Filter_Type) return Count is
   begin
      return Count (Thin.Total_Out (To_Thin_Access (Filter.Strm).all));
   end Total_Out;

   ---------------
   -- Translate --
   ---------------

   procedure Translate
     (Filter    : in out Filter_Type;
      In_Data   : in     Ada.Streams.Stream_Element_Array;
      In_Last   :    out Ada.Streams.Stream_Element_Offset;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode) is
   begin
      if Filter.Header = GZip and then Filter.Compression then
         Translate_GZip
           (Filter   => Filter,
            In_Data  => In_Data,
            In_Last  => In_Last,
            Out_Data => Out_Data,
            Out_Last => Out_Last,
            Flush    => Flush);
      else
         Translate_Auto
           (Filter   => Filter,
            In_Data  => In_Data,
            In_Last  => In_Last,
            Out_Data => Out_Data,
            Out_Last => Out_Last,
            Flush    => Flush);
      end if;
   end Translate;

   --------------------
   -- Translate_Auto --
   --------------------

   procedure Translate_Auto
     (Filter    : in out Filter_Type;
      In_Data   : in     Ada.Streams.Stream_Element_Array;
      In_Last   :    out Ada.Streams.Stream_Element_Offset;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode)
   is
      use type Thin.Int;
      Code : Thin.Int;

   begin
      if not Is_Open (Filter) then
         raise Status_Error;
      end if;

      if Out_Data'Length = 0 and then In_Data'Length = 0 then
         raise Constraint_Error;
      end if;

      Set_Out (Filter.Strm.all, Out_Data'Address, Out_Data'Length);
      Set_In  (Filter.Strm.all, In_Data'Address, In_Data'Length);

      Code := Flate (Filter.Compression).Step
        (To_Thin_Access (Filter.Strm),
         Thin.Int (Flush));

      if Code = Thin.Z_STREAM_END then
         Filter.Stream_End := True;
      else
         Check_Error (Filter.Strm.all, Code);
      end if;

      In_Last  := In_Data'Last
         - Stream_Element_Offset (Avail_In (Filter.Strm.all));
      Out_Last := Out_Data'Last
         - Stream_Element_Offset (Avail_Out (Filter.Strm.all));
   end Translate_Auto;

   --------------------
   -- Translate_GZip --
   --------------------

   procedure Translate_GZip
     (Filter    : in out Filter_Type;
      In_Data   : in     Ada.Streams.Stream_Element_Array;
      In_Last   :    out Ada.Streams.Stream_Element_Offset;
      Out_Data  :    out Ada.Streams.Stream_Element_Array;
      Out_Last  :    out Ada.Streams.Stream_Element_Offset;
      Flush     : in     Flush_Mode)
   is
      Out_First : Stream_Element_Offset;

      procedure Add_Data (Data : in Stream_Element_Array);
      --  Add data to stream from the Filter.Offset till necessary,
      --  used for add gzip headr/footer.

      procedure Put_32
        (Item : in out Stream_Element_Array;
         Data : in     Unsigned_32);
      pragma Inline (Put_32);

      --------------
      -- Add_Data --
      --------------

      procedure Add_Data (Data : in Stream_Element_Array) is
         Data_First : Stream_Element_Offset renames Filter.Offset;
         Data_Last  : Stream_Element_Offset;
         Data_Len   : Stream_Element_Offset; --  -1
         Out_Len    : Stream_Element_Offset; --  -1
      begin
         Out_First := Out_Last + 1;

         if Data_First > Data'Last then
            return;
         end if;

         Data_Len  := Data'Last     - Data_First;
         Out_Len   := Out_Data'Last - Out_First;

         if Data_Len <= Out_Len then
            Out_Last  := Out_First  + Data_Len;
            Data_Last := Data'Last;
         else
            Out_Last  := Out_Data'Last;
            Data_Last := Data_First + Out_Len;
         end if;

         Out_Data (Out_First .. Out_Last) := Data (Data_First .. Data_Last);

         Data_First := Data_Last + 1;
         Out_First  := Out_Last + 1;
      end Add_Data;

      ------------
      -- Put_32 --
      ------------

      procedure Put_32
        (Item : in out Stream_Element_Array;
         Data : in     Unsigned_32)
      is
         D : Unsigned_32 := Data;
      begin
         for J in Item'First .. Item'First + 3 loop
            Item (J) := Stream_Element (D and 16#FF#);
            D := Shift_Right (D, 8);
         end loop;
      end Put_32;

   begin
      Out_Last := Out_Data'First - 1;

      if not Filter.Stream_End then
         Add_Data (Simple_GZip_Header);

         Translate_Auto
           (Filter   => Filter,
            In_Data  => In_Data,
            In_Last  => In_Last,
            Out_Data => Out_Data (Out_First .. Out_Data'Last),
            Out_Last => Out_Last,
            Flush    => Flush);

         CRC32 (Filter.CRC, In_Data (In_Data'First .. In_Last));
      end if;

      if Filter.Stream_End and then Out_Last <= Out_Data'Last then
         --  This detection method would work only when
         --  Simple_GZip_Header'Last > Footer_Array'Last

         if Filter.Offset = Simple_GZip_Header'Last + 1 then
            Filter.Offset := Footer_Array'First;
         end if;

         declare
            Footer : Footer_Array;
         begin
            Put_32 (Footer, Filter.CRC);
            Put_32 (Footer (Footer'First + 4 .. Footer'Last),
                    Unsigned_32 (Total_In (Filter)));
            Add_Data (Footer);
         end;
      end if;
   end Translate_GZip;

   -------------
   -- Version --
   -------------

   function Version return String is
   begin
      return Interfaces.C.Strings.Value (Thin.zlibVersion);
   end Version;

   -----------
   -- Write --
   -----------

   procedure Write
     (Filter : in out Filter_Type;
      Item   : in     Ada.Streams.Stream_Element_Array;
      Flush  : in     Flush_Mode := No_Flush)
   is
      Buffer   : Stream_Element_Array (1 .. Buffer_Size);
      In_Last  : Stream_Element_Offset;
      Out_Last : Stream_Element_Offset;
      In_First : Stream_Element_Offset := Item'First;
   begin
      if Item'Length = 0 and Flush = No_Flush then
         return;
      end if;

      loop
         Translate
           (Filter   => Filter,
            In_Data  => Item (In_First .. Item'Last),
            In_Last  => In_Last,
            Out_Data => Buffer,
            Out_Last => Out_Last,
            Flush    => Flush);

         if Out_Last >= Buffer'First then
            Write (Buffer (1 .. Out_Last));
         end if;

         exit when In_Last = Item'Last or Stream_End (Filter);

         In_First := In_Last + 1;
      end loop;
   end Write;

end ZLib;
