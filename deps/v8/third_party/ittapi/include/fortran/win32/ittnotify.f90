! ========================================================================
! <copyright>
!  This file is provided under a dual BSD/GPLv2 license.  When using or
!  redistributing this file, you may do so under either license.
!
!  GPL LICENSE SUMMARY
!
!  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
!
!  This program is free software; you can redistribute it and/or modify
!  it under the terms of version 2 of the GNU General Public License as
!  published by the Free Software Foundation.
!
!  This program is distributed in the hope that it will be useful, but
!  WITHOUT ANY WARRANTY; without even the implied warranty of
!  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
!  General Public License for more details.
!
!  You should have received a copy of the GNU General Public License
!  along with this program; if not, write to the Free Software
!  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
!  The full GNU General Public License is included in this distribution
!  in the file called LICENSE.GPL.
!
!  Contact Information:
!  http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/
!
!  BSD LICENSE
!
!  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
!  All rights reserved.
!
!  Redistribution and use in source and binary forms, with or without
!  modification, are permitted provided that the following conditions
!  are met:
!
!    * Redistributions of source code must retain the above copyright
!      notice, this list of conditions and the following disclaimer.
!    * Redistributions in binary form must reproduce the above copyright
!      notice, this list of conditions and the following disclaimer in
!      the documentation and/or other materials provided with the
!      distribution.
!    * Neither the name of Intel Corporation nor the names of its
!      contributors may be used to endorse or promote products derived
!      from this software without specific prior written permission.
!
!  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
!  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
!  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
!  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
!  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
!  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
!  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
!  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
!  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
!  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
!  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
! </copyright>
! ========================================================================
!

!--------
!
! This file defines functions used by Intel(R) Parallel Inspector and
! Amplifier.
!
! Version of ittnotify that was used to generate this file. 
! This is not Fortran code that can be used to check but rather a comment 
! that only serves to identify the interface. 
! INTEL_ITT_FORTRAN_API_VERSION 3.0
!--------

module ittnotify
    use, intrinsic :: iso_c_binding, only: C_PTR, C_FUNPTR, C_INT, C_CHAR, C_NULL_CHAR, C_F_PROCPOINTER, C_LOC, C_ASSOCIATED
    implicit none

    !--------
    !
    ! Public interface
    !
    !--------
    integer, parameter :: itt_ptr     = int_ptr_kind()
    public :: itt_pause
    public :: itt_resume
    public :: itt_thread_ignore
    public :: itt_suppress_push
    public :: itt_suppress_pop
    public :: itt_suppress_mark_range
    public :: itt_suppress_clear_range
    public :: itt_sync_prepare
    public :: itt_sync_cancel
    public :: itt_sync_acquired
    public :: itt_sync_releasing
    public :: itt_fsync_prepare
    public :: itt_fsync_cancel
    public :: itt_fsync_acquired
    public :: itt_fsync_releasing
    public :: itt_sync_destroy
    public :: itt_sync_create
    public :: itt_sync_rename
    public :: itt_thread_set_name
    public :: itt_heap_record_memory_growth_begin
    public :: itt_heap_record_memory_growth_end
    public :: itt_heap_reset_detection
    public :: itt_heap_record

    integer, parameter, public :: itt_attr_barrier                 =          1
    integer, parameter, public :: itt_attr_mutex                   =          2
    integer, parameter, public :: itt_suppress_threading_errors    =        255
    integer, parameter, public :: itt_suppress_memory_errors       =      65280
    integer, parameter, public :: itt_suppress_all_errors          = 2147483647
    integer, parameter, public :: itt_unsuppress_range             =          0
    integer, parameter, public :: itt_suppress_range               =          1
    integer, parameter, public :: itt_heap_leaks                   =          1
    integer, parameter, public :: itt_heap_growth                  =          2

    private

    abstract interface

      subroutine itt_proc_none() bind(C)
          import
      end subroutine itt_proc_none

      subroutine itt_proc_sup_push(mask) bind(C)
          import
          integer, intent(in), value :: mask
      end subroutine itt_proc_sup_push

      subroutine itt_proc_sup_range(action, mask, addr, size) bind(C)
          import
          integer, intent(in), value :: action
          integer, intent(in), value :: mask
          integer(kind=itt_ptr), intent(in), value :: addr
          integer(kind=itt_ptr), intent(in), value :: size
      end subroutine itt_proc_sup_range

      subroutine itt_proc_address(addr) bind(C)
          import
          integer(kind=itt_ptr), intent(in), value :: addr
      end subroutine itt_proc_address

      subroutine itt_proc_create(addr, objname, attribute) bind(C)
          import
          integer(kind=itt_ptr), intent(in), value :: addr
          character(kind=C_CHAR), dimension(*), intent(in) :: objname
          integer, intent(in), value :: attribute
      end subroutine itt_proc_create

      subroutine itt_proc_name(name) bind(C)
          import
          character(kind=C_CHAR), dimension(*), intent(in) :: name
      end subroutine itt_proc_name

      subroutine itt_proc_rename(addr, objname) bind(C)
          import
          integer(kind=itt_ptr), intent(in), value :: addr
          character(kind=C_CHAR), dimension(*), intent(in) :: objname
      end subroutine itt_proc_rename

      subroutine itt_proc_heapmask(heapmask) bind(C)
          import
          integer, intent(in), value :: heapmask
      end subroutine itt_proc_heapmask

    end interface

    !DEC$IF DEFINED(intel64)
    type(C_FUNPTR) :: itt_pause_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_pause_ptr__3_0' :: itt_pause_fort_ptr
    type(C_FUNPTR) :: itt_resume_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_resume_ptr__3_0' :: itt_resume_fort_ptr
    type(C_FUNPTR) :: itt_thread_ignore_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_thread_ignore_ptr__3_0' :: itt_thread_ignore_fort_ptr
    type(C_FUNPTR) :: itt_suppress_push_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_suppress_push_ptr__3_0' :: itt_suppress_push_fort_ptr
    type(C_FUNPTR) :: itt_suppress_pop_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_suppress_pop_ptr__3_0' :: itt_suppress_pop_fort_ptr
    type(C_FUNPTR) :: itt_suppress_mark_range_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_suppress_mark_range_ptr__3_0' :: itt_suppress_mark_range_fort_ptr
    type(C_FUNPTR) :: itt_suppress_clear_range_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_suppress_clear_range_ptr__3_0' :: itt_suppress_clear_range_fort_ptr
    type(C_FUNPTR) :: itt_sync_prepare_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_prepare_ptr__3_0' :: itt_sync_prepare_fort_ptr
    type(C_FUNPTR) :: itt_sync_cancel_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_cancel_ptr__3_0' :: itt_sync_cancel_fort_ptr
    type(C_FUNPTR) :: itt_sync_acquired_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_acquired_ptr__3_0' :: itt_sync_acquired_fort_ptr
    type(C_FUNPTR) :: itt_sync_releasing_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_releasing_ptr__3_0' :: itt_sync_releasing_fort_ptr
    type(C_FUNPTR) :: itt_fsync_prepare_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_fsync_prepare_ptr__3_0' :: itt_fsync_prepare_fort_ptr
    type(C_FUNPTR) :: itt_fsync_cancel_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_fsync_cancel_ptr__3_0' :: itt_fsync_cancel_fort_ptr
    type(C_FUNPTR) :: itt_fsync_acquired_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_fsync_acquired_ptr__3_0' :: itt_fsync_acquired_fort_ptr
    type(C_FUNPTR) :: itt_fsync_releasing_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_fsync_releasing_ptr__3_0' :: itt_fsync_releasing_fort_ptr
    type(C_FUNPTR) :: itt_sync_destroy_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_destroy_ptr__3_0' :: itt_sync_destroy_fort_ptr
    type(C_FUNPTR) :: itt_sync_create_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_createA_ptr__3_0' :: itt_sync_create_fort_ptr
    type(C_FUNPTR) :: itt_sync_rename_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_sync_renameA_ptr__3_0' :: itt_sync_rename_fort_ptr
    type(C_FUNPTR) :: itt_thread_set_name_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_thread_set_nameA_ptr__3_0' :: itt_thread_set_name_fort_ptr
    type(C_FUNPTR) :: itt_heap_record_memory_growth_begin_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_heap_record_memory_growth_begin_ptr__3_0' :: itt_heap_record_memory_growth_begin_fort_ptr
    type(C_FUNPTR) :: itt_heap_record_memory_growth_end_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_heap_record_memory_growth_end_ptr__3_0' :: itt_heap_record_memory_growth_end_fort_ptr
    type(C_FUNPTR) :: itt_heap_reset_detection_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_heap_reset_detection_ptr__3_0' :: itt_heap_reset_detection_fort_ptr
    type(C_FUNPTR) :: itt_heap_record_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'__itt_heap_record_ptr__3_0' :: itt_heap_record_fort_ptr
    !DEC$ELSE
    type(C_FUNPTR) :: itt_pause_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_pause_ptr__3_0' :: itt_pause_fort_ptr
    type(C_FUNPTR) :: itt_resume_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_resume_ptr__3_0' :: itt_resume_fort_ptr
    type(C_FUNPTR) :: itt_thread_ignore_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_thread_ignore_ptr__3_0' :: itt_thread_ignore_fort_ptr
    type(C_FUNPTR) :: itt_suppress_push_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_suppress_push_ptr__3_0' :: itt_suppress_push_fort_ptr
    type(C_FUNPTR) :: itt_suppress_pop_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_suppress_pop_ptr__3_0' :: itt_suppress_pop_fort_ptr
    type(C_FUNPTR) :: itt_suppress_mark_range_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_suppress_mark_range_ptr__3_0' :: itt_suppress_mark_range_fort_ptr
    type(C_FUNPTR) :: itt_suppress_clear_range_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_suppress_clear_range_ptr__3_0' :: itt_suppress_clear_range_fort_ptr
    type(C_FUNPTR) :: itt_sync_prepare_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_prepare_ptr__3_0' :: itt_sync_prepare_fort_ptr
    type(C_FUNPTR) :: itt_sync_cancel_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_cancel_ptr__3_0' :: itt_sync_cancel_fort_ptr
    type(C_FUNPTR) :: itt_sync_acquired_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_acquired_ptr__3_0' :: itt_sync_acquired_fort_ptr
    type(C_FUNPTR) :: itt_sync_releasing_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_releasing_ptr__3_0' :: itt_sync_releasing_fort_ptr
    type(C_FUNPTR) :: itt_fsync_prepare_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_fsync_prepare_ptr__3_0' :: itt_fsync_prepare_fort_ptr
    type(C_FUNPTR) :: itt_fsync_cancel_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_fsync_cancel_ptr__3_0' :: itt_fsync_cancel_fort_ptr
    type(C_FUNPTR) :: itt_fsync_acquired_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_fsync_acquired_ptr__3_0' :: itt_fsync_acquired_fort_ptr
    type(C_FUNPTR) :: itt_fsync_releasing_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_fsync_releasing_ptr__3_0' :: itt_fsync_releasing_fort_ptr
    type(C_FUNPTR) :: itt_sync_destroy_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_destroy_ptr__3_0' :: itt_sync_destroy_fort_ptr
    type(C_FUNPTR) :: itt_sync_create_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_createA_ptr__3_0' :: itt_sync_create_fort_ptr
    type(C_FUNPTR) :: itt_sync_rename_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_sync_renameA_ptr__3_0' :: itt_sync_rename_fort_ptr
    type(C_FUNPTR) :: itt_thread_set_name_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_thread_set_nameA_ptr__3_0' :: itt_thread_set_name_fort_ptr
    type(C_FUNPTR) :: itt_heap_record_memory_growth_begin_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_heap_record_memory_growth_begin_ptr__3_0' :: itt_heap_record_memory_growth_begin_fort_ptr
    type(C_FUNPTR) :: itt_heap_record_memory_growth_end_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_heap_record_memory_growth_end_ptr__3_0' :: itt_heap_record_memory_growth_end_fort_ptr
    type(C_FUNPTR) :: itt_heap_reset_detection_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_heap_reset_detection_ptr__3_0' :: itt_heap_reset_detection_fort_ptr
    type(C_FUNPTR) :: itt_heap_record_fort_ptr
    !DEC$ ATTRIBUTES C, EXTERN, ALIAS:'___itt_heap_record_ptr__3_0' :: itt_heap_record_fort_ptr
    !DEC$ENDIF

    contains

      subroutine itt_pause()
          procedure(itt_proc_none), pointer :: pause_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_pause
          if (C_ASSOCIATED(itt_pause_fort_ptr)) then
              call C_F_PROCPOINTER(itt_pause_fort_ptr, pause_ptr)
              call pause_ptr()
          end if
      end subroutine itt_pause

      subroutine itt_resume()
          procedure(itt_proc_none), pointer :: resume_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_resume
          if (C_ASSOCIATED(itt_resume_fort_ptr)) then
              call C_F_PROCPOINTER(itt_resume_fort_ptr, resume_ptr)
              call resume_ptr()
          end if
      end subroutine itt_resume

      subroutine itt_thread_ignore()
          procedure(itt_proc_none), pointer :: thread_ignore_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_thread_ignore
          if (C_ASSOCIATED(itt_thread_ignore_fort_ptr)) then
              call C_F_PROCPOINTER(itt_thread_ignore_fort_ptr, thread_ignore_ptr)
              call thread_ignore_ptr()
          end if
      end subroutine itt_thread_ignore

      subroutine itt_suppress_push(mask)
          procedure(itt_proc_sup_push), pointer :: suppress_push_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_suppress_push
          integer, intent(in), value :: mask
          if (C_ASSOCIATED(itt_suppress_push_fort_ptr)) then
              call C_F_PROCPOINTER(itt_suppress_push_fort_ptr, suppress_push_ptr)
              call suppress_push_ptr(mask)
          end if
      end subroutine itt_suppress_push

      subroutine itt_suppress_pop()
          procedure(itt_proc_none), pointer :: suppress_pop_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_suppress_pop
          if (C_ASSOCIATED(itt_suppress_pop_fort_ptr)) then
              call C_F_PROCPOINTER(itt_suppress_pop_fort_ptr, suppress_pop_ptr)
              call suppress_pop_ptr()
          end if
      end subroutine itt_suppress_pop

      subroutine itt_suppress_mark_range(action, mask, addr, size)
          procedure(itt_proc_sup_range), pointer :: suppress_mark_range_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_suppress_mark_range
          integer, intent(in), value :: action
          integer, intent(in), value :: mask
          integer(kind=itt_ptr), intent(in), value :: addr
          integer(kind=itt_ptr), intent(in), value :: size
          if (C_ASSOCIATED(itt_suppress_mark_range_fort_ptr)) then
              call C_F_PROCPOINTER(itt_suppress_mark_range_fort_ptr, suppress_mark_range_ptr)
              call suppress_mark_range_ptr(action, mask, addr, size)
          end if
      end subroutine itt_suppress_mark_range

      subroutine itt_suppress_clear_range(action, mask, addr, size)
          procedure(itt_proc_sup_range), pointer :: suppress_clear_range_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_suppress_clear_range
          integer, intent(in), value :: action
          integer, intent(in), value :: mask
          integer(kind=itt_ptr), intent(in), value :: addr
          integer(kind=itt_ptr), intent(in), value :: size
          if (C_ASSOCIATED(itt_suppress_clear_range_fort_ptr)) then
              call C_F_PROCPOINTER(itt_suppress_clear_range_fort_ptr, suppress_clear_range_ptr)
              call suppress_clear_range_ptr(action, mask, addr, size)
          end if
      end subroutine itt_suppress_clear_range

      subroutine itt_sync_prepare(addr)
          procedure(itt_proc_address), pointer :: sync_prepare_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_prepare
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_sync_prepare_fort_ptr)) then
              call C_F_PROCPOINTER(itt_sync_prepare_fort_ptr, sync_prepare_ptr)
              call sync_prepare_ptr(addr)
          end if
      end subroutine itt_sync_prepare

      subroutine itt_sync_cancel(addr)
          procedure(itt_proc_address), pointer :: sync_cancel_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_cancel
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_sync_cancel_fort_ptr)) then
              call C_F_PROCPOINTER(itt_sync_cancel_fort_ptr, sync_cancel_ptr)
              call sync_cancel_ptr(addr)
          end if
      end subroutine itt_sync_cancel

      subroutine itt_sync_acquired(addr)
          procedure(itt_proc_address), pointer :: sync_acquired_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_acquired
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_sync_acquired_fort_ptr)) then
              call C_F_PROCPOINTER(itt_sync_acquired_fort_ptr, sync_acquired_ptr)
              call sync_acquired_ptr(addr)
          end if
      end subroutine itt_sync_acquired

      subroutine itt_sync_releasing(addr)
          procedure(itt_proc_address), pointer :: sync_releasing_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_releasing
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_sync_releasing_fort_ptr)) then
              call C_F_PROCPOINTER(itt_sync_releasing_fort_ptr, sync_releasing_ptr)
              call sync_releasing_ptr(addr)
          end if
      end subroutine itt_sync_releasing

      subroutine itt_fsync_prepare(addr)
          procedure(itt_proc_address), pointer :: fsync_prepare_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_fsync_prepare
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_fsync_prepare_fort_ptr)) then
              call C_F_PROCPOINTER(itt_fsync_prepare_fort_ptr, fsync_prepare_ptr)
              call fsync_prepare_ptr(addr)
          end if
      end subroutine itt_fsync_prepare

      subroutine itt_fsync_cancel(addr)
          procedure(itt_proc_address), pointer :: fsync_cancel_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_fsync_cancel
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_fsync_cancel_fort_ptr)) then
              call C_F_PROCPOINTER(itt_fsync_cancel_fort_ptr, fsync_cancel_ptr)
              call fsync_cancel_ptr(addr)
          end if
      end subroutine itt_fsync_cancel

      subroutine itt_fsync_acquired(addr)
          procedure(itt_proc_address), pointer :: fsync_acquired_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_fsync_acquired
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_fsync_acquired_fort_ptr)) then
              call C_F_PROCPOINTER(itt_fsync_acquired_fort_ptr, fsync_acquired_ptr)
              call fsync_acquired_ptr(addr)
          end if
      end subroutine itt_fsync_acquired

      subroutine itt_fsync_releasing(addr)
          procedure(itt_proc_address), pointer :: fsync_releasing_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_fsync_releasing
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_fsync_releasing_fort_ptr)) then
              call C_F_PROCPOINTER(itt_fsync_releasing_fort_ptr, fsync_releasing_ptr)
              call fsync_releasing_ptr(addr)
          end if
      end subroutine itt_fsync_releasing

      subroutine itt_sync_destroy(addr)
          procedure(itt_proc_address), pointer :: sync_destroy_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_destroy
          integer(kind=itt_ptr), intent(in), value :: addr
          if (C_ASSOCIATED(itt_sync_destroy_fort_ptr)) then
              call C_F_PROCPOINTER(itt_sync_destroy_fort_ptr, sync_destroy_ptr)
              call sync_destroy_ptr(addr)
          end if
      end subroutine itt_sync_destroy

      subroutine itt_sync_create(addr, objname, attribute)
          procedure(itt_proc_create), pointer :: sync_create_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_create
          integer(kind=itt_ptr), intent(in), value :: addr
          character(len=*), intent(in) :: objname
          integer, intent(in), value :: attribute
          CHARACTER(LEN=1,KIND=C_CHAR) :: objnametmp(LEN_TRIM(objname)+1)
          INTEGER                      :: iobjname, nobjname
          if (C_ASSOCIATED(itt_sync_create_fort_ptr)) then
              nobjname = LEN_TRIM(objname)
              DO iobjname = 1, nobjname
                  objnametmp(iobjname) = objname(iobjname:iobjname)
              END DO
              objnametmp(nobjname + 1) = C_NULL_CHAR
              call C_F_PROCPOINTER(itt_sync_create_fort_ptr, sync_create_ptr)
              call sync_create_ptr(addr, objnametmp, attribute)
          end if
      end subroutine itt_sync_create

      subroutine itt_sync_rename(addr, objname)
          procedure(itt_proc_rename), pointer :: sync_rename_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_sync_rename
          integer(kind=itt_ptr), intent(in), value :: addr
          character(len=*), intent(in) :: objname
          CHARACTER(LEN=1,KIND=C_CHAR) :: objnametmp(LEN_TRIM(objname)+1)
          INTEGER                      :: iobjname, nobjname
          if (C_ASSOCIATED(itt_sync_rename_fort_ptr)) then
              nobjname = LEN_TRIM(objname)
              DO iobjname = 1, nobjname
                  objnametmp(iobjname) = objname(iobjname:iobjname)
              END DO
              objnametmp(nobjname + 1) = C_NULL_CHAR
              call C_F_PROCPOINTER(itt_sync_rename_fort_ptr, sync_rename_ptr)
              call sync_rename_ptr(addr, objnametmp)
          end if
      end subroutine itt_sync_rename

      subroutine itt_thread_set_name(name)
          procedure(itt_proc_name), pointer :: thread_set_name_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_thread_set_name
          character(len=*), intent(in) :: name
          CHARACTER(LEN=1,KIND=C_CHAR) :: nametmp(LEN_TRIM(name)+1)
          INTEGER                      :: iname, nname
          if (C_ASSOCIATED(itt_thread_set_name_fort_ptr)) then
              nname = LEN_TRIM(name)
              DO iname = 1, nname
                  nametmp(iname) = name(iname:iname)
              END DO
              nametmp(nname + 1) = C_NULL_CHAR
              call C_F_PROCPOINTER(itt_thread_set_name_fort_ptr, thread_set_name_ptr)
              call thread_set_name_ptr(nametmp)
          end if
      end subroutine itt_thread_set_name

      subroutine itt_heap_record_memory_growth_begin()
          procedure(itt_proc_none), pointer :: heap_record_memory_growth_begin_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_heap_record_memory_growth_begin
          if (C_ASSOCIATED(itt_heap_record_memory_growth_begin_fort_ptr)) then
              call C_F_PROCPOINTER(itt_heap_record_memory_growth_begin_fort_ptr, heap_record_memory_growth_begin_ptr)
              call heap_record_memory_growth_begin_ptr()
          end if
      end subroutine itt_heap_record_memory_growth_begin

      subroutine itt_heap_record_memory_growth_end()
          procedure(itt_proc_none), pointer :: heap_record_memory_growth_end_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_heap_record_memory_growth_end
          if (C_ASSOCIATED(itt_heap_record_memory_growth_end_fort_ptr)) then
              call C_F_PROCPOINTER(itt_heap_record_memory_growth_end_fort_ptr, heap_record_memory_growth_end_ptr)
              call heap_record_memory_growth_end_ptr()
          end if
      end subroutine itt_heap_record_memory_growth_end

      subroutine itt_heap_reset_detection(heapmask)
          procedure(itt_proc_heapmask), pointer :: heap_reset_detection_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_heap_reset_detection
          integer, intent(in), value :: heapmask
          if (C_ASSOCIATED(itt_heap_reset_detection_fort_ptr)) then
              call C_F_PROCPOINTER(itt_heap_reset_detection_fort_ptr, heap_reset_detection_ptr)
              call heap_reset_detection_ptr(heapmask)
          end if
      end subroutine itt_heap_reset_detection

      subroutine itt_heap_record(heapmask)
          procedure(itt_proc_heapmask), pointer :: heap_record_ptr
          !DEC$ ATTRIBUTES DEFAULT :: itt_heap_record
          integer, intent(in), value :: heapmask
          if (C_ASSOCIATED(itt_heap_record_fort_ptr)) then
              call C_F_PROCPOINTER(itt_heap_record_fort_ptr, heap_record_ptr)
              call heap_record_ptr(heapmask)
          end if
      end subroutine itt_heap_record


    end module ittnotify

