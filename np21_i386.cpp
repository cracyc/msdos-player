#ifdef USE_DEBUGGER

/* ----------------------------------------------------------------------------
	MAME i386 disassembler
---------------------------------------------------------------------------- */

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning( disable : 4146 )
#pragma warning( disable : 4244 )
#endif

/*****************************************************************************/
/* src/emu/devcpu.h */

// CPU interface functions
#define CPU_DISASSEMBLE_NAME(name)		cpu_disassemble_##name
#define CPU_DISASSEMBLE(name)			int CPU_DISASSEMBLE_NAME(name)(char *buffer, offs_t eip, const UINT8 *oprom)
#define CPU_DISASSEMBLE_CALL(name)		CPU_DISASSEMBLE_NAME(name)(buffer, eip, oprom)

/*****************************************************************************/
/* src/emu/didisasm.h */

// Disassembler constants
const UINT32 DASMFLAG_SUPPORTED     = 0x80000000;   // are disassembly flags supported?
const UINT32 DASMFLAG_STEP_OUT      = 0x40000000;   // this instruction should be the end of a step out sequence
const UINT32 DASMFLAG_STEP_OVER     = 0x20000000;   // this instruction should be stepped over by setting a breakpoint afterwards
const UINT32 DASMFLAG_OVERINSTMASK  = 0x18000000;   // number of extra instructions to skip when stepping over
const UINT32 DASMFLAG_OVERINSTSHIFT = 27;           // bits to shift after masking to get the value
const UINT32 DASMFLAG_LENGTHMASK    = 0x0000ffff;   // the low 16-bits contain the actual length

/*****************************************************************************/
/* src/emu/memory.h */

// offsets and addresses are 32-bit (for now...)
typedef UINT32	offs_t;

/*****************************************************************************/
/* src/osd/osdcomm.h */

/* Highly useful macro for compile-time knowledge of an array size */
#define ARRAY_LENGTH(x)     (sizeof(x) / sizeof(x[0]))

#ifndef INLINE
#define INLINE inline
#endif

#include "mame/emu/cpu/i386/i386dasm.c"

#undef CPU_DISASSEMBLE

int CPU_DISASSEMBLE(UINT8 *oprom, offs_t eip, bool is_ia32, char *buffer, size_t buffer_len)
{
	if(is_ia32) {
		return CPU_DISASSEMBLE_CALL(x86_32) & DASMFLAG_LENGTHMASK;
	} else {
		return CPU_DISASSEMBLE_CALL(x86_16) & DASMFLAG_LENGTHMASK;
	}
}
#endif

#include "np21_i386c/cpucore.h"
#include "np21_i386c/ia32/ia32.mcr"
#include "np21_i386c/ia32/ctrlxfer.h"
#include "np21_i386c/ia32/instructions/ctrl_trans.h"
#include "np21_i386c/ia32/instructions/flag_ctrl.h"
#include "np21_i386c/ia32/instructions/fpu/fp.h"

#define CPU_ES_BASE			ES_BASE
#define CPU_CS_BASE			CS_BASE
#define CPU_SS_BASE			SS_BASE
#define CPU_DS_BASE			DS_BASE
#define CPU_FS_BASE			FS_BASE
#define CPU_GS_BASE			GS_BASE

#define	CPU_LOAD_SREG(idx, selector)	LOAD_SEGREG(idx, selector)

#define CPU_EIP_CHANGED			(CPU_EIP != CPU_PREV_EIP)

inline void CPU_SET_EIP(UINT32 value)
{
	CPU_EIP = value;
}

#define CPU_C_FLAG			((CPU_FLAGL & C_FLAG) != 0)
#define CPU_Z_FLAG			((CPU_FLAGL & Z_FLAG) != 0)
#define CPU_S_FLAG			((CPU_FLAGL & S_FLAG) != 0)
#define CPU_I_FLAG			((CPU_FLAGH & I_FLAG) != 0)

inline void CPU_SET_C_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  C_FLAG;
	} else {
		CPU_FLAG &= ~C_FLAG;
	}
}

inline void CPU_SET_P_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  P_FLAG;
	} else {
		CPU_FLAG &= ~P_FLAG;
	}
}

inline void CPU_SET_A_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  A_FLAG;
	} else {
		CPU_FLAG &= ~A_FLAG;
	}
}

inline void CPU_SET_Z_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  Z_FLAG;
	} else {
		CPU_FLAG &= ~Z_FLAG;
	}
}

inline void CPU_SET_S_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  S_FLAG;
	} else {
		CPU_FLAG &= ~S_FLAG;
	}
}

inline void CPU_SET_T_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  T_FLAG;
	} else {
		CPU_FLAG &= ~T_FLAG;
	}
}

inline void CPU_SET_I_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  I_FLAG;
	} else {
		CPU_FLAG &= ~I_FLAG;
	}
}

inline void CPU_SET_D_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  D_FLAG;
	} else {
		CPU_FLAG &= ~D_FLAG;
	}
}

inline void CPU_SET_O_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  O_FLAG;
	} else {
		CPU_FLAG &= ~O_FLAG;
	}
}

inline void CPU_SET_IOP_FLAG(UINT8 value)
{
	CPU_FLAG &= ~IOPL_FLAG;
	CPU_FLAG |= value << 12;
}

inline void CPU_SET_NT_FLAG(UINT8 value)
{
	if(value) {
		CPU_FLAG |=  NT_FLAG;
	} else {
		CPU_FLAG &= ~NT_FLAG;
	}
}

inline void CPU_SET_VM_FLAG(UINT8 value)
{
	if(value) {
		if(!(CPU_EFLAG & VM_FLAG)) {
			CPU_EFLAG |=  VM_FLAG;
			change_vm(1);
		}
	} else {
		if(CPU_EFLAG & VM_FLAG) {
			CPU_EFLAG &= ~VM_FLAG;
			change_vm(0);
		}
	}
}

inline void CPU_SET_CR0(UINT32 src)
{
	// from MOV_CdRd(void) in ia32/instructions/system_inst.c
	UINT32 reg = CPU_CR0;
	src &= CPU_CR0_ALL;
#if defined(USE_FPU)
	if(i386cpuid.cpu_feature & CPU_FEATURE_FPU){
		src |= CPU_CR0_ET;	/* FPU present */
		//src &= ~CPU_CR0_EM;
	} else {
		src |= CPU_CR0_EM | CPU_CR0_NE;
		src &= ~(CPU_CR0_MP | CPU_CR0_ET);
	}
#else
	src |= CPU_CR0_EM | CPU_CR0_NE;
	src &= ~(CPU_CR0_MP | CPU_CR0_ET);
#endif
	CPU_CR0 = src;

	if ((reg ^ CPU_CR0) & (CPU_CR0_PE|CPU_CR0_PG)) {
		tlb_flush_all();
	}
	if ((reg ^ CPU_CR0) & CPU_CR0_PE) {
		if (CPU_CR0 & CPU_CR0_PE) {
			change_pm(1);
		}
	}
	if ((reg ^ CPU_CR0) & CPU_CR0_PG) {
		if (CPU_CR0 & CPU_CR0_PG) {
			change_pg(1);
		} else {
			change_pg(0);
		}
	}
	if ((reg ^ CPU_CR0) & CPU_CR0_PE) {
		if (!(CPU_CR0 & CPU_CR0_PE)) {
			change_pm(0);
		}
	}

	CPU_STAT_WP = (CPU_CR0 & CPU_CR0_WP) ? 0x10 : 0;
}

inline void CPU_SET_CR3(UINT32 value)
{
	set_cr3(value);
}

inline void CPU_SET_CPL(int value)
{
	set_cpl(value);
}

inline void CPU_SET_EFLAG(UINT32 new_flags)
{
	// from modify_eflags(UINT32 new_flags, UINT32 mask) in ia32/ia32.cpp
	UINT32 orig = CPU_EFLAG;

	new_flags &= ALL_EFLAG;
//	mask &= ALL_EFLAG;
//	CPU_EFLAG = (REAL_EFLAGREG & ~mask) | (new_flags & mask);
	CPU_EFLAG = new_flags;

	CPU_OV = CPU_FLAG & O_FLAG;
	CPU_TRAP = (CPU_FLAG & (I_FLAG|T_FLAG)) == (I_FLAG|T_FLAG);
	if (CPU_STAT_PM) {
		if ((orig ^ CPU_EFLAG) & VM_FLAG) {
			if (CPU_EFLAG & VM_FLAG) {
				change_vm(1);
			} else {
				change_vm(0);
			}
		}
	}
}

inline void CPU_A20_LINE(UINT8 value)
{
	ia32a20enable(value != 0);
}

#ifdef USE_DEBUGGER
UINT16 CPU_PREV_CS;
#endif
UINT32 CPU_PREV_PC;
BOOL irq_pending = FALSE;

inline void CPU_IRQ_LINE(BOOL state)
{
	irq_pending = state;
}

void CPU_INIT()
{
	CPU_INITIALIZE();
	CPU_ADRSMASK = ~0;
//	CPU_ADRSMASK = ~(1 << 20);
	irq_pending = false;
}

void CPU_RELEASE()
{
	CPU_DEINITIALIZE();
}

void CPU_FINISH()
{
	CPU_RELEASE();
}

void CPU_RESET()
{
	// from pccore_reset(void) in pccore.c
	
	// enable all features
	i386cpuid.cpu_family = CPU_FAMILY;
	i386cpuid.cpu_model = CPU_MODEL;
	i386cpuid.cpu_stepping = CPU_STEPPING;
	i386cpuid.cpu_feature = CPU_FEATURES_ALL;
	i386cpuid.cpu_feature_ex = CPU_FEATURES_EX_ALL;
	i386cpuid.cpu_feature_ecx = CPU_FEATURES_ECX_ALL;
	i386cpuid.cpu_eflags_mask = CPU_EFLAGS_MASK;
	i386cpuid.allow_movCS = 0;
	i386cpuid.cpu_brandid = CPU_BRAND_ID_NEKOPRO2;
	strcpy(i386cpuid.cpu_vendor, CPU_VENDOR_NEKOPRO);
	strcpy(i386cpuid.cpu_brandstring, CPU_BRAND_STRING_NEKOPRO2);

//	i386cpuid.fpu_type = FPU_TYPE_SOFTFLOAT;
//	i386cpuid.fpu_type = FPU_TYPE_DOSBOX;
	i386cpuid.fpu_type = FPU_TYPE_DOSBOX2;
	fpu_initialize();

	UINT32 PREV_CPU_ADRSMASK = CPU_ADRSMASK;
//	CPU_RESET();
	ia32reset();
	CPU_TYPE = 0;
	CS_BASE = 0xf0000;
	CPU_CS = 0xf000;
	CPU_IP = 0xfff0;
	CPU_CLEARPREFETCH();
	CPU_ADRSMASK = PREV_CPU_ADRSMASK;
}

UINT32 CPU_GET_NEXT_PC();

void CPU_EXECUTE()
{
#ifdef USE_DEBUGGER
	if(now_debugging) {
		if(force_suspend) {
			force_suspend = false;
			now_suspended = true;
		} else {
			UINT32 next_pc = CPU_GET_NEXT_PC();
			for(int i = 0; i < MAX_BREAK_POINTS; i++) {
				if(break_point.table[i].status == 1 && break_point.table[i].seg == CPU_CS && break_point.table[i].ofs == CPU_EIP) {
					break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
		while(now_debugging && now_suspended) {
			Sleep(10);
		}
	}
#endif

#ifdef USE_CLOCK
	CPU_REMCLOCK = CPU_BASECLOCK = 1;
#endif
	CPU_EXEC();
//	if(nmi_pending) {
//		CPU_INTERRUPT(2, 0);
//		nmi_pending = false;
//	} else
	if(irq_pending && CPU_isEI) {
		CPU_INTERRUPT(pic_ack(), 0);
		irq_pending = false;
		pic_update();
	}
//	return CPU_BASECLOCK - CPU_REMCLOCK;

#ifdef USE_DEBUGGER
	if(now_debugging) {
		if(!now_going) {
			now_suspended = true;
		}
	}
#endif
}

void CPU_SOFT_INTERRUPT(int vect)
{
	CPU_INTERRUPT(vect, 1);
}

void CPU_JMP_FAR(UINT16 new_cs, UINT32 new_ip)
{
	descriptor_t sd;
	UINT16 sreg;

	if (!CPU_STAT_PM || CPU_STAT_VM86) {
		/* Real mode or VM86 mode */
		/* check new instrunction pointer with new code segment */
		load_segreg(CPU_CS_INDEX, new_cs, &sreg, &sd, GP_EXCEPTION);
		if (new_ip > sd.u.seg.limit) {
			EXCEPTION(GP_EXCEPTION, 0);
		}
		LOAD_SEGREG(CPU_CS_INDEX, new_cs);
		CPU_EIP = new_ip;
	} else {
		/* Protected mode */
		JMPfar_pm(new_cs, new_ip);
	}
}

void CPU_CALL_FAR(UINT16 new_cs, UINT32 new_ip)
{
	descriptor_t sd;
	UINT16 sreg;

	if (!CPU_STAT_PM || CPU_STAT_VM86) {
		/* Real mode or VM86 mode */
		CPU_SET_PREV_ESP();
		load_segreg(CPU_CS_INDEX, new_cs, &sreg, &sd, GP_EXCEPTION);
		if (new_ip > sd.u.seg.limit) {
			EXCEPTION(GP_EXCEPTION, 0);
		}

		PUSH0_16(CPU_CS);
		PUSH0_16(CPU_EIP);

		LOAD_SEGREG(CPU_CS_INDEX, new_cs);
		CPU_EIP = new_ip;
		CPU_CLEAR_PREV_ESP();
	} else {
		/* Protected mode */
		CALLfar_pm(new_cs, new_ip);
	}
}

void CPU_IRET()
{
	// Don't call msdos_syscall() in iret routine
	UINT32 tmp = CPU_PREV_PC;
	CPU_PREV_PC = 0;
	IRET();
	CPU_PREV_PC = tmp;
}

void CPU_PUSH(UINT16 reg)
{
	if (!CPU_STAT_SS32) {
		UINT16 __new_sp = CPU_SP - 2;
		cpu_vmemorywrite_w(CPU_SS_INDEX, __new_sp, reg);
		CPU_SP = __new_sp;
	} else {
		UINT32 __new_esp = CPU_ESP - 2;
		cpu_vmemorywrite_w(CPU_SS_INDEX, __new_esp, reg);
		CPU_ESP = __new_esp;
	}
}

UINT16 CPU_POP()
{
	UINT16 reg;

	if (!CPU_STAT_SS32) {
		reg = cpu_vmemoryread_w(CPU_SS_INDEX, CPU_SP);
		CPU_SP += 2;
	} else {
		reg = cpu_vmemoryread_w(CPU_SS_INDEX, CPU_ESP);
		CPU_ESP += 2;
	}
	return reg;
}

void CPU_PUSHF()
{
	PUSHF_Fw();
}

UINT16 CPU_READ_STACK()
{
	UINT16 reg;

	if (!CPU_STAT_SS32) {
		reg = cpu_vmemoryread_w(CPU_SS_INDEX, CPU_SP);
//		CPU_SP += 2;
	} else {
		reg = cpu_vmemoryread_w(CPU_SS_INDEX, CPU_ESP);
//		CPU_ESP += 2;
	}
	return reg;
}

void CPU_WRITE_STACK(UINT16 reg)
{
	if (!CPU_STAT_SS32) {
		UINT16 __new_sp = CPU_SP - 2;
		cpu_vmemorywrite_w(CPU_SS_INDEX, __new_sp, reg);
//		CPU_SP = __new_sp;
	} else {
		UINT32 __new_esp = CPU_ESP - 2;
		cpu_vmemorywrite_w(CPU_SS_INDEX, __new_esp, reg);
//		CPU_ESP = __new_esp;
	}
}

void CPU_LOAD_LDTR(UINT16 selector)
{
	load_ldtr(selector, GP_EXCEPTION);
}

void CPU_LOAD_TR(UINT16 selector)
{
	load_tr(selector);
}

UINT32 CPU_TRANS_PAGING_ADDR(UINT32 addr)
{
	if(CPU_STAT_PM && CPU_STAT_PAGING) {
		UINT32 pde_addr = CPU_STAT_PDE_BASE + ((addr >> 20) & 0xffc);
		UINT32 pde = read_dword(pde_addr & CPU_ADRSMASK);
		/* XXX: check */
		UINT32 pte_addr = (pde & CPU_PDE_BASEADDR_MASK) + ((addr >> 10) & 0xffc);
		UINT32 pte = read_dword(pte_addr & CPU_ADRSMASK);
		/* XXX: check */
		addr = (pte & CPU_PTE_BASEADDR_MASK) + (addr & CPU_PAGE_MASK);
	}
	return addr;
}

#ifdef USE_DEBUGGER
UINT32 CPU_TRANS_CODE_ADDR(UINT32 seg, UINT32 ofs)
{
	if(CPU_STAT_PM && !CPU_STAT_VM86) {
		selector_t sel;
		descriptor_t *sdp = &sel.desc;

		//segdesc_set_default(CPU_CS_INDEX, seg, sdp);
		sdp->u.seg.segbase = seg << 4;
		sdp->u.seg.limit = 0xffff;
		sdp->u.seg.c = 1;	/* code or data */
		sdp->u.seg.g = 0;	/* non 4k factor scale */
		sdp->u.seg.wr = 1;	/* execute/read(CS) or read/write(others) */
		sdp->u.seg.ec = 0;	/* nonconforming(CS) or expand-up(others) */
		sdp->valid = 1;		/* valid */
		sdp->p = 1;		/* present */
		sdp->type = CPU_SEGDESC_TYPE_WR  | (CPU_SEGDESC_H_D_C >> CPU_DESC_H_TYPE_SHIFT);
		sdp->dpl = CPU_STAT_VM86 ? 3 : 0; /* descriptor privilege level */
		sdp->rpl = CPU_STAT_VM86 ? 3 : 0; /* request privilege level */
		sdp->s = 1;		/* code/data */
		sdp->d = 0;		/* 16bit */
		sdp->flag = CPU_DESC_FLAG_READABLE|CPU_DESC_FLAG_WRITABLE;

		parse_selector(&sel, seg);
		return CPU_TRANS_PAGING_ADDR(sdp->u.seg.segbase + ofs);
	}
	return (seg << 4) + ofs;
}

UINT32 CPU_GET_PREV_PC()
{
	return CPU_PREV_PC;
}

UINT32 CPU_GET_NEXT_PC()
{
	descriptor_t *sdp = &CPU_CS_DESC;
	return CPU_TRANS_PAGING_ADDR(sdp->u.seg.segbase + CPU_EIP);
}
#endif
