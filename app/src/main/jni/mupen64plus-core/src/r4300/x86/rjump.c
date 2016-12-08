/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rjump.c                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdlib.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "main/main.h"
#include "r4300/cached_interp.h"
#include "r4300/macros.h"
#include "r4300/ops.h"
#include "r4300/r4300.h"
#include "r4300/recomp.h"
#include "r4300/recomph.h"

void dyna_jump()
{
    if (*r4300_stop() == 1)
    {
        dyna_stop();
        return;
    }

    if ((*r4300_pc_struct())->reg_cache_infos.need_map)
        *g_dev.r4300.return_address = (unsigned long) ((*r4300_pc_struct())->reg_cache_infos.jump_wrapper);
    else
        *g_dev.r4300.return_address = (unsigned long) (g_dev.r4300.cached_interp.actual->code + (*r4300_pc_struct())->local_addr);
}

#if defined(WIN32) && !defined(__GNUC__) /* this warning disable only works if placed outside of the scope of a function */
#pragma warning(disable:4731) /* frame pointer register 'ebp' modified by inline assembly code */
#endif

void dyna_start(void *code)
{
  /* save the base and stack pointers */
  /* make a call and a pop to retrieve the instruction pointer and save it too */
  /* then call the code(), which should theoretically never return.  */
  /* When dyna_stop() sets the *return_address to the saved EIP, the emulator thread will come back here. */
  /* It will jump to label 2, restore the base and stack pointers, and exit this function */
#if defined(WIN32) && !defined(__GNUC__)
   __asm
   {
     mov g_dev.r4300.save_ebp, ebp
     mov g_dev.r4300.save_esp, esp
     mov g_dev.r4300.save_ebx, ebx
     mov g_dev.r4300.save_esi, esi
     mov g_dev.r4300.save_edi, edi
     call point1
     jmp point2
   point1:
     pop eax
     mov g_dev.r4300.save_eip, eax

     sub esp, 0x10
     and esp, 0xfffffff0
     mov g_dev.r4300.return_address, esp
     sub g_dev.r4300.return_address, 4

     mov eax, code
     call eax
   point2:
     mov ebp, g_dev.r4300.save_ebp
     mov esp, g_dev.r4300.save_esp
     mov ebx, g_dev.r4300.save_ebx
     mov esi, g_dev.r4300.save_esi
     mov edi, g_dev.r4300.save_edi
   }
#elif defined(__GNUC__) && defined(__i386__)
  #if defined(__PIC__)
    /* for -fPIC (shared libraries) */
    #ifndef __GNUC_PREREQ
    #  if defined __GNUC__ && defined __GNUC_MINOR__
    #    define __GNUC_PREREQ(maj, min) \
                ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
    #  else
    #    define __GNUC_PREREQ(maj, min) 0
    #  endif
    #endif

    #if __GNUC_PREREQ (4, 7)
    #  define GET_PC_THUNK_STR(reg) "__x86.get_pc_thunk." #reg
    #else
    #  define GET_PC_THUNK_STR(reg) "__i686.get_pc_thunk." #reg
    #endif
    #define STORE_EBX
    #define LOAD_EBX "call  " GET_PC_THUNK_STR(bx) "     \n" \
                     "addl $_GLOBAL_OFFSET_TABLE_, %%ebx \n"
  #else
    /* for non-PIC binaries */
    #define STORE_EBX "movl %%ebx, %[save_ebx] \n"
    #define LOAD_EBX  "movl %[save_ebx], %%ebx \n"
  #endif

  asm volatile
    (STORE_EBX
     " movl %%ebp, %[save_ebp] \n"
     " movl %%esp, %[save_esp] \n"
     " movl %%esi, %[save_esi] \n"
     " movl %%edi, %[save_edi] \n"
     " call    1f              \n"
     " jmp     2f              \n"
     "1:                       \n"
     " popl %%eax              \n"
     " movl %%eax, %[save_eip] \n"

     " subl $16, %%esp         \n" /* save 16 bytes of padding just in case */
     " andl $-16, %%esp        \n" /* align stack on 16-byte boundary for OSX */
     " movl %%esp, %[return_address] \n"
     " subl $4, %[return_address] \n"

     " call *%[codeptr]        \n"
     "2:                       \n"
     LOAD_EBX
     " movl %[save_ebp], %%ebp \n"
     " movl %[save_esp], %%esp \n"
     " movl %[save_esi], %%esi \n"
     " movl %[save_edi], %%edi \n"
     : [save_ebp]"=m"(g_dev.r4300.save_ebp), [save_esp]"=m"(g_dev.r4300.save_esp), [save_ebx]"=m"(g_dev.r4300.save_ebx), [save_esi]"=m"(g_dev.r4300.save_esi), [save_edi]"=m"(g_dev.r4300.save_edi), [save_eip]"=m"(g_dev.r4300.save_eip), [return_address]"=m"(g_dev.r4300.return_address)
     : [codeptr]"r"(code)
     : "eax", "ecx", "edx", "memory"
     );
#endif

    /* clear the registers so we don't return here a second time; that would be a bug */
    /* this is also necessary to prevent compiler from optimizing out the static variables */
    g_dev.r4300.save_edi=0;
    g_dev.r4300.save_esi=0;
    g_dev.r4300.save_ebx=0;
    g_dev.r4300.save_ebp=0;
    g_dev.r4300.save_esp=0;
    g_dev.r4300.save_eip=0;
}

void dyna_stop()
{
  if (g_dev.r4300.save_eip == 0)
    DebugMessage(M64MSG_WARNING, "instruction pointer is 0 at dyna_stop()");
  else
  {
    *g_dev.r4300.return_address = (unsigned long) g_dev.r4300.save_eip;
  }
}

