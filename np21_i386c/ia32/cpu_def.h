/*
 * Copyright (c) 2002-2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
	Intel Architecture 32-bit Processor Interpreter Engine for Pentium

				Copyright by Yui/Studio Milmake 1999-2000
				Copyright by Norio HATTORI 2000,2001
				Copyright by NONAKA Kimihiro 2002-2004
*/

#ifndef IA32_CPU_CPU_DEF_H__
#define IA32_CPU_CPU_DEF_H__

//#define SUPPORT_FPU_DOSBOX
//#define SUPPORT_FPU_DOSBOX2
//#define SUPPORT_FPU_SOFTFLOAT
#define SUPPORT_FPU_SOFTFLOAT3
#define USE_FPU
#define USE_MMX
#define USE_3DNOW
#define USE_SSE
#define USE_SSE2
#define USE_SSE3
#define USE_SSSE3
#define USE_SSE4_1
#define USE_SSE4_2
#define USE_SSE4A
#define USE_TSC
#define USE_PAGING
#define USE_FASTPAGING
#define USE_VME
//#define USE_CPU_MODRMPREFETCH
#define USE_CPU_PLATFORMINT
#define USE_CPU_INLINEINST
#define USE_CPU_DIRECTREG
#define USE_CPU_EIPMASK
#define USE_CPU_BULKREP
//#define USE_LEGACY_MEMORY_ACCESS
//#define USE_CLOCK
//#define IA32_INSTRUCTION_TRACE
#define IA32_REBOOT_ON_PANIC

#endif	/* !IA32_CPU_CPU_DEF_H__ */
