/* ----------------------------------------------------------------------------
	MAME i386
---------------------------------------------------------------------------- */

// disable warnings for Microsoft Visual C++ 2005 or later
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
// for MAME i86/i386
#pragma warning( disable : 4065 )
#pragma warning( disable : 4146 )
#pragma warning( disable : 4267 )
#endif

#ifndef __BIG_ENDIAN__
#define LSB_FIRST
#endif

#ifndef INLINE
#define INLINE inline
#endif
#define U64(v) UINT64(v)

//#define logerror(...) fprintf(stderr, __VA_ARGS__)
#define logerror(...)
//#define popmessage(...) fprintf(stderr, __VA_ARGS__)
#define popmessage(...)

/*****************************************************************************/
/* src/emu/devcpu.h */

// CPU interface functions
#define CPU_INIT_NAME(name)			cpu_init_##name
#define CPU_INIT(name)				void CPU_INIT_NAME(name)()
#define CPU_INIT_CALL(name)			CPU_INIT_NAME(name)()

#define CPU_RESET_NAME(name)			cpu_reset_##name
#define CPU_RESET(name)				void CPU_RESET_NAME(name)()
#define CPU_RESET_CALL(name)			CPU_RESET_NAME(name)()

#define CPU_EXECUTE_NAME(name)			cpu_execute_##name
#define CPU_EXECUTE(name)			void CPU_EXECUTE_NAME(name)()
#define CPU_EXECUTE_CALL(name)			CPU_EXECUTE_NAME(name)()

#define CPU_TRANSLATE_NAME(name)		cpu_translate_##name
#define CPU_TRANSLATE(name)			int CPU_TRANSLATE_NAME(name)(address_spacenum space, int intention, offs_t *address)
#define CPU_TRANSLATE_CALL(name)		CPU_TRANSLATE_NAME(name)(space, intention, address)

#define CPU_DISASSEMBLE_NAME(name)		cpu_disassemble_##name
#define CPU_DISASSEMBLE(name)			int CPU_DISASSEMBLE_NAME(name)(char *buffer, offs_t eip, const UINT8 *oprom)
#define CPU_DISASSEMBLE_CALL(name)		CPU_DISASSEMBLE_NAME(name)(buffer, eip, oprom)

#define CPU_MODEL_STR(name)			#name
#define CPU_MODEL_NAME(name)			CPU_MODEL_STR(name)

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
/* src/emu/diexec.h */

// I/O line states
enum line_state
{
	CLEAR_LINE = 0,				// clear (a fired or held) line
	ASSERT_LINE,				// assert an interrupt immediately
	HOLD_LINE,				// hold interrupt line until acknowledged
	PULSE_LINE				// pulse interrupt line instantaneously (only for NMI, RESET)
};

// I/O line definitions
enum
{
	INPUT_LINE_IRQ = 0,
	INPUT_LINE_NMI
};

/*****************************************************************************/
/* src/emu/dimemory.h */

// Translation intentions
const int TRANSLATE_TYPE_MASK       = 0x03;     // read write or fetch
const int TRANSLATE_USER_MASK       = 0x04;     // user mode or fully privileged
const int TRANSLATE_DEBUG_MASK      = 0x08;     // debug mode (no side effects)

const int TRANSLATE_READ            = 0;        // translate for read
const int TRANSLATE_WRITE           = 1;        // translate for write
const int TRANSLATE_FETCH           = 2;        // translate for instruction fetch
const int TRANSLATE_READ_USER       = (TRANSLATE_READ | TRANSLATE_USER_MASK);
const int TRANSLATE_WRITE_USER      = (TRANSLATE_WRITE | TRANSLATE_USER_MASK);
const int TRANSLATE_FETCH_USER      = (TRANSLATE_FETCH | TRANSLATE_USER_MASK);
const int TRANSLATE_READ_DEBUG      = (TRANSLATE_READ | TRANSLATE_DEBUG_MASK);
const int TRANSLATE_WRITE_DEBUG     = (TRANSLATE_WRITE | TRANSLATE_DEBUG_MASK);
const int TRANSLATE_FETCH_DEBUG     = (TRANSLATE_FETCH | TRANSLATE_DEBUG_MASK);

/*****************************************************************************/
/* src/emu/emucore.h */

// constants for expression endianness
enum endianness_t
{
	ENDIANNESS_LITTLE,
	ENDIANNESS_BIG
};

// declare native endianness to be one or the other
#ifdef LSB_FIRST
const endianness_t ENDIANNESS_NATIVE = ENDIANNESS_LITTLE;
#else
const endianness_t ENDIANNESS_NATIVE = ENDIANNESS_BIG;
#endif

// endian-based value: first value is if 'endian' is little-endian, second is if 'endian' is big-endian
#define ENDIAN_VALUE_LE_BE(endian,leval,beval)	(((endian) == ENDIANNESS_LITTLE) ? (leval) : (beval))

// endian-based value: first value is if native endianness is little-endian, second is if native is big-endian
#define NATIVE_ENDIAN_VALUE_LE_BE(leval,beval)	ENDIAN_VALUE_LE_BE(ENDIANNESS_NATIVE, leval, beval)

// endian-based value: first value is if 'endian' matches native, second is if 'endian' doesn't match native
#define ENDIAN_VALUE_NE_NNE(endian,leval,beval)	(((endian) == ENDIANNESS_NATIVE) ? (neval) : (nneval))

/*****************************************************************************/
/* src/emu/emumem.h */

// helpers for checking address alignment
#define WORD_ALIGNED(a)                 (((a) & 1) == 0)
#define DWORD_ALIGNED(a)                (((a) & 3) == 0)
#define QWORD_ALIGNED(a)                (((a) & 7) == 0)

/*****************************************************************************/
/* src/emu/memory.h */

// address spaces
enum address_spacenum
{
	AS_0,                           // first address space
	AS_1,                           // second address space
	AS_2,                           // third address space
	AS_3,                           // fourth address space
	ADDRESS_SPACES,                 // maximum number of address spaces

	// alternate address space names for common use
	AS_PROGRAM = AS_0,              // program address space
	AS_DATA = AS_1,                 // data address space
	AS_IO = AS_2                    // I/O address space
};

// offsets and addresses are 32-bit (for now...)
typedef UINT32	offs_t;

#define read_decrypted_byte read_byte
#define read_decrypted_word read_word
#define read_decrypted_dword read_dword

#define read_raw_byte read_byte
#define write_raw_byte write_byte

#define read_word_unaligned read_word
#define write_word_unaligned write_word

#define read_io_word_unaligned read_io_word
#define write_io_word_unaligned write_io_word

/*****************************************************************************/
/* src/osd/osdcomm.h */

/* Highly useful macro for compile-time knowledge of an array size */
#define ARRAY_LENGTH(x)     (sizeof(x) / sizeof(x[0]))

static CPU_TRANSLATE(i386);
#include "mame/lib/softfloat/softfloat.c"
#include "mame/lib/softfloat/fsincos.c"
#include "mame/emu/cpu/i386/i386.c"
#include "mame/emu/cpu/vtlb.c"

#undef CPU_INIT
#undef CPU_RESET
#undef CPU_EXECUTE

#ifdef USE_DEBUGGER
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

#define CPU_EAX				REG32(EAX)
#define CPU_ECX				REG32(ECX)
#define CPU_EDX				REG32(EDX)
#define CPU_EBX				REG32(EBX)
#define CPU_ESP				REG32(ESP)
#define CPU_EBP				REG32(EBP)
#define CPU_ESI				REG32(ESI)
#define CPU_EDI				REG32(EDI)

#define CPU_AX				REG16(AX)
#define CPU_CX				REG16(CX)
#define CPU_DX				REG16(DX)
#define CPU_BX				REG16(BX)
#define CPU_SP				REG16(SP)
#define CPU_BP				REG16(BP)
#define CPU_SI				REG16(SI)
#define CPU_DI				REG16(DI)

#define CPU_AL				REG8(AL)
#define CPU_CL				REG8(CL)
#define CPU_DL				REG8(DL)
#define CPU_BL				REG8(BL)
#define CPU_AH				REG8(AH)
#define CPU_CH				REG8(CH)
#define CPU_DH				REG8(DH)
#define CPU_BH				REG8(BH)

#define CPU_ES				m_sreg[ES].selector
#define CPU_CS				m_sreg[CS].selector
#define CPU_SS				m_sreg[SS].selector
#define CPU_DS				m_sreg[DS].selector
#define CPU_FS				m_sreg[FS].selector
#define CPU_GS				m_sreg[GS].selector

#ifdef USE_DEBUGGER
#define CPU_PREV_CS			m_prev_cs
#endif

#define CPU_ES_BASE			m_sreg[ES].base
#define CPU_CS_BASE			m_sreg[CS].base
#define CPU_SS_BASE			m_sreg[SS].base
#define CPU_DS_BASE			m_sreg[DS].base
#define CPU_FS_BASE			m_sreg[FS].base
#define CPU_GS_BASE			m_sreg[GS].base

#define CPU_ES_INDEX			ES
#define CPU_CS_INDEX			CS
#define CPU_SS_INDEX			SS
#define CPU_DS_INDEX			DS
#define CPU_FS_INDEX			FS
#define CPU_GS_INDEX			GS

inline void CPU_LOAD_SREG(UINT8 reg, UINT16 selector)
{
	i386_sreg_load(selector, reg, NULL);
}

#define CPU_EIP_CHANGED			(m_eip != m_prev_eip)
#define CPU_EIP				m_eip
#define CPU_PREV_EIP			m_prev_eip

inline void CPU_SET_EIP(UINT32 value)
{
	m_eip = value;
	CHANGE_PC(m_eip);
}

#define CPU_GDTR_LIMIT			m_gdtr.limit
#define CPU_GDTR_BASE			m_gdtr.base
#define CPU_IDTR_LIMIT			m_idtr.limit
#define CPU_IDTR_BASE			m_idtr.base

#define CPU_MSW				m_cr[0]

#define CPU_CR0				m_cr[0]
#define CPU_CR1				m_cr[1]
#define CPU_CR2				m_cr[2]
#define CPU_CR3				m_cr[3]
#define CPU_CR4				m_cr[4]

#define	CPU_CR0_PE			(1 << 0)
#define	CPU_CR0_PG			(1 << 31)

#define CPU_DR(x)			m_dr[x]

#define CPU_C_FLAG			(m_CF != 0)
#define CPU_Z_FLAG			(m_ZF != 0)
#define CPU_S_FLAG			(m_SF != 0)
#define CPU_I_FLAG			(m_IF != 0)

inline void CPU_SET_C_FLAG(UINT8 value)
{
	m_CF = value;
}

inline void CPU_SET_Z_FLAG(UINT8 value)
{
	m_ZF = value;
}

inline void CPU_SET_S_FLAG(UINT8 value)
{
	m_SF = value;
}

inline void CPU_SET_I_FLAG(UINT8 value)
{
	m_IF = value;
}

inline void CPU_SET_IOP_FLAG(UINT8 value)
{
	m_IOP1 = (value     ) & 1;
	m_IOP2 = (value >> 1) & 1;
}

inline void CPU_SET_NT_FLAG(UINT8 value)
{
	m_NT = value;
}

inline void CPU_SET_VM_FLAG(UINT8 value)
{
	m_VM = value;
}

inline void CPU_SET_CR0(UINT32 value)
{
	m_cr[0] = value;
}

inline void CPU_SET_CR3(UINT32 value)
{
	m_cr[3] = value;
}

inline void CPU_SET_CPL(int value)
{
	m_CPL = value;
}

#define CPU_STAT_PM			PROTECTED_MODE
#define CPU_STAT_VM86			V8086_MODE

#define CPU_EFLAG			get_flags()
#define CPU_SET_EFLAG(x)		set_flags(x)

#define CPU_ADRSMASK			m_a20_mask

#define CPU_A20_LINE(x)			i386_set_a20_line(x)

inline void CPU_IRQ_LINE(int state)
{
	if(state) {
		i386_set_irq_line(INPUT_LINE_IRQ, HOLD_LINE);
	} else {
		i386_set_irq_line(INPUT_LINE_IRQ, CLEAR_LINE);
	}
}

#define CPU_INST_OP32			m_operand_size

void CPU_INIT()
{
	CPU_INIT_CALL(CPU_MODEL);
}

void CPU_RELEASE()
{
	vtlb_free(m_vtlb);
}

void CPU_FINISH()
{
	CPU_RELEASE();
}

void CPU_RESET()
{
	CPU_RESET_CALL(CPU_MODEL);
}

void CPU_EXECUTE()
{
	CPU_EXECUTE_CALL(i386);
}

void CPU_SOFT_INTERRUPT(int irq)
{
	m_ext = 0; // not an external interrupt
	i386_trap(irq, 1, 0);
	m_ext = 1;
}

void CPU_JMP_FAR(UINT16 selector, UINT32 address)
{
	if(PROTECTED_MODE && !V8086_MODE) {
		i386_protected_mode_jump(selector, address, 1, m_operand_size);
	} else {
		m_sreg[CS].selector = selector;
		m_performed_intersegment_jump = 1;
		i386_load_segment_descriptor(CS);
		m_eip = address;
		CHANGE_PC(m_eip);
	}
}

void CPU_CALL_FAR(UINT16 selector, UINT32 address)
{
	if(PROTECTED_MODE && !V8086_MODE) {
		i386_protected_mode_call(selector, address, 1, m_operand_size);
	} else {
		PUSH16(m_sreg[CS].selector);
		PUSH16(m_eip);
		m_sreg[CS].selector = selector;
		m_performed_intersegment_jump = 1;
		i386_load_segment_descriptor(CS);
		m_eip = address;
		CHANGE_PC(m_eip);
	}
}

void CPU_IRET()
{
	// Don't call msdos_syscall() in iret routine
	m_pc = 1;
	I386OP(iret16)();
}

void CPU_PUSH(UINT16 value)
{
	PUSH16(value);
}

UINT16 CPU_POP()
{
	return POP16();
}

void CPU_PUSHF()
{
	I386OP(pushf)();
}

UINT16 CPU_READ_STACK()
{
	UINT32 ea, new_esp;
	if( STACK_32BIT ) {
		new_esp = REG32(ESP) + 2;
		ea = i386_translate(SS, new_esp - 2, 0, 2);
	} else {
		new_esp = REG16(SP) + 2;
		ea = i386_translate(SS, (new_esp - 2) & 0xffff, 0, 2);
	}
	return READ16(ea);
}

void CPU_WRITE_STACK(UINT16 value)
{
	UINT32 ea, new_esp;
	if( STACK_32BIT ) {
		new_esp = REG32(ESP) + 2;
		ea = i386_translate(SS, new_esp - 2, 0, 2);
	} else {
		new_esp = REG16(SP) + 2;
		ea = i386_translate(SS, (new_esp - 2) & 0xffff, 0, 2);
	}
	WRITE16(ea, value);
}

void CPU_LOAD_LDTR(UINT16 selector)
{
	I386_SREG seg;
	m_ldtr.segment = selector;
	seg.selector = selector;
	i386_load_protected_mode_segment(&seg, NULL);
	m_ldtr.limit = seg.limit;
	m_ldtr.base = seg.base;
	m_ldtr.flags = seg.flags;
}

void CPU_LOAD_TR(UINT16 selector)
{
	I386_SREG seg;
	m_task.segment = selector;
	seg.selector = selector;
	i386_load_protected_mode_segment(&seg, NULL);
	m_task.limit = seg.limit;
	m_task.base = seg.base;
	m_task.flags = seg.flags;
}

UINT32 CPU_TRANS_PAGING_ADDR(UINT32 addr)
{
	translate_address(0, TRANSLATE_READ, &addr, NULL);
	return addr;
}

#ifdef USE_DEBUGGER
UINT32 CPU_TRANS_CODE_ADDR(UINT32 seg, UINT32 ofs)
{
	if(PROTECTED_MODE && !V8086_MODE) {
		I386_SREG pseg;
		pseg.selector = seg;
		i386_load_protected_mode_segment(&pseg, NULL);
		return pseg.base + ofs;
	}
	return (seg << 4) + ofs;
}

UINT32 CPU_GET_PREV_PC()
{
	return m_prev_pc;
}

UINT32 CPU_GET_NEXT_PC()
{
	return m_pc;
}
#endif
