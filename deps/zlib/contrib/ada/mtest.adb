----------------------------------------------------------------
--  ZLib for Ada thick binding.                               --
--                                                            --
--  Copyright (C) 2002-2003 Dmitriy Anisimkov                 --
--                                                            --
--  Open source license information is in the zlib.ads file.  --
----------------------------------------------------------------
--  Continuous test for ZLib multithreading. If the test would fail
--  we should provide thread safe allocation routines for the Z_Stream.
--
--  $Id: mtest.adb,v 1.4 2004/07/23 07:49:54 vagul Exp $

with ZLib;
with Ada.Streams;
with Ada.Numerics.Discrete_Random;
with Ada.Text_IO;
with Ada.Exceptions;
with Ada.Task_Identification;

procedure MTest is
   use Ada.Streams;
   use ZLib;

   Stop : Boolean := False;

   pragma Atomic (Stop);

   subtype Visible_Symbols is Stream_Element range 16#20# .. 16#7E#;

   package Random_Elements is
      new Ada.Numerics.Discrete_Random (Visible_Symbols);

   task type Test_Task;

   task body Test_Task is
      Buffer : Stream_Element_Array (1 .. 100_000);
      Gen : Random_Elements.Generator;

      Buffer_First  : Stream_Element_Offset;
      Compare_First : Stream_Element_Offset;

      Deflate : Filter_Type;
      Inflate : Filter_Type;

      procedure Further (Item : in Stream_Element_Array);

      procedure Read_Buffer
        (Item : out Ada.Streams.Stream_Element_Array;
         Last : out Ada.Streams.Stream_Element_Offset);

      -------------
      -- Further --
      -------------

      procedure Further (Item : in Stream_Element_Array) is

         procedure Compare (Item : in Stream_Element_Array);

         -------------
         -- Compare --
         -------------

         procedure Compare (Item : in Stream_Element_Array) is
            Next_First : Stream_Element_Offset := Compare_First + Item'Length;
         begin
            if Buffer (Compare_First .. Next_First - 1) /= Item then
               raise Program_Error;
            end if;

            Compare_First := Next_First;
         end Compare;

         procedure Compare_Write is new ZLib.Write (Write => Compare);
      begin
         Compare_Write (Inflate, Item, No_Flush);
      end Further;

      -----------------
      -- Read_Buffer --
      -----------------

      procedure Read_Buffer
        (Item : out Ada.Streams.Stream_Element_Array;
         Last : out Ada.Streams.Stream_Element_Offset)
      is
         Buff_Diff   : Stream_Element_Offset := Buffer'Last - Buffer_First;
         Next_First : Stream_Element_Offset;
      begin
         if Item'Length <= Buff_Diff then
            Last := Item'Last;

            Next_First := Buffer_First + Item'Length;

            Item := Buffer (Buffer_First .. Next_First - 1);

            Buffer_First := Next_First;
         else
            Last := Item'First + Buff_Diff;
            Item (Item'First .. Last) := Buffer (Buffer_First .. Buffer'Last);
            Buffer_First := Buffer'Last + 1;
         end if;
      end Read_Buffer;

      procedure Translate is new Generic_Translate
                                   (Data_In  => Read_Buffer,
                                    Data_Out => Further);

   begin
      Random_Elements.Reset (Gen);

      Buffer := (others => 20);

      Main : loop
         for J in Buffer'Range loop
            Buffer (J) := Random_Elements.Random (Gen);

            Deflate_Init (Deflate);
            Inflate_Init (Inflate);

            Buffer_First  := Buffer'First;
            Compare_First := Buffer'First;

            Translate (Deflate);

            if Compare_First /= Buffer'Last + 1 then
               raise Program_Error;
            end if;

            Ada.Text_IO.Put_Line
              (Ada.Task_Identification.Image
                 (Ada.Task_Identification.Current_Task)
               & Stream_Element_Offset'Image (J)
               & ZLib.Count'Image (Total_Out (Deflate)));

            Close (Deflate);
            Close (Inflate);

            exit Main when Stop;
         end loop;
      end loop Main;
   exception
      when E : others =>
         Ada.Text_IO.Put_Line (Ada.Exceptions.Exception_Information (E));
         Stop := True;
   end Test_Task;

   Test : array (1 .. 4) of Test_Task;

   pragma Unreferenced (Test);

   Dummy : Character;

begin
   Ada.Text_IO.Get_Immediate (Dummy);
   Stop := True;
end MTest;
