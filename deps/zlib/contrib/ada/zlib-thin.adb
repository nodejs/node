----------------------------------------------------------------
--  ZLib for Ada thick binding.                               --
--                                                            --
--  Copyright (C) 2002-2003 Dmitriy Anisimkov                 --
--                                                            --
--  Open source license information is in the zlib.ads file.  --
----------------------------------------------------------------

--  $Id: zlib-thin.adb,v 1.8 2003/12/14 18:27:31 vagul Exp $

package body ZLib.Thin is

   ZLIB_VERSION  : constant Chars_Ptr := zlibVersion;

   Z_Stream_Size : constant Int := Z_Stream'Size / System.Storage_Unit;

   --------------
   -- Avail_In --
   --------------

   function Avail_In (Strm : in Z_Stream) return UInt is
   begin
      return Strm.Avail_In;
   end Avail_In;

   ---------------
   -- Avail_Out --
   ---------------

   function Avail_Out (Strm : in Z_Stream) return UInt is
   begin
      return Strm.Avail_Out;
   end Avail_Out;

   ------------------
   -- Deflate_Init --
   ------------------

   function Deflate_Init
     (strm       : Z_Streamp;
      level      : Int;
      method     : Int;
      windowBits : Int;
      memLevel   : Int;
      strategy   : Int)
      return       Int is
   begin
      return deflateInit2
               (strm,
                level,
                method,
                windowBits,
                memLevel,
                strategy,
                ZLIB_VERSION,
                Z_Stream_Size);
   end Deflate_Init;

   ------------------
   -- Inflate_Init --
   ------------------

   function Inflate_Init (strm : Z_Streamp; windowBits : Int) return Int is
   begin
      return inflateInit2 (strm, windowBits, ZLIB_VERSION, Z_Stream_Size);
   end Inflate_Init;

   ------------------------
   -- Last_Error_Message --
   ------------------------

   function Last_Error_Message (Strm : in Z_Stream) return String is
      use Interfaces.C.Strings;
   begin
      if Strm.msg = Null_Ptr then
         return "";
      else
         return Value (Strm.msg);
      end if;
   end Last_Error_Message;

   ------------
   -- Set_In --
   ------------

   procedure Set_In
     (Strm   : in out Z_Stream;
      Buffer : in     Voidp;
      Size   : in     UInt) is
   begin
      Strm.Next_In  := Buffer;
      Strm.Avail_In := Size;
   end Set_In;

   ------------------
   -- Set_Mem_Func --
   ------------------

   procedure Set_Mem_Func
     (Strm   : in out Z_Stream;
      Opaque : in     Voidp;
      Alloc  : in     alloc_func;
      Free   : in     free_func) is
   begin
      Strm.opaque := Opaque;
      Strm.zalloc := Alloc;
      Strm.zfree  := Free;
   end Set_Mem_Func;

   -------------
   -- Set_Out --
   -------------

   procedure Set_Out
     (Strm   : in out Z_Stream;
      Buffer : in     Voidp;
      Size   : in     UInt) is
   begin
      Strm.Next_Out  := Buffer;
      Strm.Avail_Out := Size;
   end Set_Out;

   --------------
   -- Total_In --
   --------------

   function Total_In (Strm : in Z_Stream) return ULong is
   begin
      return Strm.Total_In;
   end Total_In;

   ---------------
   -- Total_Out --
   ---------------

   function Total_Out (Strm : in Z_Stream) return ULong is
   begin
      return Strm.Total_Out;
   end Total_Out;

end ZLib.Thin;
