/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#include "msdos.h"

void exit_handler();

#define fatalerror(...) { \
	fprintf(stderr, __VA_ARGS__); \
	exit_handler(); \
	exit(1); \
}
#define error(...) fprintf(stderr, "error: " __VA_ARGS__)
#define nolog(...)

//#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
	#define EXPORT_DEBUG_TO_FILE
	#define ENABLE_DEBUG_DASM
	#define ENABLE_DEBUG_SYSCALL
	#define ENABLE_DEBUG_UNIMPLEMENTED
	#define ENABLE_DEBUG_IOPORT
	
	#ifdef EXPORT_DEBUG_TO_FILE
		FILE* fdebug = NULL;
	#else
		#define fdebug stderr
	#endif
	#ifdef ENABLE_DEBUG_UNIMPLEMENTED
		#define unimplemented_10h fatalerror
		#define unimplemented_14h fatalerror
		#define unimplemented_15h fatalerror
		#define unimplemented_16h fatalerror
		#define unimplemented_1ah fatalerror
		#define unimplemented_21h fatalerror
		#define unimplemented_2fh fatalerror
		#define unimplemented_33h fatalerror
		#define unimplemented_67h fatalerror
		#define unimplemented_xms fatalerror
	#endif
#endif
#ifndef unimplemented_10h
	#define unimplemented_10h nolog
#endif
#ifndef unimplemented_14h
	#define unimplemented_14h nolog
#endif
#ifndef unimplemented_15h
	#define unimplemented_15h nolog
#endif
#ifndef unimplemented_16h
	#define unimplemented_16h nolog
#endif
#ifndef unimplemented_1ah
	#define unimplemented_1ah nolog
#endif
#ifndef unimplemented_21h
	#define unimplemented_21h nolog
#endif
#ifndef unimplemented_2fh
	#define unimplemented_2fh nolog
#endif
#ifndef unimplemented_33h
	#define unimplemented_33h nolog
#endif
#ifndef unimplemented_67h
	#define unimplemented_67h nolog
#endif
#ifndef unimplemented_xms
	#define unimplemented_xms nolog
#endif

#define my_strchr(str, chr) (char *)_mbschr((unsigned char *)(str), (unsigned int)(chr))
#define my_strtok(tok, del) (char *)_mbstok((unsigned char *)(tok), (const unsigned char *)(del))
#define my_strupr(str) (char *)_mbsupr((unsigned char *)(str))

#if defined(__MINGW32__)
extern "C" int _CRT_glob = 0;
#endif

/*
	kludge for "more-standardized" C++
*/
#if !defined(_MSC_VER)
inline int kludge_min(int a, int b) { return (a<b ? a:b); }
inline int kludge_max(int a, int b) { return (a>b ? a:b); }
#define min(a,b) kludge_min(a,b)
#define max(a,b) kludge_max(a,b)
#elif _MSC_VER >= 1400
void ignore_invalid_parameters(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
}
#endif

#define USE_THREAD

#ifdef USE_THREAD
static CRITICAL_SECTION vram_crit_sect;
#else
#define EnterCriticalSection(x)
#define LeaveCriticalSection(x)
#define vram_flush()
#endif

#define VIDEO_REGEN *(UINT16 *)(mem + 0x44c)
#define SCR_BUF(y,x) scr_buf[(y) * scr_buf_size.X + (x)]

void change_console_size(int width, int height);
void clear_scr_buffer(WORD attr);

static UINT32 vram_length_char = 0, vram_length_attr = 0;
static UINT32 vram_last_length_char = 0, vram_last_length_attr = 0;
static COORD vram_coord_char, vram_coord_attr;

char temp_file_path[MAX_PATH];
bool temp_file_created = false;

bool ignore_illegal_insn = false;
bool limit_max_memory = false;
bool no_windows = false;
//bool ctrl_break = false;
bool stay_busy = false;
UINT32 iops = 0;
bool support_ems = false;
#ifdef SUPPORT_XMS
bool support_xms = false;
#endif
int sio_port_number[4] = {0, 0, 0, 0};

BOOL is_vista_or_later;

inline void maybe_idle()
{
	// if it appears to be in a tight loop, assume waiting for input
	// allow for one updated video character, for a spinning cursor
	if(!stay_busy && iops < 1000 && vram_length_char <= 1 && vram_length_attr <= 1) {
		Sleep(10);
	}
	iops = 0;
}

/* ----------------------------------------------------------------------------
	MAME i86/i386
---------------------------------------------------------------------------- */

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
//typedef UINT32	offs_t;

// read accessors
UINT8 read_byte(offs_t byteaddress)
{
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM) {
		return mem[byteaddress];
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_byte(byteaddress & 0xfffff);
	}
	return 0;
#else
	return mem[byteaddress];
#endif
}

UINT16 read_word(offs_t byteaddress)
{
	if(byteaddress == 0x41c) {
		// pointer to first free slot in keyboard buffer
		// XXX: the buffer itself doesn't actually exist in DOS memory
		if(key_buf_char->count() == 0) {
			maybe_idle();
		}
		return (UINT16)key_buf_char->count();
	}
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM - 1) {
		return *(UINT16 *)(mem + byteaddress);
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_word(byteaddress & 0xfffff);
	}
	return 0;
#else
	return *(UINT16 *)(mem + byteaddress);
#endif
}

UINT32 read_dword(offs_t byteaddress)
{
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM - 3) {
		return *(UINT32 *)(mem + byteaddress);
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_dword(byteaddress & 0xfffff);
	}
	return 0;
#else
	return *(UINT32 *)(mem + byteaddress);
#endif
}

// write accessors
#ifdef USE_THREAD
void vram_flush_char()
{
	if(vram_length_char != 0) {
		DWORD num;
		WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), scr_char, vram_length_char, vram_coord_char, &num);
		vram_length_char = vram_last_length_char = 0;
	}
}

void vram_flush_attr()
{
	if(vram_length_attr != 0) {
		DWORD num;
		WriteConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE), scr_attr, vram_length_attr, vram_coord_attr, &num);
		vram_length_attr = vram_last_length_attr = 0;
	}
}

void vram_flush()
{
	if(vram_length_char != 0 || vram_length_attr != 0) {
		EnterCriticalSection(&vram_crit_sect);
		vram_flush_char();
		vram_flush_attr();
		LeaveCriticalSection(&vram_crit_sect);
	}
}
#endif

void write_text_vram_char(offs_t offset, UINT8 data)
{
#ifdef USE_THREAD
	static offs_t first_offset_char, last_offset_char;
	
	if(vram_length_char != 0) {
		if(offset <= last_offset_char && offset >= first_offset_char) {
			scr_char[(offset - first_offset_char) >> 1] = data;
			return;
		}
		if(offset != last_offset_char + 2) {
			vram_flush_char();
		}
	}
	if(vram_length_char == 0) {
		first_offset_char = offset;
		vram_coord_char.X = (offset >> 1) % scr_width;
		vram_coord_char.Y = (offset >> 1) / scr_width + scr_top;
	}
	scr_char[vram_length_char++] = data;
	last_offset_char = offset;
#else
	COORD co;
	DWORD num;
	
	co.X = (offset >> 1) % scr_width;
	co.Y = (offset >> 1) / scr_width;
	scr_char[0] = data;
	WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), scr_char, 1, co, &num);
#endif
}

void write_text_vram_attr(offs_t offset, UINT8 data)
{
#ifdef USE_THREAD
	static offs_t first_offset_attr, last_offset_attr;
	
	if(vram_length_attr != 0) {
		if(offset <= last_offset_attr && offset >= first_offset_attr) {
			scr_attr[(offset - first_offset_attr) >> 1] = data;
			return;
		}
		if(offset != last_offset_attr + 2) {
			vram_flush_attr();
		}
	}
	if(vram_length_attr == 0) {
		first_offset_attr = offset;
		vram_coord_attr.X = (offset >> 1) % scr_width;
		vram_coord_attr.Y = (offset >> 1) / scr_width + scr_top;
	}
	scr_attr[vram_length_attr++] = data;
	last_offset_attr = offset;
#else
	COORD co;
	DWORD num;
	
	co.X = (offset >> 1) % scr_width;
	co.Y = (offset >> 1) / scr_width;
	scr_attr[0] = data;
	WriteConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE), scr_attr, 1, co, &num);
#endif
}

void write_text_vram_byte(offs_t offset, UINT8 data)
{
	EnterCriticalSection(&vram_crit_sect);
	if(offset & 1) {
		write_text_vram_attr(offset, data);
	} else {
		write_text_vram_char(offset, data);
	}
	LeaveCriticalSection(&vram_crit_sect);
}

void write_text_vram_word(offs_t offset, UINT16 data)
{
	EnterCriticalSection(&vram_crit_sect);
	if(offset & 1) {
		write_text_vram_attr(offset    , (data     ) & 0xff);
		write_text_vram_char(offset + 1, (data >> 8) & 0xff);
	} else {
		write_text_vram_char(offset    , (data     ) & 0xff);
		write_text_vram_attr(offset + 1, (data >> 8) & 0xff);
	}
	LeaveCriticalSection(&vram_crit_sect);
}

void write_text_vram_dword(offs_t offset, UINT32 data)
{
	EnterCriticalSection(&vram_crit_sect);
	if(offset & 1) {
		write_text_vram_attr(offset    , (data      ) & 0xff);
		write_text_vram_char(offset + 1, (data >>  8) & 0xff);
		write_text_vram_attr(offset + 2, (data >> 16) & 0xff);
		write_text_vram_char(offset + 3, (data >> 24) & 0xff);
	} else {
		write_text_vram_char(offset    , (data      ) & 0xff);
		write_text_vram_attr(offset + 1, (data >>  8) & 0xff);
		write_text_vram_char(offset + 2, (data >> 16) & 0xff);
		write_text_vram_attr(offset + 3, (data >> 24) & 0xff);
	}
	LeaveCriticalSection(&vram_crit_sect);
}

void write_byte(offs_t byteaddress, UINT8 data)
{
	if(byteaddress < MEMORY_END) {
		mem[byteaddress] = data;
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_on_exit) {
			change_console_size(scr_width, scr_height);
		}
		write_text_vram_byte(byteaddress - text_vram_top_address, data);
		mem[byteaddress] = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_byte(byteaddress - shadow_buffer_top_address, data);
		}
		mem[byteaddress] = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM) {
#else
	} else {
#endif
		mem[byteaddress] = data;
	}
}

void write_word(offs_t byteaddress, UINT16 data)
{
	if(byteaddress < MEMORY_END) {
		if(byteaddress == 0x450 + mem[0x462] * 2) {
			COORD co;
			co.X = data & 0xff;
			co.Y = (data >> 8) + scr_top;
			SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), co);
		}
		*(UINT16 *)(mem + byteaddress) = data;
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_on_exit) {
			change_console_size(scr_width, scr_height);
		}
		write_text_vram_word(byteaddress - text_vram_top_address, data);
		*(UINT16 *)(mem + byteaddress) = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_word(byteaddress - shadow_buffer_top_address, data);
		}
		*(UINT16 *)(mem + byteaddress) = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM - 1) {
#else
	} else {
#endif
		*(UINT16 *)(mem + byteaddress) = data;
	}
}

void write_dword(offs_t byteaddress, UINT32 data)
{
	if(byteaddress < MEMORY_END) {
		*(UINT32 *)(mem + byteaddress) = data;
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_on_exit) {
			change_console_size(scr_width, scr_height);
		}
		write_text_vram_dword(byteaddress - text_vram_top_address, data);
		*(UINT32 *)(mem + byteaddress) = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_dword(byteaddress - shadow_buffer_top_address, data);
		}
		*(UINT32 *)(mem + byteaddress) = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM - 3) {
#else
	} else {
#endif
		*(UINT32 *)(mem + byteaddress) = data;
	}
}

#define read_decrypted_byte read_byte
#define read_decrypted_word read_word
#define read_decrypted_dword read_dword

#define read_raw_byte read_byte
#define write_raw_byte write_byte

#define read_word_unaligned read_word
#define write_word_unaligned write_word

#define read_io_word_unaligned read_io_word
#define write_io_word_unaligned write_io_word

UINT8 read_io_byte(offs_t byteaddress);
UINT16 read_io_word(offs_t byteaddress);
UINT32 read_io_dword(offs_t byteaddress);

void write_io_byte(offs_t byteaddress, UINT8 data);
void write_io_word(offs_t byteaddress, UINT16 data);
void write_io_dword(offs_t byteaddress, UINT32 data);

/*****************************************************************************/
/* src/osd/osdcomm.h */

/* Highly useful macro for compile-time knowledge of an array size */
#define ARRAY_LENGTH(x)     (sizeof(x) / sizeof(x[0]))

#if defined(HAS_I386)
	static CPU_TRANSLATE(i386);
	#include "mame/lib/softfloat/softfloat.c"
	#include "mame/emu/cpu/i386/i386.c"
	#include "mame/emu/cpu/vtlb.c"
#elif defined(HAS_I286)
	#include "mame/emu/cpu/i86/i286.c"
#else
	#include "mame/emu/cpu/i86/i86.c"
#endif
#ifdef ENABLE_DEBUG_DASM
	#include "mame/emu/cpu/i386/i386dasm.c"
	int dasm = 0;
#endif

#if defined(HAS_I386)
	#define SREG(x)				m_sreg[x].selector
	#define SREG_BASE(x)			m_sreg[x].base

	int cpu_type, cpu_step;
#else
	#define REG8(x)				m_regs.b[x]
	#define REG16(x)			m_regs.w[x]
	#define SREG(x)				m_sregs[x]
	#define SREG_BASE(x)			m_base[x]
	#define m_CF				m_CarryVal
	#define m_a20_mask			AMASK
	#define i386_load_segment_descriptor(x)	m_base[x] = SegBase(x)
	#if defined(HAS_I286)
		#define i386_set_a20_line(x)	i80286_set_a20_line(x)
	#else
		#define i386_set_a20_line(x)
	#endif
	#define i386_set_irq_line(x, y)		set_irq_line(x, y)
#endif

void i386_jmp_far(UINT16 selector, UINT32 address)
{
#if defined(HAS_I386)
	if(PROTECTED_MODE && !V8086_MODE) {
		i386_protected_mode_jump(selector, address, 1, m_operand_size);
	} else {
		SREG(CS) = selector;
		m_performed_intersegment_jump = 1;
		i386_load_segment_descriptor(CS);
		m_eip = address;
		CHANGE_PC(m_eip);
	}
#elif defined(HAS_I286)
	i80286_code_descriptor(selector, address, 1);
#else
	SREG(CS) = selector;
	i386_load_segment_descriptor(CS);
	m_pc = (SREG_BASE(CS) + address) & m_a20_mask;
#endif
}

/*
void i386_call_far(UINT16 selector, UINT32 address)
{
#if defined(HAS_I386)
	if(PROTECTED_MODE && !V8086_MODE) {
		i386_protected_mode_call(selector, address, 1, m_operand_size);
	} else {
		PUSH16(SREG(CS));
		PUSH16(m_eip);
		SREG(CS) = selector;
		m_performed_intersegment_jump = 1;
		i386_load_segment_descriptor(CS);
		m_eip = address;
		CHANGE_PC(m_eip);
	}
#else
	UINT16 ip = m_pc - SREG_BASE(CS);
	UINT16 cs = SREG(CS);
#if defined(HAS_I286)
	i80286_code_descriptor(selector, address, 2);
#else
	SREG(CS) = selector;
	i386_load_segment_descriptor(CS);
	m_pc = (SREG_BASE(CS) + address) & m_a20_mask;
#endif
	PUSH(cs);
	PUSH(ip);
	CHANGE_PC(m_pc);
#endif
}
*/

UINT16 i386_read_stack()
{
#if defined(HAS_I386)
	UINT32 ea, new_esp;
	if( STACK_32BIT ) {
		new_esp = REG32(ESP) + 2;
		ea = i386_translate(SS, new_esp - 2, 0);
	} else {
		new_esp = REG16(SP) + 2;
		ea = i386_translate(SS, (new_esp - 2) & 0xffff, 0);
	}
	return READ16(ea);
#else
	UINT16 sp = m_regs.w[SP] + 2;
	return ReadWord(((m_base[SS] + ((sp - 2) & 0xffff)) & AMASK));
#endif
}

/* ----------------------------------------------------------------------------
	main
---------------------------------------------------------------------------- */

BOOL WINAPI ctrl_handler(DWORD dwCtrlType)
{
	if(dwCtrlType == CTRL_BREAK_EVENT) {
		// try to finish this program normally
		m_halted = true;
		return TRUE;
	} else if(dwCtrlType == CTRL_C_EVENT) {
		ctrl_c_pressed = true;
		return TRUE;
	} else if(dwCtrlType == CTRL_CLOSE_EVENT) {
		// this program will be terminated abnormally, do minimum end process
		exit_handler();
		exit(1);
	}
	return FALSE;
}

void exit_handler()
{
	if(temp_file_created) {
		DeleteFile(temp_file_path);
		temp_file_created = false;
	}
	if(key_buf_char != NULL) {
		key_buf_char->release();
		delete key_buf_char;
		key_buf_char = NULL;
	}
	if(key_buf_scan != NULL) {
		key_buf_scan->release();
		delete key_buf_scan;
		key_buf_scan = NULL;
	}
	hardware_release();
}

#ifdef USE_THREAD
DWORD WINAPI vram_thread(LPVOID)
{
	while(!m_halted) {
		EnterCriticalSection(&vram_crit_sect);
		if(vram_length_char != 0 && vram_length_char == vram_last_length_char) {
			vram_flush_char();
		}
		if(vram_length_attr != 0 && vram_length_attr == vram_last_length_attr) {
			vram_flush_attr();
		}
		vram_last_length_char = vram_length_char;
		vram_last_length_attr = vram_length_attr;
		LeaveCriticalSection(&vram_crit_sect);
		// this is about half the maximum keyboard repeat rate - any
		// lower tends to be jerky, any higher misses updates
		Sleep(15);
	}
	return 0;
}
#endif

long get_section_in_exec_file(FILE *fp, char *name)
{
	UINT8 header[0x400];
	
	long position = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(header, sizeof(header), 1, fp);
	fseek(fp, position, SEEK_SET);
	
	try {
		_IMAGE_DOS_HEADER *dosHeader = (_IMAGE_DOS_HEADER *)(header + 0);
		DWORD dwTopOfSignature = dosHeader->e_lfanew;
		DWORD dwTopOfCoffHeader = dwTopOfSignature + 4;
		_IMAGE_FILE_HEADER *coffHeader = (_IMAGE_FILE_HEADER *)(header + dwTopOfCoffHeader);
		DWORD dwTopOfOptionalHeader = dwTopOfCoffHeader + sizeof(_IMAGE_FILE_HEADER);
		DWORD dwTopOfFirstSectionHeader = dwTopOfOptionalHeader + coffHeader->SizeOfOptionalHeader;
		
		for(int i = 0; i < coffHeader->NumberOfSections; i++) {
			_IMAGE_SECTION_HEADER *sectionHeader = (_IMAGE_SECTION_HEADER *)(header + dwTopOfFirstSectionHeader + IMAGE_SIZEOF_SECTION_HEADER * i);
			if(memcmp(sectionHeader->Name, name, strlen(name)) == 0) {
				return(sectionHeader->PointerToRawData);
			}
		}
	} catch(...) {
	}
	return(0);
}

bool is_started_from_command_prompt()
{
	bool ret = false;
	
	HMODULE hLibrary = LoadLibrary(_T("Kernel32.dll"));
	if(hLibrary) {
		typedef DWORD (WINAPI *GetConsoleProcessListFunction)(__out LPDWORD lpdwProcessList, __in DWORD dwProcessCount);
		GetConsoleProcessListFunction lpfnGetConsoleProcessList;
		lpfnGetConsoleProcessList = reinterpret_cast<GetConsoleProcessListFunction>(::GetProcAddress(hLibrary, "GetConsoleProcessList"));
		if(lpfnGetConsoleProcessList) {
			DWORD pl;
			ret = (lpfnGetConsoleProcessList(&pl, 1) > 1);
			FreeLibrary(hLibrary);
			return(ret);
		}
		FreeLibrary(hLibrary);
	}
	
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnapshot != INVALID_HANDLE_VALUE) {
		DWORD dwParentProcessID = 0;
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if(Process32First(hSnapshot, &pe32)) {
			do {
				if(pe32.th32ProcessID == GetCurrentProcessId()) {
					dwParentProcessID = pe32.th32ParentProcessID;
					break;
				}
			} while(Process32Next(hSnapshot, &pe32));
		}
		CloseHandle(hSnapshot);
		if(dwParentProcessID != 0) {
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwParentProcessID);
			if(hProcess != NULL) {
				HMODULE hMod;
				DWORD cbNeeded;
				if(EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
					char module_name[MAX_PATH];
					if(GetModuleBaseName(hProcess, hMod, module_name, sizeof(module_name))) {
						ret = (_strnicmp(module_name, "cmd.exe", 7) == 0);
					}
				}
				CloseHandle(hProcess);
			}
		}
	}
	return(ret);
}

BOOL is_greater_windows_version(DWORD dwMajorVersion, DWORD dwMinorVersion, WORD wServicePackMajor, WORD wServicePackMinor)
{
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms725491(v=vs.85).aspx
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;
	int op = VER_GREATER_EQUAL;
	
	// Initialize the OSVERSIONINFOEX structure.
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = dwMajorVersion;
	osvi.dwMinorVersion = dwMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;
	osvi.wServicePackMinor = wServicePackMinor;
	
	 // Initialize the condition mask.
	VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, op );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, op );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, op );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMINOR, op );
	
	// Perform the test.
	return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, dwlConditionMask);
}

void get_sio_port_numbers()
{
	SP_DEVINFO_DATA DeviceInfoData = {sizeof(SP_DEVINFO_DATA)};
	HDEVINFO hDevInfo = 0;
	HKEY hKey = 0;
	if((hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, NULL, NULL, (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE))) != 0) {
		for(int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++) {
			if((hKey = SetupDiOpenDevRegKey(hDevInfo, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE)) != INVALID_HANDLE_VALUE) {
				char chData[256];
				DWORD dwType = 0;
				DWORD dwSize = sizeof(chData);
				int port_number = 0;
				
				if(RegQueryValueEx(hKey, _T("PortName"), NULL, &dwType, (BYTE *)chData, &dwSize) == ERROR_SUCCESS) {
					if(_strnicmp(chData, "COM", 3) == 0) {
						port_number = atoi(chData + 3);
					}
				}
				RegCloseKey(hKey);
				
				if(sio_port_number[0] == port_number || sio_port_number[1] == port_number || sio_port_number[2] == port_number || sio_port_number[3] == port_number) {
					continue;
				}
				if(sio_port_number[0] == 0) {
					sio_port_number[0] = port_number;
				} else if(sio_port_number[1] == 0) {
					sio_port_number[1] = port_number;
				} else if(sio_port_number[2] == 0) {
					sio_port_number[2] = port_number;
				} else if(sio_port_number[3] == 0) {
					sio_port_number[3] = port_number;
				}
				if(sio_port_number[0] != 0 && sio_port_number[1] != 0 && sio_port_number[2] != 0 && sio_port_number[3] != 0) {
					break;
				}
			}
		}
	}
}

#define IS_NUMERIC(c) ((c) >= '0' && (c) <= '9')

int main(int argc, char *argv[], char *envp[])
{
	int arg_offset = 0;
	int standard_env = 0;
	int buf_width = 0, buf_height = 0;
	bool get_console_info_success = false;
	bool screen_size_changed = false;
	
	_TCHAR path[MAX_PATH], full[MAX_PATH], *name = NULL;
	GetModuleFileName(NULL, path, MAX_PATH);
	GetFullPathName(path, MAX_PATH, full, &name);
	
	char dummy_argv_0[] = "msdos.exe";
	char dummy_argv_1[MAX_PATH];
	char *dummy_argv[256] = {dummy_argv_0, dummy_argv_1, 0};
	char new_exec_file[MAX_PATH];
	bool convert_cmd_file = false;
	unsigned int code_page = 0;
	
	if(name != NULL && stricmp(name, "msdos.exe") != 0) {
		// check if command file is embedded to this execution file
		// if this execution file name is msdos.exe, don't check
		FILE* fp = fopen(full, "rb");
		long offset = get_section_in_exec_file(fp, ".msdos");
		if(offset != 0) {
			UINT8 buffer[16];
			fseek(fp, offset, SEEK_SET);
			fread(buffer, sizeof(buffer), 1, fp);
			
			// restore flags
			stay_busy           = ((buffer[0] & 0x01) != 0);
			no_windows          = ((buffer[0] & 0x02) != 0);
			standard_env        = ((buffer[0] & 0x04) != 0);
			ignore_illegal_insn = ((buffer[0] & 0x08) != 0);
			limit_max_memory    = ((buffer[0] & 0x10) != 0);
			if((buffer[0] & 0x20) != 0) {
				get_sio_port_numbers();
			}
			if((buffer[0] & 0x40) != 0) {
				UMB_TOP = EMS_TOP + EMS_SIZE;
				support_ems = true;
			}
#ifdef SUPPORT_XMS
			if((buffer[0] & 0x80) != 0) {
				support_xms = true;
			}
#endif
			if((buffer[1] != 0 || buffer[2] != 0) && (buffer[3] != 0 || buffer[4] != 0)) {
				buf_width  = buffer[1] | (buffer[2] << 8);
				buf_height = buffer[3] | (buffer[4] << 8);
			}
			if(buffer[5] != 0) {
				dos_major_version = buffer[5];
				dos_minor_version = buffer[6];
			}
			if(buffer[7] != 0) {
				win_major_version = buffer[7];
				win_minor_version = buffer[8];
			}
			if((code_page = buffer[9] | (buffer[10] << 8)) != 0) {
				SetConsoleCP(code_page);
				SetConsoleOutputCP(code_page);
			}
			int name_len = buffer[11];
			int file_len = buffer[12] | (buffer[13] << 8) | (buffer[14] << 16) | (buffer[15] << 24);
			
			// restore command file name
			memset(dummy_argv_1, 0, sizeof(dummy_argv_1));
			fread(dummy_argv_1, name_len, 1, fp);
			
			if(name_len != 0 && _access(dummy_argv_1, 0) == 0) {
				// if original command file exists, create a temporary file name
				if(GetTempFileName(_T("."), _T("DOS"), 0, dummy_argv_1) != 0) {
					// create a temporary command file in the current director
					DeleteFile(dummy_argv_1);
				} else {
					// create a temporary command file in the temporary folder
					GetTempPath(MAX_PATH, path);
					if(GetTempFileName(path, _T("DOS"), 0, dummy_argv_1) != 0) {
						DeleteFile(dummy_argv_1);
					} else {
						sprintf(dummy_argv_1, "%s$DOSPRG$.TMP", path);
					}
				}
				// check the command file type
				fread(buffer, 2, 1, fp);
				fseek(fp, -2, SEEK_CUR);
				if(memcmp(buffer, "MZ", 2) != 0) {
					memcpy(dummy_argv_1 + strlen(dummy_argv_1) - 4, ".COM", 4);
				} else {
					memcpy(dummy_argv_1 + strlen(dummy_argv_1) - 4, ".EXE", 4);
				}
			}
			
			// restore command file
			FILE* fo = fopen(dummy_argv_1, "wb");
			for(int i = 0; i < file_len; i++) {
				fputc(fgetc(fp), fo);
			}
			fclose(fo);
			
			GetFullPathName(dummy_argv_1, MAX_PATH, temp_file_path, NULL);
			temp_file_created = true;
			SetFileAttributes(temp_file_path, FILE_ATTRIBUTE_HIDDEN);
			
			// adjust argc/argv
			for(int i = 1; i < argc && (i + 1) < 256; i++) {
				dummy_argv[i + 1] = argv[i];
			}
			argc++;
			argv = dummy_argv;
		}
		fclose(fp);
	}
	for(int i = 1; i < argc; i++) {
		if(_strnicmp(argv[i], "-b", 2) == 0) {
			stay_busy = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-c", 2) == 0) {
			if(argv[i][2] != '\0') {
				strcpy(new_exec_file, &argv[i][2]);
			} else {
				strcpy(new_exec_file, "new_exec_file.exe");
			}
			convert_cmd_file = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-p", 2) == 0) {
			if(IS_NUMERIC(argv[i][2])) {
				code_page = atoi(&argv[i][2]);
			} else {
				code_page = GetConsoleCP();
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-d", 2) == 0) {
			no_windows = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-e", 2) == 0) {
			standard_env = 1;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-i", 2) == 0) {
			ignore_illegal_insn = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-m", 2) == 0) {
			limit_max_memory = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-n", 2) == 0) {
			if(sscanf(argv[1] + 2, "%d,%d", &buf_height, &buf_width) != 2) {
				buf_width = buf_height = 0;
			}
			if(buf_width <= 0 || buf_width > 0x7fff) {
				buf_width = 80;
			}
			if(buf_height <= 0 || buf_height > 0x7fff) {
				buf_height = 25;
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-s", 2) == 0) {
			if(IS_NUMERIC(argv[i][2])) {
				char *p0 = &argv[i][2], *p1, *p2, *p3;
				if((p1 = strchr(p0, ',')) != NULL && IS_NUMERIC(p1[1])) {
					sio_port_number[1] = atoi(p1 + 1);
					if((p2 = strchr(p1, ',')) != NULL && IS_NUMERIC(p2[1])) {
						sio_port_number[2] = atoi(p2 + 1);
						if((p3 = strchr(p2, ',')) != NULL && IS_NUMERIC(p3[1])) {
							sio_port_number[3] = atoi(p3 + 1);
						}
					}
				}
				sio_port_number[0] = atoi(p0);
			}
			if(sio_port_number[0] == 0 || sio_port_number[1] == 0 || sio_port_number[2] == 0 || sio_port_number[3] == 0) {
				get_sio_port_numbers();
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-v", 2) == 0) {
			if(strlen(argv[i]) >= 5 && IS_NUMERIC(argv[i][2]) && argv[i][3] == '.' && IS_NUMERIC(argv[i][4]) && (argv[i][5] == '\0' || IS_NUMERIC(argv[i][5]))) {
				dos_major_version = argv[i][2] - '0';
				dos_minor_version = (argv[i][4] - '0') * 10 + (argv[i][5] ? (argv[i][5] - '0') : 0);
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-w", 2) == 0) {
			if(strlen(argv[i]) >= 5 && IS_NUMERIC(argv[i][2]) && argv[i][3] == '.' && IS_NUMERIC(argv[i][4]) && (argv[i][5] == '\0' || IS_NUMERIC(argv[i][5]))) {
				win_major_version = argv[i][2] - '0';
				win_minor_version = (argv[i][4] - '0') * 10 + (argv[i][5] ? (argv[i][5] - '0') : 0);
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-x", 2) == 0) {
			UMB_TOP = EMS_TOP + EMS_SIZE;
			support_ems = true;
#ifdef SUPPORT_XMS
			support_xms = true;
#endif
			arg_offset++;
		} else {
			break;
		}
	}
	if(argc < 2 + arg_offset) {
#ifdef _WIN64
		fprintf(stderr, "MS-DOS Player (" CPU_MODEL_NAME(CPU_MODEL) ") for Win32-x64 console\n\n");
#else
		fprintf(stderr, "MS-DOS Player (" CPU_MODEL_NAME(CPU_MODEL) ") for Win32 console\n\n");
#endif
		fprintf(stderr,
			"Usage: MSDOS [-b] [-c[(new exec file)] [-p[P]]] [-d] [-e] [-i] [-m] [-n[L[,C]]]\n"
			"             [-s[P1[,P2[,P3[,P4]]]]] [-vX.XX] [-wX.XX] [-x] (command) [options]\n"
			"\n"
			"\t-b\tstay busy during keyboard polling\n"
#ifdef _WIN64
			"\t-c\tconvert command file to 64bit execution file\n"
#else
			"\t-c\tconvert command file to 32bit execution file\n"
#endif
			"\t-p\trecord current code page when convert command file\n"
			"\t-d\tpretend running under straight DOS, not Windows\n"
			"\t-e\tuse a reduced environment block\n"
			"\t-i\tignore invalid instructions\n"
			"\t-m\trestrict free memory to 0x7FFF paragraphs\n"
			"\t-n\tcreate a new buffer (25 lines, 80 columns by default)\n"
			"\t-s\tenable serial I/O and set host's COM port numbers\n"
			"\t-v\tset the DOS version\n"
			"\t-w\tset the Windows version\n"
#ifdef SUPPORT_XMS
			"\t-x\tenable XMS and LIM EMS\n"
#else
			"\t-x\tenable LIM EMS\n"
#endif
		);
		
		if(!is_started_from_command_prompt()) {
			fprintf(stderr, "\nStart this program from a command prompt!\n\nHit any key to quit...");
			while(!_kbhit()) {
				Sleep(10);
			}
		}
#ifdef _DEBUG
		_CrtDumpMemoryLeaks();
#endif
		return(EXIT_FAILURE);
	}
	if(convert_cmd_file) {
		retval = EXIT_FAILURE;
		if(name != NULL/* && stricmp(name, "msdos.exe") == 0*/) {
			FILE *fp = NULL, *fs = NULL, *fo = NULL;
			int len = strlen(argv[arg_offset + 1]), data;
			
			if(!(len > 4 && (stricmp(argv[arg_offset + 1] + len - 4, ".COM") == 0 || stricmp(argv[arg_offset + 1] + len - 4, ".EXE") == 0))) {
				fprintf(stderr, "Specify command file with extenstion (.COM or .EXE)\n");
			} else if((fp = fopen(full, "rb")) == NULL) {
				fprintf(stderr, "Can't open '%s'\n", name);
			} else {
				long offset = get_section_in_exec_file(fp, ".msdos");
				if(offset != 0) {
					UINT8 buffer[14];
					fseek(fp, offset, SEEK_SET);
					fread(buffer, sizeof(buffer), 1, fp);
					memset(path, 0, sizeof(path));
					fread(path, buffer[9], 1, fp);
					fprintf(stderr, "Command file '%s' was already embedded to '%s'\n", path, name);
				} else if((fs = fopen(argv[arg_offset + 1], "rb")) == NULL) {
					fprintf(stderr, "Can't open '%s'\n", argv[arg_offset + 1]);
				} else if((fo = fopen(new_exec_file, "wb")) == NULL) {
					fprintf(stderr, "Can't open '%s'\n", new_exec_file);
				} else {
					// read pe header of msdos.exe
					UINT8 header[0x400];
					fseek(fp, 0, SEEK_SET);
					fread(header, sizeof(header), 1, fp);
					
					_IMAGE_DOS_HEADER *dosHeader = (_IMAGE_DOS_HEADER *)(header + 0);
					DWORD dwTopOfSignature = dosHeader->e_lfanew;
					DWORD dwTopOfCoffHeader = dwTopOfSignature + 4;
					_IMAGE_FILE_HEADER *coffHeader = (_IMAGE_FILE_HEADER *)(header + dwTopOfCoffHeader);
					DWORD dwTopOfOptionalHeader = dwTopOfCoffHeader + sizeof(_IMAGE_FILE_HEADER);
					_IMAGE_OPTIONAL_HEADER *optionalHeader = (_IMAGE_OPTIONAL_HEADER *)(header + dwTopOfOptionalHeader);
					DWORD dwTopOfFirstSectionHeader = dwTopOfOptionalHeader + coffHeader->SizeOfOptionalHeader;
					
					_IMAGE_SECTION_HEADER *lastSectionHeader = (_IMAGE_SECTION_HEADER *)(header + dwTopOfFirstSectionHeader + IMAGE_SIZEOF_SECTION_HEADER * (coffHeader->NumberOfSections - 1));
					DWORD dwEndOfFile = lastSectionHeader->PointerToRawData + lastSectionHeader->SizeOfRawData;
					DWORD dwLastSectionSize = lastSectionHeader->SizeOfRawData;
					DWORD dwExtraLastSectionBytes = dwLastSectionSize % optionalHeader->SectionAlignment;
					if(dwExtraLastSectionBytes != 0) {
						DWORD dwRemain = optionalHeader->SectionAlignment - dwExtraLastSectionBytes;
						dwLastSectionSize += dwRemain;
					}
					DWORD dwVirtualAddress = lastSectionHeader->VirtualAddress + dwLastSectionSize;
					
					// store msdos.exe
					fseek(fp, 0, SEEK_SET);
					for(int i = 0; i < dwEndOfFile; i++) {
						if((data = fgetc(fp)) != EOF) {
							fputc(data, fo);
						} else {
							// we should not reach here :-(
							fputc(0, fo);
						}
					}
					
					// store options
					UINT8 flags = 0;
					if(stay_busy) {
						flags |= 0x01;
					}
					if(no_windows) {
						flags |= 0x02;
					}
					if(standard_env) {
						flags |= 0x04;
					}
					if(ignore_illegal_insn) {
						flags |= 0x08;
					}
					if(limit_max_memory) {
						flags |= 0x10;
					}
					if(sio_port_number[0] != 0 || sio_port_number[1] != 0 || sio_port_number[2] != 0 || sio_port_number[3] != 0) {
						flags |= 0x20;
					}
					if(support_ems) {
						flags |= 0x40;
					}
#ifdef SUPPORT_XMS
					if(support_xms) {
						flags |= 0x80;
					}
#endif
					
					fputc(flags, fo);
					fputc((buf_width  >> 0) & 0xff, fo);
					fputc((buf_width  >> 8) & 0xff, fo);
					fputc((buf_height >> 0) & 0xff, fo);
					fputc((buf_height >> 8) & 0xff, fo);
					fputc(dos_major_version, fo);
					fputc(dos_minor_version, fo);
					fputc(win_major_version, fo);
					fputc(win_minor_version, fo);
					fputc((code_page >> 0) & 0xff, fo);
					fputc((code_page >> 8) & 0xff, fo);
					
					// store command file info
					GetFullPathName(argv[arg_offset + 1], MAX_PATH, full, &name);
					int name_len = strlen(name);
					fseek(fs, 0, SEEK_END);
					long file_size = ftell(fs);
					
					fputc(name_len, fo);
					fputc((file_size >>  0) & 0xff, fo);
					fputc((file_size >>  8) & 0xff, fo);
					fputc((file_size >> 16) & 0xff, fo);
					fputc((file_size >> 24) & 0xff, fo);
					fwrite(name, name_len, 1, fo);
					
					// store command file
					fseek(fs, 0, SEEK_SET);
					for(int i = 0; i < file_size; i++) {
						if((data = fgetc(fs)) != EOF) {
							fputc(data, fo);
						} else {
							// we should not reach here :-(
							fputc(0, fo);
						}
					}
					
					// store padding data and update pe header
					_IMAGE_SECTION_HEADER *newSectionHeader = (_IMAGE_SECTION_HEADER *)(header + dwTopOfFirstSectionHeader + IMAGE_SIZEOF_SECTION_HEADER * coffHeader->NumberOfSections);
					coffHeader->NumberOfSections++;
					memset(newSectionHeader, 0, IMAGE_SIZEOF_SECTION_HEADER);
					memcpy(newSectionHeader->Name, ".msdos", 6);
					newSectionHeader->VirtualAddress = dwVirtualAddress;
					newSectionHeader->PointerToRawData = dwEndOfFile;
					newSectionHeader->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_DISCARDABLE;
					newSectionHeader->SizeOfRawData = 14 + name_len + file_size;
					DWORD dwExtraRawBytes = newSectionHeader->SizeOfRawData % optionalHeader->FileAlignment;
					if(dwExtraRawBytes != 0) {
						static const char padding[] = "PADDINGXXPADDING";
						DWORD dwRemain = optionalHeader->FileAlignment - dwExtraRawBytes;
						for(int i = 0; i < dwRemain; i++) {
							if(i < 2) {
								fputc(padding[i & 15], fo);
							} else {
								fputc(padding[(i - 2) & 15], fo);
							}
						}
						newSectionHeader->SizeOfRawData += dwRemain;
					}
					newSectionHeader->Misc.VirtualSize = newSectionHeader->SizeOfRawData;
					
					DWORD dwNewSectionSize = newSectionHeader->SizeOfRawData;
					DWORD dwExtraNewSectionBytes = dwNewSectionSize % optionalHeader->SectionAlignment;
					if(dwExtraNewSectionBytes != 0) {
						DWORD dwRemain = optionalHeader->SectionAlignment - dwExtraNewSectionBytes;
						dwNewSectionSize += dwRemain;
					}
					optionalHeader->SizeOfImage += dwNewSectionSize;
					
					fseek(fo, 0, SEEK_SET);
					fwrite(header, sizeof(header), 1, fo);
					
					fprintf(stderr, "'%s' is successfully created\n", new_exec_file);
					retval = EXIT_SUCCESS;
				}
			}
			if(fp != NULL) {
				fclose(fp);
			}
			if(fs != NULL) {
				fclose(fs);
			}
			if(fo != NULL) {
				fclose(fo);
			}
		}
#ifdef _DEBUG
		_CrtDumpMemoryLeaks();
#endif
		return(retval);
	}
	
	is_vista_or_later = is_greater_windows_version(6, 0, 0, 0);
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CONSOLE_CURSOR_INFO ci;
	
	get_console_info_success = (GetConsoleScreenBufferInfo(hStdout, &csbi) != 0);
	GetConsoleCursorInfo(hStdout, &ci);
	GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &dwConsoleMode);
	
	for(int y = 0; y < SCR_BUF_WIDTH; y++) {
		for(int x = 0; x < SCR_BUF_HEIGHT; x++) {
			SCR_BUF(y,x).Char.AsciiChar = ' ';
			SCR_BUF(y,x).Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		}
	}
	if(get_console_info_success) {
		scr_width = csbi.dwSize.X;
		scr_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		
		// v-text shadow buffer size must be lesser than 0x7fd0
		if((scr_width > SCR_BUF_WIDTH) || (scr_height > SCR_BUF_HEIGHT) || (scr_width * scr_height * 2 > 0x7fd0) ||
		   (buf_width != 0 && buf_width != scr_width) || (buf_height != 0 && buf_height != scr_height)) {
			scr_width = min(buf_width != 0 ? buf_width : scr_width, SCR_BUF_WIDTH);
			scr_height = min(buf_height != 0 ? buf_height : scr_height, SCR_BUF_HEIGHT);
			if(scr_width * scr_height * 2 > 0x7fd0) {
				scr_width = 80;
				scr_height = 25;
			}
			screen_size_changed = true;
		}
	} else {
		// for a proof (not a console)
		scr_width = 80;
		scr_height = 25;
	}
	scr_buf_size.X = scr_width;
	scr_buf_size.Y = scr_height;
	scr_buf_pos.X = scr_buf_pos.Y = 0;
	scr_top = csbi.srWindow.Top;
	cursor_moved = false;
	
	key_buf_char = new FIFO(256);
	key_buf_scan = new FIFO(256);
	
	hardware_init();
	
	if(msdos_init(argc - (arg_offset + 1), argv + (arg_offset + 1), envp, standard_env)) {
		retval = EXIT_FAILURE;
	} else {
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
		_set_invalid_parameter_handler((_invalid_parameter_handler)ignore_invalid_parameters);
#endif
		SetConsoleCtrlHandler(ctrl_handler, TRUE);
		
		if(screen_size_changed) {
			change_console_size(scr_width, scr_height);
		}
		TIMECAPS caps;
		timeGetDevCaps(&caps, sizeof(TIMECAPS));
		timeBeginPeriod(caps.wPeriodMin);
#ifdef USE_THREAD
		InitializeCriticalSection(&vram_crit_sect);
		CloseHandle(CreateThread(NULL, 4096, vram_thread, NULL, 0, NULL));
#endif
		hardware_run();
#ifdef USE_THREAD
		vram_flush();
		DeleteCriticalSection(&vram_crit_sect);
#endif
		timeEndPeriod(caps.wPeriodMin);
		
		// hStdin/hStdout (and all handles) will be closed in msdos_finish()...
		if(get_console_info_success) {
			hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			if(restore_console_on_exit) {
				// window can't be bigger than buffer,
				// buffer can't be smaller than window,
				// so make a tiny window,
				// set the required buffer,
				// then set the required window
				SMALL_RECT rect;
				SET_RECT(rect, 0, csbi.srWindow.Top, 0, csbi.srWindow.Top);
				SetConsoleWindowInfo(hStdout, TRUE, &rect);
				SetConsoleScreenBufferSize(hStdout, csbi.dwSize);
				SET_RECT(rect, 0, 0, csbi.srWindow.Right - csbi.srWindow.Left, csbi.srWindow.Bottom - csbi.srWindow.Top);
				SetConsoleWindowInfo(hStdout, TRUE, &rect);
			}
			SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			SetConsoleCursorInfo(hStdout, &ci);
		}
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode);
		
		msdos_finish();
		
		SetConsoleCtrlHandler(ctrl_handler, FALSE);
	}
	
	hardware_finish();
	
	if(key_buf_char != NULL) {
		key_buf_char->release();
		delete key_buf_char;
		key_buf_char = NULL;
	}
	if(key_buf_scan != NULL) {
		key_buf_scan->release();
		delete key_buf_scan;
		key_buf_scan = NULL;
	}
	if(temp_file_created) {
		DeleteFile(temp_file_path);
		temp_file_created = false;
	}
//	if(argv == dummy_argv) {
//		if(!is_started_from_command_prompt()) {
//			fprintf(stderr, "\nHit any key to quit...");
//			while(!_kbhit()) {
//				Sleep(10);
//			}
//		}
//	}
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return(retval);
}

/* ----------------------------------------------------------------------------
	console
---------------------------------------------------------------------------- */

void change_console_size(int width, int height)
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	SMALL_RECT rect;
	COORD co;
	
	GetConsoleScreenBufferInfo(hStdout, &csbi);
	if(csbi.srWindow.Top != 0 || csbi.dwCursorPosition.Y > height - 1) {
		if(csbi.srWindow.Right - csbi.srWindow.Left + 1 == width && csbi.srWindow.Bottom - csbi.srWindow.Top + 1 == height) {
			ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &csbi.srWindow);
			SET_RECT(rect, 0, 0, width - 1, height - 1);
			WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		} else if(csbi.dwCursorPosition.Y > height - 1) {
			SET_RECT(rect, 0, csbi.dwCursorPosition.Y - (height - 1), width - 1, csbi.dwCursorPosition.Y);
			ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
			SET_RECT(rect, 0, 0, width - 1, height - 1);
			WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		}
	}
	if(csbi.dwCursorPosition.Y > height - 1) {
		co.X = csbi.dwCursorPosition.X;
		co.Y = min(height - 1, csbi.dwCursorPosition.Y - csbi.srWindow.Top);
		SetConsoleCursorPosition(hStdout, co);
		cursor_moved = true;
	}
	
	// window can't be bigger than buffer,
	// buffer can't be smaller than window,
	// so make a tiny window,
	// set the required buffer,
	// then set the required window
	SET_RECT(rect, 0, csbi.srWindow.Top, 0, csbi.srWindow.Top);
	SetConsoleWindowInfo(hStdout, TRUE, &rect);
	co.X = width;
	co.Y = height;
	SetConsoleScreenBufferSize(hStdout, co);
	SET_RECT(rect, 0, 0, width - 1, height - 1);
	SetConsoleWindowInfo(hStdout, TRUE, &rect);
	
	scr_width = scr_buf_size.X = width;
	scr_height = scr_buf_size.Y = height;
	scr_top = 0;
	
	clear_scr_buffer(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	
	int regen = min(scr_width * scr_height * 2, 0x8000);
	text_vram_end_address = text_vram_top_address + regen;
	shadow_buffer_end_address = shadow_buffer_top_address + regen;
	
	if(regen > 0x4000) {
		regen = 0x8000;
		vram_pages = 1;
	} else if(regen > 0x2000) {
		regen = 0x4000;
		vram_pages = 2;
	} else if(regen > 0x1000) {
		regen = 0x2000;
		vram_pages = 4;
	} else {
		regen = 0x1000;
		vram_pages = 8;
	}
	*(UINT16 *)(mem + 0x44a) = scr_width;
	*(UINT16 *)(mem + 0x44c) = regen;
	*(UINT8  *)(mem + 0x484) = scr_height - 1;
	
	mouse.min_position.x = 0;
	mouse.min_position.y = 0;
	mouse.max_position.x = 8 * scr_width  - 1;
	mouse.max_position.y = 8 * scr_height - 1;
	
	restore_console_on_exit = true;
}

void clear_scr_buffer(WORD attr)
{
	for(int y = 0; y < scr_height; y++) {
		for(int x = 0; x < scr_width; x++) {
			SCR_BUF(y,x).Char.AsciiChar = ' ';
			SCR_BUF(y,x).Attributes = attr;
		}
	}
}

bool update_console_input()
{
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD dwNumberOfEvents = 0;
	DWORD dwRead;
	INPUT_RECORD ir[16];
	CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
	bool result = false;
	
	if(GetNumberOfConsoleInputEvents(hStdin, &dwNumberOfEvents) && dwNumberOfEvents != 0) {
		if(ReadConsoleInputA(hStdin, ir, 16, &dwRead)) {
			for(int i = 0; i < dwRead; i++) {
				if(ir[i].EventType & MOUSE_EVENT) {
					if(mouse.active) {
						if(ir[i].Event.MouseEvent.dwEventFlags == 0) {
							for(int i = 0; i < MAX_MOUSE_BUTTONS; i++) {
								static const DWORD bits[] = {
									FROM_LEFT_1ST_BUTTON_PRESSED,	// left
									RIGHTMOST_BUTTON_PRESSED,	// right
									FROM_LEFT_2ND_BUTTON_PRESSED,	// middle
								};
								bool prev_status = mouse.buttons[i].status;
								mouse.buttons[i].status = ((ir[i].Event.MouseEvent.dwButtonState & bits[i]) != 0);
								
								if(!prev_status && mouse.buttons[i].status) {
									mouse.buttons[i].pressed_times++;
									mouse.buttons[i].pressed_position.x = mouse.position.x;
									mouse.buttons[i].pressed_position.y = mouse.position.y;
									mouse.status |= 2 << (i * 2);
								} else if(prev_status && !mouse.buttons[i].status) {
									mouse.buttons[i].released_times++;
									mouse.buttons[i].released_position.x = mouse.position.x;
									mouse.buttons[i].released_position.y = mouse.position.y;
									mouse.status |= 4 << (i * 2);
								}
							}
						} else if(ir[i].Event.MouseEvent.dwEventFlags & MOUSE_MOVED) {
							// NOTE: if restore_console_on_exit, console is not scrolled
							if(!restore_console_on_exit && csbi.srWindow.Bottom == 0) {
								GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
							}
							// FIXME: character size is always 8x8 ???
							int x = 3 + 8 * (ir[i].Event.MouseEvent.dwMousePosition.X);
							int y = 4 + 8 * (ir[i].Event.MouseEvent.dwMousePosition.Y - csbi.srWindow.Top);
							if(mouse.position.x != x || mouse.position.y != y) {
								mouse.position.x = x;
								mouse.position.y = y;
								mouse.status |= 1;
							}
						}
					}
				} else if(ir[i].EventType & KEY_EVENT) {
					// set scan code of last pressed/release key to kbd_data (in-port 60h)
					kbd_data = ir[i].Event.KeyEvent.wVirtualScanCode;
					if(!ir[i].Event.KeyEvent.bKeyDown) {
						// break
						kbd_data |= 0x80;
					} else {
						// make
						kbd_data &= 0x7f;
						
						// update dos key buffer
						UINT8 chr = ir[i].Event.KeyEvent.uChar.AsciiChar;
						UINT8 scn = ir[i].Event.KeyEvent.wVirtualScanCode & 0xff;
						
						if(chr == 0) {
							if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
								if(scn >= 0x3b && scn <= 0x44) {
									scn += 0x68 - 0x3b;	// F1 to F10
								} else if(scn == 0x57 || scn == 0x58) {
									scn += 0x8b - 0x57;	// F11 & F12
								} else if(scn >= 0x47 && scn <= 0x53) {
									scn += 0x97 - 0x47;	// edit/arrow clusters
								} else if(scn == 0x35) {
									scn = 0xa4;		// keypad /
								}
							} else if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
								if(scn == 0x07) {
									chr = 0x1e;	// Ctrl+^
								} else if(scn == 0x0c) {
									chr = 0x1f;	// Ctrl+_
								} else if(scn >= 0x35 && scn <= 0x58) {
									static const UINT8 ctrl_map[] = {
										0x95,	// keypad /
										0,
										0x96,	// keypad *
										0, 0, 0,
										0x5e,	// F1
										0x5f,	// F2
										0x60,	// F3
										0x61,	// F4
										0x62,	// F5
										0x63,	// F6
										0x64,	// F7
										0x65,	// F8
										0x66,	// F9
										0x67,	// F10
										0,
										0,
										0x77,	// Home
										0x8d,	// Up
										0x84,	// PgUp
										0x8e,	// keypad -
										0x73,	// Left
										0x8f,	// keypad center
										0x74,	// Right
										0x90,	// keyapd +
										0x75,	// End
										0x91,	// Down
										0x76,	// PgDn
										0x92,	// Insert
										0x93,	// Delete
										0, 0, 0,
										0x89,	// F11
										0x8a,	// F12
									};
									scn = ctrl_map[scn - 0x35];
								}
							} else if(ir[i].Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) {
								if(scn >= 0x3b && scn <= 0x44) {
									scn += 0x54 - 0x3b;	// F1 to F10
								} else if(scn == 0x57 || scn == 0x58) {
									scn += 0x87 - 0x57;	// F11 & F12
								}
							} else if(scn == 0x57 || scn == 0x58) {
								scn += 0x85 - 0x57;
							}
							// ignore shift, ctrl, alt, win and menu keys
							if(scn != 0x1d && scn != 0x2a && scn != 0x36 && scn != 0x38 && (scn < 0x5b || scn > 0x5e)) {
								if(chr == 0) {
									key_buf_char->write(0x00);
									key_buf_scan->write(ir[i].Event.KeyEvent.dwControlKeyState & ENHANCED_KEY ? 0xe0 : 0x00);
								}
								key_buf_char->write(chr);
								key_buf_scan->write(scn);
							}
						} else {
							if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
								chr = 0;
								if(scn >= 0x02 && scn <= 0x0e) {
									scn += 0x78 - 0x02;	// 1 to 0 - =
								}
							}
							key_buf_char->write(chr);
							key_buf_scan->write(scn);
						}
					}
					result = key_changed = true;
				}
			}
		}
	}
	return(result);
}

bool update_key_buffer()
{
	return(update_console_input() || key_buf_char->count() != 0);
}

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

void msdos_psp_set_file_table(int fd, UINT8 value, int psp_seg);
int msdos_psp_get_file_table(int fd, int psp_seg);
void msdos_putch(UINT8 data);

// process info

process_t *msdos_process_info_create(UINT16 psp_seg)
{
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == 0 || process[i].psp == psp_seg) {
			memset(&process[i], 0, sizeof(process_t));
			process[i].psp = psp_seg;
			return(&process[i]);
		}
	}
	fatalerror("too many processes\n");
	return(NULL);
}

process_t *msdos_process_info_get(UINT16 psp_seg)
{
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			return(&process[i]);
		}
	}
	fatalerror("invalid psp address\n");
	return(NULL);
}

void msdos_sda_update(int psp_seg)
{
	sda_t *sda = (sda_t *)(mem + SDA_TOP);
	
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			sda->switchar = process[i].switchar;
			sda->current_dta.w.l = process[i].dta.w.l;
			sda->current_dta.w.h = process[i].dta.w.h;
			sda->current_psp = process[i].psp;
			break;
		}
	}
	sda->malloc_strategy = malloc_strategy;
	sda->return_code = retval;
	sda->current_drive = _getdrive();
}

// dta info

void msdos_dta_info_init()
{
	for(int i = 0; i < MAX_DTAINFO; i++) {
		dtalist[i].find_handle = INVALID_HANDLE_VALUE;
	}
}

dtainfo_t *msdos_dta_info_get(UINT16 psp_seg, UINT32 dta_laddr)
{
	dtainfo_t *free_dta = NULL;
	for(int i = 0; i < MAX_DTAINFO; i++) {
		if(dtalist[i].find_handle == INVALID_HANDLE_VALUE) {
			if(free_dta == NULL) {
				free_dta = &dtalist[i];
			}
		} else if(dta_laddr < LFN_DTA_LADDR && dtalist[i].dta == dta_laddr) {
			return(&dtalist[i]);
		}
	}
	if(free_dta) {
		free_dta->psp = psp_seg;
		free_dta->dta = dta_laddr;
		return(free_dta);
	}
	fatalerror("too many dta\n");
	return(NULL);
}

void msdos_dta_info_free(UINT16 psp_seg)
{
	for(int i = 0; i < MAX_DTAINFO; i++) {
		if(dtalist[i].psp == psp_seg && dtalist[i].find_handle != INVALID_HANDLE_VALUE) {
			FindClose(dtalist[i].find_handle);
			dtalist[i].find_handle = INVALID_HANDLE_VALUE;
		}
	}
}

void msdos_cds_update(int drv)
{
	cds_t *cds = (cds_t *)(mem + CDS_TOP);
	
	memset(mem + CDS_TOP, 0, CDS_SIZE);
	sprintf(cds->path_name, "%c:\\", 'A' + drv);
	cds->drive_attrib = 0x4000;	// physical drive
	cds->physical_drive_number = drv;
}

// nls information tables

// uppercase table (func 6502h)
void msdos_upper_table_update()
{
	*(UINT16 *)(mem + UPPERTABLE_TOP) = 0x80;
	for(unsigned i = 0; i < 0x80; ++i) {
		UINT8 c[4];
		*(UINT32 *)c = 0;				// reset internal conversion state
		CharUpperBuffA((LPSTR)c, 4);	// (workaround for MBCS codepage) 
		c[0] = 0x80 + i;
		DWORD rc = CharUpperBuffA((LPSTR)c, 1);
		mem[UPPERTABLE_TOP + 2 + i] = (rc == 1 && c[0]) ? c[0] : 0x80 + i;
	}
}

// lowercase table (func 6503h)
void msdos_lower_table_update()
{
	*(UINT16 *)(mem + LOWERTABLE_TOP) = 0x80;
	for(unsigned i = 0; i < 0x80; ++i) {
		UINT8 c[4];
		*(UINT32 *)c = 0;				// reset internal conversion state
		CharLowerBuffA((LPSTR)c, 4);	// (workaround for MBCS codepage) 
		c[0] = 0x80 + i;
		DWORD rc = CharLowerBuffA((LPSTR)c, 1);
		mem[LOWERTABLE_TOP + 2 + i] = (rc == 1 && c[0]) ? c[0] : 0x80 + i;
	}
}

// filename uppercase table (func 6504h)
void msdos_filename_upper_table_init()
{
	// depended on (file)system, not on active codepage
	// temporary solution: just filling data
	*(UINT16 *)(mem + FILENAME_UPPERTABLE_TOP) = 0x80;
	for(unsigned i = 0; i < 0x80; ++i) {
		mem[FILENAME_UPPERTABLE_TOP + 2 + i] = 0x80 + i;
	}
}

// filaname terminator table (func 6505h)
void msdos_filename_terminator_table_init()
{
	const char illegal_chars[] = ".\"/\\[]:|<>+=;,";	// for standard MS-DOS fs.
	UINT8 *data = mem + FILENAME_TERMINATOR_TOP;
	
	data[2] = 1;		// marker? (permissible character value)
	data[3] = 0x00;		// 00h...FFh
	data[4] = 0xff;
	data[5] = 0;		// marker? (excluded character)
	data[6] = 0x00;		// 00h...20h
	data[7] = 0x20;
	data[8] = 2;		// marker? (illegal characters for filename)
	data[9] = (UINT8)strlen(illegal_chars);
	memcpy(data + 10, illegal_chars, data[9]);
	
	// total length
	*(UINT16 *)data = (10 - 2) + data[9];
}

// collating table (func 6506h)
void msdos_collating_table_update()
{
	// temporary solution: just filling data
	*(UINT16 *)(mem + COLLATING_TABLE_TOP) = 0x100;
	for(unsigned i = 0; i < 256; ++i) {
		mem[COLLATING_TABLE_TOP + 2 + i] = i;
	}
}

// dbcs

void msdos_dbcs_table_update()
{
	UINT8 dbcs_data[DBCS_SIZE];
	memset(dbcs_data, 0, sizeof(dbcs_data));
	
	CPINFO info;
	GetCPInfo(active_code_page, &info);
	
	if(info.MaxCharSize != 1) {
		for(int i = 0;; i += 2) {
			UINT8 lo = info.LeadByte[i + 0];
			UINT8 hi = info.LeadByte[i + 1];
			dbcs_data[2 + i + 0] = lo;
			dbcs_data[2 + i + 1] = hi;
			if(lo == 0 && hi == 0) {
				dbcs_data[0] = i + 2;
				break;
			}
		}
	} else {
		dbcs_data[0] = 2;	// ???
	}
	memcpy(mem + DBCS_TOP, dbcs_data, sizeof(dbcs_data));
}

void msdos_dbcs_table_finish()
{
	if(active_code_page != system_code_page) {
		_setmbcp(system_code_page);
	}
}

void msdos_nls_tables_init()
{
	system_code_page = active_code_page = _getmbcp();
	msdos_upper_table_update();
	msdos_lower_table_update();
	msdos_filename_terminator_table_init();
	msdos_filename_upper_table_init();
	msdos_collating_table_update();
	msdos_dbcs_table_update();
}

void msdos_nls_tables_update()
{
	msdos_dbcs_table_update();
	msdos_upper_table_update();
	msdos_lower_table_update();
//	msdos_collating_table_update();
}

int msdos_lead_byte_check(UINT8 code)
{
	UINT8 *dbcs_table = mem + DBCS_TABLE;
	
	for(int i = 0;; i += 2) {
		UINT8 lo = dbcs_table[i + 0];
		UINT8 hi = dbcs_table[i + 1];
		if(lo == 0 && hi == 0) {
			break;
		}
		if(lo <= code && code <= hi) {
			return(1);
		}
	}
	return(0);
}

int msdos_ctrl_code_check(UINT8 code)
{
	return (code >= 0x01 && code <= 0x1a && code != 0x07 && code != 0x08 && code != 0x09 && code != 0x0a && code != 0x0d);
}

// file control

char *msdos_remove_double_quote(char *path)
{
	static char tmp[MAX_PATH];
	
	memset(tmp, 0, sizeof(tmp));
	if(strlen(path) >= 2 && path[0] == '"' && path[strlen(path) - 1] == '"') {
		memcpy(tmp, path + 1, strlen(path) - 2);
	} else {
		strcpy(tmp, path);
	}
	return(tmp);
}

char *msdos_combine_path(char *dir, const char *file)
{
	static char tmp[MAX_PATH];
	char *tmp_dir = msdos_remove_double_quote(dir);
	
	if(strlen(tmp_dir) == 0) {
		strcpy(tmp, file);
	} else if(tmp_dir[strlen(tmp_dir) - 1] == '\\') {
		sprintf(tmp, "%s%s", tmp_dir, file);
	} else {
		sprintf(tmp, "%s\\%s", tmp_dir, file);
	}
	return(tmp);
}

char *msdos_trimmed_path(char *path, int lfn)
{
	static char tmp[MAX_PATH];
	
	if(lfn) {
		strcpy(tmp, path);
	} else {
		// remove space in the path
		char *src = path, *dst = tmp;
		
		while(*src != '\0') {
			if(msdos_lead_byte_check(*src)) {
				*dst++ = *src++;
				*dst++ = *src++;
			} else if(*src != ' ') {
				*dst++ = *src++;
			} else {
				src++;	// skip space
			}
		}
		*dst = '\0';
	}
	if(_stricmp(tmp, "C:\\COMMAND.COM") == 0) {
		// redirect C:\COMMAND.COM to comspec_path
		strcpy(tmp, comspec_path);
	} else if(is_vista_or_later && _access(tmp, 0) != 0) {
		// redirect new files (without wildcards) in C:\ to %TEMP%, since C:\ is not usually writable
		static int root_drive_protected = -1;
		char temp[MAX_PATH], name[MAX_PATH], *name_temp = NULL;
		dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
		
		if(GetFullPathName(tmp, MAX_PATH, temp, &name_temp) != 0 &&
		   name_temp != NULL && strstr(name_temp, "?") == NULL && strstr(name_temp, "*") == NULL) {
			strcpy(name, name_temp);
			name_temp[0] = '\0';
			
			if((temp[0] == 'A' + dos_info->boot_drive - 1 || temp[0] == 'a' + dos_info->boot_drive - 1) &&
			   (temp[1] == ':') && (temp[2] == '\\' || temp[2] == '/') && (temp[3] == '\0')) {
				if(root_drive_protected == -1) {
					FILE *fp = NULL;
					
					sprintf(temp, "%c:\\MS-DOS_Player.$$$", 'A' + dos_info->boot_drive - 1);
					root_drive_protected = 1;
					try {
						if((fp = fopen(temp, "w")) != NULL) {
							if(fprintf(fp, "TEST") == 4) {
								root_drive_protected = 0;
							}
						}
					} catch(...) {
					}
					if(fp != NULL) {
						fclose(fp);
					}
					if(_access(temp, 0) == 0) {
						remove(temp);
					}
				}
				if(root_drive_protected == 1) {
					if(GetEnvironmentVariable("TEMP", temp, MAX_PATH) != 0 ||
					   GetEnvironmentVariable("TMP",  temp, MAX_PATH) != 0) {
						strcpy(tmp, msdos_combine_path(temp, name));
					}
				}
			}
		}
	}
	return(tmp);
}

char *msdos_get_multiple_short_path(char *src)
{
	// LONGPATH;LONGPATH;LONGPATH to SHORTPATH;SHORTPATH;SHORTPATH
	static char env_path[ENV_SIZE];
	char tmp[ENV_SIZE], *token;
	
	memset(env_path, 0, sizeof(env_path));
	strcpy(tmp, src);
	token = my_strtok(tmp, ";");
	
	while(token != NULL) {
		if(token[0] != '\0') {
			char *path = msdos_remove_double_quote(token), short_path[MAX_PATH];
			if(strlen(path) != 0) {
				if(GetShortPathName(path, short_path, MAX_PATH) == 0) {
					strcat(env_path, path);
				} else {
					my_strupr(short_path);
					strcat(env_path, short_path);
				}
				strcat(env_path, ";");
			}
		}
		token = my_strtok(NULL, ";");
	}
	int len = strlen(env_path);
	if(len != 0 && env_path[len - 1] == ';') {
		env_path[len - 1] = '\0';
	}
	return(env_path);
}

bool match(char *text, char *pattern)
{
	// http://www.prefield.com/algorithm/string/wildcard.html
	switch(*pattern) {
	case '\0':
		return !*text;
	case '*':
		return match(text, pattern + 1) || (*text && match(text + 1, pattern));
	case '?':
		return *text && match(text + 1, pattern + 1);
	default:
		return (*text == *pattern) && match(text + 1, pattern + 1);
	}
}

bool msdos_match_volume_label(char *path, char *volume)
{
	char *p;
	
	if(!*volume) {
		return false;
	} else if((p = my_strchr(path, ':')) != NULL) {
		return msdos_match_volume_label(p + 1, volume);
	} else if((p = my_strchr(path, '\\')) != NULL) {
		return msdos_match_volume_label(p + 1, volume);
	} else if((p = my_strchr(path, '.')) != NULL) {
		char tmp[MAX_PATH];
		sprintf(tmp, "%.*s%s", (int)(p - path), path, p + 1);
		return match(volume, tmp);
	} else {
		return match(volume, path);
	}
}

char *msdos_fcb_path(fcb_t *fcb)
{
	static char tmp[MAX_PATH];
	char name[9], ext[4];
	
	memset(name, 0, sizeof(name));
	memcpy(name, fcb->file_name, 8);
	strcpy(name, msdos_trimmed_path(name, 0));
	
	memset(ext, 0, sizeof(ext));
	memcpy(ext, fcb->file_name + 8, 3);
	strcpy(ext, msdos_trimmed_path(ext, 0));
	
	if(name[0] == '\0' || strcmp(name, "????????") == 0) {
		strcpy(name, "*");
	}
	if(ext[0] == '\0') {
		strcpy(tmp, name);
	} else {
		if(strcmp(ext, "???") == 0) {
			strcpy(ext, "*");
		}
		sprintf(tmp, "%s.%s", name, ext);
	}
	return(tmp);
}

void msdos_set_fcb_path(fcb_t *fcb, char *path)
{
	char *ext = my_strchr(path, '.');
	
	memset(fcb->file_name, 0x20, 8 + 3);
	if(ext != NULL && path[0] != '.') {
		*ext = '\0';
		memcpy(fcb->file_name + 8, ext + 1, strlen(ext + 1));
	}
	memcpy(fcb->file_name, path, strlen(path));
}

char *msdos_short_path(char *path)
{
	static char tmp[MAX_PATH];
	
	if(GetShortPathName(path, tmp, MAX_PATH) == 0) {
		strcpy(tmp, path);
	}
	my_strupr(tmp);
	return(tmp);
}

char *msdos_short_name(WIN32_FIND_DATA *fd)
{
	static char tmp[MAX_PATH];

	if(fd->cAlternateFileName[0]) {
		strcpy(tmp, fd->cAlternateFileName);
	} else {
		strcpy(tmp, fd->cFileName);
	}
	my_strupr(tmp);
	return(tmp);
}

char *msdos_short_full_path(char *path)
{
	static char tmp[MAX_PATH];
	char full[MAX_PATH], *name;
	
	// Full works with non-existent files, but Short does not
	GetFullPathName(path, MAX_PATH, full, &name);
	*tmp = '\0';
	if(GetShortPathName(full, tmp, MAX_PATH) == 0 && name > path) {
		name[-1] = '\0';
		DWORD len = GetShortPathName(full, tmp, MAX_PATH);
		if(len == 0) {
			strcpy(tmp, full);
		} else {
			tmp[len++] = '\\';
			strcpy(tmp + len, name);
		}
	}
	my_strupr(tmp);
	return(tmp);
}

char *msdos_short_full_dir(char *path)
{
	static char tmp[MAX_PATH];
	char full[MAX_PATH], *name;
	
	GetFullPathName(path, MAX_PATH, full, &name);
	name[-1] = '\0';
	if(GetShortPathName(full, tmp, MAX_PATH) == 0) {
		strcpy(tmp, full);
	}
	my_strupr(tmp);
	return(tmp);
}

char *msdos_local_file_path(char *path, int lfn)
{
	char *trimmed = msdos_trimmed_path(path, lfn);
#if 0
	// I have forgotten the reason of this routine... :-(
	if(_access(trimmed, 0) != 0) {
		process_t *process = msdos_process_info_get(current_psp);
		static char tmp[MAX_PATH];
		
		sprintf(tmp, "%s\\%s", process->module_dir, trimmed);
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
#endif
	return(trimmed);
}

bool msdos_is_device_path(char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathName(path, MAX_PATH, full, &name) != 0) {
		if(_stricmp(full, "\\\\.\\AUX" ) == 0 ||
		   _stricmp(full, "\\\\.\\CON" ) == 0 ||
		   _stricmp(full, "\\\\.\\NUL" ) == 0 ||
		   _stricmp(full, "\\\\.\\PRN" ) == 0 ||
		   _stricmp(full, "\\\\.\\COM1") == 0 ||
		   _stricmp(full, "\\\\.\\COM2") == 0 ||
		   _stricmp(full, "\\\\.\\COM3") == 0 ||
		   _stricmp(full, "\\\\.\\COM4") == 0 ||
		   _stricmp(full, "\\\\.\\COM5") == 0 ||
		   _stricmp(full, "\\\\.\\COM6") == 0 ||
		   _stricmp(full, "\\\\.\\COM7") == 0 ||
		   _stricmp(full, "\\\\.\\COM8") == 0 ||
		   _stricmp(full, "\\\\.\\COM9") == 0 ||
		   _stricmp(full, "\\\\.\\LPT1") == 0 ||
		   _stricmp(full, "\\\\.\\LPT2") == 0 ||
		   _stricmp(full, "\\\\.\\LPT3") == 0 ||
		   _stricmp(full, "\\\\.\\LPT4") == 0 ||
		   _stricmp(full, "\\\\.\\LPT5") == 0 ||
		   _stricmp(full, "\\\\.\\LPT6") == 0 ||
		   _stricmp(full, "\\\\.\\LPT7") == 0 ||
		   _stricmp(full, "\\\\.\\LPT8") == 0 ||
		   _stricmp(full, "\\\\.\\LPT9") == 0) {
			return(true);
		} else if(name != NULL) {
			if(_stricmp(name, "CLOCK$"  ) == 0 ||
			   _stricmp(name, "CONFIG$" ) == 0 ||
			   _stricmp(name, "EMMXXXX0") == 0 ||
			   _stricmp(name, "$IBMAIAS") == 0) {
				return(true);
			}
		}
	}
	return(false);
}

bool msdos_is_con_path(char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathName(path, MAX_PATH, full, &name) != 0) {
		return(_stricmp(full, "\\\\.\\CON") == 0);
	}
	return(false);
}

int msdos_is_comm_path(char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathName(path, MAX_PATH, full, &name) != 0) {
		if(_stricmp(full, "\\\\.\\COM1") == 0) {
			return(1);
		} else if(_stricmp(full, "\\\\.\\COM2") == 0) {
			return(2);
		} else if(_stricmp(full, "\\\\.\\COM3") == 0) {
			return(3);
		} else if(_stricmp(full, "\\\\.\\COM4") == 0) {
			return(4);
		}
	}
	return(0);
}

int msdos_is_prn_path(char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathName(path, MAX_PATH, full, &name) != 0) {
		if(_stricmp(full, "\\\\.\\PRN") == 0) {
			return(1);
		} else if(_stricmp(full, "\\\\.\\LPT1") == 0) {
			return(1);
		} else if(_stricmp(full, "\\\\.\\LPT2") == 0) {
			return(2);
		} else if(_stricmp(full, "\\\\.\\LPT3") == 0) {
			return(3);
		}
	}
	return(0);
}

char *msdos_create_comm_path(char *path, int port)
{
	static char tmp[MAX_PATH];
	char *p = NULL;
	
	sprintf(tmp, "COM%d", port);
	if((p = strchr(path, ':')) != NULL) {
		strcat(tmp, p);
	}
	return(tmp);
}

bool msdos_is_existing_file(char *path)
{
	// http://d.hatena.ne.jp/yu-hr/20100317/1268826458
	WIN32_FIND_DATA FindData;
	HANDLE hFind;
	
	if((hFind = FindFirstFile(path, &FindData)) != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return(!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
	}
	return(false);
}

char *msdos_search_command_com(char *command_path, char *env_path)
{
	static char tmp[MAX_PATH];
	char path[ENV_SIZE], *file_name;
	
	// check if COMMAND.COM is in the same directory as the target program file
	if(GetFullPathName(command_path, MAX_PATH, tmp, &file_name) != 0) {
		sprintf(file_name, "COMMAND.COM");
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	
	// check if COMMAND.COM is in the same directory as the running msdos.exe
	if(GetModuleFileName(NULL, path, MAX_PATH) != 0 && GetFullPathName(path, MAX_PATH, tmp, &file_name) != 0) {
		sprintf(file_name, "COMMAND.COM");
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	
	// check if COMMAND.COM is in the current directory
	if(GetFullPathName("COMMAND.COM", MAX_PATH, tmp, &file_name) != 0) {
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	
	// cehck if COMMAND.COM is in the directory that is in MSDOS_PATH and PATH environment variables
	strcpy(path, env_path);
	char *token = my_strtok(path, ";");
	while(token != NULL) {
		if(strlen(token) != 0 && token[0] != '%') {
			strcpy(tmp, msdos_combine_path(token, "COMMAND.COM"));
			if(_access(tmp, 0) == 0) {
				return(tmp);
			}
		}
		token = my_strtok(NULL, ";");
	}
	return(NULL);
}

int msdos_drive_number(const char *path)
{
	char tmp[MAX_PATH], *name;
	
	GetFullPathName(path, MAX_PATH, tmp, &name);
	if(tmp[0] >= 'a' && tmp[0] <= 'z') {
		return(tmp[0] - 'a');
	} else {
		return(tmp[0] - 'A');
	}
}

char *msdos_volume_label(char *path)
{
	static char tmp[MAX_PATH];
	char volume[] = "A:\\";
	
	if(path[1] == ':') {
		volume[0] = path[0];
	} else {
		volume[0] = 'A' + _getdrive() - 1;
	}
	if(!GetVolumeInformation(volume, tmp, MAX_PATH, NULL, NULL, NULL, NULL, 0)) {
		memset(tmp, 0, sizeof(tmp));
	}
	return(tmp);
}

char *msdos_short_volume_label(char *label)
{
	static char tmp[(8 + 1 + 3) + 1];
	char *src = label;
	int remain = strlen(label);
	char *dst_n = tmp;
	char *dst_e = tmp + 9;
	
	strcpy(tmp, "        .   ");
	for(int i = 0; i < 8 && remain > 0; i++) {
		if(msdos_lead_byte_check(*src)) {
			if(++i == 8) {
				break;
			}
			*dst_n++ = *src++;
			remain--;
		}
		*dst_n++ = *src++;
		remain--;
	}
	if(remain > 0) {
		for(int i = 0; i < 3 && remain > 0; i++) {
			if(msdos_lead_byte_check(*src)) {
				if(++i == 3) {
					break;
				}
				*dst_e++ = *src++;
				remain--;
			}
			*dst_e++ = *src++;
			remain--;
		}
		*dst_e = '\0';
	} else {
		*dst_n = '\0';
	}
	my_strupr(tmp);
	return(tmp);
}

errno_t msdos_maperr(unsigned long oserrno)
{
	_doserrno = oserrno;
	switch(oserrno) {
	case ERROR_FILE_NOT_FOUND:         // 2
	case ERROR_PATH_NOT_FOUND:         // 3
	case ERROR_INVALID_DRIVE:          // 15
	case ERROR_NO_MORE_FILES:          // 18
	case ERROR_BAD_NETPATH:            // 53
	case ERROR_BAD_NET_NAME:           // 67
	case ERROR_BAD_PATHNAME:           // 161
	case ERROR_FILENAME_EXCED_RANGE:   // 206
		return ENOENT;
	case ERROR_TOO_MANY_OPEN_FILES:    // 4
		return EMFILE;
	case ERROR_ACCESS_DENIED:          // 5
	case ERROR_CURRENT_DIRECTORY:      // 16
	case ERROR_NETWORK_ACCESS_DENIED:  // 65
	case ERROR_CANNOT_MAKE:            // 82
	case ERROR_FAIL_I24:               // 83
	case ERROR_DRIVE_LOCKED:           // 108
	case ERROR_SEEK_ON_DEVICE:         // 132
	case ERROR_NOT_LOCKED:             // 158
	case ERROR_LOCK_FAILED:            // 167
		return EACCES;
	case ERROR_INVALID_HANDLE:         // 6
	case ERROR_INVALID_TARGET_HANDLE:  // 114
	case ERROR_DIRECT_ACCESS_HANDLE:   // 130
		return EBADF;
	case ERROR_ARENA_TRASHED:          // 7
	case ERROR_NOT_ENOUGH_MEMORY:      // 8
	case ERROR_INVALID_BLOCK:          // 9
	case ERROR_NOT_ENOUGH_QUOTA:       // 1816
		return ENOMEM;
	case ERROR_BAD_ENVIRONMENT:        // 10
		return E2BIG;
	case ERROR_BAD_FORMAT:             // 11
		return ENOEXEC;
	case ERROR_NOT_SAME_DEVICE:        // 17
		return EXDEV;
	case ERROR_FILE_EXISTS:            // 80
	case ERROR_ALREADY_EXISTS:         // 183
		return EEXIST;
	case ERROR_NO_PROC_SLOTS:          // 89
	case ERROR_MAX_THRDS_REACHED:      // 164
	case ERROR_NESTING_NOT_ALLOWED:    // 215
		return EAGAIN;
	case ERROR_BROKEN_PIPE:            // 109
		return EPIPE;
	case ERROR_DISK_FULL:              // 112
		return ENOSPC;
	case ERROR_WAIT_NO_CHILDREN:       // 128
	case ERROR_CHILD_NOT_COMPLETE:     // 129
		return ECHILD;
	case ERROR_DIR_NOT_EMPTY:          // 145
		return ENOTEMPTY;
	}
	if(oserrno >= ERROR_WRITE_PROTECT /* 19 */ &&
		oserrno <= ERROR_SHARING_BUFFER_EXCEEDED /* 36 */) {
		return EACCES;
	}
	if(oserrno >= ERROR_INVALID_STARTING_CODESEG /* 188 */ &&
		oserrno <= ERROR_INFLOOP_IN_RELOC_CHAIN /* 202 */) {
		return ENOEXEC;
	}
	return EINVAL;
}

int msdos_open(const char *filename, int oflag)
{
	if((oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR)) != _O_RDONLY) {
		return _open(filename, oflag);
	}
	
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, !(oflag & _O_NOINHERIT) };
	DWORD disposition;
	switch(oflag & (_O_CREAT | _O_EXCL | _O_TRUNC)) {
	default:
	case _O_EXCL:
		disposition = OPEN_EXISTING;
		break;
	case _O_CREAT:
		disposition = OPEN_ALWAYS;
		break;
	case _O_CREAT | _O_EXCL:
	case _O_CREAT | _O_TRUNC | _O_EXCL:
		disposition = CREATE_NEW;
		break;
	case _O_TRUNC:
	case _O_TRUNC | _O_EXCL:
		disposition = TRUNCATE_EXISTING;
		break;
	case _O_CREAT | _O_TRUNC:
		disposition = CREATE_ALWAYS;
		break;
	}
	
	HANDLE h = CreateFile(filename, GENERIC_READ | FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disposition,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if(h == INVALID_HANDLE_VALUE) {
		// FILE_WRITE_ATTRIBUTES may not be granted for standard users.
		// Retry without FILE_WRITE_ATTRIBUTES.
		h = CreateFile(filename, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disposition,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if(h == INVALID_HANDLE_VALUE) {
			errno = msdos_maperr(GetLastError());
			return -1;
		}
	}
	
	int fd = _open_osfhandle((intptr_t) h, oflag);
	if(fd == -1) {
		CloseHandle(h);
	}
	return fd;
}

void msdos_file_handler_open(int fd, const char *path, int atty, int mode, UINT16 info, UINT16 psp_seg)
{
	static int id = 0;
	char full[MAX_PATH], *name;
	
	if(GetFullPathName(path, MAX_PATH, full, &name) != 0) {
		strcpy(file_handler[fd].path, full);
	} else {
		strcpy(file_handler[fd].path, path);
	}
	// isatty makes no distinction between CON & NUL
	// GetFileSize fails on CON, succeeds on NUL
	if(atty && (info != 0x80d3 || GetFileSize((HANDLE)_get_osfhandle(fd), NULL) == 0)) {
		info = 0x8084;
		atty = 0;
	} else if(!atty && info == 0x80d3) {
		info = msdos_drive_number(".");
	}
	file_handler[fd].valid = 1;
	file_handler[fd].id = id++;	// dummy id for int 21h ax=71a6h
	file_handler[fd].atty = atty;
	file_handler[fd].mode = mode;
	file_handler[fd].info = info;
	file_handler[fd].psp = psp_seg;
	
	// init system file table
	if(fd < 20) {
		UINT8 *sft = mem + SFT_TOP + 6 + 0x3b * fd;
		
		memset(sft, 0, 0x3b);
		
		*(UINT16 *)(sft + 0x00) = 1;
		*(UINT16 *)(sft + 0x02) = file_handler[fd].mode;
		*(UINT8  *)(sft + 0x04) = GetFileAttributes(file_handler[fd].path) & 0xff;
		*(UINT16 *)(sft + 0x05) = file_handler[fd].info & 0xff;
		
		if(!(file_handler[fd].info & 0x80)) {
			*(UINT16 *)(sft + 0x07) = sizeof(dpb_t) * (file_handler[fd].info & 0x1f);
			*(UINT16 *)(sft + 0x09) = DPB_TOP >> 4;
			
			FILETIME time, local;
			HANDLE hHandle;
			WORD dos_date = 0, dos_time = 0;
			DWORD file_size = 0;
			if((hHandle = (HANDLE)_get_osfhandle(fd)) != INVALID_HANDLE_VALUE) {
				if(GetFileTime(hHandle, NULL, NULL, &time)) {
					FileTimeToLocalFileTime(&time, &local);
					FileTimeToDosDateTime(&local, &dos_date, &dos_time);
				}
				file_size = GetFileSize(hHandle, NULL);
			}
			*(UINT16 *)(sft + 0x0d) = dos_time;
			*(UINT16 *)(sft + 0x0f) = dos_date;
			*(UINT32 *)(sft + 0x11) = file_size;
		}
		
		char fname[MAX_PATH] = {0}, ext[MAX_PATH] = {0};
		_splitpath(file_handler[fd].path, NULL, NULL, fname, ext);
		my_strupr(fname);
		my_strupr(ext);
		memset(sft + 0x20, 0x20, 11);
		memcpy(sft + 0x20, fname, min(strlen(fname), 8));
		memcpy(sft + 0x28, ext + 1, min(strlen(ext + 1), 3));
		
		*(UINT16 *)(sft + 0x31) = psp_seg;
	}
}

void msdos_file_handler_dup(int dst, int src, UINT16 psp_seg)
{
	strcpy(file_handler[dst].path, file_handler[src].path);
	file_handler[dst].valid = 1;
	file_handler[dst].id = file_handler[src].id;
	file_handler[dst].atty = file_handler[src].atty;
	file_handler[dst].mode = file_handler[src].mode;
	file_handler[dst].info = file_handler[src].info;
	file_handler[dst].psp = psp_seg;
}

void msdos_file_handler_close(int fd)
{
	file_handler[fd].valid = 0;
	
	if(fd < 20) {
		memset(mem + SFT_TOP + 6 + 0x3b * fd, 0, 0x3b);
	}
}

inline int msdos_file_attribute_create(UINT16 new_attr)
{
	return(new_attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
	                   FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE |
	                   FILE_ATTRIBUTE_DIRECTORY));
}

// find file

int msdos_find_file_check_attribute(int attribute, int allowed_mask, int required_mask)
{
	if((allowed_mask & 0x08) && !(attribute & FILE_ATTRIBUTE_DIRECTORY)) {
		return(0);	// search directory only !!!
	} else if(!(allowed_mask & 0x02) && (attribute & FILE_ATTRIBUTE_HIDDEN)) {
		return(0);
	} else if(!(allowed_mask & 0x04) && (attribute & FILE_ATTRIBUTE_SYSTEM)) {
		return(0);
	} else if(!(allowed_mask & 0x10) && (attribute & FILE_ATTRIBUTE_DIRECTORY)) {
		return(0);
	} else if((attribute & required_mask) != required_mask) {
		return(0);
	} else {
		return(1);
	}
}

int msdos_find_file_has_8dot3name(WIN32_FIND_DATA *fd)
{
	if(fd->cAlternateFileName[0]) {
		return 1;
	}
	size_t len = strlen(fd->cFileName);
	if(len > 12) {
		return 0;
	}
	const char *ext = strrchr(fd->cFileName, '.');
	if((ext ? ext - fd->cFileName : len) > 8) {
		return 0;
	}
	return 1;
}

void msdos_find_file_conv_local_time(WIN32_FIND_DATA *fd)
{
	FILETIME local;
	
	FileTimeToLocalFileTime(&fd->ftCreationTime, &local);
	fd->ftCreationTime.dwLowDateTime = local.dwLowDateTime;
	fd->ftCreationTime.dwHighDateTime = local.dwHighDateTime;
	
	FileTimeToLocalFileTime(&fd->ftLastAccessTime, &local);
	fd->ftLastAccessTime.dwLowDateTime = local.dwLowDateTime;
	fd->ftLastAccessTime.dwHighDateTime = local.dwHighDateTime;
	
	FileTimeToLocalFileTime(&fd->ftLastWriteTime, &local);
	fd->ftLastWriteTime.dwLowDateTime = local.dwLowDateTime;
	fd->ftLastWriteTime.dwHighDateTime = local.dwHighDateTime;
}

// i/o

void msdos_stdio_reopen()
{
	if(!file_handler[0].valid) {
		_dup2(DUP_STDIN, 0);
		msdos_file_handler_open(0, "STDIN", _isatty(0), 0, 0x80d3, 0);
	}
	if(!file_handler[1].valid) {
		_dup2(DUP_STDOUT, 1);
		msdos_file_handler_open(1, "STDOUT", _isatty(1), 1, 0x80d3, 0);
	}
	if(!file_handler[2].valid) {
		_dup2(DUP_STDERR, 2);
		msdos_file_handler_open(2, "STDERR", _isatty(2), 1, 0x80d3, 0);
	}
	if(!file_handler[3].valid) {
		_dup2(DUP_STDAUX, 3);
		msdos_file_handler_open(3, "STDAUX", 0, 2, 0x80c0, 0);
	}
	if(!file_handler[4].valid) {
		_dup2(DUP_STDPRN, 4);
		msdos_file_handler_open(4, "STDPRN", 0, 1, 0xa8c0, 0);
	}
	for(int i = 0; i < 5; i++) {
		if(msdos_psp_get_file_table(i, current_psp) == 0xff) {
			msdos_psp_set_file_table(i, i, current_psp);
		}
	}
}

int msdos_kbhit()
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(0, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdin is redirected to file
		return(eof(fd) == 0);
	}
	
	// check keyboard status
	if(key_buf_char->count() != 0 || key_code != 0) {
		return(1);
	} else {
		return(_kbhit());
	}
}

int msdos_getch_ex(int echo)
{
	static char prev = 0;
	
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(0, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdin is redirected to file
retry:
		char data;
		if(_read(fd, &data, 1) == 1) {
			char tmp = data;
			if(data == 0x0a) {
				if(prev == 0x0d) {
					goto retry; // CRLF -> skip LF
				} else {
					data = 0x0d; // LF only -> CR
				}
			}
			prev = tmp;
			return(data);
		}
		return(EOF);
	}
	
	// input from console
	int key_char, key_scan;
	if(key_code != 0) {
		key_char = (key_code >> 0) & 0xff;
		key_scan = (key_code >> 8) & 0xff;
		key_code >>= 16;
	} else {
		while(key_buf_char->count() == 0 && !m_halted && !ctrl_c_pressed) {
			if(!(fd < process->max_files && file_handler[fd].valid && file_handler[fd].atty && file_mode[file_handler[fd].mode].in)) {
				// NOTE: stdin is redirected to stderr when we do "type (file) | more" on freedos's command.com
				if(_kbhit()) {
					key_buf_char->write(_getch());
					key_buf_scan->write(0);
				} else {
					Sleep(10);
				}
			} else {
				if(!update_key_buffer()) {
					Sleep(10);
				}
			}
		}
		if(m_halted) {
			// ctrl-break pressed - insert CR to terminate input loops
			key_char = 0x0d;
			key_scan = 0;
		} else if(ctrl_c_pressed) {
			// ctrl-c pressed
			key_char = 0x03;
			key_scan = 0;
		} else {
			key_char = key_buf_char->read();
			key_scan = key_buf_scan->read();
		}
	}
	if(echo && key_char) {
		msdos_putch(key_char);
	}
	return key_char ? key_char : (key_scan != 0xe0) ? key_scan : 0;
}

inline int msdos_getch()
{
	return(msdos_getch_ex(0));
}

inline int msdos_getche()
{
	return(msdos_getch_ex(1));
}

int msdos_write(int fd, const void *buffer, unsigned int count)
{
	static int is_cr = 0;
	
	if(fd == 1 && !file_handler[1].atty) {
		// CR+LF -> LF
		UINT8 *buf = (UINT8 *)buffer;
		for(unsigned int i = 0; i < count; i++) {
			UINT8 data = buf[i];
			if(is_cr) {
				if(data != 0x0a) {
					UINT8 tmp = 0x0d;
					_write(1, &tmp, 1);
				}
				_write(1, &data, 1);
				is_cr = 0;
			} else if(data == 0x0d) {
				is_cr = 1;
			} else {
				_write(1, &data, 1);
			}
		}
		return(count);
	}
	vram_flush();
	return(_write(fd, buffer, count));
}

void msdos_putch(UINT8 data)
{
	static int p = 0;
	static int is_kanji = 0;
	static int is_esc = 0;
	static int stored_x;
	static int stored_y;
	static WORD stored_a;
	static char tmp[64], out[64];
	
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(1, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdout is redirected to file
		msdos_write(fd, &data, 1);
		return;
	}
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	
	// output to console
	tmp[p++] = data;
	
	vram_flush();
	
	if(is_kanji) {
		// kanji character
		is_kanji = 0;
	} else if(is_esc) {
		// escape sequense
		if((tmp[1] == ')' || tmp[1] == '(') && p == 3) {
			p = is_esc = 0;
		} else if(tmp[1] == '=' && p == 4) {
			COORD co;
			co.X = tmp[3] - 0x20;
			co.Y = tmp[2] - 0x20 + scr_top;
			SetConsoleCursorPosition(hStdout, co);
			mem[0x450 + mem[0x462] * 2] = co.X;
			mem[0x451 + mem[0x462] * 2] = co.Y - scr_top;
			cursor_moved = false;
			p = is_esc = 0;
		} else if((data >= 'a' && data <= 'z') || (data >= 'A' && data <= 'Z') || data == '*') {
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			COORD co;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			co.X = csbi.dwCursorPosition.X;
			co.Y = csbi.dwCursorPosition.Y;
			WORD wAttributes = csbi.wAttributes;
			
			if(tmp[1] == 'D') {
				co.Y++;
			} else if(tmp[1] == 'E') {
				co.X = 0;
				co.Y++;
			} else if(tmp[1] == 'M') {
				co.Y--;
			} else if(tmp[1] == '*') {
				SMALL_RECT rect;
				SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
				WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
				co.X = 0;
				co.Y = csbi.srWindow.Top;
			} else if(tmp[1] == '[') {
				int param[256], params = 0;
				memset(param, 0, sizeof(param));
				for(int i = 2; i < p; i++) {
					if(tmp[i] >= '0' && tmp[i] <= '9') {
						param[params] *= 10;
						param[params] += tmp[i] - '0';
					} else {
						params++;
					}
				}
				if(data == 'A') {
					co.Y -= (params == 0) ? 1 : param[0];
				} else if(data == 'B') {
					co.Y += (params == 0) ? 1 : param[0];
				} else if(data == 'C') {
					co.X += (params == 0) ? 1 : param[0];
				} else if(data == 'D') {
					co.X -= (params == 0) ? 1 : param[0];
				} else if(data == 'H' || data == 'f') {
					co.X = (param[1] == 0 ? 1 : param[1]) - 1;
					co.Y = (param[0] == 0 ? 1 : param[0]) - 1 + csbi.srWindow.Top;
				} else if(data == 'J') {
					SMALL_RECT rect;
					clear_scr_buffer(csbi.wAttributes);
					if(param[0] == 0) {
						SET_RECT(rect, co.X, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						if(co.Y < csbi.srWindow.Bottom) {
							SET_RECT(rect, 0, co.Y + 1, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
							WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						}
					} else if(param[0] == 1) {
						if(co.Y > csbi.srWindow.Top) {
							SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, co.Y - 1);
							WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						}
						SET_RECT(rect, 0, co.Y, co.X, co.Y);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 2) {
						SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						co.X = co.Y = 0;
					}
				} else if(data == 'K') {
					SMALL_RECT rect;
					clear_scr_buffer(csbi.wAttributes);
					if(param[0] == 0) {
						SET_RECT(rect, co.X, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 1) {
						SET_RECT(rect, 0, co.Y, co.X, co.Y);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 2) {
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					}
				} else if(data == 'L') {
					SMALL_RECT rect;
					if(params == 0) {
						param[0] = 1;
					}
					SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
					ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					SET_RECT(rect, 0, co.Y + param[0], csbi.dwSize.X - 1, csbi.srWindow.Bottom);
					WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					clear_scr_buffer(csbi.wAttributes);
					SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, co.Y + param[0] - 1);
					WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					co.X = 0;
				} else if(data == 'M') {
					SMALL_RECT rect;
					if(params == 0) {
						param[0] = 1;
					}
					if(co.Y + param[0] > csbi.srWindow.Bottom) {
						clear_scr_buffer(csbi.wAttributes);
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else {
						SET_RECT(rect, 0, co.Y + param[0], csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						clear_scr_buffer(csbi.wAttributes);
					}
					co.X = 0;
				} else if(data == 'h') {
					if(tmp[2] == '>' && tmp[3] == '5') {
						CONSOLE_CURSOR_INFO cur;
						GetConsoleCursorInfo(hStdout, &cur);
						if(cur.bVisible) {
							cur.bVisible = FALSE;
//							SetConsoleCursorInfo(hStdout, &cur);
						}
					}
				} else if(data == 'l') {
					if(tmp[2] == '>' && tmp[3] == '5') {
						CONSOLE_CURSOR_INFO cur;
						GetConsoleCursorInfo(hStdout, &cur);
						if(!cur.bVisible) {
							cur.bVisible = TRUE;
//							SetConsoleCursorInfo(hStdout, &cur);
						}
					}
				} else if(data == 'm') {
					wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
					int reverse = 0, hidden = 0;
					for(int i = 0; i < params; i++) {
						if(param[i] == 1) {
							wAttributes |= FOREGROUND_INTENSITY;
						} else if(param[i] == 4) {
							wAttributes |= COMMON_LVB_UNDERSCORE;
						} else if(param[i] == 7) {
							reverse = 1;
						} else if(param[i] == 8 || param[i] == 16) {
							hidden = 1;
						} else if((param[i] >= 17 && param[i] <= 23) || (param[i] >= 30 && param[i] <= 37)) {
							wAttributes &= ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
							if(param[i] >= 17 && param[i] <= 23) {
								param[i] -= 16;
							} else {
								param[i] -= 30;
							}
							if(param[i] & 1) {
								wAttributes |= FOREGROUND_RED;
							}
							if(param[i] & 2) {
								wAttributes |= FOREGROUND_GREEN;
							}
							if(param[i] & 4) {
								wAttributes |= FOREGROUND_BLUE;
							}
						} else if(param[i] >= 40 && param[i] <= 47) {
							wAttributes &= ~(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
							if((param[i] - 40) & 1) {
								wAttributes |= BACKGROUND_RED;
							}
							if((param[i] - 40) & 2) {
								wAttributes |= BACKGROUND_GREEN;
							}
							if((param[i] - 40) & 4) {
								wAttributes |= BACKGROUND_BLUE;
							}
						}
					}
					if(reverse) {
						wAttributes &= ~0xff;
						wAttributes |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
					}
					if(hidden) {
						wAttributes &= ~0x0f;
						wAttributes |= (wAttributes >> 4) & 0x0f;
					}
				} else if(data == 'n') {
					if(param[0] == 6) {
						char tmp[16];
						sprintf(tmp, "\x1b[%d;%dR", co.Y + 1, co.X + 1);
						int len = strlen(tmp);
						for(int i = 0; i < len; i++) {
							key_buf_char->write(tmp[i]);
							key_buf_scan->write(0x00);
						}
					}
				} else if(data == 's') {
					stored_x = co.X;
					stored_y = co.Y;
					stored_a = wAttributes;
				} else if(data == 'u') {
					co.X = stored_x;
					co.Y = stored_y;
					wAttributes = stored_a;
				}
			}
			if(co.X < 0) {
				co.X = 0;
			} else if(co.X >= csbi.dwSize.X) {
				co.X = csbi.dwSize.X - 1;
			}
			if(co.Y < csbi.srWindow.Top) {
				co.Y = csbi.srWindow.Top;
			} else if(co.Y > csbi.srWindow.Bottom) {
				co.Y = csbi.srWindow.Bottom;
			}
			if(co.X != csbi.dwCursorPosition.X || co.Y != csbi.dwCursorPosition.Y) {
				SetConsoleCursorPosition(hStdout, co);
				mem[0x450 + mem[0x462] * 2] = co.X;
				mem[0x451 + mem[0x462] * 2] = co.Y - csbi.srWindow.Top;
				cursor_moved = false;
			}
			if(wAttributes != csbi.wAttributes) {
				SetConsoleTextAttribute(hStdout, wAttributes);
			}
			p = is_esc = 0;
		}
		return;
	} else {
		if(msdos_lead_byte_check(data)) {
			is_kanji = 1;
			return;
		} else if(data == 0x1b) {
			is_esc = 1;
			return;
		}
	}
	
	DWORD q = 0, num;
	is_kanji = 0;
	for(int i = 0; i < p; i++) {
		UINT8 c = tmp[i];
		if(is_kanji) {
			is_kanji = 0;
		} else if(msdos_lead_byte_check(data)) {
			is_kanji = 1;
		} else if(msdos_ctrl_code_check(data)) {
			out[q++] = '^';
			c += 'A' - 1;
		}
		out[q++] = c;
	}
	WriteConsole(hStdout, out, q, &num, NULL);
	p = 0;
	
	if(!restore_console_on_exit) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(hStdout, &csbi);
		scr_top = csbi.srWindow.Top;
	}
	cursor_moved = true;
}

int msdos_aux_in()
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(3, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !eof(fd)) {
		char data = 0;
		_read(fd, &data, 1);
		return(data);
	} else {
		return(EOF);
	}
}

void msdos_aux_out(char data)
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(3, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		msdos_write(fd, &data, 1);
	}
}

void msdos_prn_out(char data)
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(4, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		msdos_write(fd, &data, 1);
	}
}

// memory control

mcb_t *msdos_mcb_create(int mcb_seg, UINT8 mz, UINT16 psp, int paragraphs)
{
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->mz = mz;
	mcb->psp = psp;
	mcb->paragraphs = paragraphs;
	return(mcb);
}

void msdos_mcb_check(mcb_t *mcb)
{
	if(!(mcb->mz == 'M' || mcb->mz == 'Z')) {
		#if 0
			// shutdown now !!!
			fatalerror("broken memory control block\n");
		#else
			// return error code and continue
			throw(0x07); // broken memory control block
		#endif
	}
}

int msdos_mem_split(int seg, int paragraphs)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	
	if(mcb->paragraphs > paragraphs) {
		int new_seg = mcb_seg + 1 + paragraphs;
		int new_paragraphs = mcb->paragraphs - paragraphs - 1;
		
		msdos_mcb_create(new_seg, mcb->mz, 0, new_paragraphs);
		mcb->mz = 'M';
		mcb->paragraphs = paragraphs;
		return(0);
	}
	return(-1);
}

void msdos_mem_merge(int seg)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	
	while(1) {
		if(mcb->mz == 'Z') {
			break;
		}
		int next_seg = mcb_seg + 1 + mcb->paragraphs;
		mcb_t *next_mcb = (mcb_t *)(mem + (next_seg << 4));
		msdos_mcb_check(next_mcb);
		
		if(next_mcb->psp != 0) {
			break;
		}
		mcb->mz = next_mcb->mz;
		mcb->paragraphs = mcb->paragraphs + 1 + next_mcb->paragraphs;
	}
}

int msdos_mem_alloc(int mcb_seg, int paragraphs, int new_process)
{
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		
		if(mcb->psp == 0) {
			msdos_mem_merge(mcb_seg + 1);
		} else {
			msdos_mcb_check(mcb);
		}
		if(!(new_process && mcb->mz != 'Z')) {
			if(mcb->psp == 0 && mcb->paragraphs >= paragraphs) {
				msdos_mem_split(mcb_seg + 1, paragraphs);
				mcb->psp = current_psp;
				return(mcb_seg + 1);
			}
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(-1);
}

int msdos_mem_realloc(int seg, int paragraphs, int *max_paragraphs)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	int current_paragraphs = mcb->paragraphs;
	
	msdos_mem_merge(seg);
	if(paragraphs > mcb->paragraphs) {
		if(max_paragraphs) {
			*max_paragraphs = mcb->paragraphs;
		}
		msdos_mem_split(seg, current_paragraphs);
		return(-1);
	}
	msdos_mem_split(seg, paragraphs);
	return(0);
}

void msdos_mem_free(int seg)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	
	mcb->psp = 0;
	msdos_mem_merge(seg);
}

int msdos_mem_get_free(int mcb_seg, int new_process)
{
	int max_paragraphs = 0;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		
		if(!(new_process && mcb->mz != 'Z')) {
			if(mcb->psp == 0 && mcb->paragraphs > max_paragraphs) {
				max_paragraphs = mcb->paragraphs;
			}
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(max_paragraphs > 0x7fff && limit_max_memory ? 0x7fff : max_paragraphs);
}

int msdos_mem_get_last_mcb(int mcb_seg, UINT16 psp)
{
	int last_seg = -1;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		
		if(mcb->psp == psp) {
			last_seg = mcb_seg;
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(last_seg);
}

int msdos_mem_get_umb_linked()
{
	int mcb_seg = first_mcb;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		
		if(mcb->mz == 'Z') {
			if(mcb_seg >= (UMB_TOP >> 4)) {
				return(-1);
			}
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(0);
}

int msdos_mem_link_umb()
{
	int mcb_seg = first_mcb;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		mcb_seg += 1 + mcb->paragraphs;
		
		if(mcb->mz == 'Z') {
			if(mcb_seg == (MEMORY_END >> 4) - 1) {
				mcb->mz = 'M';
				((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked |= 0x01;
				return(-1);
			}
			break;
		}
	}
	return(0);
}

int msdos_mem_unlink_umb()
{
	int mcb_seg = first_mcb;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		mcb_seg += 1 + mcb->paragraphs;
		
		if(mcb->mz == 'Z') {
			break;
		} else {
			if(mcb_seg == (MEMORY_END >> 4) - 1) {
				mcb->mz = 'Z';
				((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked &= ~0x01;
				return(-1);
			}
		}
	}
	return(0);
}

#ifdef SUPPORT_HMA

hma_mcb_t *msdos_hma_mcb_create(int offset, int owner, int size, int next)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	mcb->ms[0] = 'M';
	mcb->ms[1] = 'S';
	mcb->owner = owner;
	mcb->size = size;
	mcb->next = next;
	return(mcb);
}

bool msdos_is_hma_mcb_valid(hma_mcb_t *mcb)
{
	return(mcb->ms[0] == 'M' && mcb->ms[1] == 'S');
}

int msdos_hma_mem_split(int offset, int size)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return(-1);
	}
	if(mcb->size >= size + 0x10) {
		int new_offset = offset + 0x10 + size;
		int new_size = mcb->size - 0x10 - size;
		
		msdos_hma_mcb_create(new_offset, 0, new_size, mcb->next);
		mcb->size = size;
		mcb->next = new_offset;
		return(0);
	}
	return(-1);
}

void msdos_hma_mem_merge(int offset)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return;
	}
	while(1) {
		if(mcb->next == 0) {
			break;
		}
		hma_mcb_t *next_mcb = (hma_mcb_t *)(mem + 0xffff0 + mcb->next);
		
		if(!msdos_is_hma_mcb_valid(next_mcb)) {
			return;
		}
		if(next_mcb->owner != 0) {
			break;
		}
		mcb->size += 0x10 + next_mcb->size;
		mcb->next = next_mcb->next;
	}
}

int msdos_hma_mem_alloc(int size, UINT16 owner)
{
	int offset = 0x10; // first mcb in HMA
	
	while(1) {
		hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
		
		if(!msdos_is_hma_mcb_valid(mcb)) {
			return(-1);
		}
		if(mcb->owner == 0) {
			msdos_hma_mem_merge(offset);
		}
		if(mcb->owner == 0 && mcb->size >= size) {
			msdos_hma_mem_split(offset, size);
			mcb->owner = owner;
			return(offset);
		}
		if(mcb->next == 0) {
			break;
		}
		offset = mcb->next;
	}
	return(-1);
}

int msdos_hma_mem_realloc(int offset, int size)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return(-1);
	}
	if(mcb->size < size) {
		return(-1);
	}
	msdos_hma_mem_split(offset, size);
	return(0);
}

void msdos_hma_mem_free(int offset)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return;
	}
	mcb->owner = 0;
	msdos_hma_mem_merge(offset);
}

int msdos_hma_mem_get_free(int *available_offset)
{
	int offset = 0x10; // first mcb in HMA
	int size = 0;
	
	while(1) {
		hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
		
		if(!msdos_is_hma_mcb_valid(mcb)) {
			return(0);
		}
		if(mcb->owner == 0 && size < mcb->size) {
			if(available_offset != NULL) {
				*available_offset = offset;
			}
			size = mcb->size;
		}
		if(mcb->next == 0) {
			break;
		}
		offset = mcb->next;
	}
	return(size);
}

#endif

// environment

void msdos_env_set_argv(int env_seg, char *argv)
{
	char *dst = (char *)(mem + (env_seg << 4));
	
	while(1) {
		if(dst[0] == 0) {
			break;
		}
		dst += strlen(dst) + 1;
	}
	*dst++ = 0; // end of environment
	*dst++ = 1; // top of argv[0]
	*dst++ = 0;
	memcpy(dst, argv, strlen(argv));
	dst += strlen(argv);
	*dst++ = 0;
	*dst++ = 0;
}

char *msdos_env_get_argv(int env_seg)
{
	static char env[ENV_SIZE];
	char *src = env;
	
	memcpy(src, mem + (env_seg << 4), ENV_SIZE);
	while(1) {
		if(src[0] == 0) {
			if(src[1] == 1) {
				return(src + 3);
			}
			break;
		}
		src += strlen(src) + 1;
	}
	return(NULL);
}

char *msdos_env_get(int env_seg, const char *name)
{
	static char env[ENV_SIZE];
	char *src = env;
	
	memcpy(src, mem + (env_seg << 4), ENV_SIZE);
	while(1) {
		if(src[0] == 0) {
			break;
		}
		int len = strlen(src);
		char *n = my_strtok(src, "=");
		char *v = src + strlen(n) + 1;
		
		if(_stricmp(name, n) == 0) {
			return(v);
		}
		src += len + 1;
	}
	return(NULL);
}

void msdos_env_set(int env_seg, char *name, char *value)
{
	char env[ENV_SIZE];
	char *src = env;
	char *dst = (char *)(mem + (env_seg << 4));
	char *argv = msdos_env_get_argv(env_seg);
	int done = 0;
	
	memcpy(src, dst, ENV_SIZE);
	memset(dst, 0, ENV_SIZE);
	while(1) {
		if(src[0] == 0) {
			break;
		}
		int len = strlen(src);
		char *n = my_strtok(src, "=");
		char *v = src + strlen(n) + 1;
		char tmp[1024];
		
		if(_stricmp(name, n) == 0) {
			sprintf(tmp, "%s=%s", n, value);
			done = 1;
		} else {
			sprintf(tmp, "%s=%s", n, v);
		}
		memcpy(dst, tmp, strlen(tmp));
		dst += strlen(tmp) + 1;
		src += len + 1;
	}
	if(!done) {
		char tmp[1024];
		
		sprintf(tmp, "%s=%s", name, value);
		memcpy(dst, tmp, strlen(tmp));
		dst += strlen(tmp) + 1;
	}
	if(argv) {
		*dst++ = 0; // end of environment
		*dst++ = 1; // top of argv[0]
		*dst++ = 0;
		memcpy(dst, argv, strlen(argv));
		dst += strlen(argv);
		*dst++ = 0;
		*dst++ = 0;
	}
}

// process

psp_t *msdos_psp_create(int psp_seg, UINT16 mcb_seg, UINT16 parent_psp, UINT16 env_seg)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	memset(psp, 0, PSP_SIZE);
	psp->exit[0] = 0xcd;
	psp->exit[1] = 0x20;
	psp->first_mcb = mcb_seg;
	psp->far_call = 0xea;
	psp->cpm_entry.w.l = 0xfff1;	// int 21h, retf
	psp->cpm_entry.w.h = 0xf000;
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = parent_psp;
	if(parent_psp == (UINT16)-1) {
		for(int i = 0; i < 20; i++) {
			if(file_handler[i].valid) {
				psp->file_table[i] = i;
			} else {
				psp->file_table[i] = 0xff;
			}
		}
	} else {
		memcpy(psp->file_table, ((psp_t *)(mem + (parent_psp << 4)))->file_table, 20);
	}
	psp->env_seg = env_seg;
	psp->stack.w.l = REG16(SP);
	psp->stack.w.h = SREG(SS);
	psp->file_table_size = 20;
	psp->file_table_ptr.w.l = 0x18;
	psp->file_table_ptr.w.h = psp_seg;
	psp->service[0] = 0xcd;
	psp->service[1] = 0x21;
	psp->service[2] = 0xcb;
	return(psp);
}

void msdos_psp_set_file_table(int fd, UINT8 value, int psp_seg)
{
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		psp->file_table[fd] = value;
	}
}

int msdos_psp_get_file_table(int fd, int psp_seg)
{
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		fd = psp->file_table[fd];
	}
	return fd;
}

int msdos_process_exec(char *cmd, param_block_t *param, UINT8 al)
{
	// load command file
	int fd = -1;
	int dos_command = 0;
	char command[MAX_PATH], path[MAX_PATH], opt[MAX_PATH], *name = NULL, name_tmp[MAX_PATH];
	
	int opt_ofs = (param->cmd_line.w.h << 4) + param->cmd_line.w.l;
	int opt_len = mem[opt_ofs];
	memset(opt, 0, sizeof(opt));
	memcpy(opt, mem + opt_ofs + 1, opt_len);
	
	if(strlen(cmd) >= 5 && _stricmp(&cmd[strlen(cmd) - 4], ".BAT") == 0) {
		// this is a batch file, run command.com
		char tmp[MAX_PATH];
		if(opt_len != 0) {
			sprintf(tmp, "/C %s %s", cmd, opt);
		} else {
			sprintf(tmp, "/C %s", cmd);
		}
		strcpy(opt, tmp);
		opt_len = strlen(opt);
		mem[opt_ofs] = opt_len;
		sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
		strcpy(command, comspec_path);
		strcpy(name_tmp, "COMMAND.COM");
	} else {
		if(_stricmp(cmd, "C:\\COMMAND.COM") == 0) {
			// redirect C:\COMMAND.COM to comspec_path
			strcpy(command, comspec_path);
		} else {
			strcpy(command, cmd);
		}
		if(GetFullPathName(command, MAX_PATH, path, &name) == 0) {
			return(-1);
		}
		memset(name_tmp, 0, sizeof(name_tmp));
		strcpy(name_tmp, name);
		
		// check command.com
		if(_stricmp(name, "COMMAND.COM") == 0 || _stricmp(name, "COMMAND") == 0) {
			if(opt_len == 0) {
//				process_t *current_process = msdos_process_info_get(current_psp);
				process_t *current_process = NULL;
				for(int i = 0; i < MAX_PROCESS; i++) {
					if(process[i].psp == current_psp) {
						current_process = &process[i];
						break;
					}
				}
				if(current_process != NULL) {
					param->cmd_line.dw = current_process->dta.dw;
					opt_ofs = (param->cmd_line.w.h << 4) + param->cmd_line.w.l;
					opt_len = mem[opt_ofs];
					memset(opt, 0, sizeof(opt));
					memcpy(opt, mem + opt_ofs + 1, opt_len);
				}
			}
			for(int i = 0; i < opt_len; i++) {
				if(opt[i] == ' ') {
					continue;
				}
				if(opt[i] == '/' && (opt[i + 1] == 'c' || opt[i + 1] == 'C') && opt[i + 2] == ' ') {
					for(int j = i + 3; j < opt_len; j++) {
						if(opt[j] == ' ') {
							continue;
						}
						char *token = my_strtok(opt + j, " ");
						
						if(strlen(token) >= 5 && _stricmp(&token[strlen(token) - 4], ".BAT") == 0) {
							// this is a batch file, okay to run command.com
						} else {
							// run program directly without command.com
							strcpy(command, token);
							char tmp[MAX_PATH];
							strcpy(tmp, token + strlen(token) + 1);
							strcpy(opt, tmp);
							opt_len = strlen(opt);
							mem[opt_ofs] = opt_len;
							sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
							dos_command = 1;
						}
						break;
					}
				}
				break;
			}
		}
	}
	
	// load command file
	strcpy(path, command);
	if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
		sprintf(path, "%s.COM", command);
		if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
			sprintf(path, "%s.EXE", command);
			if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
				sprintf(path, "%s.BAT", command);
				if(_access(path, 0) == 0) {
					// this is a batch file, run command.com
					char tmp[MAX_PATH];
					if(opt_len != 0) {
						sprintf(tmp, "/C %s %s", path, opt);
					} else {
						sprintf(tmp, "/C %s", path);
					}
					strcpy(opt, tmp);
					opt_len = strlen(opt);
					mem[opt_ofs] = opt_len;
					sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
					strcpy(path, comspec_path);
					strcpy(name_tmp, "COMMAND.COM");
					fd = _open(path, _O_RDONLY | _O_BINARY);
				} else {
					// search path in parent environments
					psp_t *parent_psp = (psp_t *)(mem + (current_psp << 4));
					char *env = msdos_env_get(parent_psp->env_seg, "PATH");
					if(env != NULL) {
						char env_path[4096];
						strcpy(env_path, env);
						char *token = my_strtok(env_path, ";");
						
						while(token != NULL) {
							if(strlen(token) != 0) {
								sprintf(path, "%s", msdos_combine_path(token, command));
								if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
									break;
								}
								sprintf(path, "%s.COM", msdos_combine_path(token, command));
								if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
									break;
								}
								sprintf(path, "%s.EXE", msdos_combine_path(token, command));
								if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
									break;
								}
								sprintf(path, "%s.BAT", msdos_combine_path(token, command));
								if(_access(path, 0) == 0) {
									// this is a batch file, run command.com
									char tmp[MAX_PATH];
									if(opt_len != 0) {
										sprintf(tmp, "/C %s %s", path, opt);
									} else {
										sprintf(tmp, "/C %s", path);
									}
									strcpy(opt, tmp);
									opt_len = strlen(opt);
									mem[opt_ofs] = opt_len;
									sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
									strcpy(path, comspec_path);
									strcpy(name_tmp, "COMMAND.COM");
									fd = _open(path, _O_RDONLY | _O_BINARY);
									break;
								}
							}
							token = my_strtok(NULL, ";");
						}
					}
				}
			}
		}
	}
	if(fd == -1) {
		if(dos_command) {
			// may be dos command
			char tmp[MAX_PATH];
			sprintf(tmp, "%s %s", command, opt);
			system(tmp);
			return(0);
		} else {
			return(-1);
		}
	}
	_read(fd, file_buffer, sizeof(file_buffer));
	_close(fd);
	
	// copy environment
	int umb_linked, env_seg, psp_seg;
	
	if((umb_linked = msdos_mem_get_umb_linked()) != 0) {
		msdos_mem_unlink_umb();
	}
	if((env_seg = msdos_mem_alloc(first_mcb, ENV_SIZE >> 4, 1)) == -1) {
		if((env_seg = msdos_mem_alloc(UMB_TOP >> 4, ENV_SIZE >> 4, 1)) == -1) {
			if(umb_linked != 0) {
				msdos_mem_link_umb();
			}
			return(-1);
		}
	}
	if(param->env_seg == 0) {
		psp_t *parent_psp = (psp_t *)(mem + (current_psp << 4));
		memcpy(mem + (env_seg << 4), mem + (parent_psp->env_seg << 4), ENV_SIZE);
	} else {
		memcpy(mem + (env_seg << 4), mem + (param->env_seg << 4), ENV_SIZE);
	}
	msdos_env_set_argv(env_seg, msdos_short_full_path(path));
	
	// check exe header
	exe_header_t *header = (exe_header_t *)file_buffer;
	int paragraphs, free_paragraphs = msdos_mem_get_free(first_mcb, 1);
	UINT16 cs, ss, ip, sp;
	
	if(header->mz == 0x4d5a || header->mz == 0x5a4d) {
		// memory allocation
		int header_size = header->header_size * 16;
		int load_size = header->pages * 512 - header_size;
		if(header_size + load_size < 512) {
			load_size = 512 - header_size;
		}
		paragraphs = (PSP_SIZE + load_size) >> 4;
		if(paragraphs + header->min_alloc > free_paragraphs) {
			msdos_mem_free(env_seg);
			return(-1);
		}
		paragraphs += header->max_alloc ? header->max_alloc : header->min_alloc;
		if(paragraphs > free_paragraphs) {
			paragraphs = free_paragraphs;
		}
		if((psp_seg = msdos_mem_alloc(first_mcb, paragraphs, 1)) == -1) {
			if((psp_seg = msdos_mem_alloc(UMB_TOP >> 4, paragraphs, 1)) == -1) {
				if(umb_linked != 0) {
					msdos_mem_link_umb();
				}
				msdos_mem_free(env_seg);
				return(-1);
			}
		}
		// relocation
		int start_seg = psp_seg + (PSP_SIZE >> 4);
		for(int i = 0; i < header->relocations; i++) {
			int ofs = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 0);
			int seg = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 2);
			*(UINT16 *)(file_buffer + header_size + (seg << 4) + ofs) += start_seg;
		}
		memcpy(mem + (start_seg << 4), file_buffer + header_size, load_size);
		// segments
		cs = header->init_cs + start_seg;
		ss = header->init_ss + start_seg;
		ip = header->init_ip;
		sp = header->init_sp - 2; // for symdeb
	} else {
		// memory allocation
		paragraphs = free_paragraphs;
		if((psp_seg = msdos_mem_alloc(first_mcb, paragraphs, 1)) == -1) {
			if((psp_seg = msdos_mem_alloc(UMB_TOP >> 4, paragraphs, 1)) == -1) {
				if(umb_linked != 0) {
					msdos_mem_link_umb();
				}
				msdos_mem_free(env_seg);
				return(-1);
			}
		}
		int start_seg = psp_seg + (PSP_SIZE >> 4);
		memcpy(mem + (start_seg << 4), file_buffer, 0x10000 - PSP_SIZE);
		// segments
		cs = ss = psp_seg;
		ip = 0x100;
		sp = 0xfffe;
	}
	if(umb_linked != 0) {
		msdos_mem_link_umb();
	}
	
	// create psp
#if defined(HAS_I386)
	*(UINT16 *)(mem + 4 * 0x22 + 0) = m_eip;
#else
	*(UINT16 *)(mem + 4 * 0x22 + 0) = m_pc - SREG_BASE(CS);
#endif
	*(UINT16 *)(mem + 4 * 0x22 + 2) = SREG(CS);
	psp_t *psp = msdos_psp_create(psp_seg, psp_seg + paragraphs, current_psp, env_seg);
	memcpy(psp->fcb1, mem + (param->fcb1.w.h << 4) + param->fcb1.w.l, sizeof(psp->fcb1));
	memcpy(psp->fcb2, mem + (param->fcb2.w.h << 4) + param->fcb2.w.l, sizeof(psp->fcb2));
	memcpy(psp->buffer, mem + (param->cmd_line.w.h << 4) + param->cmd_line.w.l, sizeof(psp->buffer));
	
	mcb_t *mcb_env = (mcb_t *)(mem + ((env_seg - 1) << 4));
	mcb_t *mcb_psp = (mcb_t *)(mem + ((psp_seg - 1) << 4));
	mcb_psp->psp = mcb_env->psp = psp_seg;
	
	for(int i = 0; i < 8; i++) {
		if(name_tmp[i] == '.') {
			mcb_psp->prog_name[i] = '\0';
			break;
		} else if(i < 7 && msdos_lead_byte_check(name_tmp[i])) {
			mcb_psp->prog_name[i] = name_tmp[i];
			i++;
			mcb_psp->prog_name[i] = name_tmp[i];
		} else if(name_tmp[i] >= 'a' && name_tmp[i] <= 'z') {
			mcb_psp->prog_name[i] = name_tmp[i] - 'a' + 'A';
		} else {
			mcb_psp->prog_name[i] = name_tmp[i];
		}
	}
	
	// process info
	process_t *process = msdos_process_info_create(psp_seg);
	strcpy(process->module_dir, msdos_short_full_dir(path));
	process->dta.w.l = 0x80;
	process->dta.w.h = psp_seg;
	process->switchar = '/';
	process->max_files = 20;
	process->parent_int_10h_feh_called = int_10h_feh_called;
	process->parent_int_10h_ffh_called = int_10h_ffh_called;
	process->parent_ds = SREG(DS);
	process->parent_es = SREG(ES);
	
	current_psp = psp_seg;
	msdos_sda_update(current_psp);
	
	if(al == 0x00) {
		int_10h_feh_called = int_10h_ffh_called = false;
		
		// registers and segments
		REG16(AX) = REG16(BX) = 0x00;
		REG16(CX) = 0xff;
		REG16(DX) = psp_seg;
		REG16(SI) = ip;
		REG16(DI) = sp;
		REG16(SP) = sp;
		SREG(DS) = SREG(ES) = psp_seg;
		SREG(SS) = ss;
		i386_load_segment_descriptor(DS);
		i386_load_segment_descriptor(ES);
		i386_load_segment_descriptor(SS);
		
		*(UINT16 *)(mem + (ss << 4) + sp) = 0;
		i386_jmp_far(cs, ip);
	} else if(al == 0x01) {
		// copy ss:sp and cs:ip to param block
		param->sp = sp;
		param->ss = ss;
		param->ip = ip;
		param->cs = cs;
		
		// the AX value to be passed to the child program is put on top of the child's stack
		*(UINT16 *)(mem + (ss << 4) + sp) = REG16(AX);
	}
	return(0);
}

void msdos_process_terminate(int psp_seg, int ret, int mem_free)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	*(UINT32 *)(mem + 4 * 0x22) = psp->int_22h.dw;
	*(UINT32 *)(mem + 4 * 0x23) = psp->int_23h.dw;
	*(UINT32 *)(mem + 4 * 0x24) = psp->int_24h.dw;
	
	SREG(SS) = psp->stack.w.h;
	i386_load_segment_descriptor(SS);
	REG16(SP) = psp->stack.w.l;
	i386_jmp_far(psp->int_22h.w.h, psp->int_22h.w.l);
	
//	process_t *current_process = msdos_process_info_get(psp_seg);
	process_t *current_process = NULL;
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			current_process = &process[i];
			break;
		}
	}
	if(current_process == NULL) {
		throw(0x1f); // general failure
	}
	int_10h_feh_called = current_process->parent_int_10h_feh_called;
	int_10h_ffh_called = current_process->parent_int_10h_ffh_called;
	if(current_process->called_by_int2eh) {
		REG16(AX) = ret;
	}
	SREG(DS) = current_process->parent_ds;
	SREG(ES) = current_process->parent_es;
	i386_load_segment_descriptor(DS);
	i386_load_segment_descriptor(ES);
	
	if(mem_free) {
		int mcb_seg;
		while((mcb_seg = msdos_mem_get_last_mcb(first_mcb, psp_seg)) != -1) {
			msdos_mem_free(mcb_seg + 1);
		}
		while((mcb_seg = msdos_mem_get_last_mcb(UMB_TOP >> 4, psp_seg)) != -1) {
			msdos_mem_free(mcb_seg + 1);
		}
		
		for(int i = 0; i < MAX_FILES; i++) {
			if(file_handler[i].valid && file_handler[i].psp == psp_seg) {
				_close(i);
				msdos_file_handler_close(i);
				msdos_psp_set_file_table(i, 0x0ff, psp_seg); // FIXME: consider duplicated file handles
			}
		}
		msdos_dta_info_free(psp_seg);
	}
	msdos_stdio_reopen();
	
	memset(current_process, 0, sizeof(process_t));
	
	current_psp = psp->parent_psp;
	retval = ret;
	msdos_sda_update(current_psp);
}

// drive

int msdos_drive_param_block_update(int drive_num, UINT16 *seg, UINT16 *ofs, int force_update)
{
	*seg = DPB_TOP >> 4;
	*ofs = sizeof(dpb_t) * drive_num;
	dpb_t *dpb = (dpb_t *)(mem + (*seg << 4) + *ofs);
	
	if(!force_update && dpb->free_clusters != 0) {
		return(dpb->bytes_per_sector ? 1 : 0);
	}
	memset(dpb, 0, sizeof(dpb_t));
	
	int res = 0;
	char dev[64];
	sprintf(dev, "\\\\.\\%c:", 'A' + drive_num);
	
	HANDLE hFile = CreateFile(dev, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile != INVALID_HANDLE_VALUE) {
		DISK_GEOMETRY geo;
		DWORD dwSize;
		if(DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo, sizeof(geo), &dwSize, NULL)) {
			dpb->bytes_per_sector = (UINT16)geo.BytesPerSector;
			dpb->highest_sector_num = (UINT8)(geo.SectorsPerTrack - 1);
			dpb->highest_cluster_num = (UINT16)(geo.TracksPerCylinder * geo.Cylinders.QuadPart + 1);
			dpb->maximum_cluster_num = (UINT32)(geo.TracksPerCylinder * geo.Cylinders.QuadPart + 1);
			switch(geo.MediaType) {
			case F5_320_512:	// floppy, double-sided, 8 sectors per track (320K)
				dpb->media_type = 0xff;
				break;
			case F5_160_512:	// floppy, single-sided, 8 sectors per track (160K)
				dpb->media_type = 0xfe;
				break;
			case F5_360_512:	// floppy, double-sided, 9 sectors per track (360K)
				dpb->media_type = 0xfd;
				break;
			case F5_180_512:	// floppy, single-sided, 9 sectors per track (180K)
				dpb->media_type = 0xfc;
				break;
			case F5_1Pt2_512:	// floppy, double-sided, 15 sectors per track (1.2M)
			case F3_720_512:	// floppy, double-sided, 9 sectors per track (720K,3.5")
				dpb->media_type = 0xf9;
				break;
			case FixedMedia:	// hard disk
			case RemovableMedia:
			case Unknown:
				dpb->media_type = 0xf8;
				break;
			default:
				dpb->media_type = 0xf0;
				break;
			}
			res = 1;
		}
		dpb->drive_num = drive_num;
		dpb->unit_num = drive_num;
		dpb->next_dpb_ofs = *ofs + sizeof(dpb_t);
		dpb->next_dpb_seg = *seg;
		dpb->info_sector = 0xffff;
		dpb->backup_boot_sector = 0xffff;
		dpb->free_clusters = 0xffff;
		dpb->free_search_cluster = 0xffffffff;
		CloseHandle(hFile);
	}
	return(res);
}

// pc bios

UINT32 get_ticks_since_midnight(UINT32 cur_msec)
{
	static unsigned __int64 start_msec_since_midnight = 0;
	static unsigned __int64 start_msec_since_hostboot = 0;
	
	if(start_msec_since_midnight == 0) {
		SYSTEMTIME time;
		GetLocalTime(&time);
		start_msec_since_midnight = ((time.wHour * 60 + time.wMinute) * 60 + time.wSecond) * 1000 + time.wMilliseconds;
		start_msec_since_hostboot = cur_msec;
	}
	unsigned __int64 msec = (start_msec_since_midnight + cur_msec - start_msec_since_hostboot) % (24 * 60 * 60 * 1000);
	unsigned __int64 tick = msec * 0x1800b0 / (24 * 60 * 60 * 1000);
	return (UINT32)tick;
}

void pcbios_update_daily_timer_counter(UINT32 cur_msec)
{
	UINT32 prev_tick = *(UINT32 *)(mem + 0x46c);
	UINT32 next_tick = get_ticks_since_midnight(cur_msec);
	
	if(prev_tick > next_tick) {
		mem[0x470] = 1;
	}
	*(UINT32 *)(mem + 0x46c) = next_tick;
}

inline void pcbios_irq0()
{
	//++*(UINT32 *)(mem + 0x46c);
	pcbios_update_daily_timer_counter(timeGetTime());
}

int pcbios_get_text_vram_address(int page)
{
	if(/*mem[0x449] == 0x03 || */mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		return TEXT_VRAM_TOP;
	} else {
		return TEXT_VRAM_TOP + VIDEO_REGEN * (page % vram_pages);
	}
}

int pcbios_get_shadow_buffer_address(int page)
{
	if(!int_10h_feh_called) {
		return pcbios_get_text_vram_address(page);
	} else if(/*mem[0x449] == 0x03 || */mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		return SHADOW_BUF_TOP;
	} else {
		return SHADOW_BUF_TOP + VIDEO_REGEN * (page % vram_pages);
	}
}

int pcbios_get_shadow_buffer_address(int page, int x, int y)
{
	return pcbios_get_shadow_buffer_address(page) + (x + y * scr_width) * 2;
}

void pcbios_set_console_size(int width, int height, bool clr_screen)
{
	// clear the existing screen, not just the new one
	int clr_height = max(height, scr_height);
	
	if(scr_width != width || scr_height != height) {
		change_console_size(width, height);
	}
	mem[0x462] = 0;
	*(UINT16 *)(mem + 0x44e) = 0;
	
	text_vram_top_address = pcbios_get_text_vram_address(0);
	text_vram_end_address = text_vram_top_address + width * height * 2;
	shadow_buffer_top_address = pcbios_get_shadow_buffer_address(0);
	shadow_buffer_end_address = shadow_buffer_top_address + width * height * 2;
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if(clr_screen) {
		for(int ofs = shadow_buffer_top_address; ofs < shadow_buffer_end_address;) {
			mem[ofs++] = 0x20;
			mem[ofs++] = 0x07;
		}
		
		EnterCriticalSection(&vram_crit_sect);
		for(int y = 0; y < clr_height; y++) {
			for(int x = 0; x < scr_width; x++) {
				SCR_BUF(y,x).Char.AsciiChar = ' ';
				SCR_BUF(y,x).Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
			}
		}
		SMALL_RECT rect;
		SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + clr_height - 1);
		WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		vram_length_char = vram_last_length_char = 0;
		vram_length_attr = vram_last_length_attr = 0;
		LeaveCriticalSection(&vram_crit_sect);
	}
	COORD co;
	co.X = 0;
	co.Y = scr_top;
	SetConsoleCursorPosition(hStdout, co);
	cursor_moved = true;
	SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

inline void pcbios_int_10h_00h()
{
	switch(REG8(AL) & 0x7f) {
	case 0x70: // v-text mode
	case 0x71: // extended cga v-text mode
		pcbios_set_console_size(scr_width, scr_height, !(REG8(AL) & 0x80));
		break;
	default:
		pcbios_set_console_size(80, 25, !(REG8(AL) & 0x80));
		break;
	}
	if(REG8(AL) & 0x80) {
		mem[0x487] |= 0x80;
	} else {
		mem[0x487] &= ~0x80;
	}
	mem[0x449] = REG8(AL) & 0x7f;
}

inline void pcbios_int_10h_01h()
{
	mem[0x460] = REG8(CL);
	mem[0x461] = REG8(CH);
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO ci;
	GetConsoleCursorInfo(hStdout, &ci);
//	ci.bVisible = !(REG8(CH) & 0x60) && REG8(CH) <= REG8(CL);
//	if(ci.bVisible) {
		int lines = max(8, REG8(CL) + 1);
		ci.dwSize = (REG8(CL) - REG8(CH) + 1) * 100 / lines;
//	}
	SetConsoleCursorInfo(hStdout, &ci);
}

inline void pcbios_int_10h_02h()
{
	// continuously setting the cursor effectively stops it blinking
	if(mem[0x462] == REG8(BH) && (REG8(DL) != mem[0x450 + REG8(BH) * 2] || REG8(DH) != mem[0x451 + REG8(BH) * 2])) {
		COORD co;
		co.X = REG8(DL);
		co.Y = REG8(DH) + scr_top;
		
		// some programs hide the cursor by moving it off screen
		static bool hidden = false;
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_CURSOR_INFO ci;
		GetConsoleCursorInfo(hStdout, &ci);
		
		if(REG8(DH) >= scr_height || !SetConsoleCursorPosition(hStdout, co)) {
			if(ci.bVisible) {
				ci.bVisible = FALSE;
//				SetConsoleCursorInfo(hStdout, &ci);
				hidden = true;
			}
		} else if(hidden) {
			if(!ci.bVisible) {
				ci.bVisible = TRUE;
//				SetConsoleCursorInfo(hStdout, &ci);
			}
			hidden = false;
		}
	}
	mem[0x450 + (REG8(BH) % vram_pages) * 2] = REG8(DL);
	mem[0x451 + (REG8(BH) % vram_pages) * 2] = REG8(DH);
}

inline void pcbios_int_10h_03h()
{
	REG8(DL) = mem[0x450 + (REG8(BH) % vram_pages) * 2];
	REG8(DH) = mem[0x451 + (REG8(BH) % vram_pages) * 2];
	REG8(CL) = mem[0x460];
	REG8(CH) = mem[0x461];
}

inline void pcbios_int_10h_05h()
{
	if(REG8(AL) >= vram_pages) {
		return;
	}
	if(mem[0x462] != REG8(AL)) {
		vram_flush();
		
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		SMALL_RECT rect;
		SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + scr_height - 1);
		ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		
		for(int y = 0, ofs = pcbios_get_shadow_buffer_address(mem[0x462]); y < scr_height; y++) {
			for(int x = 0; x < scr_width; x++) {
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar;
				mem[ofs++] = SCR_BUF(y,x).Attributes;
			}
		}
		for(int y = 0, ofs = pcbios_get_shadow_buffer_address(REG8(AL)); y < scr_height; y++) {
			for(int x = 0; x < scr_width; x++) {
				SCR_BUF(y,x).Char.AsciiChar = mem[ofs++];
				SCR_BUF(y,x).Attributes = mem[ofs++];
			}
		}
		WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		
		COORD co;
		co.X = mem[0x450 + REG8(AL) * 2];
		co.Y = mem[0x451 + REG8(AL) * 2] + scr_top;
		if(co.Y < scr_top + scr_height) {
			SetConsoleCursorPosition(hStdout, co);
		}
	}
	mem[0x462] = REG8(AL);
	*(UINT16 *)(mem + 0x44e) = REG8(AL) * VIDEO_REGEN;
	int regen = min(scr_width * scr_height * 2, 0x8000);
	text_vram_top_address = pcbios_get_text_vram_address(mem[0x462]);
	text_vram_end_address = text_vram_top_address + regen;
	shadow_buffer_top_address = pcbios_get_shadow_buffer_address(mem[0x462]);
	shadow_buffer_end_address = shadow_buffer_top_address + regen;
}

inline void pcbios_int_10h_06h()
{
	if(REG8(CH) >= scr_height || REG8(CH) > REG8(DH) ||
	   REG8(CL) >= scr_width  || REG8(CL) > REG8(DL)) {
		return;
	}
	vram_flush();
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	SMALL_RECT rect;
	SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + scr_height - 1);
	ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
	
	int right = min(REG8(DL), scr_width - 1);
	int bottom = min(REG8(DH), scr_height - 1);
	
	if(REG8(AL) == 0) {
		for(int y = REG8(CH); y <= bottom; y++) {
			for(int x = REG8(CL), ofs = pcbios_get_shadow_buffer_address(mem[0x462], REG8(CL), y); x <= right; x++) {
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar = ' ';
				mem[ofs++] = SCR_BUF(y,x).Attributes = REG8(BH);
			}
		}
	} else {
		for(int y = REG8(CH), y2 = min(REG8(CH) + REG8(AL), bottom + 1); y <= bottom; y++, y2++) {
			for(int x = REG8(CL), ofs = pcbios_get_shadow_buffer_address(mem[0x462], REG8(CL), y); x <= right; x++) {
				if(y2 <= bottom) {
					SCR_BUF(y,x) = SCR_BUF(y2,x);
				} else {
					SCR_BUF(y,x).Char.AsciiChar = ' ';
					SCR_BUF(y,x).Attributes = REG8(BH);
				}
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar;
				mem[ofs++] = SCR_BUF(y,x).Attributes;
			}
		}
	}
	WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
}

inline void pcbios_int_10h_07h()
{
	if(REG8(CH) >= scr_height || REG8(CH) > REG8(DH) ||
	   REG8(CL) >= scr_width  || REG8(CL) > REG8(DL)) {
		return;
	}
	vram_flush();
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	SMALL_RECT rect;
	SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + scr_height - 1);
	ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
	
	int right = min(REG8(DL), scr_width - 1);
	int bottom = min(REG8(DH), scr_height - 1);
	
	if(REG8(AL) == 0) {
		for(int y = REG8(CH); y <= bottom; y++) {
			for(int x = REG8(CL), ofs = pcbios_get_shadow_buffer_address(mem[0x462], REG8(CL), y); x <= right; x++) {
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar = ' ';
				mem[ofs++] = SCR_BUF(y,x).Attributes = REG8(BH);
			}
		}
	} else {
		for(int y = bottom, y2 = max(REG8(CH) - 1, bottom - REG8(AL)); y >= REG8(CH); y--, y2--) {
			for(int x = REG8(CL), ofs = pcbios_get_shadow_buffer_address(mem[0x462], REG8(CL), y); x <= right; x++) {
				if(y2 >= REG8(CH)) {
					SCR_BUF(y,x) = SCR_BUF(y2,x);
				} else {
					SCR_BUF(y,x).Char.AsciiChar = ' ';
					SCR_BUF(y,x).Attributes = REG8(BH);
				}
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar;
				mem[ofs++] = SCR_BUF(y,x).Attributes;
			}
		}
	}
	WriteConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
}

inline void pcbios_int_10h_08h()
{
	COORD co;
	DWORD num;
	
	co.X = mem[0x450 + (REG8(BH) % vram_pages) * 2];
	co.Y = mem[0x451 + (REG8(BH) % vram_pages) * 2];
	
	if(mem[0x462] == REG8(BH)) {
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		co.Y += scr_top;
		vram_flush();
		ReadConsoleOutputCharacter(hStdout, scr_char, 1, co, &num);
		ReadConsoleOutputAttribute(hStdout, scr_attr, 1, co, &num);
		REG8(AL) = scr_char[0];
		REG8(AH) = scr_attr[0];
	} else {
		REG16(AX) = *(UINT16 *)(mem + pcbios_get_shadow_buffer_address(REG8(BH), co.X, co.Y));
	}
}

inline void pcbios_int_10h_09h()
{
	COORD co;
	
	co.X = mem[0x450 + (REG8(BH) % vram_pages) * 2];
	co.Y = mem[0x451 + (REG8(BH) % vram_pages) * 2];
	
	int dest = pcbios_get_shadow_buffer_address(REG8(BH), co.X, co.Y);
	int end = min(dest + REG16(CX) * 2, pcbios_get_shadow_buffer_address(REG8(BH), 0, scr_height));
	
	if(mem[0x462] == REG8(BH)) {
		EnterCriticalSection(&vram_crit_sect);
		int vram = pcbios_get_shadow_buffer_address(REG8(BH));
		while(dest < end) {
			write_text_vram_char(dest - vram, REG8(AL));
			mem[dest++] = REG8(AL);
			write_text_vram_attr(dest - vram, REG8(BL));
			mem[dest++] = REG8(BL);
		}
		LeaveCriticalSection(&vram_crit_sect);
	} else {
		while(dest < end) {
			mem[dest++] = REG8(AL);
			mem[dest++] = REG8(BL);
		}
	}
}

inline void pcbios_int_10h_0ah()
{
	COORD co;
	
	co.X = mem[0x450 + (REG8(BH) % vram_pages) * 2];
	co.Y = mem[0x451 + (REG8(BH) % vram_pages) * 2];
	
	int dest = pcbios_get_shadow_buffer_address(REG8(BH), co.X, co.Y);
	int end = min(dest + REG16(CX) * 2, pcbios_get_shadow_buffer_address(REG8(BH), 0, scr_height));
	
	if(mem[0x462] == REG8(BH)) {
		EnterCriticalSection(&vram_crit_sect);
		int vram = pcbios_get_shadow_buffer_address(REG8(BH));
		while(dest < end) {
			write_text_vram_char(dest - vram, REG8(AL));
			mem[dest++] = REG8(AL);
			dest++;
		}
		LeaveCriticalSection(&vram_crit_sect);
	} else {
		while(dest < end) {
			mem[dest++] = REG8(AL);
			dest++;
		}
	}
}

inline void pcbios_int_10h_0eh()
{
	DWORD num;
	COORD co;
	
	co.X = mem[0x450 + (REG8(BH) % vram_pages) * 2];
	co.Y = mem[0x451 + (REG8(BH) % vram_pages) * 2];
	
	if(REG8(AL) == 7) {
		//MessageBeep(-1);
	} else if(REG8(AL) == 8 || REG8(AL) == 10 || REG8(AL) == 13) {
		if(REG8(AL) == 10) {
			vram_flush();
		}
		WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), &REG8(AL), 1, &num, NULL);
		cursor_moved = true;
	} else {
		int dest = pcbios_get_shadow_buffer_address(REG8(BH), co.X, co.Y);
		if(mem[0x462] == REG8(BH)) {
			EnterCriticalSection(&vram_crit_sect);
			int vram = pcbios_get_shadow_buffer_address(REG8(BH));
			write_text_vram_char(dest - vram, REG8(AL));
			LeaveCriticalSection(&vram_crit_sect);
			
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			if(++co.X == scr_width) {
				co.X = 0;
				if(++co.Y == scr_height) {
					vram_flush();
					WriteConsole(hStdout, "\n", 1, &num, NULL);
					cursor_moved = true;
				}
			}
			if(!cursor_moved) {
				co.Y += scr_top;
				SetConsoleCursorPosition(hStdout, co);
				cursor_moved = true;
			}
		}
		mem[dest] = REG8(AL);
	}
}

inline void pcbios_int_10h_0fh()
{
	REG8(AL) = mem[0x449];
	REG8(AH) = mem[0x44a];
	REG8(BH) = mem[0x462];
}

inline void pcbios_int_10h_11h()
{
	switch(REG8(AL)) {
	case 0x01:
	case 0x11:
		pcbios_set_console_size(80, 28, true);
		break;
	case 0x02:
	case 0x12:
		pcbios_set_console_size(80, 50, true);
		break;
	case 0x04:
	case 0x14:
		pcbios_set_console_size(80, 25, true);
		break;
	case 0x18:
		pcbios_set_console_size(80, 50, true);
		break;
	case 0x30:
		SREG(ES) = 0;
		i386_load_segment_descriptor(ES);
		REG16(BP) = 0;
		REG16(CX) = mem[0x485];
		REG8(DL) = mem[0x484];
		break;
	}
}

inline void pcbios_int_10h_12h()
{
	switch(REG8(BL)) {
	case 0x10:
		REG16(BX) = 0x0003;
		REG16(CX) = 0x0009;
		break;
	}
}

inline void pcbios_int_10h_13h()
{
	int ofs = SREG_BASE(ES) + REG16(BP);
	COORD co;
	DWORD num;
	
	co.X = REG8(DL);
	co.Y = REG8(DH) + scr_top;
	
	vram_flush();
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		if(mem[0x462] == REG8(BH)) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			SetConsoleCursorPosition(hStdout, co);
			
			if(csbi.wAttributes != REG8(BL)) {
				SetConsoleTextAttribute(hStdout, REG8(BL));
			}
			WriteConsole(hStdout, &mem[ofs], REG16(CX), &num, NULL);
			
			if(csbi.wAttributes != REG8(BL)) {
				SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			}
			if(REG8(AL) == 0x00) {
				if(!restore_console_on_exit) {
					GetConsoleScreenBufferInfo(hStdout, &csbi);
					scr_top = csbi.srWindow.Top;
				}
				co.X = mem[0x450 + REG8(BH) * 2];
				co.Y = mem[0x451 + REG8(BH) * 2] + scr_top;
				SetConsoleCursorPosition(hStdout, co);
			} else {
				cursor_moved = true;
			}
		} else {
			m_CF = 1;
		}
		break;
	case 0x02:
	case 0x03:
		if(mem[0x462] == REG8(BH)) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			SetConsoleCursorPosition(hStdout, co);
			
			WORD wAttributes = csbi.wAttributes;
			for(int i = 0; i < REG16(CX); i++, ofs += 2) {
				if(wAttributes != mem[ofs + 1]) {
					SetConsoleTextAttribute(hStdout, mem[ofs + 1]);
					wAttributes = mem[ofs + 1];
				}
				WriteConsole(hStdout, &mem[ofs], 1, &num, NULL);
			}
			if(csbi.wAttributes != wAttributes) {
				SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			}
			if(REG8(AL) == 0x02) {
				co.X = mem[0x450 + REG8(BH) * 2];
				co.Y = mem[0x451 + REG8(BH) * 2] + scr_top;
				SetConsoleCursorPosition(hStdout, co);
			} else {
				cursor_moved = true;
			}
		} else {
			m_CF = 1;
		}
		break;
	case 0x10:
	case 0x11:
		if(mem[0x462] == REG8(BH)) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			ReadConsoleOutputCharacter(hStdout, scr_char, REG16(CX), co, &num);
			ReadConsoleOutputAttribute(hStdout, scr_attr, REG16(CX), co, &num);
			for(int i = 0; i < num; i++) {
				mem[ofs++] = scr_char[i];
				mem[ofs++] = scr_attr[i];
				if(REG8(AL) == 0x11) {
					mem[ofs++] = 0;
					mem[ofs++] = 0;
				}
			}
		} else {
			for(int i = 0, src = pcbios_get_shadow_buffer_address(REG8(BH), co.X, co.Y - scr_top); i < REG16(CX); i++) {
				mem[ofs++] = mem[src++];
				mem[ofs++] = mem[src++];
				if(REG8(AL) == 0x11) {
					mem[ofs++] = 0;
					mem[ofs++] = 0;
				}
				if(++co.X == scr_width) {
					if(++co.Y == scr_height) {
						break;
					}
					co.X = 0;
				}
			}
		}
		break;
	case 0x20:
	case 0x21:
		if(mem[0x462] == REG8(BH)) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			int len = min(REG16(CX), scr_width * scr_height);
			for(int i = 0; i < len; i++) {
				scr_char[i] = mem[ofs++];
				scr_attr[i] = mem[ofs++];
				if(REG8(AL) == 0x21) {
					ofs += 2;
				}
			}
			WriteConsoleOutputCharacter(hStdout, scr_char, len, co, &num);
			WriteConsoleOutputAttribute(hStdout, scr_attr, len, co, &num);
		} else {
			for(int i = 0, dest = pcbios_get_shadow_buffer_address(REG8(BH), co.X, co.Y - scr_top); i < REG16(CX); i++) {
				mem[dest++] = mem[ofs++];
				mem[dest++] = mem[ofs++];
				if(REG8(AL) == 0x21) {
					ofs += 2;
				}
				if(++co.X == scr_width) {
					if(++co.Y == scr_height) {
						break;
					}
					co.X = 0;
				}
			}
		}
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_18h()
{
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
//		REG8(AL) = 0x86;
		REG8(AL) = 0x00;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_1ah()
{
	switch(REG8(AL)) {
	case 0x00:
		REG8(AL) = 0x1a;
		REG8(BL) = 0x08;
		REG8(BH) = 0x00;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_1dh()
{
	switch(REG8(AL)) {
	case 0x01:
		break;
	case 0x02:
		REG16(BX) = 0;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_4fh()
{
	switch(REG8(AL)) {
	case 0x00:
		REG8(AH) = 0x02; // not supported
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
		REG8(AH) = 0x01; // failed
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_82h()
{
	static UINT8 mode = 0;
	
	switch(REG8(AL)) {
	case 0x00:
		if(REG8(BL) != 0xff) {
			mode = REG8(BL);
		}
		REG8(AL) = mode;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_83h()
{
	static UINT8 mode = 0;
	
	switch(REG8(AL)) {
	case 0x00:
		REG16(AX) = 0; // offset???
		SREG(ES) = (SHADOW_BUF_TOP >> 4);
		i386_load_segment_descriptor(ES);
		REG16(BX) = (SHADOW_BUF_TOP & 0x0f);
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_10h_90h()
{
	REG8(AL) = mem[0x449];
}

inline void pcbios_int_10h_91h()
{
	REG8(AL) = 0x04; // VGA
}

inline void pcbios_int_10h_efh()
{
	REG16(DX) = 0xffff;
}

inline void pcbios_int_10h_feh()
{
	if(mem[0x449] == 0x03 || mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		SREG(ES) = (SHADOW_BUF_TOP >> 4);
		i386_load_segment_descriptor(ES);
		REG16(DI) = (SHADOW_BUF_TOP & 0x0f);
	}
	int_10h_feh_called = true;
}

inline void pcbios_int_10h_ffh()
{
	if(mem[0x449] == 0x03 || mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD co;
		DWORD num;
		
		vram_flush();
		
		co.X = (REG16(DI) >> 1) % scr_width;
		co.Y = (REG16(DI) >> 1) / scr_width;
		int ofs = pcbios_get_shadow_buffer_address(0, co.X, co.Y);
		int end = min(ofs + REG16(CX) * 2, pcbios_get_shadow_buffer_address(0, 0, scr_height));
		int len;
		for(len = 0; ofs < end; len++) {
			scr_char[len] = mem[ofs++];
			scr_attr[len] = mem[ofs++];
		}
		co.Y += scr_top;
		WriteConsoleOutputCharacter(hStdout, scr_char, len, co, &num);
		WriteConsoleOutputAttribute(hStdout, scr_attr, len, co, &num);
	}
	int_10h_ffh_called = true;
}

inline void pcbios_int_14h_00h()
{
	if(REG16(DX) < 4) {
		static const unsigned int rate[] = {110, 150, 300, 600, 1200, 2400, 4800, 9600};
		UINT8 selector = sio_read(REG16(DX), 3);
		selector &= ~0x3f;
		selector |= REG8(AL) & 0x1f;
		UINT16 divisor = 115200 / rate[REG8(AL) >> 5];
		sio_write(REG16(DX), 3, selector | 0x80);
		sio_write(REG16(DX), 0, divisor & 0xff);
		sio_write(REG16(DX), 1, divisor >> 8);
		sio_write(REG16(DX), 3, selector);
		REG8(AH) = sio_read(REG16(DX), 5);
		REG8(AL) = sio_read(REG16(DX), 6);
	} else {
		REG8(AH) = 0x80;
	}
}

inline void pcbios_int_14h_01h()
{
	if(REG16(DX) < 4) {
		UINT8 selector = sio_read(REG16(DX), 3);
		sio_write(REG16(DX), 3, selector & ~0x80);
		sio_write(REG16(DX), 0, REG8(AL));
		sio_write(REG16(DX), 3, selector);
		REG8(AH) = sio_read(REG16(DX), 5);
	} else {
		REG8(AH) = 0x80;
	}
}

inline void pcbios_int_14h_02h()
{
	if(REG16(DX) < 4) {
		UINT8 selector = sio_read(REG16(DX), 3);
		sio_write(REG16(DX), 3, selector & ~0x80);
		REG8(AL) = sio_read(REG16(DX), 0);
		sio_write(REG16(DX), 3, selector);
		REG8(AH) = sio_read(REG16(DX), 5);
	} else {
		REG8(AH) = 0x80;
	}
}

inline void pcbios_int_14h_03h()
{
	if(REG16(DX) < 4) {
		REG8(AH) = sio_read(REG16(DX), 5);
		REG8(AL) = sio_read(REG16(DX), 6);
	} else {
		REG8(AH) = 0x80;
	}
}

inline void pcbios_int_14h_04h()
{
	if(REG16(DX) < 4) {
		UINT8 selector = sio_read(REG16(DX), 3);
		if(REG8(CH) <= 0x03) {
			selector = (selector & ~0x03) | REG8(CH);
		}
		if(REG8(BL) == 0x00) {
			selector &= ~0x04;
		} else if(REG8(BL) == 0x01) {
			selector |= 0x04;
		}
		if(REG8(BH) == 0x00) {
			selector = (selector & ~0x38) | 0x00;
		} else if(REG8(BH) == 0x01) {
			selector = (selector & ~0x38) | 0x08;
		} else if(REG8(BH) == 0x02) {
			selector = (selector & ~0x38) | 0x18;
		} else if(REG8(BH) == 0x03) {
			selector = (selector & ~0x38) | 0x28;
		} else if(REG8(BH) == 0x04) {
			selector = (selector & ~0x38) | 0x38;
		}
		if(REG8(AL) == 0x00) {
			selector |= 0x40;
		} else if(REG8(AL) == 0x01) {
			selector &= ~0x40;
		}
		if(REG8(CL) <= 0x0b) {
			static const unsigned int rate[] = {110, 150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
			UINT16 divisor = 115200 / rate[REG8(CL)];
			sio_write(REG16(DX), 3, selector | 0x80);
			sio_write(REG16(DX), 0, divisor & 0xff);
			sio_write(REG16(DX), 1, divisor >> 8);
		}
		sio_write(REG16(DX), 3, selector);
		REG8(AH) = sio_read(REG16(DX), 5);
		REG8(AL) = sio_read(REG16(DX), 6);
	} else {
		REG8(AH) = 0x80;
	}
}

inline void pcbios_int_14h_05h()
{
	if(REG16(DX) < 4) {
		if(REG8(AL) == 0x00) {
			REG8(BL) = sio_read(REG16(DX), 4);
			REG8(AH) = sio_read(REG16(DX), 5);
			REG8(AL) = sio_read(REG16(DX), 6);
		} else if(REG8(AL) == 0x01) {
			sio_write(REG16(DX), 4, REG8(BL));
			REG8(AH) = sio_read(REG16(DX), 5);
			REG8(AL) = sio_read(REG16(DX), 6);
		} else {
			unimplemented_14h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x14, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		}
	} else {
		REG8(AH) = 0x80;
	}
}

inline void pcbios_int_15h_10h()
{
	switch(REG8(AL)) {
	case 0x00:
		Sleep(10);
		hardware_update();
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
	}
}

inline void pcbios_int_15h_23h()
{
	switch(REG8(AL)) {
	case 0x00:
		REG8(CL) = cmos_read(0x2d);
		REG8(CH) = cmos_read(0x2e);
		break;
	case 0x01:
		cmos_write(0x2d, REG8(CL));
		cmos_write(0x2e, REG8(CH));
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_15h_24h()
{
	switch(REG8(AL)) {
	case 0x00:
		i386_set_a20_line(0);
		REG8(AH) = 0;
		break;
	case 0x01:
		i386_set_a20_line(1);
		REG8(AH) = 0;
		break;
	case 0x02:
		REG8(AH) = 0;
		REG8(AL) = (m_a20_mask >> 20) & 1;
		REG16(CX) = 0;
		break;
	case 0x03:
		REG16(AX) = 0;
		REG16(BX) = 0;
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_15h_49h()
{
	REG8(AH) = 0x00;
	REG8(BL) = 0x00; // DOS/V
}

inline void pcbios_int_15h_50h()
{
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		if(REG8(BH) != 0x00 && REG8(BH) != 0x01) {
			REG8(AH) = 0x01; // invalid font type in bh
			m_CF = 1;
		} else if(REG8(BL) != 0x00) {
			REG8(AH) = 0x02; // bl not zero
			m_CF = 1;
		} else if(REG16(BP) != 0 && REG16(BP) != 437 && REG16(BP) != 932 && REG16(BP) != 934 && REG16(BP) != 936 && REG16(BP) != 938) {
			REG8(AH) = 0x04; // invalid code page
			m_CF = 1;
		} else if(REG8(AL) == 0x01) {
			REG8(AH) = 0x06; // font is read only
			m_CF = 1;
		} else {
			// dummy font read routine is at fffd:000d
			SREG(ES) = 0xfffd;
			i386_load_segment_descriptor(ES);
			REG16(BX) = 0x0d;
			REG8(AH) = 0x00; // success
		}
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_15h_53h()
{
	switch(REG8(AL)) {
	case 0x00:
		// APM is not installed
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_15h_86h()
{
	UINT32 usec = (REG16(CX) << 16) | REG16(DX);
	UINT32 msec = usec / 1000;
	
	while(msec) {
		UINT32 tmp = min(msec, 100);
		if(msec - tmp < 10) {
			tmp = msec;
		}
		Sleep(tmp);
		
		if(m_halted) {
			return;
		}
		msec -= tmp;
	}
}

inline void pcbios_int_15h_87h()
{
	// copy extended memory (from DOSBox)
	int len = REG16(CX) * 2;
	int ofs = SREG_BASE(ES) + REG16(SI);
	int src = (*(UINT32 *)(mem + ofs + 0x12) & 0xffffff); // + (mem[ofs + 0x16] << 24);
	int dst = (*(UINT32 *)(mem + ofs + 0x1a) & 0xffffff); // + (mem[ofs + 0x1e] << 24);
	memcpy(mem + dst, mem + src, len);
	REG16(AX) = 0x00;
}

inline void pcbios_int_15h_88h()
{
	REG16(AX) = ((min(MAX_MEM, 0x4000000) - 0x100000) >> 10);
}

inline void pcbios_int_15h_89h()
{
#if defined(HAS_I286) || defined(HAS_I386)
	// switch to protected mode (from DOSBox)
	write_io_byte(0x20, 0x10);
	write_io_byte(0x21, REG8(BH));
	write_io_byte(0x21, 0x00);
	write_io_byte(0xa0, 0x10);
	write_io_byte(0xa1, REG8(BL));
	write_io_byte(0xa1, 0x00);
	i386_set_a20_line(1);
	int ofs = SREG_BASE(ES) + REG16(SI);
	m_gdtr.limit = *(UINT16 *)(mem + ofs + 0x08);
	m_gdtr.base = *(UINT32 *)(mem + ofs + 0x08 + 0x02) & 0xffffff;
	m_idtr.limit = *(UINT16 *)(mem + ofs + 0x10);
	m_idtr.base = *(UINT32 *)(mem + ofs + 0x10 + 0x02) & 0xffffff;
#if defined(HAS_I386)
	m_cr[0] |= 1;
#else
	m_msw |= 1;
#endif
	SREG(DS) = 0x18;
	SREG(ES) = 0x20;
	SREG(SS) = 0x28;
	i386_load_segment_descriptor(DS);
	i386_load_segment_descriptor(ES);
	i386_load_segment_descriptor(SS);
	UINT16 offset = *(UINT16 *)(mem + SREG_BASE(SS) + REG16(SP));
	REG16(SP) += 6;
#if defined(HAS_I386)
	UINT32 flags = get_flags();
	flags &= (0x20000 | 0x40000 | 0x80000 | 0x100000 | 0x200000);
	set_flags(flags);
#else
	UINT32 flags = CompressFlags();
	flags &= (0x20000 | 0x40000 | 0x80000 | 0x100000 | 0x200000);
	ExpandFlags(flags);
#endif
	REG16(AX) = 0x00;
	i386_jmp_far(0x30, /*REG16(CX)*/offset);
#else
	// i86/i186/v30: protected mode is not supported
	REG8(AH) = 0x86;
	m_CF = 1;
#endif
}

inline void pcbios_int_15h_8ah()
{
	UINT32 size = MAX_MEM - 0x100000;
	REG16(AX) = size & 0xffff;
	REG16(DX) = size >> 16;
}

#if defined(HAS_I386)
inline void pcbios_int_15h_c9h()
{
	REG8(AH) = 0x00;
	REG8(CH) = cpu_type;
	REG8(CL) = cpu_step;
}
#endif

inline void pcbios_int_15h_cah()
{
	switch(REG8(AL)) {
	case 0x00:
		if(REG8(BL) > 0x3f) {
			REG8(AH) = 0x03;
			m_CF = 1;
		} else if(REG8(BL) < 0x0e) {
			REG8(AH) = 0x04;
			m_CF = 1;
		} else {
			REG8(CL) = cmos_read(REG8(BL));
		}
		break;
	case 0x01:
		if(REG8(BL) > 0x3f) {
			REG8(AH) = 0x03;
			m_CF = 1;
		} else if(REG8(BL) < 0x0e) {
			REG8(AH) = 0x04;
			m_CF = 1;
		} else {
			cmos_write(REG8(BL), REG8(CL));
		}
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_15h_e8h()
{
	switch(REG8(AL)) {
#if defined(HAS_I386)
	case 0x01:
		REG16(AX) = REG16(CX) = ((min(MAX_MEM, 0x1000000) - 0x100000) >> 10);
		REG16(BX) = REG16(DX) = ((max(MAX_MEM, 0x1000000) - 0x1000000) >> 16);
		break;
#endif
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x86;
		m_CF = 1;
		break;
	}
}

UINT32 pcbios_get_key_code(bool clear_buffer)
{
	UINT32 code = 0;
	
	if(key_buf_char->count() == 0) {
		if(!update_key_buffer()) {
			if(clear_buffer) {
				Sleep(10);
			} else {
				maybe_idle();
			}
		}
	}
	if(!clear_buffer) {
		key_buf_char->store_buffer();
		key_buf_scan->store_buffer();
	}
	if(key_buf_char->count() != 0) {
		code = key_buf_char->read() | (key_buf_scan->read() << 8);
	}
	if(key_buf_char->count() != 0) {
		code |= (key_buf_char->read() << 16) | (key_buf_scan->read() << 24);
	}
	if(!clear_buffer) {
		key_buf_char->restore_buffer();
		key_buf_scan->restore_buffer();
	}
	return code;
}

inline void pcbios_int_16h_00h()
{
	while(key_code == 0 && !m_halted) {
		key_code = pcbios_get_key_code(true);
	}
	if((key_code & 0xffff) == 0x0000 || (key_code & 0xffff) == 0xe000) {
		if(REG8(AH) == 0x10) {
			key_code = ((key_code >> 8) & 0xff) | ((key_code >> 16) & 0xff00);
		} else {
			key_code = ((key_code >> 16) & 0xff00);
		}
	}
	REG16(AX) = key_code & 0xffff;
	key_code >>= 16;
}

inline void pcbios_int_16h_01h()
{
	UINT32 key_code_tmp = key_code;
	
	if(key_code_tmp == 0) {
		key_code_tmp = pcbios_get_key_code(false);
	}
	if((key_code_tmp & 0xffff) == 0x0000 || (key_code_tmp & 0xffff) == 0xe000) {
		if(REG8(AH) == 0x11) {
			key_code_tmp = ((key_code_tmp >> 8) & 0xff) | ((key_code_tmp >> 16) & 0xff00);
		} else {
			key_code_tmp = ((key_code_tmp >> 16) & 0xff00);
		}
	}
	if(key_code_tmp != 0) {
		REG16(AX) = key_code_tmp & 0xffff;
	}
#if defined(HAS_I386)
	m_ZF = (key_code_tmp == 0);
#else
	m_ZeroVal = (key_code_tmp != 0);
#endif
}

inline void pcbios_int_16h_02h()
{
	REG8(AL)  = (GetAsyncKeyState(VK_INSERT ) & 0x0001) ? 0x80 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_CAPITAL) & 0x0001) ? 0x40 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_NUMLOCK) & 0x0001) ? 0x20 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_SCROLL ) & 0x0001) ? 0x10 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_MENU   ) & 0x8000) ? 0x08 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 0x04 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_LSHIFT ) & 0x8000) ? 0x02 : 0;
	REG8(AL) |= (GetAsyncKeyState(VK_RSHIFT ) & 0x8000) ? 0x01 : 0;
}

inline void pcbios_int_16h_03h()
{
	static UINT16 status = 0;
	
	switch(REG8(AL)) {
	case 0x05:
		status = REG16(BX);
		break;
	case 0x06:
		REG16(BX) = status;
		break;
	default:
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_16h_05h()
{
	key_buf_char->write(REG8(CL));
	key_buf_scan->write(REG8(CH));
	REG8(AL) = 0x00;
}

inline void pcbios_int_16h_12h()
{
	pcbios_int_16h_02h();
	
	REG8(AH)  = 0;//(GetAsyncKeyState(VK_SYSREQ  ) & 0x8000) ? 0x80 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_CAPITAL ) & 0x8000) ? 0x40 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_NUMLOCK ) & 0x8000) ? 0x20 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_SCROLL  ) & 0x8000) ? 0x10 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_RMENU   ) & 0x8000) ? 0x08 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_RCONTROL) & 0x8000) ? 0x04 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_LMENU   ) & 0x8000) ? 0x02 : 0;
	REG8(AH) |= (GetAsyncKeyState(VK_LCONTROL) & 0x8000) ? 0x01 : 0;
}

inline void pcbios_int_16h_13h()
{
	static UINT16 status = 0;
	
	switch(REG8(AL)) {
	case 0x00:
		status = REG16(DX);
		break;
	case 0x01:
		REG16(DX) = status;
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_16h_14h()
{
	static UINT8 status = 0;
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		status = REG8(AL);
		break;
	case 0x02:
		REG8(AL) = status;
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_16h_55h()
{
	switch(REG8(AL)) {
	case 0x00:
		// keyboard tsr is not present
		break;
	case 0xfe:
		// handlers for INT 08, INT 09, INT 16, INT 1B, and INT 1C are installed
		break;
	case 0xff:
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_16h_6fh()
{
	switch(REG8(AL)) {
	case 0x00:
		// HP Vectra EX-BIOS - "F16_INQUIRE" - Extended BIOS is not installed
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		m_CF = 1;
		break;
	}
}

inline void pcbios_int_1ah_00h()
{
	pcbios_update_daily_timer_counter(timeGetTime());
	REG16(CX) = *(UINT16 *)(mem + 0x46e);
	REG16(DX) = *(UINT16 *)(mem + 0x46c);
	REG8(AL) = mem[0x470];
	mem[0x470] = 0;
}

inline int to_bcd(int t)
{
	int u = (t % 100) / 10;
	return (u << 4) | (t % 10);
}

inline void pcbios_int_1ah_02h()
{
	SYSTEMTIME time;
	
	GetLocalTime(&time);
	REG8(CH) = to_bcd(time.wHour);
	REG8(CL) = to_bcd(time.wMinute);
	REG8(DH) = to_bcd(time.wSecond);
	REG8(DL) = 0x00;
}

inline void pcbios_int_1ah_04h()
{
	SYSTEMTIME time;
	
	GetLocalTime(&time);
	REG8(CH) = to_bcd(time.wYear / 100);
	REG8(CL) = to_bcd(time.wYear);
	REG8(DH) = to_bcd(time.wMonth);
	REG8(DL) = to_bcd(time.wDay);
}

inline void pcbios_int_1ah_0ah()
{
	SYSTEMTIME time;
	FILETIME file_time;
	WORD dos_date, dos_time;
	
	GetLocalTime(&time);
	SystemTimeToFileTime(&time, &file_time);
	FileTimeToDosDateTime(&file_time, &dos_date, &dos_time);
	REG16(CX) = dos_date;
}

// msdos system call

inline void msdos_int_21h_00h()
{
	msdos_process_terminate(SREG(CS), retval, 1);
}

inline void msdos_int_21h_01h()
{
	REG8(AL) = msdos_getche();
	ctrl_c_detected = ctrl_c_pressed;
	
	// some seconds may be passed in console
	hardware_update();
}

inline void msdos_int_21h_02h()
{
	msdos_putch(REG8(DL));
	ctrl_c_detected = ctrl_c_pressed;
}

inline void msdos_int_21h_03h()
{
	REG8(AL) = msdos_aux_in();
}

inline void msdos_int_21h_04h()
{
	msdos_aux_out(REG8(DL));
}

inline void msdos_int_21h_05h()
{
	msdos_prn_out(REG8(DL));
}

inline void msdos_int_21h_06h()
{
	if(REG8(DL) == 0xff) {
		if(msdos_kbhit()) {
			REG8(AL) = msdos_getch();
#if defined(HAS_I386)
			m_ZF = 0;
#else
			m_ZeroVal = 1;
#endif
		} else {
			REG8(AL) = 0;
#if defined(HAS_I386)
			m_ZF = 1;
#else
			m_ZeroVal = 0;
#endif
			maybe_idle();
		}
	} else {
		msdos_putch(REG8(DL));
	}
}

inline void msdos_int_21h_07h()
{
	REG8(AL) = msdos_getch();
	
	// some seconds may be passed in console
	hardware_update();
}

inline void msdos_int_21h_08h()
{
	REG8(AL) = msdos_getch();
	ctrl_c_detected = ctrl_c_pressed;
	
	// some seconds may be passed in console
	hardware_update();
}

inline void msdos_int_21h_09h()
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(1, current_psp);
	
	char *str = (char *)(mem + SREG_BASE(DS) + REG16(DX));
	int len = 0;
	
	while(str[len] != '$' && len < 0x10000) {
		len++;
	}
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdout is redirected to file
		msdos_write(fd, str, len);
	} else {
		for(int i = 0; i < len; i++) {
			msdos_putch(str[i]);
		}
	}
	ctrl_c_detected = ctrl_c_pressed;
}

inline void msdos_int_21h_0ah()
{
	int ofs = SREG_BASE(DS) + REG16(DX);
	int max = mem[ofs] - 1;
	UINT8 *buf = mem + ofs + 2;
	int chr, p = 0;
	
	while((chr = msdos_getch()) != 0x0d) {
		if(ctrl_c_pressed) {
			p = 0;
			msdos_putch(chr);
			break;
		} else if(chr == 0x00) {
			// skip 2nd byte
			msdos_getch();
		} else if(chr == 0x08) {
			// back space
			if(p > 0) {
				p--;
				if(msdos_ctrl_code_check(buf[p])) {
					msdos_putch(chr);
					msdos_putch(chr);
					msdos_putch(' ');
					msdos_putch(' ');
					msdos_putch(chr);
					msdos_putch(chr);
				} else {
					msdos_putch(chr);
					msdos_putch(' ');
					msdos_putch(chr);
				}
			}
		} else if(p < max) {
			buf[p++] = chr;
			msdos_putch(chr);
		}
	}
	buf[p] = 0x0d;
	mem[ofs + 1] = p;
	ctrl_c_detected = ctrl_c_pressed;
	
	// some seconds may be passed in console
	hardware_update();
}

inline void msdos_int_21h_0bh()
{
	if(msdos_kbhit()) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0x00;
		maybe_idle();
	}
	ctrl_c_detected = ctrl_c_pressed;
}

inline void msdos_int_21h_0ch()
{
	// clear key buffer
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(0, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdin is redirected to file
	} else {
		while(msdos_kbhit()) {
			msdos_getch();
		}
	}
	
	switch(REG8(AL)) {
	case 0x01:
		msdos_int_21h_01h();
		break;
	case 0x06:
		msdos_int_21h_06h();
		break;
	case 0x07:
		msdos_int_21h_07h();
		break;
	case 0x08:
		msdos_int_21h_08h();
		break;
	case 0x0a:
		msdos_int_21h_0ah();
		break;
	default:
//		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
//		REG16(AX) = 0x01;
//		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_0dh()
{
}

inline void msdos_int_21h_0eh()
{
	if(REG8(DL) < 26) {
		_chdrive(REG8(DL) + 1);
		msdos_cds_update(REG8(DL));
		msdos_sda_update(current_psp);
	}
	REG8(AL) = 26; // zdrive
}

inline void msdos_int_21h_0fh()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	char *path = msdos_fcb_path(fcb);
	HANDLE hFile = CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0;
		fcb->current_block = 0;
		fcb->record_size = 128;
		fcb->file_size = GetFileSize(hFile, NULL);
		fcb->handle = hFile;
		fcb->cur_record = 0;
	}
}

inline void msdos_int_21h_10h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	REG8(AL) = CloseHandle(fcb->handle) ? 0 : 0xff;
}

inline void msdos_int_21h_11h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(mem + SREG_BASE(DS) + REG16(DX) + (ext_fcb->flag == 0xff ? 7 : 0));
	
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	ext_fcb_t *ext_find = (ext_fcb_t *)(mem + dta_laddr);
	find_fcb_t *find = (find_fcb_t *)(mem + dta_laddr + (ext_fcb->flag == 0xff ? 7 : 0));
	char *path = msdos_fcb_path(fcb);
	WIN32_FIND_DATA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, dta_laddr);
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(dtainfo->find_handle);
		dtainfo->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	dtainfo->allowable_mask = (ext_fcb->flag == 0xff) ? ext_fcb->attribute : 0x20;
	bool label_only = (dtainfo->allowable_mask == 8);
	
	if((dtainfo->allowable_mask & 8) && !msdos_match_volume_label(path, msdos_short_volume_label(process->volume_label))) {
		dtainfo->allowable_mask &= ~8;
	}
	if(!label_only && (dtainfo->find_handle = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
		      !msdos_find_file_has_8dot3name(&fd)) {
			if(!FindNextFile(dtainfo->find_handle, &fd)) {
				FindClose(dtainfo->find_handle);
				dtainfo->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_name(&fd));
		find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = fd.nFileSizeLow;
		REG8(AL) = 0x00;
	} else if(dtainfo->allowable_mask & 8) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = 8;
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_volume_label(process->volume_label));
		find->attribute = 8;
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = 0;
		dtainfo->allowable_mask &= ~8;
		REG8(AL) = 0x00;
	} else {
		REG8(AL) = 0xff;
	}
}

inline void msdos_int_21h_12h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
//	fcb_t *fcb = (fcb_t *)(mem + SREG_BASE(DS) + REG16(DX) + (ext_fcb->flag == 0xff ? 7 : 0));
	
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	ext_fcb_t *ext_find = (ext_fcb_t *)(mem + dta_laddr);
	find_fcb_t *find = (find_fcb_t *)(mem + dta_laddr + (ext_fcb->flag == 0xff ? 7 : 0));
	WIN32_FIND_DATA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, dta_laddr);
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFile(dtainfo->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
			      !msdos_find_file_has_8dot3name(&fd)) {
				if(!FindNextFile(dtainfo->find_handle, &fd)) {
					FindClose(dtainfo->find_handle);
					dtainfo->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(dtainfo->find_handle);
			dtainfo->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_name(&fd));
		find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = fd.nFileSizeLow;
		REG8(AL) = 0x00;
	} else if(dtainfo->allowable_mask & 8) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = 8;
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_volume_label(process->volume_label));
		find->attribute = 8;
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = 0;
		dtainfo->allowable_mask &= ~8;
		REG8(AL) = 0x00;
	} else {
		REG8(AL) = 0xff;
	}
}

inline void msdos_int_21h_13h()
{
	if(remove(msdos_fcb_path((fcb_t *)(mem + SREG_BASE(DS) + REG16(DX))))) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0x00;
	}
}

inline void msdos_int_21h_14h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	DWORD num = 0;
	
	memset(mem + dta_laddr, 0, fcb->record_size);
	if(!ReadFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL) || num == 0) {
		REG8(AL) = 1;
	} else {
		UINT32 position = fcb->current_block * fcb->record_size + fcb->cur_record + num;
		fcb->current_block = (position & 0xffffff) / fcb->record_size;
		fcb->cur_record = (position & 0xffffff) % fcb->record_size;
		REG8(AL) = (num == fcb->record_size) ? 0 : 3;
	}
}

inline void msdos_int_21h_15h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	DWORD num = 0;
	
	if(!WriteFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL) || num == 0) {
		REG8(AL) = 1;
	} else {
		fcb->file_size = GetFileSize(fcb->handle, NULL);
		UINT32 position = fcb->current_block * fcb->record_size + fcb->cur_record + num;
		fcb->current_block = (position & 0xffffff) / fcb->record_size;
		fcb->cur_record = (position & 0xffffff) % fcb->record_size;
		REG8(AL) = (num == fcb->record_size) ? 0 : 1;
	}
}

inline void msdos_int_21h_16h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	char *path = msdos_fcb_path(fcb);
	HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, ext_fcb->flag == 0xff ? ext_fcb->attribute : FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0;
		fcb->current_block = 0;
		fcb->record_size = 128;
		fcb->file_size = 0;
		fcb->handle = hFile;
		fcb->cur_record = 0;
	}
}

inline void msdos_int_21h_17h()
{
	ext_fcb_t *ext_fcb_src = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb_src = (fcb_t *)(ext_fcb_src + (ext_fcb_src->flag == 0xff ? 1 : 0));
	char *path_src = msdos_fcb_path(fcb_src);
	ext_fcb_t *ext_fcb_dst = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX) + 16);
	fcb_t *fcb_dst = (fcb_t *)(ext_fcb_dst + (ext_fcb_dst->flag == 0xff ? 1 : 0));
	char *path_dst = msdos_fcb_path(fcb_dst);
	
	if(rename(path_src, path_dst)) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0;
	}
}

inline void msdos_int_21h_18h()
{
	REG8(AL) = 0x00;
}

inline void msdos_int_21h_19h()
{
	REG8(AL) = _getdrive() - 1;
}

inline void msdos_int_21h_1ah()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	process->dta.w.l = REG16(DX);
	process->dta.w.h = SREG(DS);
	msdos_sda_update(current_psp);
}

inline void msdos_int_21h_1bh()
{
	int drive_num = _getdrive() - 1;
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		REG8(AL) = dpb->highest_sector_num + 1;
		REG16(CX) = dpb->bytes_per_sector;
		REG16(DX) = dpb->highest_cluster_num - 1;
		*(UINT8 *)(mem + SREG_BASE(DS) + REG16(BX)) = dpb->media_type;
	} else {
		REG8(AL) = 0xff;
		m_CF = 1;
	}

}

inline void msdos_int_21h_1ch()
{
	int drive_num = REG8(DL) ? (REG8(DL) - 1) : (_getdrive() - 1);
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		REG8(AL) = dpb->highest_sector_num + 1;
		REG16(CX) = dpb->bytes_per_sector;
		REG16(DX) = dpb->highest_cluster_num - 1;
		*(UINT8 *)(mem + SREG_BASE(DS) + REG16(BX)) = dpb->media_type;
	} else {
		REG8(AL) = 0xff;
		m_CF = 1;
	}

}

inline void msdos_int_21h_1dh()
{
	REG8(AL) = 0;
}

inline void msdos_int_21h_1eh()
{
	REG8(AL) = 0;
}

inline void msdos_int_21h_1fh()
{
	int drive_num = _getdrive() - 1;
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		REG8(AL) = 0;
		SREG(DS) = seg;
		i386_load_segment_descriptor(DS);
		REG16(BX) = ofs;
	} else {
		REG8(AL) = 0xff;
		m_CF = 1;
	}
}

inline void msdos_int_21h_20h()
{
	REG8(AL) = 0;
}

inline void msdos_int_21h_21h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	if(SetFilePointer(fcb->handle, fcb->record_size * (fcb->rand_record & 0xffffff), NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		REG8(AL) = 1;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		memset(mem + dta_laddr, 0, fcb->record_size);
		DWORD num = 0;
		if(!ReadFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL) || num == 0) {
			REG8(AL) = 1;
		} else {
			fcb->current_block = (fcb->rand_record & 0xffffff) / fcb->record_size;
			fcb->cur_record = (fcb->rand_record & 0xffffff) % fcb->record_size;
			REG8(AL) = (num == fcb->record_size) ? 0 : 3;
		}
	}
}

inline void msdos_int_21h_22h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	if(SetFilePointer(fcb->handle, fcb->record_size * (fcb->rand_record & 0xffffff), NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		REG8(AL) = 0xff;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		DWORD num = 0;
		WriteFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL);
		fcb->file_size = GetFileSize(fcb->handle, NULL);
		fcb->current_block = (fcb->rand_record & 0xffffff) / fcb->record_size;
		fcb->cur_record = (fcb->rand_record & 0xffffff) % fcb->record_size;
		REG8(AL) = (num == fcb->record_size) ? 0 : 1;
	}
}

inline void msdos_int_21h_23h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	char *path = msdos_fcb_path(fcb);
	HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		REG8(AL) = 0xff;
	} else {
		UINT32 size = GetFileSize(hFile, NULL);
		fcb->rand_record = size / fcb->record_size + ((size % fcb->record_size) != 0);
		REG8(AL) = 0;
	}
}

inline void msdos_int_21h_24h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	fcb->rand_record = fcb->current_block * fcb->record_size + fcb->cur_record;
}

inline void msdos_int_21h_25h()
{
	*(UINT16 *)(mem + 4 * REG8(AL) + 0) = REG16(DX);
	*(UINT16 *)(mem + 4 * REG8(AL) + 2) = SREG(DS);
}

inline void msdos_int_21h_26h()
{
	psp_t *psp = (psp_t *)(mem + (REG16(DX) << 4));
	
	memcpy(mem + (REG16(DX) << 4), mem + (current_psp << 4), sizeof(psp_t));
	psp->first_mcb = REG16(DX) + 16;
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = 0;
}

inline void msdos_int_21h_27h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	if(SetFilePointer(fcb->handle, fcb->record_size * (fcb->rand_record & 0xffffff), NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		REG8(AL) = 1;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		memset(mem + dta_laddr, 0, fcb->record_size * REG16(CX));
		DWORD num = 0;
		if(!ReadFile(fcb->handle, mem + dta_laddr, fcb->record_size * REG16(CX), &num, NULL) || num == 0) {
			REG8(AL) = 1;
		} else {
			fcb->current_block = (fcb->rand_record & 0xffffff) / fcb->record_size;
			fcb->cur_record = (fcb->rand_record & 0xffffff) % fcb->record_size;
			REG8(AL) = (num == fcb->record_size) ? 0 : 3;
			REG16(CX) = num / fcb->record_size + ((num % fcb->record_size) != 0);
		}
	}
}

inline void msdos_int_21h_28h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + SREG_BASE(DS) + REG16(DX));
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	if(SetFilePointer(fcb->handle, fcb->record_size * (fcb->rand_record & 0xffffff), NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		REG8(AL) = 0xff;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		DWORD num = 0;
		WriteFile(fcb->handle, mem + dta_laddr, fcb->record_size * REG16(CX), &num, NULL);
		fcb->file_size = GetFileSize(fcb->handle, NULL);
		fcb->current_block = (fcb->rand_record & 0xffffff) / fcb->record_size;
		fcb->cur_record = (fcb->rand_record & 0xffffff) % fcb->record_size;
		REG8(AL) = (num == fcb->record_size) ? 0 : 1;
		REG16(CX) = num / fcb->record_size + ((num % fcb->record_size) != 0);
	}
}

inline void msdos_int_21h_29h()
{
	int ofs = 0;//SREG_BASE(DS) + REG16(SI);
	char buffer[1024], name[MAX_PATH], ext[MAX_PATH];
	UINT8 drv = 0;
	char sep_chars[] = ":.;,=+";
	char end_chars[] = "\\<>|/\"[]";
	char spc_chars[] = " \t";
	
	memcpy(buffer, mem + SREG_BASE(DS) + REG16(SI), 1023);
	buffer[1023] = 0;
	memset(name, 0x20, sizeof(name));
	memset(ext, 0x20, sizeof(ext));
	
	if(REG8(AL) & 1) {
		ofs += strspn((char *)(buffer + ofs), spc_chars);
		if(my_strchr(sep_chars, buffer[ofs]) && buffer[ofs] != '\0') {
			ofs++;
		}
	}
	ofs += strspn((char *)(buffer + ofs), spc_chars);
	
	if(buffer[ofs + 1] == ':') {
		if(buffer[ofs] >= 'a' && buffer[ofs] <= 'z') {
			drv = buffer[ofs] - 'a' + 1;
			ofs += 2;
			if(buffer[ofs] == '\\' || buffer[ofs] == '/') {
				ofs++;
			}
		} else if(buffer[ofs] >= 'A' && buffer[ofs] <= 'Z') {
			drv = buffer[ofs] - 'A' + 1;
			ofs += 2;
			if(buffer[ofs] == '\\' || buffer[ofs] == '/') {
				ofs++;
			}
		}
	}
	for(int i = 0, is_kanji = 0; i < MAX_PATH; i++) {
		UINT8 c = buffer[ofs];
		if(is_kanji) {
			is_kanji = 0;
		} else if(msdos_lead_byte_check(c)) {
			is_kanji = 1;
		} else if(c <= 0x20 || my_strchr(end_chars, c) || my_strchr(sep_chars, c)) {
			break;
		} else if(c >= 'a' && c <= 'z') {
			c -= 0x20;
		}
		ofs++;
		name[i] = c;
	}
	if(buffer[ofs] == '.') {
		ofs++;
		for(int i = 0, is_kanji = 0; i < MAX_PATH; i++) {
			UINT8 c = buffer[ofs];
			if(is_kanji) {
				is_kanji = 0;
			} else if(msdos_lead_byte_check(c)) {
				is_kanji = 1;
			} else if(c <= 0x20 || my_strchr(end_chars, c) || my_strchr(sep_chars, c)) {
				break;
			} else if(c >= 'a' && c <= 'z') {
				c -= 0x20;
			}
			ofs++;
			ext[i] = c;
		}
	}
	int si = REG16(SI) + ofs;
	int ds = SREG(DS);
	while(si > 0xffff) {
		si -= 0x10;
		ds++;
	}
	REG16(SI) = si;
	SREG(DS) = ds;
	i386_load_segment_descriptor(DS);
	
	UINT8 *fcb = mem + SREG_BASE(ES) + REG16(DI);
	if(!(REG8(AL) & 2) || drv != 0) {
		fcb[0] = drv;
	}
	if(!(REG8(AL) & 4) || name[0] != 0x20) {
		memcpy(fcb + 1, name, 8);
	}
	if(!(REG8(AL) & 8) || ext[0] != 0x20) {
		memcpy(fcb + 9, ext, 3);
	}
	for(int i = 1, found_star = 0; i < 1 + 8; i++) {
		if(fcb[i] == '*') {
			found_star = 1;
		}
		if(found_star) {
			fcb[i] = '?';
		}
	}
	for(int i = 9, found_star = 0; i < 9 + 3; i++) {
		if(fcb[i] == '*') {
			found_star = 1;
		}
		if(found_star) {
			fcb[i] = '?';
		}
	}
	
	if(drv == 0 || (drv > 0 && drv <= 26 && (GetLogicalDrives() & ( 1 << (drv - 1) )))) {
		if(memchr(fcb + 1, '?', 8 + 3)) {
			REG8(AL) = 0x01;
		} else {
			REG8(AL) = 0x00;
		}
	} else {
		REG8(AL) = 0xff;
	}
}

inline void msdos_int_21h_2ah()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	REG16(CX) = sTime.wYear;
	REG8(DH) = (UINT8)sTime.wMonth;
	REG8(DL) = (UINT8)sTime.wDay;
	REG8(AL) = (UINT8)sTime.wDayOfWeek;
}

inline void msdos_int_21h_2bh()
{
	REG8(AL) = 0xff;
}

inline void msdos_int_21h_2ch()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	REG8(CH) = (UINT8)sTime.wHour;
	REG8(CL) = (UINT8)sTime.wMinute;
	REG8(DH) = (UINT8)sTime.wSecond;
	REG8(DL) = (UINT8)sTime.wMilliseconds / 10;
}

inline void msdos_int_21h_2dh()
{
	REG8(AL) = 0x00;
}

inline void msdos_int_21h_2eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	process->verify = REG8(AL);
}

inline void msdos_int_21h_2fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	REG16(BX) = process->dta.w.l;
	SREG(ES) = process->dta.w.h;
	i386_load_segment_descriptor(ES);
}

inline void msdos_int_21h_30h()
{
	// Version Flag / OEM
	if(REG8(AL) == 0x01) {
#ifdef SUPPORT_HMA
		REG16(BX) = 0x0000;
#else
		REG16(BX) = 0x1000; // DOS is in HMA
#endif
	} else {
		// NOTE: EXDEB invites BL shows the machine type (0=PC-98, 1=PC/AT, 2=FMR),
		// but this is not correct on Windows 98 SE
//		REG16(BX) = 0xff01;	// OEM = Microsoft, PC/AT
		REG16(BX) = 0xff00;	// OEM = Microsoft
	}
	REG16(CX) = 0x0000;
	REG8(AL) = dos_major_version;	// 7
	REG8(AH) = dos_minor_version;	// 10
}

inline void msdos_int_21h_31h()
{
	try {
		msdos_mem_realloc(current_psp, REG16(DX), NULL);
	} catch(...) {
		// recover the broken mcb
		int mcb_seg = current_psp - 1;
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		if(mcb_seg < (MEMORY_END >> 4)) {
			if(((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked & 0x01) {
				mcb->mz = 'M';
			} else {
				mcb->mz = 'Z';
			}
			mcb->paragraphs = (MEMORY_END >> 4) - mcb_seg - 2;
			msdos_mcb_create((MEMORY_END >> 4) - 1, 'M', -1, (UMB_TOP >> 4) - (MEMORY_END >> 4));
		} else {
			mcb->mz = 'Z';
			mcb->paragraphs = (UMB_END >> 4) - mcb_seg - 1;
		}
		msdos_mem_realloc(current_psp, REG16(DX), NULL);
	}
	msdos_process_terminate(current_psp, REG8(AL) | 0x300, 0);
}

inline void msdos_int_21h_32h()
{
	int drive_num = (REG8(DL) == 0) ? (_getdrive() - 1) : (REG8(DL) - 1);
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		REG8(AL) = 0;
		SREG(DS) = seg;
		i386_load_segment_descriptor(DS);
		REG16(BX) = ofs;
	} else {
		REG8(AL) = 0xff;
		m_CF = 1;
	}
}

inline void msdos_int_21h_33h()
{
	char path[MAX_PATH];
	
	switch(REG8(AL)) {
	case 0x00:
		REG8(DL) = ctrl_c_checking;
		break;
	case 0x01:
		ctrl_c_checking = REG8(DL);
		break;
	case 0x05:
		GetSystemDirectory(path, MAX_PATH);
		if(path[0] >= 'a' && path[0] <= 'z') {
			REG8(DL) = path[0] - 'a' + 1;
		} else {
			REG8(DL) = path[0] - 'A' + 1;
		}
		break;
	case 0x06:
		// MS-DOS version (7.10)
		REG8(BL) = 7;
		REG8(BH) = 10;
		REG8(DL) = 0;
#ifdef SUPPORT_HMA
		REG8(DH) = 0x00;
#else
		REG8(DH) = 0x10; // DOS is in HMA
#endif
		break;
	case 0x07:
		if(REG8(DL) == 0) {
			((dos_info_t *)(mem + DOS_INFO_TOP))->dos_flag &= ~0x20;
		} else if(REG8(DL) == 1) {
			((dos_info_t *)(mem + DOS_INFO_TOP))->dos_flag |= 0x20;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_34h()
{
	SREG(ES) = SDA_TOP >> 4;
	i386_load_segment_descriptor(ES);
	REG16(BX) = offsetof(sda_t, indos_flag);;
}

inline void msdos_int_21h_35h()
{
	REG16(BX) = *(UINT16 *)(mem + 4 * REG8(AL) + 0);
	SREG(ES) = *(UINT16 *)(mem + 4 * REG8(AL) + 2);
	i386_load_segment_descriptor(ES);
}

inline void msdos_int_21h_36h()
{
	struct _diskfree_t df = {0};
	
	if(_getdiskfree(REG8(DL), &df) == 0) {
		REG16(AX) = (UINT16)df.sectors_per_cluster;
		REG16(CX) = (UINT16)df.bytes_per_sector;
		REG16(BX) = df.avail_clusters > 0xFFFF ? 0xFFFF : (UINT16)df.avail_clusters;
		REG16(DX) = df.total_clusters > 0xFFFF ? 0xFFFF : (UINT16)df.total_clusters;
	} else {
		REG16(AX) = 0xffff;
	}
}

inline void msdos_int_21h_37h()
{
	static UINT8 dev_flag = 0xff;
	
	switch(REG8(AL)) {
	case 0x00:
		{
			process_t *process = msdos_process_info_get(current_psp);
			REG8(AL) = 0x00;
			REG8(DL) = process->switchar;
		}
		break;
	case 0x01:
		{
			process_t *process = msdos_process_info_get(current_psp);
			REG8(AL) = 0x00;
			process->switchar = REG8(DL);
			msdos_sda_update(current_psp);
		}
		break;
	case 0x02:
		REG8(DL) = dev_flag;
		break;
	case 0x03:
		dev_flag = REG8(DL);
		break;
	case 0xd0:
	case 0xd1:
	case 0xd2:
	case 0xd3:
	case 0xd4:
	case 0xd5:
	case 0xd6:
	case 0xd7:
	case 0xdc:
	case 0xdd:
	case 0xde:
	case 0xdf:
		// diet ???
		REG16(AX) = 1;
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 1;
		break;
	}
}

int get_country_info(country_info_t *ci)
{
	char LCdata[80];
	
	ZeroMemory(ci, offsetof(country_info_t, reserved));
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICURRDIGITS, LCdata, sizeof(LCdata));
	ci->currency_dec_digits = atoi(LCdata);
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICURRENCY, LCdata, sizeof(LCdata));
	ci->currency_format = *LCdata - '0';
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDATE, LCdata, sizeof(LCdata));
	ci->date_format = *LCdata - '0';
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SCURRENCY, LCdata, sizeof(LCdata));
	memcpy(&ci->currency_symbol, LCdata, 4);
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDATE, LCdata, sizeof(LCdata));
	*ci->date_sep = *LCdata;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, LCdata, sizeof(LCdata));
	*ci->dec_sep = *LCdata;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SLIST, LCdata, sizeof(LCdata));
	*ci->list_sep = *LCdata;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, LCdata, sizeof(LCdata));
	*ci->thou_sep = *LCdata;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIME, LCdata, sizeof(LCdata));
	*ci->time_sep = *LCdata;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, LCdata, sizeof(LCdata));
	if(strchr(LCdata, 'H') != NULL) {
		ci->time_format = 1;
	}
	ci->case_map.w.l = 0x000a; // dummy case map routine is at fffd:000a
	ci->case_map.w.h = 0xfffd;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, LCdata, sizeof(LCdata));
	return atoi(LCdata);
}

inline void msdos_int_21h_38h()
{
	switch(REG8(AL)) {
	case 0x00:
		REG16(BX) = get_country_info((country_info_t *)(mem + SREG_BASE(DS) + REG16(DX)));
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 2;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_39h(int lfn)
{
	if(_mkdir(msdos_trimmed_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_3ah(int lfn)
{
	if(_rmdir(msdos_trimmed_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_3bh(int lfn)
{
	if(_chdir(msdos_trimmed_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), lfn))) {
		REG16(AX) = 3;	// must be 3 (path not found)
		m_CF = 1;
	}
}

inline void msdos_int_21h_3ch()
{
	char *path = msdos_local_file_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), 0);
	int attr = GetFileAttributes(path);
	int fd = -1, c;
	UINT16 info;
	
	if(msdos_is_con_path(path)) {
		fd = _open("CON", _O_WRONLY | _O_BINARY);
		info = 0x80d3;
	} else if((c = msdos_is_comm_path(path)) != 0 && sio_port_number[c - 1] != 0) {
		if((fd = _open(msdos_create_comm_path(path, sio_port_number[c - 1]), _O_WRONLY | _O_BINARY)) == -1) {
			fd = _open("NUL", _O_WRONLY | _O_BINARY);
		}
		info = 0x80d3;
	} else if(msdos_is_device_path(path)) {
		fd = _open("NUL", _O_WRONLY | _O_BINARY);
		info = 0x80d3;
	} else {
		fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		info = msdos_drive_number(path);
	}
	if(fd != -1) {
		if(attr == -1) {
			attr = msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY;
		}
		SetFileAttributes(path, attr);
		REG16(AX) = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, info, current_psp);
		msdos_psp_set_file_table(fd, fd, current_psp);
	} else {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_3dh()
{
	char *path = msdos_local_file_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), 0);
	int mode = REG8(AL) & 0x03;
	int fd = -1, c;
	UINT16 info;
	
	if(mode < 0x03) {
		if(msdos_is_con_path(path)) {
			fd = msdos_open("CON", file_mode[mode].mode);
			info = 0x80d3;
		} else if((c = msdos_is_comm_path(path)) != 0 && sio_port_number[c - 1] != 0) {
			if((fd = _open(msdos_create_comm_path(path, sio_port_number[c - 1]), file_mode[mode].mode)) == -1) {
				fd = msdos_open("NUL", file_mode[mode].mode);
			}
			info = 0x80d3;
		} else if(msdos_is_device_path(path)) {
			fd = msdos_open("NUL", file_mode[mode].mode);
			info = 0x80d3;
		} else {
			fd = msdos_open(path, file_mode[mode].mode);
			info = msdos_drive_number(path);
		}
		if(fd != -1) {
			REG16(AX) = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), mode, info, current_psp);
			msdos_psp_set_file_table(fd, fd, current_psp);
		} else {
			REG16(AX) = errno;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x0c;
		m_CF = 1;
	}
}

inline void msdos_int_21h_3eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		_close(fd);
		msdos_file_handler_close(fd);
		msdos_psp_set_file_table(REG16(BX), 0x0ff, current_psp);
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_3fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(file_mode[file_handler[fd].mode].in) {
			if(file_handler[fd].atty) {
				// BX is stdin or is redirected to stdin
				UINT8 *buf = mem + SREG_BASE(DS) + REG16(DX);
				int max = REG16(CX);
				int p = 0;
				
				while(max > p) {
					int chr = msdos_getch();
					
					if(ctrl_c_pressed) {
						p = 0;
						buf[p++] = 0x0d;
						if(max > p) {
							buf[p++] = 0x0a;
						}
						msdos_putch(chr);
						msdos_putch('\n');
						break;
					} else if(chr == 0x00) {
						// skip 2nd byte
						msdos_getch();
					} else if(chr == 0x0d) {
						// carriage return
						buf[p++] = 0x0d;
						if(max > p) {
							buf[p++] = 0x0a;
						}
						msdos_putch('\n');
						break;
					} else if(chr == 0x08) {
						// back space
						if(p > 0) {
							p--;
							if(msdos_ctrl_code_check(buf[p])) {
								msdos_putch(chr);
								msdos_putch(chr);
								msdos_putch(' ');
								msdos_putch(' ');
								msdos_putch(chr);
								msdos_putch(chr);
							} else {
								msdos_putch(chr);
								msdos_putch(' ');
								msdos_putch(chr);
							}
						}
					} else {
						buf[p++] = chr;
						msdos_putch(chr);
					}
				}
				REG16(AX) = p;
				
				// some seconds may be passed in console
				hardware_update();
			} else {
				REG16(AX) = _read(fd, mem + SREG_BASE(DS) + REG16(DX), REG16(CX));
			}
		} else {
			REG16(AX) = 0x05;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_40h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(file_mode[file_handler[fd].mode].out) {
			if(REG16(CX)) {
				if(file_handler[fd].atty) {
					// BX is stdout/stderr or is redirected to stdout
					for(int i = 0; i < REG16(CX); i++) {
						msdos_putch(mem[SREG_BASE(DS) + REG16(DX) + i]);
					}
					REG16(AX) = REG16(CX);
				} else {
					REG16(AX) = msdos_write(fd, mem + SREG_BASE(DS) + REG16(DX), REG16(CX));
				}
			} else {
				UINT32 pos = _tell(fd);
				_lseek(fd, 0, SEEK_END);
				UINT32 size = _tell(fd);
				if(pos < size) {
					_lseek(fd, pos, SEEK_SET);
					SetEndOfFile((HANDLE)_get_osfhandle(fd));
				} else {
					for(UINT32 i = size; i < pos; i++) {
						UINT8 tmp = 0;
						msdos_write(fd, &tmp, 1);
					}
					_lseek(fd, pos, SEEK_SET);
				}
				REG16(AX) = 0;
			}
		} else {
			REG16(AX) = 0x05;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_41h(int lfn)
{
	if(remove(msdos_trimmed_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_42h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(REG8(AL) < 0x03) {
			static int ptrname[] = { SEEK_SET, SEEK_CUR, SEEK_END };
			_lseek(fd, (REG16(CX) << 16) | REG16(DX), ptrname[REG8(AL)]);
			UINT32 pos = _tell(fd);
			REG16(AX) = pos & 0xffff;
			REG16(DX) = (pos >> 16);
		} else {
			REG16(AX) = 0x01;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_43h(int lfn)
{
	char *path = msdos_local_file_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), lfn);
	int attr;
	
	if(!lfn && REG8(AL) > 2) {
		REG16(AX) = 0x01;
		m_CF = 1;
		return;
	}
	switch(REG8(lfn ? BL : AL)) {
	case 0x00:
		if((attr = GetFileAttributes(path)) != -1) {
			REG16(CX) = (UINT16)msdos_file_attribute_create((UINT16)attr);
		} else {
			REG16(AX) = (UINT16)GetLastError();
			m_CF = 1;
		}
		break;
	case 0x01:
		if(!SetFileAttributes(path, msdos_file_attribute_create(REG16(CX)))) {
			REG16(AX) = (UINT16)GetLastError();
			m_CF = 1;
		}
		break;
	case 0x02:
		{
			DWORD size = GetCompressedFileSize(path, NULL);
			if(size != INVALID_FILE_SIZE) {
				if(size != 0 && size == GetFileSize(path, NULL)) {
					DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
					// this isn't correct if the file is in the NTFS MFT
					if(GetDiskFreeSpace(path, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters)) {
						size = ((size - 1) | (sectors_per_cluster * bytes_per_sector - 1)) + 1;
					}
				}
				REG16(AX) = LOWORD(size);
				REG16(DX) = HIWORD(size);
			} else {
				REG16(AX) = (UINT16)GetLastError();
				m_CF = 1;
			}
		}
		break;
	case 0x03:
	case 0x05:
	case 0x07:
		{
			HANDLE hFile = CreateFile(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE) {
				FILETIME local, time;
				DosDateTimeToFileTime(REG16(DI), /*REG8(BL) == 5 ? 0 : */REG16(CX), &local);
				if(REG8(BL) == 7) {
					ULARGE_INTEGER hund;
					hund.LowPart = local.dwLowDateTime;
					hund.HighPart = local.dwHighDateTime;
					hund.QuadPart += REG16(SI) * 100000;
					local.dwLowDateTime = hund.LowPart;
					local.dwHighDateTime = hund.HighPart;
				}
				LocalFileTimeToFileTime(&local, &time);
				if(!SetFileTime(hFile, REG8(BL) == 0x07 ? &time : NULL,
						       REG8(BL) == 0x05 ? &time : NULL,
						       REG8(BL) == 0x03 ? &time : NULL)) {
					REG16(AX) = (UINT16)GetLastError();
					m_CF = 1;
				}
				CloseHandle(hFile);
			} else {
				REG16(AX) = (UINT16)GetLastError();
				m_CF = 1;
			}
		}
		break;
	case 0x04:
	case 0x06:
	case 0x08:
		{
			WIN32_FILE_ATTRIBUTE_DATA fad;
			if(GetFileAttributesEx(path, GetFileExInfoStandard, (LPVOID)&fad)) {
				FILETIME *time, local;
				time = REG8(BL) == 0x04 ? &fad.ftLastWriteTime :
						   0x06 ? &fad.ftLastAccessTime :
							  &fad.ftCreationTime;
				FileTimeToLocalFileTime(time, &local);
				FileTimeToDosDateTime(&local, &REG16(DI), &REG16(CX));
				if(REG8(BL) == 0x08) {
					ULARGE_INTEGER hund;
					hund.LowPart = local.dwLowDateTime;
					hund.HighPart = local.dwHighDateTime;
					hund.QuadPart /= 100000;
					REG16(SI) = (UINT16)(hund.QuadPart % 200);
				}
			} else {
				REG16(AX) = (UINT16)GetLastError();
				m_CF = 1;
			}
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_44h()
{
	static UINT16 iteration_count = 0;
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	UINT32 val = DRIVE_NO_ROOT_DIR;
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		if(fd >= process->max_files || !file_handler[fd].valid) {
			REG16(AX) = 0x06;
			m_CF = 1;
			return;
		}
		break;
	case 0x08:
	case 0x09:
		if(REG8(BL) >= ('Z' - 'A' + 1)) {
			// invalid drive number
			REG16(AX) = 0x0f;
			m_CF = 1;
			return;
		} else {
			if(REG8(BL) == 0) {
				val = GetDriveType(NULL);
			} else {
				char tmp[8];
				sprintf(tmp, "%c:\\", 'A' + REG8(BL) - 1);
				val = GetDriveType(tmp);
			}
			if(val == DRIVE_NO_ROOT_DIR) {
				// no drive
				REG16(AX) = 0x0f;
				m_CF = 1;
				return;
			}
		}
		break;
	}
	switch(REG8(AL)) {
	case 0x00: // get ioctrl data
		REG16(DX) = file_handler[fd].info;
		break;
	case 0x01: // set ioctrl data
		file_handler[fd].info |= REG8(DL);
		break;
	case 0x02: // recv from character device
	case 0x03: // send to character device
	case 0x04: // recv from block device
	case 0x05: // send to block device
		REG16(AX) = 0x05;
		m_CF = 1;
		break;
	case 0x06: // get read status
		if(file_mode[file_handler[fd].mode].in) {
			if(file_handler[fd].atty) {
				REG8(AL) = msdos_kbhit() ? 0xff : 0x00;
			} else {
				REG8(AL) = eof(fd) ? 0x00 : 0xff;
			}
		} else {
			REG8(AL) = 0x00;
		}
		break;
	case 0x07: // get write status
		if(file_mode[file_handler[fd].mode].out) {
			REG8(AL) = 0xff;
		} else {
			REG8(AL) = 0x00;
		}
		break;
	case 0x08: // check removable drive
		if(val == DRIVE_REMOVABLE || val == DRIVE_CDROM) {
			// removable drive
			REG16(AX) = 0x00;
		} else {
			// fixed drive
			REG16(AX) = 0x01;
		}
		break;
	case 0x09: // check remote drive
		if(val == DRIVE_REMOTE) {
			// remote drive
			REG16(DX) = 0x1000;
		} else {
			// local drive
			REG16(DX) = 0x00;
		}
		break;
	case 0x0a: // check remote handle
		REG16(DX) = 0x00; // FIXME
		break;
	case 0x0b: // set retry count
		break;
	case 0x0c: // generic character device request
		if(REG8(CL) == 0x45) {
			// set iteration (retry) count
			iteration_count = *(UINT8 *)(mem + SREG_BASE(DS) + REG16(DX));
		} else if(REG8(CL) == 0x4a) {
			// select code page
			active_code_page = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 2);
			msdos_nls_tables_update();
		} else if(REG8(CL) == 0x65) {
			// get iteration (retry) count
			*(UINT8 *)(mem + SREG_BASE(DS) + REG16(DX)) = iteration_count;
		} else if(REG8(CL) == 0x6a) {
			// query selected code page
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0) = 2; // FIXME
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 2) = active_code_page;
			
			CPINFO info;
			GetCPInfo(active_code_page, &info);
			
			if(info.MaxCharSize != 1) {
				for(int i = 0;; i++) {
					UINT8 lo = info.LeadByte[2 * i + 0];
					UINT8 hi = info.LeadByte[2 * i + 1];
					
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 4 + 4 * i + 0) = lo;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 4 + 4 * i + 2) = hi;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0) = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0) + 4;
					
					if(lo == 0 && hi == 0) {
						break;
					}
				}
			}
		} else if(REG8(CL) == 0x7f) {
			*(UINT8  *)(mem + SREG_BASE(DS) + REG16(DX) + 0x00) = 0;
			*(UINT8  *)(mem + SREG_BASE(DS) + REG16(DX) + 0x01) = 0;
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x02) = 14;
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x04) = 1;
			*(UINT8  *)(mem + SREG_BASE(DS) + REG16(DX) + 0x06) = 1;
			*(UINT8  *)(mem + SREG_BASE(DS) + REG16(DX) + 0x07) = 0;
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x08) = 4;
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x0a) =  8 * (*(UINT16 *)(mem + 0x44a));
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x0c) = 16 * (*(UINT8  *)(mem + 0x484) + 1);
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x0e) = *(UINT16 *)(mem + 0x44a);
			*(UINT16 *)(mem + SREG_BASE(DS) + REG16(DX) + 0x10) = *(UINT8  *)(mem + 0x484) + 1;
		} else {
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01; // invalid function
			m_CF = 1;
		}
		break;
	case 0x0d: // generic block device request
		if(REG8(CL) == 0x40) {
			// set device parameters
		} else if(REG8(CL) == 0x46) {
			// set volume serial number
		} else if(REG8(CL) == 0x4a) {
			// lock logical volume
		} else if(REG8(CL) == 0x4b) {
			// lock physical volume
		} else if(REG8(CL) == 0x60) {
			// get device parameters
			char dev[] = "\\\\.\\A:";
			dev[4] = 'A' + (REG8(BL) ? REG8(BL) : _getdrive()) - 1;
			
			HANDLE hFile = CreateFile(dev, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE) {
				DISK_GEOMETRY geo;
				DWORD dwSize;
				if(DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo, sizeof(geo), &dwSize, NULL)) {
					*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0x07; // ???
					switch(geo.MediaType) {
					case F5_360_512:
					case F5_320_512:
					case F5_320_1024:
					case F5_180_512:
					case F5_160_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x00; // 320K/360K disk
						break;
					case F5_1Pt2_512:
					case F3_1Pt2_512:
					case F3_1Pt23_1024:
					case F5_1Pt23_1024:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x01; // 1.2M disk
						break;
					case F3_720_512:
					case F3_640_512:
					case F5_640_512:
					case F5_720_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x02; // 720K disk
						break;
					case F8_256_128:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x03; // single-density 8-inch disk
						break;
					case FixedMedia:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x05; // fixed disk
						break;
					case F3_1Pt44_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x07; // (DOS 3.3+) other type of block device, normally 1.44M floppy
						break;
					case F3_2Pt88_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x09; // (DOS 5+) 2.88M floppy
						break;
					default:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x05; // fixed disk
//						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x05; // (DOS 3.3+) other type of block device, normally 1.44M floppy
						break;
					}
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x02) = (geo.MediaType == FixedMedia) ? 0x01 : 0x00;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x04) = (geo.Cylinders.QuadPart > 0xffff) ? 0xffff : geo.Cylinders.QuadPart;
					switch(geo.MediaType) {
					case F5_360_512:
					case F5_320_512:
					case F5_320_1024:
					case F5_180_512:
					case F5_160_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x06) = 0x01; // 320K/360K disk
						break;
					default:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x06) = 0x00; // other drive types
						break;
					}
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x00) = geo.BytesPerSector;
					*(UINT8  *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x02) = geo.SectorsPerTrack * geo.TracksPerCylinder;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x03) = 0;
					*(UINT8  *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x05) = 0;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x06) = 0;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x08) = 0;
					switch(geo.MediaType) {
					case F5_320_512:	// floppy, double-sided, 8 sectors per track (320K)
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xff;
						break;
					case F5_160_512:	// floppy, single-sided, 8 sectors per track (160K)
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xfe;
						break;
					case F5_360_512:	// floppy, double-sided, 9 sectors per track (360K)
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xfd;
						break;
					case F5_180_512:	// floppy, single-sided, 9 sectors per track (180K)
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xfc;
						break;
					case F5_1Pt2_512:	// floppy, double-sided, 15 sectors per track (1.2M)
					case F3_720_512:	// floppy, double-sided, 9 sectors per track (720K,3.5")
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xf9;
						break;
					case FixedMedia:	// hard disk
					case RemovableMedia:
					case Unknown:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xf8;
						break;
					default:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0a) = 0xf0;
						break;
					}
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0d) = geo.SectorsPerTrack;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x0f) = 1;
					*(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x11) = 0;
					*(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x15) = geo.TracksPerCylinder * geo.Cylinders.QuadPart;
					*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07 + 0x1f) = geo.Cylinders.QuadPart;
					// 21h	BYTE	device type
					// 22h	WORD	device attributes (removable or not, etc)
				} else {
					REG16(AX) = 0x0f; // invalid drive
					m_CF = 1;
				}
				CloseHandle(hFile);
			} else {
				REG16(AX) = 0x0f; // invalid drive
				m_CF = 1;
			}
		} else if(REG8(CL) == 0x66) {
			// get volume serial number
			char path[] = "A:\\";
			char volume_label[MAX_PATH];
			DWORD serial_number = 0;
			char file_system[MAX_PATH];
			
			path[0] = 'A' + (REG8(BL) ? REG8(BL) : _getdrive()) - 1;
			
			if(GetVolumeInformation(path, volume_label, MAX_PATH, &serial_number, NULL, NULL, file_system, MAX_PATH)) {
				*(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0;
				*(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x02) = serial_number;
				memset(mem + SREG_BASE(DS) + REG16(SI) + 0x06, 0x20, 11);
				memcpy(mem + SREG_BASE(DS) + REG16(SI) + 0x06, volume_label, min(strlen(volume_label), 11));
				memset(mem + SREG_BASE(DS) + REG16(SI) + 0x11, 0x20,  8);
				memcpy(mem + SREG_BASE(DS) + REG16(SI) + 0x11, file_system, min(strlen(file_system), 8));
			} else {
				REG16(AX) = 0x0f; // invalid drive
				m_CF = 1;
			}
		} else if(REG8(CL) == 0x67) {
			// get access flag
			*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0;
			*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 1;
		} else if(REG8(CL) == 0x68) {
			// sense media type
			char dev[64];
			sprintf(dev, "\\\\.\\%c:", 'A' + (REG8(BL) ? REG8(BL) : _getdrive()) - 1);
			
			HANDLE hFile = CreateFile(dev, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE) {
				DISK_GEOMETRY geo;
				DWORD dwSize;
				if(DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo, sizeof(geo), &dwSize, NULL)) {
					switch(geo.MediaType) {
					case F3_720_512:
					case F5_720_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0x01;
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x02;
						break;
					case F3_1Pt44_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0x01;
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x07;
						break;
					case F3_2Pt88_512:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0x01;
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x09;
						break;
					default:
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00) = 0x00;
						*(UINT8 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x01) = 0x00; // ???
						break;
					}
				} else {
					REG16(AX) = 0x0f; // invalid drive
					m_CF = 1;
				}
				CloseHandle(hFile);
			} else {
				REG16(AX) = 0x0f; // invalid drive
				m_CF = 1;
			}
		} else if(REG8(CL) == 0x6a) {
			// unlock logical volume
		} else if(REG8(CL) == 0x6b) {
			// unlock physical volume
		} else {
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01; // invalid function
			m_CF = 1;
		}
		break;
	case 0x0e: // get logical drive map
		{
			DWORD bits = 1 << ((REG8(BL) ? REG8(BL) : _getdrive()) - 1);
			if(!(GetLogicalDrives() & bits)) {
				REG16(AX) = 0x0f; // invalid drive
				m_CF = 1;
			} else {
				REG8(AL) = 0;
			}
		}
		break;
	case 0x0f: // set logical drive map
		{
			DWORD bits = 1 << ((REG8(BL) ? REG8(BL) : _getdrive()) - 1);
			if(!(GetLogicalDrives() & bits)) {
				REG16(AX) = 0x0f; // invalid drive
				m_CF = 1;
			}
		}
		break;
	case 0x10: // query generic ioctrl capability (handle)
		switch(REG8(CL)) {
		case 0x45:
		case 0x4a:
		case 0x65:
		case 0x6a:
		case 0x7f:
			REG16(AX) = 0x0000; // supported
			break;
		default:
			REG8(AL) = 0x01; // ioctl capability not available
			m_CF = 1;
			break;
		}
		break;
	case 0x11: // query generic ioctrl capability (drive)
		switch(REG8(CL)) {
		case 0x40:
		case 0x46:
		case 0x4a:
		case 0x4b:
		case 0x60:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x6a:
		case 0x6b:
			REG16(AX) = 0x0000; // supported
			break;
		default:
			REG8(AL) = 0x01; // ioctl capability not available
			m_CF = 1;
			break;
		}
		break;
	case 0x12: // determine dos type
	case 0x51: // concurrent dos v3.2+ - installation check
	case 0x52: // determine dos type/get dr dos versuin
		REG16(AX) = 0x01; // this  is not DR-DOS
		m_CF = 1;
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_45h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		int dup_fd = _dup(fd);
		if(dup_fd != -1) {
			REG16(AX) = dup_fd;
			msdos_file_handler_dup(dup_fd, fd, current_psp);
//			msdos_psp_set_file_table(dup_fd, fd, current_psp);
			msdos_psp_set_file_table(dup_fd, dup_fd, current_psp);
		} else {
			REG16(AX) = errno;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_46h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	int dup_fd = REG16(CX);
	int tmp_fd = msdos_psp_get_file_table(REG16(CX), current_psp);
	
	if(REG16(BX) == REG16(CX)) {
		REG16(AX) = 0x06;
		m_CF = 1;
	} else if(fd < process->max_files && file_handler[fd].valid && dup_fd < process->max_files) {
		if(tmp_fd < process->max_files && file_handler[tmp_fd].valid) {
			_close(tmp_fd);
			msdos_file_handler_close(tmp_fd);
			msdos_psp_set_file_table(dup_fd, 0x0ff, current_psp);
		}
		if(_dup2(fd, dup_fd) != -1) {
			msdos_file_handler_dup(dup_fd, fd, current_psp);
//			msdos_psp_set_file_table(dup_fd, fd, current_psp);
			msdos_psp_set_file_table(dup_fd, dup_fd, current_psp);
		} else {
			REG16(AX) = errno;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_47h(int lfn)
{
	char path[MAX_PATH];
	
	if(_getdcwd(REG8(DL), path, MAX_PATH) != NULL) {
		if(path[1] == ':') {
			// the returned path does not include a drive or the initial backslash
			strcpy((char *)(mem + SREG_BASE(DS) + REG16(SI)), (lfn ? path : msdos_short_path(path)) + 3);
		} else {
			strcpy((char *)(mem + SREG_BASE(DS) + REG16(SI)), lfn ? path : msdos_short_path(path));
		}
	} else {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_48h()
{
	int seg, umb_linked;
	
	if((malloc_strategy & 0xf0) == 0x00) {
		// unlink umb not to allocate memory in umb
		if((umb_linked = msdos_mem_get_umb_linked()) != 0) {
			msdos_mem_unlink_umb();
		}
		if((seg = msdos_mem_alloc(first_mcb, REG16(BX), 0)) != -1) {
			REG16(AX) = seg;
		} else {
			REG16(AX) = 0x08;
			REG16(BX) = msdos_mem_get_free(first_mcb, 0);
			m_CF = 1;
		}
		if(umb_linked != 0) {
			msdos_mem_link_umb();
		}
	} else if((malloc_strategy & 0xf0) == 0x40) {
		if((seg = msdos_mem_alloc(UMB_TOP >> 4, REG16(BX), 0)) != -1) {
			REG16(AX) = seg;
		} else {
			REG16(AX) = 0x08;
			REG16(BX) = msdos_mem_get_free(UMB_TOP >> 4, 0);
			m_CF = 1;
		}
	} else if((malloc_strategy & 0xf0) == 0x80) {
		if((seg = msdos_mem_alloc(UMB_TOP >> 4, REG16(BX), 0)) != -1) {
			REG16(AX) = seg;
		} else if((seg = msdos_mem_alloc(first_mcb, REG16(BX), 0)) != -1) {
			REG16(AX) = seg;
		} else {
			REG16(AX) = 0x08;
			REG16(BX) = max(msdos_mem_get_free(UMB_TOP >> 4, 0), msdos_mem_get_free(first_mcb, 0));
			m_CF = 1;
		}
	}
}

inline void msdos_int_21h_49h()
{
	int mcb_seg = SREG(ES) - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		msdos_mem_free(SREG(ES));
	} else {
		REG16(AX) = 0x09; // illegal memory block address 
		m_CF = 1;
	}
}

inline void msdos_int_21h_4ah()
{
	int mcb_seg = SREG(ES) - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	int max_paragraphs;
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		if(msdos_mem_realloc(SREG(ES), REG16(BX), &max_paragraphs)) {
			REG16(AX) = 0x08;
			REG16(BX) = max_paragraphs > 0x7fff && limit_max_memory ? 0x7fff : max_paragraphs;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x09; // illegal memory block address 
		m_CF = 1;
	}
}

inline void msdos_int_21h_4bh()
{
	char *command = (char *)(mem + SREG_BASE(DS) + REG16(DX));
	param_block_t *param = (param_block_t *)(mem + SREG_BASE(ES) + REG16(BX));
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		if(msdos_process_exec(command, param, REG8(AL))) {
			REG16(AX) = 0x02;
			m_CF = 1;
		}
		break;
	case 0x03:
		{
			int fd;
			if((fd = _open(command, _O_RDONLY | _O_BINARY)) == -1) {
				REG16(AX) = 0x02;
				m_CF = 1;
				break;
			}
			int size = _read(fd, file_buffer, sizeof(file_buffer));
			_close(fd);
			
			UINT16 *overlay = (UINT16 *)param;
			
			// check exe header
			exe_header_t *header = (exe_header_t *)file_buffer;
			int header_size = 0;
			if(header->mz == 0x4d5a || header->mz == 0x5a4d) {
				header_size = header->header_size * 16;
				// relocation
				int start_seg = overlay[1];
				for(int i = 0; i < header->relocations; i++) {
					int ofs = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 0);
					int seg = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 2);
					*(UINT16 *)(file_buffer + header_size + (seg << 4) + ofs) += start_seg;
				}
			}
			memcpy(mem + (overlay[0] << 4), file_buffer + header_size, size - header_size);
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_4ch()
{
	msdos_process_terminate(current_psp, REG8(AL), 1);
}

inline void msdos_int_21h_4dh()
{
	REG16(AX) = retval;
}

inline void msdos_int_21h_4eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	find_t *find = (find_t *)(mem + dta_laddr);
	char *path = msdos_trimmed_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), 0);
	WIN32_FIND_DATA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, LFN_DTA_LADDR);
	find->find_magic = FIND_MAGIC;
	find->dta_index = dtainfo - dtalist;
	strcpy(process->volume_label, msdos_volume_label(path));
	dtainfo->allowable_mask = REG8(CL);
	bool label_only = (dtainfo->allowable_mask == 8);
	
	if((dtainfo->allowable_mask & 8) && !msdos_match_volume_label(path, msdos_short_volume_label(process->volume_label))) {
		dtainfo->allowable_mask &= ~8;
	}
	if(!label_only && (dtainfo->find_handle = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
		      !msdos_find_file_has_8dot3name(&fd)) {
			if(!FindNextFile(dtainfo->find_handle, &fd)) {
				FindClose(dtainfo->find_handle);
				dtainfo->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = (UINT8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_name(&fd));
		REG16(AX) = 0;
	} else if(dtainfo->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
		REG16(AX) = 0;
	} else {
		REG16(AX) = 0x12;	// NOTE: return 0x02 if file path is invalid
		m_CF = 1;
	}
}

inline void msdos_int_21h_4fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	find_t *find = (find_t *)(mem + dta_laddr);
	WIN32_FIND_DATA fd;
	
	if(find->find_magic != FIND_MAGIC || find->dta_index >= MAX_DTAINFO) {
		REG16(AX) = 0x12;
		m_CF = 1;
		return;
	}
	dtainfo_t *dtainfo = &dtalist[find->dta_index];
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFile(dtainfo->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
			      !msdos_find_file_has_8dot3name(&fd)) {
				if(!FindNextFile(dtainfo->find_handle, &fd)) {
					FindClose(dtainfo->find_handle);
					dtainfo->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(dtainfo->find_handle);
			dtainfo->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = (UINT8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_name(&fd));
		REG16(AX) = 0;
	} else if(dtainfo->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
		REG16(AX) = 0;
	} else {
		REG16(AX) = 0x12;
		m_CF = 1;
	}
}

inline void msdos_int_21h_50h()
{
	if(current_psp != REG16(BX)) {
		process_t *process = msdos_process_info_get(current_psp);
		if(process != NULL) {
			process->psp = REG16(BX);
		}
		current_psp = REG16(BX);
		msdos_sda_update(current_psp);
	}
}

inline void msdos_int_21h_51h()
{
	REG16(BX) = current_psp;
}

inline void msdos_int_21h_52h()
{
	SREG(ES) = DOS_INFO_TOP >> 4;
	i386_load_segment_descriptor(ES);
	REG16(BX) = offsetof(dos_info_t, first_dpb);
}

inline void msdos_int_21h_54h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	REG8(AL) = process->verify;
}

inline void msdos_int_21h_55h()
{
	psp_t *psp = (psp_t *)(mem + (REG16(DX) << 4));
	
	memcpy(mem + (REG16(DX) << 4), mem + (current_psp << 4), sizeof(psp_t));
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = current_psp;
}

inline void msdos_int_21h_56h(int lfn)
{
	char src[MAX_PATH], dst[MAX_PATH];
	strcpy(src, msdos_trimmed_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), lfn));
	strcpy(dst, msdos_trimmed_path((char *)(mem + SREG_BASE(ES) + REG16(DI)), lfn));
	
	if(rename(src, dst)) {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_57h()
{
	FILETIME time, local;
	FILETIME *ctime, *atime, *mtime;
	HANDLE hHandle;
	
	if((hHandle = (HANDLE)_get_osfhandle(REG16(BX))) == INVALID_HANDLE_VALUE) {
		REG16(AX) = (UINT16)GetLastError();
		m_CF = 1;
		return;
	}
	ctime = atime = mtime = NULL;
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		mtime = &time;
		break;
	case 0x04:
	case 0x05:
		atime = &time;
		break;
	case 0x06:
	case 0x07:
		ctime = &time;
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		return;
	}
	if(REG8(AL) & 1) {
		DosDateTimeToFileTime(REG16(DX), REG16(CX), &local);
		LocalFileTimeToFileTime(&local, &time);
		if(!SetFileTime(hHandle, ctime, atime, mtime)) {
			REG16(AX) = (UINT16)GetLastError();
			m_CF = 1;
		}
	} else {
		if(!GetFileTime(hHandle, ctime, atime, mtime)) {
			// assume a device and use the current time
			GetSystemTimeAsFileTime(&time);
		}
		FileTimeToLocalFileTime(&time, &local);
		FileTimeToDosDateTime(&local, &REG16(DX), &REG16(CX));
	}
}

inline void msdos_int_21h_58h()
{
	switch(REG8(AL)) {
	case 0x00:
		REG16(AX) = malloc_strategy;
		break;
	case 0x01:
//		switch(REG16(BX)) {
		switch(REG8(BL)) {
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0040:
		case 0x0041:
		case 0x0042:
		case 0x0080:
		case 0x0081:
		case 0x0082:
			malloc_strategy = REG16(BX);
			msdos_sda_update(current_psp);
			break;
		default:
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
			break;
		}
		break;
	case 0x02:
		REG8(AL) = msdos_mem_get_umb_linked() ? 1 : 0;
		break;
	case 0x03:
//		switch(REG16(BX)) {
		switch(REG8(BL)) {
		case 0x0000:
			msdos_mem_unlink_umb();
			break;
		case 0x0001:
			msdos_mem_link_umb();
			break;
		default:
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
			break;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_59h()
{
	sda_t *sda = (sda_t *)(mem + SDA_TOP);
	
	REG16(AX) = sda->extended_error_code;
	REG8(BH) = sda->error_class;
	REG8(BL) = sda->suggested_action;
	REG8(CH) = sda->locus_of_last_error;
}

inline void msdos_int_21h_5ah()
{
	char *path = (char *)(mem + SREG_BASE(DS) + REG16(DX));
	int len = strlen(path);
	char tmp[MAX_PATH];
	
	if(GetTempFileName(path, "TMP", 0, tmp)) {
		int fd = _open(tmp, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		
		SetFileAttributes(tmp, msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY);
		REG16(AX) = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
		msdos_psp_set_file_table(fd, fd, current_psp);
		
		strcpy(path, tmp);
		int dx = REG16(DX) + len;
		int ds = SREG(DS);
		while(dx > 0xffff) {
			dx -= 0x10;
			ds++;
		}
		REG16(DX) = dx;
		SREG(DS) = ds;
		i386_load_segment_descriptor(DS);
	} else {
		REG16(AX) = (UINT16)GetLastError();
		m_CF = 1;
	}
}

inline void msdos_int_21h_5bh()
{
	char *path = msdos_local_file_path((char *)(mem + SREG_BASE(DS) + REG16(DX)), 0);
	
	if(msdos_is_existing_file(path)) {
		// already exists
		REG16(AX) = 0x50;
		m_CF = 1;
	} else {
		int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		
		if(fd != -1) {
			SetFileAttributes(path, msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY);
			REG16(AX) = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
			msdos_psp_set_file_table(fd, fd, current_psp);
		} else {
			REG16(AX) = errno;
			m_CF = 1;
		}
	}
}

inline void msdos_int_21h_5ch()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(REG8(AL) == 0 || REG8(AL) == 1) {
			static int modes[2] = {_LK_LOCK, _LK_UNLCK};
			UINT32 pos = _tell(fd);
			_lseek(fd, (REG16(CX) << 16) | REG16(DX), SEEK_SET);
			if(_locking(fd, modes[REG8(AL)], (REG16(SI) << 16) | REG16(DI))) {
				REG16(AX) = errno;
				m_CF = 1;
			}
			_lseek(fd, pos, SEEK_SET);
			
			// some seconds may be passed in _locking()
			hardware_update();
		} else {
			REG16(AX) = 0x01;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_5dh()
{
	switch(REG8(AL)) {
	case 0x06: // get address of dos swappable data area
		SREG(DS) = (SDA_TOP >> 4);
		i386_load_segment_descriptor(DS);
		REG16(SI) = offsetof(sda_t, crit_error_flag);
		REG16(CX) = 0x80;
		REG16(DX) = 0x1a;
		break;
	case 0x0b: // get dos swappable data areas
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	case 0x08: // set redirected printer mode
	case 0x09: // flush redirected printer output
	case 0x0a: // set extended error information
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_5fh()
{
	switch(REG8(AL)) {
	case 0x02:
		{
			DWORD drives = GetLogicalDrives();
			for(int i = 0, index = 0; i < 26; i++) {
				if(drives & (1 << i)) {
					char volume[] = "A:\\";
					volume[0] = 'A' + i;
					if(GetDriveType(volume) == DRIVE_REMOTE) {
						if(index == REG16(BX)) {
							DWORD dwSize = 128;
							volume[2] = '\0';
							strcpy((char *)(mem + SREG_BASE(DS) + REG16(SI)), volume);
							WNetGetConnection(volume, (char *)(mem + SREG_BASE(ES) + REG16(DI)), &dwSize);
							REG8(BH) = 0x00; // valid
							REG8(BL) = 0x04; // disk drive
							REG16(CX) = 0x00;
							return;
						}
						index++;
					}
				}
			}
		}
		REG16(AX) = 0x12; // no more files
		m_CF = 1;
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_60h(int lfn)
{
	char full[MAX_PATH], *path;
	
	if(lfn) {
		char *name;
		*full = '\0';
		GetFullPathName((char *)(mem + SREG_BASE(DS) + REG16(SI)), MAX_PATH, full, &name);
		switch(REG8(CL)) {
		case 1:
			GetShortPathName(full, full, MAX_PATH);
			my_strupr(full);
			break;
		case 2:
			GetLongPathName(full, full, MAX_PATH);
			break;
		}
		path = full;
	} else {
		path = msdos_short_full_path((char *)(mem + SREG_BASE(DS) + REG16(SI)));
	}
	if(*path != '\0') {
		strcpy((char *)(mem + SREG_BASE(ES) + REG16(DI)), path);
	} else {
		REG16(AX) = (UINT16)GetLastError();
		m_CF = 1;
	}
}

inline void msdos_int_21h_61h()
{
	REG8(AL) = 0;
}

inline void msdos_int_21h_62h()
{
	REG16(BX) = current_psp;
}

inline void msdos_int_21h_63h()
{
	switch(REG8(AL)) {
	case 0x00:
		SREG(DS) = (DBCS_TABLE >> 4);
		i386_load_segment_descriptor(DS);
		REG16(SI) = (DBCS_TABLE & 0x0f);
		REG8(AL) = 0x00;
		break;
	case 0x01: // set korean input mode
	case 0x02: // get korean input mode
		REG8(AL) = 0xff; // not supported
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

UINT16 get_extended_country_info(UINT8 func)
{
	switch(func) {
	case 0x01:
		if(REG16(CX) >= 5) {
			UINT8 data[1 + 2 + 2 + 2 + sizeof(country_info_t)];
			if(REG16(CX) > sizeof(data))		// cx = actual transfer size
				REG16(CX) = sizeof(data);
			ZeroMemory(data, sizeof(data));
			data[0] = 0x01;
			*(UINT16 *)(data + 1) = REG16(CX) - 3;
			*(UINT16 *)(data + 3) = get_country_info((country_info_t*)(data + 7));
			*(UINT16 *)(data + 5) = active_code_page;
			memcpy(mem + SREG_BASE(ES) + REG16(DI), data, REG16(CX));
//			REG16(AX) = active_code_page;
		} else {
			return(0x08); // insufficient memory
		}
		break;
	case 0x02:
		*(UINT8  *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 0x02;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 1) = (UPPERTABLE_TOP & 0x0f);
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 3) = (UPPERTABLE_TOP >> 4);
//		REG16(AX) = active_code_page;
		REG16(CX) = 0x05;
		break;
	case 0x03:
		*(UINT8  *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 0x02;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 1) = (LOWERTABLE_TOP & 0x0f);
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 3) = (LOWERTABLE_TOP >> 4);
//		REG16(AX) = active_code_page;
		REG16(CX) = 0x05;
		break;
	case 0x04:
		*(UINT8  *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 0x04;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 1) = (FILENAME_UPPERTABLE_TOP & 0x0f);
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 3) = (FILENAME_UPPERTABLE_TOP >> 4);
//		REG16(AX) = active_code_page;
		REG16(CX) = 0x05;
		break;
	case 0x05:
		*(UINT8  *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 0x05;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 1) = (FILENAME_TERMINATOR_TOP & 0x0f);
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 3) = (FILENAME_TERMINATOR_TOP >> 4);
//		REG16(AX) = active_code_page;
		REG16(CX) = 0x05;
		break;
	case 0x06:
		*(UINT8  *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 0x06;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 1) = (COLLATING_TABLE_TOP & 0x0f);
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 3) = (COLLATING_TABLE_TOP >> 4);
//		REG16(AX) = active_code_page;
		REG16(CX) = 0x05;
		break;
	case 0x07:
		*(UINT8  *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 0x07;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 1) = (DBCS_TOP & 0x0f);
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 3) = (DBCS_TOP >> 4);
//		REG16(AX) = active_code_page;
		REG16(CX) = 0x05;
		break;
	default:
		return(0x01); // function number invalid
	}
	return(0x00);
}

inline void msdos_int_21h_65h()
{
	char tmp[0x10000];
	
	switch(REG8(AL)) {
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		{
			UINT16 result = get_extended_country_info(REG8(AL));
			if(result) {
				REG16(AX) = result;
				m_CF = 1;
			} else {
				REG16(AX) = active_code_page; // FIXME: is this correct???
			}
		}
		break;
	case 0x20:
	case 0xa0:
		memset(tmp, 0, sizeof(tmp));
		tmp[0] = REG8(DL);
		my_strupr(tmp);
		REG8(DL) = tmp[0];
		break;
	case 0x21:
	case 0xa1:
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, mem + SREG_BASE(DS) + REG16(DX), REG16(CX));
		my_strupr(tmp);
		memcpy(mem + SREG_BASE(DS) + REG16(DX), tmp, REG16(CX));
		break;
	case 0x22:
	case 0xa2:
		my_strupr((char *)(mem + SREG_BASE(DS) + REG16(DX)));
		break;
	case 0x23:
		// FIXME: need to check multi-byte (kanji) charactre?
		if(REG8(DL) == 'N' || REG8(DL) == 'n' || (REG8(DL) == 0x82 && REG8(DH) == 0x78) || (REG8(DL) == 0x82 && REG8(DH) == 0x99)) {
			// 8278h/8299h: multi-byte (kanji) Y and y
			REG16(AX) = 0x00;
		} else if(REG8(DL) == 'Y' || REG8(DL) == 'y' || (REG8(DL) == 0x82 && REG8(DH) == 0x6d) || (REG8(DL) == 0x82 && REG8(DH) == 0x8e)) {
			// 826dh/828eh: multi-byte (kanji) N and n
			REG16(AX) = 0x01;
		} else {
			REG16(AX) = 0x02;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_66h()
{
	switch(REG8(AL)) {
	case 0x01:
		REG16(BX) = active_code_page;
		REG16(DX) = system_code_page;
		break;
	case 0x02:
		if(active_code_page == REG16(BX)) {
			REG16(AX) = 0xeb41;
		} else if(_setmbcp(REG16(BX)) == 0) {
			active_code_page = REG16(BX);
			msdos_nls_tables_update();
			REG16(AX) = 0xeb41;
		} else {
			REG16(AX) = 0x25;
			m_CF = 1;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_67h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) <= MAX_FILES) {
		process->max_files = max(REG16(BX), 20);
	} else {
		REG16(AX) = 0x08;
		m_CF = 1;
	}
}

inline void msdos_int_21h_68h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		// fflush(_fdopen(fd, ""));
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_69h()
{
	drive_info_t *info = (drive_info_t *)(mem + SREG_BASE(DS) + REG16(DX));
	char path[] = "A:\\";
	char volume_label[MAX_PATH];
	DWORD serial_number = 0;
	char file_system[MAX_PATH];
	
	if(REG8(BL) == 0) {
		path[0] = 'A' + _getdrive() - 1;
	} else {
		path[0] = 'A' + REG8(BL) - 1;
	}
	
	switch(REG8(AL)) {
	case 0x00:
		if(GetVolumeInformation(path, volume_label, MAX_PATH, &serial_number, NULL, NULL, file_system, MAX_PATH)) {
			info->info_level = 0;
			info->serial_number = serial_number;
			memset(info->volume_label, 0x20, 11);
			memcpy(info->volume_label, volume_label, min(strlen(volume_label), 11));
			memset(info->file_system, 0x20, 8);
			memcpy(info->file_system, file_system, min(strlen(file_system), 8));
		} else {
			REG16(AX) = errno;
			m_CF = 1;
		}
		break;
	case 0x01:
		REG16(AX) = 0x03;
		m_CF = 1;
	}
}

inline void msdos_int_21h_6ah()
{
	REG8(AH) = 0x68;
	msdos_int_21h_68h();
}

inline void msdos_int_21h_6bh()
{
	REG8(AL) = 0;
}

inline void msdos_int_21h_6ch(int lfn)
{
	char *path = msdos_local_file_path((char *)(mem + SREG_BASE(DS) + REG16(SI)), lfn);
	int mode = REG8(BL) & 0x03;
	
	if(mode < 0x03) {
		if(msdos_is_device_path(path) || msdos_is_existing_file(path)) {
			// file exists
			if(REG8(DL) & 1) {
				int fd = -1, c;
				UINT16 info;
				
				if(msdos_is_con_path(path)) {
					fd = msdos_open("CON", file_mode[mode].mode);
					info = 0x80d3;
				} else if((c = msdos_is_comm_path(path)) != 0 && sio_port_number[c - 1] != 0) {
					if((fd = _open(msdos_create_comm_path(path, sio_port_number[c - 1]), file_mode[mode].mode)) == -1) {
						fd = msdos_open("NUL", file_mode[mode].mode);
					}
					info = 0x80d3;
				} else if(msdos_is_device_path(path)) {
					fd = msdos_open("NUL", file_mode[mode].mode);
					info = 0x80d3;
				} else {
					fd = msdos_open(path, file_mode[mode].mode);
					info = msdos_drive_number(path);
				}
				if(fd != -1) {
					REG16(AX) = fd;
					REG16(CX) = 1;
					msdos_file_handler_open(fd, path, _isatty(fd), mode, info, current_psp);
					msdos_psp_set_file_table(fd, fd, current_psp);
				} else {
					REG16(AX) = errno;
					m_CF = 1;
				}
			} else if(REG8(DL) & 2) {
				int attr = GetFileAttributes(path);
				int fd = -1, c;
				UINT16 info;
				
				if(msdos_is_con_path(path)) {
					fd = msdos_open("CON", file_mode[mode].mode);
					info = 0x80d3;
				} else if((c = msdos_is_comm_path(path)) != 0 && sio_port_number[c - 1] != 0) {
					if((fd = _open(msdos_create_comm_path(path, sio_port_number[c - 1]), file_mode[mode].mode)) == -1) {
						fd = msdos_open("NUL", file_mode[mode].mode);
					}
					info = 0x80d3;
				} else if(msdos_is_device_path(path)) {
					fd = msdos_open("NUL", file_mode[mode].mode);
					info = 0x80d3;
				} else {
					fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
					info = msdos_drive_number(path);
				}
				if(fd != -1) {
					if(attr == -1) {
						attr = msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY;
					}
					SetFileAttributes(path, attr);
					REG16(AX) = fd;
					REG16(CX) = 3;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, info, current_psp);
					msdos_psp_set_file_table(fd, fd, current_psp);
				} else {
					REG16(AX) = errno;
					m_CF = 1;
				}
			} else {
				REG16(AX) = 0x50;
				m_CF = 1;
			}
		} else {
			// file not exists
			if(REG8(DL) & 0x10) {
				int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
				
				if(fd != -1) {
					SetFileAttributes(path, msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY);
					REG16(AX) = fd;
					REG16(CX) = 2;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
					msdos_psp_set_file_table(fd, fd, current_psp);
				} else {
					REG16(AX) = errno;
					m_CF = 1;
				}
			} else {
				REG16(AX) = 0x02;
				m_CF = 1;
			}
		}
	} else {
		REG16(AX) = 0x0c;
		m_CF = 1;
	}
}

inline void msdos_int_21h_710dh()
{
	// reset drive
}

inline void msdos_int_21h_7141h(int lfn)
{
	if(REG16(SI) == 0) {
		msdos_int_21h_41h(lfn);
		return;
	}
	if(REG16(SI) != 1) {
		REG16(AX) = 5;
		m_CF = 1;
	}
	/* wild card and matching attributes... */
	char tmp[MAX_PATH * 2];
	// copy search pathname (and quick check overrun)
	ZeroMemory(tmp, sizeof(tmp));
	tmp[MAX_PATH - 1] = '\0';
	tmp[MAX_PATH] = 1;
	strncpy(tmp, (char *)(mem + SREG_BASE(DS) + REG16(DX)), MAX_PATH);
	
	if(tmp[MAX_PATH - 1] != '\0' || tmp[MAX_PATH] != 1) {
		REG16(AX) = 1;
		m_CF = 1;
		return;
	}
	for(char *s = tmp; *s; ++s) {
		if(*s == '/') {
			*s = '\\';
		}
	}
	char *tmp_name = (char *)_mbsrchr((unsigned char *)tmp, '\\');
	if(tmp_name) {
		++tmp_name;
	} else {
		tmp_name = strchr(tmp, ':');
		tmp_name = tmp_name ? tmp_name + 1 : tmp;
	}
	
	WIN32_FIND_DATAA fd;
	HANDLE fh = FindFirstFileA(tmp, &fd);
	if(fh == INVALID_HANDLE_VALUE) {
		REG16(AX) = 2;
		m_CF = 1;
		return;
	}
	do {
		if(msdos_find_file_check_attribute(fd.dwFileAttributes, REG8(CL), REG8(CH)) && msdos_find_file_has_8dot3name(&fd)) {
			strcpy(tmp_name, fd.cFileName);
			if(remove(msdos_trimmed_path(tmp, lfn))) {
				REG16(AX) = 5;
				m_CF = 1;
				break;
			}
		}
	} while(FindNextFileA(fh, &fd));
	if(!m_CF) {
		if(GetLastError() != ERROR_NO_MORE_FILES) {
			m_CF = 1;
			REG16(AX) = 2;
		}
	}
	FindClose(fh);
}

inline void msdos_int_21h_714eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + SREG_BASE(ES) + REG16(DI));
	char *path = (char *)(mem + SREG_BASE(DS) + REG16(DX));
	WIN32_FIND_DATA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, LFN_DTA_LADDR);
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(dtainfo->find_handle);
		dtainfo->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	dtainfo->allowable_mask = REG8(CL);
	dtainfo->required_mask = REG8(CH);
	bool label_only = (dtainfo->allowable_mask == 8);
	
	if((dtainfo->allowable_mask & 8) && !msdos_match_volume_label(path, msdos_short_volume_label(process->volume_label))) {
		dtainfo->allowable_mask &= ~8;
	}
	if(!label_only && (dtainfo->find_handle = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, dtainfo->required_mask)) {
			if(!FindNextFile(dtainfo->find_handle, &fd)) {
				FindClose(dtainfo->find_handle);
				dtainfo->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = fd.dwFileAttributes;
		msdos_find_file_conv_local_time(&fd);
		if(REG16(SI) == 0) {
			find->ctime_lo.dw = fd.ftCreationTime.dwLowDateTime;
			find->ctime_hi.dw = fd.ftCreationTime.dwHighDateTime;
			find->atime_lo.dw = fd.ftLastAccessTime.dwLowDateTime;
			find->atime_hi.dw = fd.ftLastAccessTime.dwHighDateTime;
			find->mtime_lo.dw = fd.ftLastWriteTime.dwLowDateTime;
			find->mtime_hi.dw = fd.ftLastWriteTime.dwHighDateTime;
		} else {
			FileTimeToDosDateTime(&fd.ftCreationTime, &find->ctime_lo.w.h, &find->ctime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->atime_lo.w.h, &find->atime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->mtime_lo.w.h, &find->mtime_lo.w.l);
		}
		find->size_hi = fd.nFileSizeHigh;
		find->size_lo = fd.nFileSizeLow;
		strcpy(find->full_name, fd.cFileName);
		strcpy(find->short_name, fd.cAlternateFileName);
		REG16(AX) = dtainfo - dtalist + 1;
	} else if(dtainfo->allowable_mask & 8) {
		// volume label
		find->attrib = 8;
		find->size_hi = find->size_lo = 0;
		strcpy(find->full_name, process->volume_label);
		strcpy(find->short_name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
		REG16(AX) = dtainfo - dtalist + 1;
	} else {
		REG16(AX) = 0x12;	// NOTE: return 0x02 if file path is invalid
		m_CF = 1;
	}
}

inline void msdos_int_21h_714fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + SREG_BASE(ES) + REG16(DI));
	WIN32_FIND_DATA fd;
	
	if(REG16(BX) - 1u >= MAX_DTAINFO) {
		REG16(AX) = 6;
		m_CF = 1;
		return;
	}
	dtainfo_t *dtainfo = &dtalist[REG16(BX) - 1];
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFile(dtainfo->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, dtainfo->required_mask)) {
				if(!FindNextFile(dtainfo->find_handle, &fd)) {
					FindClose(dtainfo->find_handle);
					dtainfo->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(dtainfo->find_handle);
			dtainfo->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = fd.dwFileAttributes;
		msdos_find_file_conv_local_time(&fd);
		if(REG16(SI) == 0) {
			find->ctime_lo.dw = fd.ftCreationTime.dwLowDateTime;
			find->ctime_hi.dw = fd.ftCreationTime.dwHighDateTime;
			find->atime_lo.dw = fd.ftLastAccessTime.dwLowDateTime;
			find->atime_hi.dw = fd.ftLastAccessTime.dwHighDateTime;
			find->mtime_lo.dw = fd.ftLastWriteTime.dwLowDateTime;
			find->mtime_hi.dw = fd.ftLastWriteTime.dwHighDateTime;
		} else {
			FileTimeToDosDateTime(&fd.ftCreationTime, &find->ctime_lo.w.h, &find->ctime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->atime_lo.w.h, &find->atime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->mtime_lo.w.h, &find->mtime_lo.w.l);
		}
		find->size_hi = fd.nFileSizeHigh;
		find->size_lo = fd.nFileSizeLow;
		strcpy(find->full_name, fd.cFileName);
		strcpy(find->short_name, fd.cAlternateFileName);
	} else if(dtainfo->allowable_mask & 8) {
		// volume label
		find->attrib = 8;
		find->size_hi = find->size_lo = 0;
		strcpy(find->full_name, process->volume_label);
		strcpy(find->short_name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
	} else {
		REG16(AX) = 0x12;
		m_CF = 1;
	}
}

inline void msdos_int_21h_71a0h()
{
	DWORD max_component_len, file_sys_flag;
	
	if(GetVolumeInformation((char *)(mem + SREG_BASE(DS) + REG16(DX)), NULL, 0, NULL, &max_component_len, &file_sys_flag, REG16(CX) == 0 ? NULL : (char *)(mem + SREG_BASE(ES) + REG16(DI)), REG16(CX))) {
		REG16(BX) = (UINT16)file_sys_flag & (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_VOLUME_IS_COMPRESSED);
		REG16(BX) |= 0x4000;				// supports LFN functions
		REG16(CX) = (UINT16)max_component_len;		// 255
		REG16(DX) = (UINT16)max_component_len + 5;	// 260
	} else {
		REG16(AX) = (UINT16)GetLastError();
		m_CF = 1;
	}
}

inline void msdos_int_21h_71a1h()
{
	if(REG16(BX) - 1u >= MAX_DTAINFO) {
		REG16(AX) = 6;
		m_CF = 1;
		return;
	}
	dtainfo_t *dtainfo = &dtalist[REG16(BX) - 1];
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(dtainfo->find_handle);
		dtainfo->find_handle = INVALID_HANDLE_VALUE;
	}
}

inline void msdos_int_21h_71a6h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
	
	UINT8 *buffer = (UINT8 *)(mem + SREG_BASE(DS) + REG16(DX));
	struct _stat64 status;
	DWORD serial_number = 0;
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(_fstat64(fd, &status) == 0) {
			if(file_handler[fd].path[1] == ':') {
				// NOTE: we need to consider the network file path "\\host\share\"
				char volume[] = "A:\\";
				volume[0] = file_handler[fd].path[1];
				GetVolumeInformation(volume, NULL, 0, &serial_number, NULL, NULL, NULL, 0);
			}
			*(UINT32 *)(buffer + 0x00) = GetFileAttributes(file_handler[fd].path);
			*(UINT32 *)(buffer + 0x04) = (UINT32)(status.st_ctime & 0xffffffff);
			*(UINT32 *)(buffer + 0x08) = (UINT32)((status.st_ctime >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x0c) = (UINT32)(status.st_atime & 0xffffffff);
			*(UINT32 *)(buffer + 0x10) = (UINT32)((status.st_atime >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x14) = (UINT32)(status.st_mtime & 0xffffffff);
			*(UINT32 *)(buffer + 0x18) = (UINT32)((status.st_mtime >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x1c) = serial_number;
			*(UINT32 *)(buffer + 0x20) = (UINT32)((status.st_size >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x24) = (UINT32)(status.st_size & 0xffffffff);
			*(UINT32 *)(buffer + 0x28) = status.st_nlink;
			// this is dummy id and it will be changed when it is reopened...
			*(UINT32 *)(buffer + 0x2c) = 0;
			*(UINT32 *)(buffer + 0x30) = file_handler[fd].id;
		} else {
			REG16(AX) = errno;
			m_CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		m_CF = 1;
	}
}

inline void msdos_int_21h_71a7h()
{
	switch(REG8(BL)) {
	case 0x00:
		if(!FileTimeToDosDateTime((FILETIME *)(mem + SREG_BASE(DS) + REG16(SI)), &REG16(DX), &REG16(CX))) {
			REG16(AX) = (UINT16)GetLastError();
			m_CF = 1;
		}
		break;
	case 0x01:
		// NOTE: we need to check BH that shows 10-millisecond untils past time in CX
		if(!DosDateTimeToFileTime(REG16(DX), REG16(CX), (FILETIME *)(mem + SREG_BASE(ES) + REG16(DI)))) {
			REG16(AX) = (UINT16)GetLastError();
			m_CF = 1;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_71a8h()
{
	if(REG8(DH) == 0) {
		char tmp[MAX_PATH], fcb[MAX_PATH];
		strcpy(tmp, msdos_short_path((char *)(mem + SREG_BASE(DS) + REG16(SI))));
		memset(fcb, 0x20, sizeof(fcb));
		int len = strlen(tmp);
		for(int i = 0, pos = 0; i < len; i++) {
			if(tmp[i] == '.') {
				pos = 8;
			} else {
				if(msdos_lead_byte_check(tmp[i])) {
					fcb[pos++] = tmp[i++];
				}
				fcb[pos++] = tmp[i];
			}
		}
		memcpy((char *)(mem + SREG_BASE(ES) + REG16(DI)), fcb, 11);
	} else {
		strcpy((char *)(mem + SREG_BASE(ES) + REG16(DI)), msdos_short_path((char *)(mem + SREG_BASE(DS) + REG16(SI))));
	}
}

inline void msdos_int_21h_71aah()
{
	char drv[] = "A:", path[MAX_PATH];
	char *hoge=(char *)(mem + SREG_BASE(DS) + REG16(DX));
	
	if(REG8(BL) == 0) {
		drv[0] = 'A' + _getdrive() - 1;
	} else {
		drv[0] = 'A' + REG8(BL) - 1;
	}
	switch(REG8(BH)) {
	case 0x00:
		if(DefineDosDevice(0, drv, (char *)(mem + SREG_BASE(DS) + REG16(DX))) == 0) {
			DWORD bits = 1 << ((REG8(BL) ? REG8(BL) : _getdrive()) - 1);
			if(GetLogicalDrives() & bits) {
				REG16(AX) = 0x0f; // invalid drive
			} else {
				REG16(AX) = 0x03; // path not found
			}
			m_CF = 1;
		}
		break;
	case 0x01:
		if(DefineDosDevice(DDD_REMOVE_DEFINITION, drv, NULL) == 0) {
			REG16(AX) = 0x0f; // invalid drive
			m_CF = 1;
		}
		break;
	case 0x02:
		if(QueryDosDevice(drv, path, MAX_PATH) == 0) {
			REG16(AX) = 0x0f; // invalid drive
			m_CF = 1;
		} else if(strncmp(path, "\\??\\", 4) != 0) {
			REG16(AX) = 0x0f; // invalid drive
			m_CF = 1;
		} else {
			strcpy((char *)(mem + SREG_BASE(DS) + REG16(DX)), path + 4);
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_21h_7300h()
{
	if(REG8(AL) == 0) {
		REG8(AL) = REG8(CL);
		REG8(AH) = 0;
	} else {
		REG16(AX) = 0x01;
		m_CF = 1;
	}
}

inline void msdos_int_21h_7302h()
{
	int drive_num = (REG8(DL) == 0) ? (_getdrive() - 1) : (REG8(DL) - 1);
	UINT16 seg, ofs;
	
	if(REG16(CX) < 0x3f) {
		REG8(AL) = 0x18;
		m_CF = 1;
	} else if(!msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		REG8(AL) = 0xff;
		m_CF = 1;
	} else {
		memcpy(mem + SREG_BASE(ES) + REG16(DI) + 2, mem + (seg << 4) + ofs, sizeof(dpb_t));
	}
}

inline void msdos_int_21h_7303h()
{
	char *path = (char *)(mem + SREG_BASE(DS) + REG16(DX));
	ext_space_info_t *info = (ext_space_info_t *)(mem + SREG_BASE(ES) + REG16(DI));
	DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
	
	if(GetDiskFreeSpace(path, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters)) {
		info->size_of_structure = sizeof(ext_space_info_t);
		info->structure_version = 0;
		info->sectors_per_cluster = sectors_per_cluster;
		info->bytes_per_sector = bytes_per_sector;
		info->available_clusters_on_drive = free_clusters;
		info->total_clusters_on_drive = total_clusters;
		info->available_sectors_on_drive = sectors_per_cluster * free_clusters;
		info->total_sectors_on_drive = sectors_per_cluster * total_clusters;
		info->available_allocation_units = free_clusters;	// ???
		info->total_allocation_units = total_clusters;		// ???
	} else {
		REG16(AX) = errno;
		m_CF = 1;
	}
}

inline void msdos_int_21h_dbh()
{
	// Novell NetWare - Workstation - Get Number of Local Drives
	dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
	REG8(AL) = dos_info->last_drive;
}

inline void msdos_int_21h_dch()
{
	// Novell NetWare - Connection Services - Get Connection Number
	REG8(AL) = 0x00;
}

inline void msdos_int_25h()
{
	UINT16 seg, ofs;
	DWORD dwSize;
	
#if defined(HAS_I386)
	I386OP(pushf)();
#else
	PREFIX86(_pushf());
#endif
	
	if(!(REG8(AL) < 26)) {
		REG8(AL) = 0x01; // unit unknown
		m_CF = 1;
	} else if(!msdos_drive_param_block_update(REG8(AL), &seg, &ofs, 0)) {
		REG8(AL) = 0x02; // drive not ready
		m_CF = 1;
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + REG8(AL));
		
		HANDLE hFile = CreateFile(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			REG8(AL) = 0x02; // drive not ready
			m_CF = 1;
		} else {
			UINT32 top_sector  = REG16(DX);
			UINT16 sector_num  = REG16(CX);
			UINT32 buffer_addr = SREG_BASE(DS) + REG16(BX);
			
			if(sector_num == 0xffff) {
				top_sector  = *(UINT32 *)(mem + buffer_addr + 0);
				sector_num  = *(UINT16 *)(mem + buffer_addr + 4);
				UINT16 ofs  = *(UINT16 *)(mem + buffer_addr + 6);
				UINT16 seg  = *(UINT16 *)(mem + buffer_addr + 8);
				buffer_addr = (seg << 4) + ofs;
			}
//			if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
//				REG8(AL) = 0x02; // drive not ready
//				m_CF = 1;
//			} else 
			if(SetFilePointer(hFile, top_sector * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				REG8(AL) = 0x08; // sector not found
				m_CF = 1;
			} else if(ReadFile(hFile, mem + buffer_addr, sector_num * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
				REG8(AL) = 0x0b; // read error
				m_CF = 1;
			}
			CloseHandle(hFile);
		}
	}
}

inline void msdos_int_26h()
{
	// this operation may cause serious damage for drives, so always returns error...
	UINT16 seg, ofs;
	DWORD dwSize;
	
#if defined(HAS_I386)
	I386OP(pushf)();
#else
	PREFIX86(_pushf());
#endif
	
	if(!(REG8(AL) < 26)) {
		REG8(AL) = 0x01; // unit unknown
		m_CF = 1;
	} else if(!msdos_drive_param_block_update(REG8(AL), &seg, &ofs, 0)) {
		REG8(AL) = 0x02; // drive not ready
		m_CF = 1;
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + REG8(AL));
		
		if(dpb->media_type == 0xf8) {
			// this drive is not a floppy
//			if(!(((dos_info_t *)(mem + DOS_INFO_TOP))->dos_flag & 0x40)) {
//				fatalerror("This application tried the absolute disk write to drive %c:\n", 'A' + REG8(AL));
//			}
			REG8(AL) = 0x02; // drive not ready
			m_CF = 1;
		} else {
			HANDLE hFile = CreateFile(dev, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
			if(hFile == INVALID_HANDLE_VALUE) {
				REG8(AL) = 0x02; // drive not ready
				m_CF = 1;
			} else {
				UINT32 top_sector  = REG16(DX);
				UINT16 sector_num  = REG16(CX);
				UINT32 buffer_addr = SREG_BASE(DS) + REG16(BX);
				
				if(sector_num == 0xffff) {
					top_sector  = *(UINT32 *)(mem + buffer_addr + 0);
					sector_num  = *(UINT16 *)(mem + buffer_addr + 4);
					UINT16 ofs  = *(UINT16 *)(mem + buffer_addr + 6);
					UINT16 seg  = *(UINT16 *)(mem + buffer_addr + 8);
					buffer_addr = (seg << 4) + ofs;
				}
				if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
					REG8(AL) = 0x02; // drive not ready
					m_CF = 1;
				} else if(SetFilePointer(hFile, top_sector * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
					REG8(AL) = 0x08; // sector not found
					m_CF = 1;
				} else if(WriteFile(hFile, mem + buffer_addr, sector_num * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
					REG8(AL) = 0x0a; // write error
					m_CF = 1;
				}
				CloseHandle(hFile);
			}
		}
	}
}

inline void msdos_int_27h()
{
	int paragraphs = (min(REG16(DX), 0xfff0) + 15) >> 4;
	try {
		msdos_mem_realloc(SREG(CS), paragraphs, NULL);
	} catch(...) {
		// recover the broken mcb
		int mcb_seg = SREG(CS) - 1;
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		if(mcb_seg < (MEMORY_END >> 4)) {
			if(((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked & 0x01) {
				mcb->mz = 'M';
			} else {
				mcb->mz = 'Z';
			}
			mcb->paragraphs = (MEMORY_END >> 4) - mcb_seg - 2;
			msdos_mcb_create((MEMORY_END >> 4) - 1, 'M', -1, (UMB_TOP >> 4) - (MEMORY_END >> 4));
		} else {
			mcb->mz = 'Z';
			mcb->paragraphs = (UMB_END >> 4) - mcb_seg - 1;
		}
		msdos_mem_realloc(SREG(CS), paragraphs, NULL);
	}
	msdos_process_terminate(SREG(CS), retval | 0x300, 0);
}

inline void msdos_int_29h()
{
#if 1
	// need to check escape sequences
	msdos_putch(REG8(AL));
#else
	DWORD num;
	vram_flush();
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), &REG8(AL), 1, &num, NULL);
	cursor_moved = true;
#endif
}

inline void msdos_int_2eh()
{
	char tmp[MAX_PATH], command[MAX_PATH], opt[MAX_PATH];
	memset(tmp, 0, sizeof(tmp));
	strcpy(tmp, (char *)(mem + SREG_BASE(DS) + REG16(SI)));
	char *token = my_strtok(tmp, " ");
	strcpy(command, token);
	strcpy(opt, token + strlen(token) + 1);
	
	param_block_t *param = (param_block_t *)(mem + WORK_TOP);
	param->env_seg = 0;
	param->cmd_line.w.l = 44;
	param->cmd_line.w.h = (WORK_TOP >> 4);
	param->fcb1.w.l = 24;
	param->fcb1.w.h = (WORK_TOP >> 4);
	param->fcb2.w.l = 24;
	param->fcb2.w.h = (WORK_TOP >> 4);
	
	memset(mem + WORK_TOP + 24, 0x20, 20);
	
	cmd_line_t *cmd_line = (cmd_line_t *)(mem + WORK_TOP + 44);
	cmd_line->len = strlen(opt);
	strcpy(cmd_line->cmd, opt);
	cmd_line->cmd[cmd_line->len] = 0x0d;
	
	try {
		if(msdos_process_exec(command, param, 0)) {
			REG16(AX) = 0xffff; // error before processing command
		} else {
			// set flag to set retval to ax when the started process is terminated
			process_t *process = msdos_process_info_get(current_psp);
			process->called_by_int2eh = true;
		}
	} catch(...) {
		REG16(AX) = 0xffff; // error before processing command
	}
}

inline void msdos_int_2fh_05h()
{
	switch(REG8(AL)) {
	case 0x00:
		// Critical Error Handler is not installed
//		REG8(AL) = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		// error code can't be converted to string
//		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_11h()
{
	switch(REG8(AL)) {
	case 0x00:
		if(i386_read_stack() == 0xdada) {
			// MSCDEX is not installed
//			REG8(AL) = 0x00;
		} else {
			// Network Redirector is not installed
//			REG8(AL) = 0x00;
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x49; //  network software not installed
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_12h()
{
	switch(REG8(AL)) {
	case 0x00:
		// DOS 3.0+ internal functions are installed
		REG8(AL) = 0xff;
		break;
//	case 0x01: // DOS 3.0+ internal - Close Current File
	case 0x02:
		{
			UINT16 stack = i386_read_stack();
			REG16(BX) = *(UINT16 *)(mem + 4 * stack + 0);
			SREG(ES) = *(UINT16 *)(mem + 4 * stack + 2);
			i386_load_segment_descriptor(ES);
		}
		break;
	case 0x03:
		SREG(DS) = (DEVICE_TOP >> 4);
		i386_load_segment_descriptor(DS);
		break;
	case 0x04:
		{
			UINT16 stack = i386_read_stack();
			REG8(AL) = (stack == '/') ? '\\' : stack;
#if defined(HAS_I386)
			m_ZF = (REG8(AL) == '\\');
#else
			m_ZeroVal = (REG8(AL) != '\\');
#endif
		}
		break;
	case 0x05:
		msdos_putch(i386_read_stack());
		break;
//	case 0x06: // DOS 3.0+ internal - Invole Critical Error
//	case 0x07: // DOS 3.0+ internal - Make Disk Buffer Most Recentry Used
//	case 0x08: // DOS 3.0+ internal - Decrement SFT Reference Count
//	case 0x09: // DOS 3.0+ internal - Flush and FREE Disk Buffer
//	case 0x0a: // DOS 3.0+ internal - Perform Critical Error Interrupt
//	case 0x0b: // DOS 3.0+ internal - Signal Sharing Violation to User
//	case 0x0c: // DOS 3.0+ internal - Open Device and Set SFT Owner/Mode
	case 0x0d:
		{
			SYSTEMTIME time;
			FILETIME file_time;
			WORD dos_date, dos_time;
			GetLocalTime(&time);
			SystemTimeToFileTime(&time, &file_time);
			FileTimeToDosDateTime(&file_time, &dos_date, &dos_time);
			REG16(AX) = dos_date;
			REG16(DX) = dos_time;
		}
		break;
//	case 0x0e: // DOS 3.0+ internal - Mark All Disk Buffers Unreferenced
//	case 0x0f: // DOS 3.0+ internal - Make Buffer Most Recentry Used
//	case 0x10: // DOS 3.0+ internal - Find Unreferenced Disk Buffer
	case 0x11:
		{
			char path[MAX_PATH], *p;
			strcpy(path, (char *)(mem + SREG_BASE(DS) + REG16(SI)));
			my_strupr(path);
			while((p = my_strchr(path, '/')) != NULL) {
				*p = '\\';
			}
			strcpy((char *)(mem + SREG_BASE(ES) + REG16(DI)), path);
		}
		break;
	case 0x12:
		REG16(CX) = strlen((char *)(mem + SREG_BASE(ES) + REG16(DI)));
		break;
	case 0x13:
		{
			char tmp[2] = {0};
			tmp[0] = i386_read_stack();
			my_strupr(tmp);
			REG8(AL) = tmp[0];
		}
		break;
	case 0x14:
#if defined(HAS_I386)
		m_ZF = (SREG_BASE(DS) + REG16(SI) == SREG_BASE(ES) + REG16(DI));
#else
		m_ZeroVal = (SREG_BASE(DS) + REG16(SI) != SREG_BASE(ES) + REG16(DI));
#endif
		m_CF = (SREG_BASE(DS) + REG16(SI) != SREG_BASE(ES) + REG16(DI));
		break;
//	case 0x15: // DOS 3.0+ internal - Flush Buffer
	case 0x16:
		if(REG16(BX) < 20) {
			SREG(ES) = SFT_TOP >> 4;
			i386_load_segment_descriptor(ES);
			REG16(DI) = 6 + 0x3b * REG16(BX);
			
			// update system file table
			UINT8* sft = mem + SFT_TOP + 6 + 0x3b * REG16(BX);
			if(file_handler[REG16(BX)].valid) {
				int count = 0;
				for(int i = 0; i < 20; i++) {
					if(msdos_psp_get_file_table(i, current_psp) == REG16(BX)) {
						count++;
					}
				}
				*(UINT16 *)(sft + 0x00) = count ? count : 0xffff;
				*(UINT32 *)(sft + 0x15) = _tell(REG16(BX));
				_lseek(REG16(BX), 0, SEEK_END);
				*(UINT32 *)(sft + 0x11) = _tell(REG16(BX));
				_lseek(REG16(BX), *(UINT32 *)(sft + 0x15), SEEK_SET);
			} else {
				memset(sft, 0, 0x3b);
			}
		} else {
			REG16(AX) = 0x06;
			m_CF = 1;
		}
		break;
//	case 0x17: // DOS 3.0+ internal - Get Current Directory Structure for Drive
//	case 0x18: // DOS 3.0+ internal - Get Caller's Registers
//	case 0x19: // DOS 3.0+ internal - Set Drive???
	case 0x1a:
		{
			char *path = (char *)(mem + SREG_BASE(DS) + REG16(SI)), full[MAX_PATH];
			if(path[1] == ':') {
				if(path[0] >= 'a' && path[0] <= 'z') {
					REG8(AL) = path[0] - 'a' + 1;
				} else if(path[0] >= 'A' && path[0] <= 'Z') {
					REG8(AL) = path[0] - 'A' + 1;
				} else {
					REG8(AL) = 0xff; // invalid
				}
				strcpy(full, path);
				strcpy(path, full + 2);
			} else if(GetFullPathName(path, MAX_PATH, full, NULL) != 0 && full[1] == ':') {
				if(full[0] >= 'a' && full[0] <= 'z') {
					REG8(AL) = full[0] - 'a' + 1;
				} else if(full[0] >= 'A' && full[0] <= 'Z') {
					REG8(AL) = full[0] - 'A' + 1;
				} else {
					REG8(AL) = 0xff; // invalid
				}
			} else {
				REG8(AL) = 0x00; // default
			}
		}
		break;
	case 0x1b:
		{
			int year = REG16(CX) + 1980;
			REG8(AL) = ((year % 4) == 0 && (year % 100) != 0 && (year % 400) == 0) ? 29 : 28;
		}
		break;
//	case 0x1c: // DOS 3.0+ internal - Check Sum Memory
//	case 0x1d: // DOS 3.0+ internal - Sum Memory
	case 0x1e:
		{
			char *path_1st = (char *)(mem + SREG_BASE(DS) + REG16(SI)), full_1st[MAX_PATH];
			char *path_2nd = (char *)(mem + SREG_BASE(ES) + REG16(DI)), full_2nd[MAX_PATH];
			if(GetFullPathName(path_1st, MAX_PATH, full_1st, NULL) != 0 && GetFullPathName(path_2nd, MAX_PATH, full_2nd, NULL) != 0) {
#if defined(HAS_I386)
				m_ZF = (strcmp(full_1st, full_2nd) == 0);
#else
				m_ZeroVal = (strcmp(full_1st, full_2nd) != 0);
#endif
			} else {
#if defined(HAS_I386)
				m_ZF = (strcmp(path_1st, path_2nd) == 0);
#else
				m_ZeroVal = (strcmp(path_1st, path_2nd) != 0);
#endif
			}
		}
		break;
//	case 0x1f: // DOS 3.0+ internal - Build Current Directory Structure
	case 0x20:
		{
			int fd = msdos_psp_get_file_table(REG16(BX), current_psp);
			
			if(fd < 20) {
				SREG(ES) = current_psp;
				i386_load_segment_descriptor(ES);
				REG16(DI) = offsetof(psp_t, file_table) + fd;
			} else {
				REG16(AX) = 0x06;
				m_CF = 1;
			}
		}
		break;
	case 0x21:
		msdos_int_21h_60h(0);
		break;
//	case 0x22: // DOS 3.0+ internal - Set Extended Error Info
//	case 0x23: // DOS 3.0+ internal - Check If Character Device
//	case 0x24: // DOS 3.0+ internal - Sharing Retry Delay
	case 0x25:
		REG16(CX) = strlen((char *)(mem + SREG_BASE(DS) + REG16(SI)));
		break;
	case 0x26:
		REG8(AL) = REG8(CL);
		msdos_int_21h_3dh();
		break;
	case 0x27:
		msdos_int_21h_3eh();
		break;
	case 0x28:
		REG16(AX) = REG16(BP);
		msdos_int_21h_42h();
		break;
	case 0x29:
		msdos_int_21h_3fh();
		break;
//	case 0x2a: // DOS 3.0+ internal - Set Fast Open Entry Point
	case 0x2b:
		REG16(AX) = REG16(BP);
		msdos_int_21h_44h();
		break;
	case 0x2c:
		REG16(BX) = DEVICE_TOP >> 4;
		REG16(AX) = 22;
		break;
	case 0x2d:
		{
			sda_t *sda = (sda_t *)(mem + SDA_TOP);
			REG16(AX) = sda->extended_error_code;
		}
		break;
	case 0x2e:
		if(REG8(DL) == 0x00 || REG8(DL) == 0x02 || REG8(DL) == 0x04 || REG8(DL) == 0x06) {
			SREG(ES) = ERR_TABLE_TOP >> 4;
			i386_load_segment_descriptor(ES);
			REG16(DI) = 0;
		}
		break;
	case 0x2f:
		if(REG16(DX) != 0) {
			dos_major_version = REG8(DL);
			dos_minor_version = REG8(DH);
		} else {
			REG8(DL) = 7;
			REG8(DH) = 10;
		}
		break;
//	case 0x30: // Windows95 - Find SFT Entry in Internal File Tables
//	case 0x31: // Windows95 - Set/Clear "Report Windows to DOS Programs" Flag
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_13h()
{
	static UINT16 prevDS = 0, prevDX = 0;
	static UINT16 prevES = 0, prevBX = 0;
	UINT16 tmp;
	
	tmp = SREG(DS); SREG(DS) = prevDS; prevDS = tmp;
	i386_load_segment_descriptor(DS);
	tmp = REG16(DX); REG16(DX) = prevDX; prevDX = tmp;
	
	tmp = SREG(ES); SREG(ES) = prevES; prevES = tmp;
	i386_load_segment_descriptor(ES);
	tmp = REG16(BX); REG16(BX) = prevBX; prevBX = tmp;
}

inline void msdos_int_2fh_14h()
{
	switch(REG8(AL)) {
	case 0x00:
		// NLSFUNC.COM is installed
		REG8(AL) = 0xff;
		break;
	case 0x01:
	case 0x03:
		REG8(AL) = 0x00;
		active_code_page = REG16(BX);
		msdos_nls_tables_update();
		break;
	case 0x02:
		REG8(AL) = get_extended_country_info(REG16(BP));
		break;
	case 0x04:
		REG8(AL) = 0x00;
		get_country_info((country_info_t *)(mem + SREG_BASE(ES) + REG16(DI)));
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_15h()
{
	switch(REG8(AL)) {
	case 0x00: // CD-ROM - Installation Check
		if(REG16(BX) == 0x0000) {
			// MSCDEX is not installed
//			REG8(AL) = 0x00;
		} else {
			// GRAPHICS.COM is not installed
//			REG8(AL) = 0x00;
		}
		break;
	case 0xff:
		if(REG16(BX) == 0x0000) {
			// CORELCDX is not installed
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_16h()
{
	switch(REG8(AL)) {
	case 0x00:
		if(no_windows) {
			// neither Windows 3.x enhanced mode nor Windows/386 2.x running
//			REG8(AL) = 0x00;
		} else {
			REG8(AL) = win_major_version;
			REG8(AH) = win_minor_version;
		}
		break;
	case 0x05:
		// from DOSBox
		i386_set_a20_line(1);
		break;
	case 0x0a:
		if(!no_windows) {
			REG16(AX) = 0x0000;
			REG8(BH) = win_major_version;
			REG8(BL) = win_minor_version;
			REG16(CX) = 0x0003; // enhanced
		}
		break;
	case 0x0b:
		// no TRS, keep ES:DI = 0000h:0000h
	case 0x0e:
	case 0x0f:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x87:
	case 0x89:
		// function not supported, do not clear AX
		break;
	case 0x80:
		Sleep(10);
		hardware_update();
		REG8(AL) = 0x00;
		break;
	case 0x8e:
		REG16(AX) = 0x00; // failed
		break;
	case 0x8f:
		switch(REG8(DH)) {
		case 0x00:
		case 0x02:
		case 0x03:
			REG16(AX) = 0x00;
			break;
		case 0x01:
			REG16(AX) = 0x168f;
			break;
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_19h()
{
	switch(REG8(AL)) {
	case 0x00:
		// SHELLB.COM is not installed
//		REG8(AL) = 0x00;
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	case 0x80:
		// IBM ROM-DOS v4.0 is not installed
//		REG8(AL) = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_1ah()
{
	switch(REG8(AL)) {
	case 0x00:
		// ANSI.SYS is installed
		REG8(AL) = 0xff;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_40h()
{
	switch(REG8(AL)) {
	case 0x00:
		// Windows 3+ - Get Virtual Device Driver (VDD) Capabilities
		REG8(AL) = 0x01; // does not virtualize video access
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_43h()
{
	switch(REG8(AL)) {
	case 0x00:
		// XMS is installed ?
#ifdef SUPPORT_XMS
		if(support_xms) {
			REG8(AL) = 0x80;
		} else
#endif
		REG8(AL) = 0x00;
		break;
	case 0x10:
		SREG(ES) = XMS_TOP >> 4;
		i386_load_segment_descriptor(ES);
		REG16(BX) = 0x15;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_46h()
{
	switch(REG8(AL)) {
	case 0x80:
		// Windows v3.0 is not installed
//		REG8(AL) = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_48h()
{
	switch(REG8(AL)) {
	case 0x00:
		// DOSKEY is not installed
//		REG8(AL) = 0x00;
		break;
	case 0x10:
		msdos_int_21h_0ah();
		REG16(AX) = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_4ah()
{
	switch(REG8(AL)) {
#ifdef SUPPORT_HMA
	case 0x01: // DOS 5.0+ - Query Free HMA Space
		if(!is_hma_used_by_xms && !is_hma_used_by_int_2fh) {
			if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
				// restore first free mcb in high memory area
				msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
			}
			int offset = 0xffff;
			if((REG16(BX) = msdos_hma_mem_get_free(&offset)) != 0) {
				REG16(DI) = offset + 0x10;
			} else {
				REG16(DI) = 0xffff;
			}
		} else {
			// HMA is already used
			REG16(BX) = 0;
			REG16(DI) = 0xffff;
		}
		SREG(ES) = 0xffff;
		i386_load_segment_descriptor(ES);
		break;
	case 0x02: // DOS 5.0+ - Allocate HMA Space
		if(!is_hma_used_by_xms && !is_hma_used_by_int_2fh) {
			if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
				// restore first free mcb in high memory area
				msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
			}
			int size = REG16(BX), offset;
			if((size % 16) != 0) {
				size &= ~15;
				size += 16;
			}
			if((offset = msdos_hma_mem_alloc(size, current_psp)) != -1) {
				REG16(BX) = size;
				REG16(DI) = offset + 0x10;
				is_hma_used_by_int_2fh = true;
			} else {
				REG16(BX) = 0;
				REG16(DI) = 0xffff;
			}
		} else {
			// HMA is already used
			REG16(BX) = 0;
			REG16(DI) = 0xffff;
		}
		SREG(ES) = 0xffff;
		i386_load_segment_descriptor(ES);
		break;
	case 0x03: // Windows95 - (De)Allocate HMA Memory Block
		if(REG8(DL) == 0x00) {
			if(!is_hma_used_by_xms) {
				if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
					// restore first free mcb in high memory area
					msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
					is_hma_used_by_int_2fh = false;
				}
				int size = REG16(BX), offset;
				if((size % 16) != 0) {
					size &= ~15;
					size += 16;
				}
				if((offset = msdos_hma_mem_alloc(size, REG16(CX))) != -1) {
//					REG16(BX) = size;
					SREG(ES) = 0xffff;
					i386_load_segment_descriptor(ES);
					REG16(DI) = offset + 0x10;
					is_hma_used_by_int_2fh = true;
				} else {
					REG16(DI) = 0xffff;
				}
			} else {
				REG16(DI) = 0xffff;
			}
		} else if(REG8(DL) == 0x01) {
			if(!is_hma_used_by_xms) {
				int size = REG16(BX);
				if((size % 16) != 0) {
					size &= ~15;
					size += 16;
				}
				if(msdos_hma_mem_realloc(SREG_BASE(ES) + REG16(DI) - 0xffff0 - 0x10, size) != -1) {
					// memory block address is not changed
				} else {
					REG16(DI) = 0xffff;
				}
			} else {
				REG16(DI) = 0xffff;
			}
		} else if(REG8(DL) == 0x02) {
			if(!is_hma_used_by_xms) {
				if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
					// restore first free mcb in high memory area
					msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
					is_hma_used_by_int_2fh = false;
				} else {
					msdos_hma_mem_free(SREG_BASE(ES) + REG16(DI) - 0xffff0 - 0x10);
					if(msdos_hma_mem_get_free(NULL) == 0xffe0) {
						is_hma_used_by_int_2fh = false;
					}
				}
			}
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
		}
		break;
	case 0x04: // Windows95 - Get Start of HMA Memory Chain
		if(!is_hma_used_by_xms) {
			if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
				// restore first free mcb in high memory area
				msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
				is_hma_used_by_int_2fh = false;
			}
			REG16(AX) = 0x0000;
			SREG(ES) = 0xffff;
			i386_load_segment_descriptor(ES);
			REG16(DI) = 0x10;
		}
		break;
#else
	case 0x01:
	case 0x02:
		// HMA is already used
		REG16(BX) = 0x0000;
		SREG(ES) = 0xffff;
		i386_load_segment_descriptor(ES);
		REG16(DI) = 0xffff;
		break;
	case 0x03:
		// unable to allocate
		REG16(DI) = 0xffff;
		break;
	case 0x04:
		// function not supported, do not clear AX
		break;
#endif
	case 0x10:
		if(REG16(BX) == 0x0000) {
			// SMARTDRV is not installed
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
		}
		break;
	case 0x11:
		if(REG16(BX) == 0x0000) {
			// DBLSPACE.BIN is not installed
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_4bh()
{
	switch(REG8(AL)) {
	case 0x01:
	case 0x02:
		// Task Switcher is not installed
		break;
	case 0x03:
		// this call is available from within DOSSHELL even if the task switcher is not installed
		REG16(AX) = REG16(BX) = 0x0000; // no more avaiable switcher id
		break;
	case 0x04:
		REG16(BX) = 0x0000; // free switcher id successfully
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_4fh()
{
	switch(REG8(AL)) {
	case 0x00:
		// BILING is installed
		REG16(AX) = 0x0000;
		REG8(DL) = 0x01;	// major version
		REG8(DH) = 0x00;	// minor version
		break;
	case 0x01:
		REG16(AX) = 0x0000;
		REG16(BX) = active_code_page;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_55h()
{
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
//		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_80h()
{
	switch(REG8(AL)) {
	case 0x00:
		if(REG16(DX) == 0x0000) {
			// FAX BIOS is not installed
//			REG8(AL) = 0x00;
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG16(AX) = 0x01;
			m_CF = 1;
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_adh()
{
	switch(REG8(AL)) {
	case 0x00:
		// DISPLAY.SYS is installed
		REG8(AL) = 0xff;
		REG16(BX) = 0x100; // ???
		break;
	case 0x01:
		active_code_page = REG16(BX);
		msdos_nls_tables_update();
		REG16(AX) = 0x01;
		break;
	case 0x02:
		REG16(BX) = active_code_page;
		break;
	case 0x03:
		// FIXME
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 0) = 1;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 2) = 3;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4) = 1;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 6) = active_code_page;
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 8) = active_code_page;
		break;
	case 0x80:
		break; // keyb.com is not installed
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_aeh()
{
	switch(REG8(AL)) {
	case 0x00:
		// FIXME: we need to check the given command line
		REG8(AL) = 0x00; // the command should be executed as usual
//		REG8(AL) = 0xff; // this command is a TSR extension to COMMAND.COM
		break;
	case 0x01:
		{
			char command[MAX_PATH];
			memset(command, 0, sizeof(command));
			memcpy(command, mem + SREG_BASE(DS) + REG16(SI) + 1, mem[SREG_BASE(DS) + REG16(SI)]);
			
			param_block_t *param = (param_block_t *)(mem + WORK_TOP);
			param->env_seg = 0;
			param->cmd_line.w.l = 44;
			param->cmd_line.w.h = (WORK_TOP >> 4);
			param->fcb1.w.l = 24;
			param->fcb1.w.h = (WORK_TOP >> 4);
			param->fcb2.w.l = 24;
			param->fcb2.w.h = (WORK_TOP >> 4);
			
			memset(mem + WORK_TOP + 24, 0x20, 20);
			
			cmd_line_t *cmd_line = (cmd_line_t *)(mem + WORK_TOP + 44);
			cmd_line->len = mem[SREG_BASE(DS) + REG16(BX) + 1];
			memcpy(cmd_line->cmd, mem + SREG_BASE(DS) + REG16(BX) + 2, cmd_line->len);
			cmd_line->cmd[cmd_line->len] = 0x0d;
			
			try {
				msdos_process_exec(command, param, 0);
			} catch(...) {
				fatalerror("failed to start '%s' by int 2Fh, AX=AE01h\n", command);
			}
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_b7h()
{
	switch(REG8(AL)) {
	case 0x00:
		// APPEND is not installed
//		REG8(AL) = 0x00;
		break;
	case 0x07:
	case 0x11:
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_2fh_d7h()
{
	switch(REG8(AL)) {
	case 0x01:
		// Banyan VINES v4+ is not installed
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG16(AX) = 0x01;
		m_CF = 1;
		break;
	}
}

inline void msdos_int_33h_0000h()
{
	REG16(AX) = 0xffff; // hardware/driver installed
	REG16(BX) = MAX_MOUSE_BUTTONS;
}

inline void msdos_int_33h_0001h()
{
	if(!mouse.active) {
		if(!(dwConsoleMode & ENABLE_MOUSE_INPUT)) {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode | ENABLE_MOUSE_INPUT);
		}
		mouse.active = true;
		pic[1].imr &= ~0x10;	// enable irq12
	}
}

inline void msdos_int_33h_0002h()
{
	if(mouse.active) {
		if(!(dwConsoleMode & ENABLE_MOUSE_INPUT)) {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode);
		}
		mouse.active = false;
		pic[1].imr |= 0x10;	// disable irq12
	}
}

inline void msdos_int_33h_0003h()
{
	REG16(BX) = mouse.get_buttons();
	REG16(CX) = max(mouse.min_position.x, min(mouse.max_position.x, mouse.position.x));
	REG16(DX) = max(mouse.min_position.y, min(mouse.max_position.y, mouse.position.y));
}

inline void msdos_int_33h_0005h()
{
	if(REG16(BX) < MAX_MOUSE_BUTTONS) {
		int idx = REG16(BX);
		REG16(BX) = mouse.buttons[idx].pressed_times;
		REG16(CX) = max(mouse.min_position.x, min(mouse.max_position.x, mouse.buttons[idx].pressed_position.x));
		REG16(DX) = max(mouse.min_position.y, min(mouse.max_position.y, mouse.buttons[idx].pressed_position.y));
		mouse.buttons[idx].pressed_times = 0;
	} else {
		REG16(BX) = REG16(CX) = REG16(DX) = 0x0000;
	}
	REG16(AX) = mouse.get_buttons();
}

inline void msdos_int_33h_0006h()
{
	if(REG16(BX) < MAX_MOUSE_BUTTONS) {
		int idx = REG16(BX);
		REG16(BX) = mouse.buttons[idx].released_times;
		REG16(CX) = max(mouse.min_position.x, min(mouse.max_position.x, mouse.buttons[idx].released_position.x));
		REG16(DX) = max(mouse.min_position.y, min(mouse.max_position.y, mouse.buttons[idx].released_position.y));
		mouse.buttons[idx].released_times = 0;
	} else {
		REG16(BX) = REG16(CX) = REG16(DX) = 0x0000;
	}
	REG16(AX) = mouse.get_buttons();
}

inline void msdos_int_33h_0007h()
{
	mouse.min_position.x = min(REG16(CX), REG16(DX));
	mouse.max_position.x = max(REG16(CX), REG16(DX));
}

inline void msdos_int_33h_0008h()
{
	mouse.min_position.y = min(REG16(CX), REG16(DX));
	mouse.max_position.y = max(REG16(CX), REG16(DX));
}

inline void msdos_int_33h_0009h()
{
	mouse.hot_spot[0] = REG16(BX);
	mouse.hot_spot[1] = REG16(CX);
}

inline void msdos_int_33h_000bh()
{
	int dx = (mouse.position.x - mouse.prev_position.x) * mouse.mickey.x / 8;
	int dy = (mouse.position.y - mouse.prev_position.y) * mouse.mickey.y / 8;
	mouse.prev_position.x = mouse.position.x;
	mouse.prev_position.y = mouse.position.y;
	REG16(CX) = dx;
	REG16(DX) = dy;
}

inline void msdos_int_33h_000ch()
{
	mouse.call_mask = REG16(CX);
	mouse.call_addr.w.l = REG16(DX);
	mouse.call_addr.w.h = SREG(ES);
}

inline void msdos_int_33h_000fh()
{
	mouse.mickey.x = REG16(CX);
	mouse.mickey.y = REG16(DX);
}

inline void msdos_int_33h_0011h()
{
	REG16(AX) = 0xffff;
	REG16(BX) = MAX_MOUSE_BUTTONS;
}

inline void msdos_int_33h_0014h()
{
	UINT16 old_mask = mouse.call_mask;
	UINT16 old_ofs = mouse.call_addr.w.l;
	UINT16 old_seg = mouse.call_addr.w.h;
	
	mouse.call_mask = REG16(CX);
	mouse.call_addr.w.l = REG16(DX);
	mouse.call_addr.w.h = SREG(ES);
	
	REG16(CX) = old_mask;
	REG16(DX) = old_ofs;
	SREG(ES) = old_seg;
	i386_load_segment_descriptor(ES);
}

inline void msdos_int_33h_0015h()
{
	REG16(BX) = sizeof(mouse);
}

inline void msdos_int_33h_0016h()
{
	memcpy(mem + SREG_BASE(ES) + REG16(DX), &mouse, sizeof(mouse));
}

inline void msdos_int_33h_0017h()
{
	memcpy(&mouse, mem + SREG_BASE(ES) + REG16(DX), sizeof(mouse));
}

inline void msdos_int_33h_001ah()
{
	mouse.sensitivity[0] = REG16(BX);
	mouse.sensitivity[1] = REG16(CX);
	mouse.sensitivity[2] = REG16(DX);
}

inline void msdos_int_33h_001bh()
{
	REG16(BX) = mouse.sensitivity[0];
	REG16(CX) = mouse.sensitivity[1];
	REG16(DX) = mouse.sensitivity[2];
}

inline void msdos_int_33h_001dh()
{
	mouse.display_page = REG16(BX);
}

inline void msdos_int_33h_001eh()
{
	REG16(BX) = mouse.display_page;
}

inline void msdos_int_33h_0021h()
{
	REG16(AX) = 0xffff;
	REG16(BX) = MAX_MOUSE_BUTTONS;
}

inline void msdos_int_33h_0022h()
{
	mouse.language = REG16(BX);
}

inline void msdos_int_33h_0023h()
{
	REG16(BX) = mouse.language;
}

inline void msdos_int_33h_0024h()
{
	REG16(BX) = 0x0805; // V8.05
	REG16(CX) = 0x0400; // PS/2
}

inline void msdos_int_33h_0026h()
{
	REG16(BX) = 0x0000;
	REG16(CX) = mouse.max_position.x;
	REG16(DX) = mouse.max_position.y;
}

inline void msdos_int_33h_002ah()
{
	REG16(AX) = mouse.active ? 0 : 0xffff;
	REG16(BX) = mouse.hot_spot[0];
	REG16(CX) = mouse.hot_spot[1];
	REG16(DX) = 4; // PS/2
}

inline void msdos_int_33h_0031h()
{
	REG16(AX) = mouse.min_position.x;
	REG16(BX) = mouse.min_position.y;
	REG16(CX) = mouse.max_position.x;
	REG16(DX) = mouse.max_position.y;
}

inline void msdos_int_33h_0032h()
{
	REG16(AX) = 0;
//	REG16(AX) |= 0x8000; // 0025h
	REG16(AX) |= 0x4000; // 0026h
//	REG16(AX) |= 0x2000; // 0027h
//	REG16(AX) |= 0x1000; // 0028h
//	REG16(AX) |= 0x0800; // 0029h
	REG16(AX) |= 0x0400; // 002ah
//	REG16(AX) |= 0x0200; // 002bh
//	REG16(AX) |= 0x0100; // 002ch
//	REG16(AX) |= 0x0080; // 002dh
//	REG16(AX) |= 0x0040; // 002eh
	REG16(AX) |= 0x0020; // 002fh
//	REG16(AX) |= 0x0010; // 0030h
	REG16(AX) |= 0x0008; // 0031h
	REG16(AX) |= 0x0004; // 0032h
//	REG16(AX) |= 0x0002; // 0033h
//	REG16(AX) |= 0x0001; // 0034h
}

inline void msdos_int_67h_40h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else {
		REG8(AH) = 0x00;
	}
}

inline void msdos_int_67h_41h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else {
		REG8(AH) = 0x00;
		REG16(BX) = EMS_TOP >> 4;
	}
}

inline void msdos_int_67h_42h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else {
		REG8(AH) = 0x00;
		REG16(BX) = free_ems_pages;
		REG16(DX) = MAX_EMS_PAGES;
	}
}

inline void msdos_int_67h_43h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG16(BX) > MAX_EMS_PAGES) {
		REG8(AH) = 0x87;
	} else if(REG16(BX) > free_ems_pages) {
		REG8(AH) = 0x88;
	} else if(REG16(BX) == 0) {
		REG8(AH) = 0x89;
	} else {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(!ems_handles[i].allocated) {
				ems_allocate_pages(i, REG16(BX));
				REG8(AH) = 0x00;
				REG16(DX) = i;
				return;
			}
		}
		REG8(AH) = 0x85;
	}
}

inline void msdos_int_67h_44h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
		REG8(AH) = 0x83;
	} else if(!(REG16(BX) == 0xffff || REG16(BX) < ems_handles[REG16(DX)].pages)) {
		REG8(AH) = 0x8a;
//	} else if(!(REG8(AL) < 4)) {
//		REG8(AH) = 0x8b;
	} else if(REG16(BX) == 0xffff) {
		ems_unmap_page(REG8(AL) & 3);
		REG8(AH) = 0x00;
	} else {
		ems_map_page(REG8(AL) & 3, REG16(DX), REG16(BX));
		REG8(AH) = 0x00;
	}
}

inline void msdos_int_67h_45h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
		REG8(AH) = 0x83;
	} else {
		ems_release_pages(REG16(DX));
		REG8(AH) = 0x00;
	}
}

inline void msdos_int_67h_46h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else {
//		REG16(AX) = 0x0032; // EMS 3.2
		REG16(AX) = 0x0040; // EMS 4.0
	}
}

inline void msdos_int_67h_47h()
{
	// NOTE: the map data should be stored in the specified ems page, not process data
	process_t *process = msdos_process_info_get(current_psp);
	
	if(!support_ems) {
		REG8(AH) = 0x84;
//	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
//		REG8(AH) = 0x83;
	} else if(process->ems_pages_stored) {
		REG8(AH) = 0x8d;
	} else {
		for(int i = 0; i < 4; i++) {
			process->ems_pages[i].handle = ems_pages[i].handle;
			process->ems_pages[i].page   = ems_pages[i].page;
			process->ems_pages[i].mapped = ems_pages[i].mapped;
		}
		process->ems_pages_stored = true;
		REG8(AH) = 0x00;
	}
}

inline void msdos_int_67h_48h()
{
	// NOTE: the map data should be restored from the specified ems page, not process data
	process_t *process = msdos_process_info_get(current_psp);
	
	if(!support_ems) {
		REG8(AH) = 0x84;
//	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
//		REG8(AH) = 0x83;
	} else if(!process->ems_pages_stored) {
		REG8(AH) = 0x8e;
	} else {
		for(int i = 0; i < 4; i++) {
			if(process->ems_pages[i].mapped) {
				ems_map_page(i, process->ems_pages[i].handle, process->ems_pages[i].page);
			} else {
				ems_unmap_page(i);
			}
		}
		process->ems_pages_stored = false;
		REG8(AH) = 0x00;
	}
}

inline void msdos_int_67h_4bh()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else {
		REG8(AH) = 0x00;
		REG16(BX) = 0;
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated) {
				REG16(BX)++;
			}
		}
	}
}

inline void msdos_int_67h_4ch()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
		REG8(AH) = 0x83;
	} else {
		REG8(AH) = 0x00;
		REG16(BX) = ems_handles[REG16(DX)].pages;
	}
}

inline void msdos_int_67h_4dh()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else {
		REG8(AH) = 0x00;
		REG16(BX) = 0;
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated) {
				*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4 * REG16(BX) + 0) = i;
				*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4 * REG16(BX) + 2) = ems_handles[i].pages;
				REG16(BX)++;
			}
		}
	}
}

inline void msdos_int_67h_4eh()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG8(AL) == 0x00 || REG8(AL) == 0x01 || REG8(AL) == 0x02) {
		if(REG8(AL) == 0x00 || REG8(AL) == 0x02) {
			// save page map
			for(int i = 0; i < 4; i++) {
				*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4 * i + 0) = ems_pages[i].mapped ? ems_pages[i].handle : 0xffff;
				*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4 * i + 2) = ems_pages[i].mapped ? ems_pages[i].page   : 0xffff;
			}
		}
		if(REG8(AL) == 0x01 || REG8(AL) == 0x02) {
			// restore page map
			for(int i = 0; i < 4; i++) {
				UINT16 handle = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 4 * i + 0);
				UINT16 page   = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 4 * i + 2);
				
				if(handle >= 1 && handle <= MAX_EMS_HANDLES && ems_handles[handle].allocated && page < ems_handles[handle].pages) {
					ems_map_page(i, handle, page);
				} else {
					ems_unmap_page(i);
				}
			}
		}
		REG8(AH) = 0x00;
	} else if(REG8(AL) == 0x03) {
		REG8(AH) = 0x00;
		REG8(AL) = 4 * 4;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_4fh()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG8(AL) == 0x00) {
		int count = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI));
		
		*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI)) = count;
		for(int i = 0; i < count; i++) {
			UINT16 segment  = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 2 + 2 * i);
			UINT16 physical = ((segment << 4) - EMS_TOP) / 0x4000;
			
//			if(!(physical < 4)) {
//				REG8(AH) = 0x8b;
//				return;
//			}
			*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 2 + 6 * i + 0) = segment;
			*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 2 + 6 * i + 2) = ems_pages[physical & 3].handle;
			*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 2 + 6 * i + 4) = ems_pages[physical & 3].mapped ? ems_pages[physical & 3].page : 0xffff;
		}
		REG8(AH) = 0x00;
	} else if(REG8(AL) == 0x01) {
		int count = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI));
		
		for(int i = 0; i < count; i++) {
			UINT16 segment  = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 2 + 6 * i + 0);
			UINT16 physical = ((segment << 4) - EMS_TOP) / 0x4000;
			UINT16 handle   = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 2 + 6 * i + 2);
			UINT16 logical  = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 2 + 6 * i + 4);
			
//			if(!(physical < 4)) {
//				REG8(AH) = 0x8b;
//				return;
//			} else
			if(!(handle >= 1 && handle <= MAX_EMS_HANDLES && ems_handles[handle].allocated)) {
				REG8(AH) = 0x83;
				return;
			} else if(logical == 0xffff) {
				ems_unmap_page(physical & 3);
			} else if(logical < ems_handles[handle].pages) {
				ems_map_page(physical & 3, handle, logical);
			} else {
				REG8(AH) = 0x8a;
				return;
			}
		}
		REG8(AH) = 0x00;
	} else if(REG8(AL) == 0x02) {
		REG8(AH) = 0x00;
		REG8(AL) = 2 + REG16(BX) * 6;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_50h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
		REG8(AH) = 0x83;
	} else if(REG8(AL) == 0x00 || REG8(AL) == 0x01) {
		for(int i = 0; i < REG16(CX); i++) {
			int logical  = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 4 * i + 0);
			int physical = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 4 * i + 2);
			
			if(REG8(AL) == 0x01) {
				physical = ((physical << 4) - EMS_TOP) / 0x4000;
			}
//			if(!(physical < 4)) {
//				REG8(AH) = 0x8b;
//				return;
//			} else
			if(logical == 0xffff) {
				ems_unmap_page(physical & 3);
			} else if(logical < ems_handles[REG16(DX)].pages) {
				ems_map_page(physical & 3, REG16(DX), logical);
			} else {
				REG8(AH) = 0x8a;
				return;
			}
		}
		REG8(AH) = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_51h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
		REG8(AH) = 0x83;
	} else if(REG16(BX) > MAX_EMS_PAGES) {
		REG8(AH) = 0x87;
	} else if(REG16(BX) > free_ems_pages + ems_handles[REG16(DX)].pages) {
		REG8(AH) = 0x88;
	} else {
		ems_reallocate_pages(REG16(DX), REG16(BX));
		REG8(AH) = 0x00;
	}
}

inline void msdos_int_67h_52h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
//	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
//		REG8(AH) = 0x83;
	} else if(REG8(AL) == 0x00) {
		REG8(AL) = 0x00; // handle is volatile
		REG8(AH) = 0x00;
	} else if(REG8(AL) == 0x01) {
		if(REG8(BL) == 0x00) {
			REG8(AH) = 0x00;
		} else {
			REG8(AH) = 0x90; // undefined attribute type
		}
	} else if(REG8(AL) == 0x02) {
		REG8(AL) = 0x00; // only volatile handles supported
		REG8(AH) = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_53h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(!(REG16(DX) >= 1 && REG16(DX) <= MAX_EMS_HANDLES && ems_handles[REG16(DX)].allocated)) {
		REG8(AH) = 0x83;
	} else if(REG8(AL) == 0x00) {
		memcpy(mem + SREG_BASE(ES) + REG16(DI), ems_handles[REG16(DX)].name, 8);
		REG8(AH) = 0x00;
	} else if(REG8(AL) == 0x01) {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated && memcmp(ems_handles[REG16(DX)].name, mem + SREG_BASE(DS) + REG16(SI), 8) == 0) {
				REG8(AH) = 0xa1;
				return;
			}
		}
		REG8(AH) = 0x00;
		memcpy(ems_handles[REG16(DX)].name, mem + SREG_BASE(DS) + REG16(SI), 8);
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_54h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG8(AL) == 0x00) {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated) {
				memcpy(mem + SREG_BASE(ES) + REG16(DI) + 10 * i + 2, ems_handles[i].name, 10);
			} else {
				memset(mem + SREG_BASE(ES) + REG16(DI) + 10 * i + 2, 0, 10);
			}
			*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 10 * i + 0) = i;
		}
		REG8(AH) = 0x00;
		REG8(AL) = MAX_EMS_HANDLES;
	} else if(REG8(AL) == 0x01) {
		REG8(AH) = 0xa0; // not found
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated && memcmp(ems_handles[REG16(DX)].name, mem + SREG_BASE(DS) + REG16(SI), 8) == 0) {
				REG8(AH) = 0x00;
				REG16(DX) = i;
				break;
			}
		}
	} else if(REG8(AL) == 0x02) {
		REG8(AH) = 0x00;
		REG16(BX) = MAX_EMS_HANDLES;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_57h_tmp()
{
	UINT32 copy_length = *(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00);
	UINT8  src_type    = *(UINT8  *)(mem + SREG_BASE(DS) + REG16(SI) + 0x04);
	UINT16 src_handle  = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x05);
	UINT16 src_ofs     = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x07);
	UINT16 src_seg     = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x09);
	UINT8  dest_type   = *(UINT8  *)(mem + SREG_BASE(DS) + REG16(SI) + 0x0b);
	UINT16 dest_handle = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x0c);
	UINT16 dest_ofs    = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x0e);
	UINT16 dest_seg    = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x10);
	
	UINT8 *src_buffer, *dest_buffer;
	UINT32 src_addr, dest_addr;
	UINT32 src_addr_max, dest_addr_max;
	
	if(src_type == 0) {
		src_buffer = mem;
		src_addr = (src_seg << 4) + src_ofs;
		src_addr_max = MAX_MEM;
	} else {
		if(!(src_handle >= 1 && src_handle <= MAX_EMS_HANDLES && ems_handles[src_handle].allocated)) {
			REG8(AH) = 0x83;
			return;
		} else if(!(src_seg < ems_handles[src_handle].pages)) {
			REG8(AH) = 0x8a;
			return;
		}
		src_buffer = ems_handles[src_handle].buffer + 0x4000 * src_seg;
		src_addr = src_ofs;
		src_addr_max = 0x4000;
	}
	if(dest_type == 0) {
		dest_buffer = mem;
		dest_addr = (dest_seg << 4) + dest_ofs;
		dest_addr_max = MAX_MEM;
	} else {
		if(!(dest_handle >= 1 && dest_handle <= MAX_EMS_HANDLES && ems_handles[dest_handle].allocated)) {
			REG8(AH) = 0x83;
			return;
		} else if(!(dest_seg < ems_handles[dest_handle].pages)) {
			REG8(AH) = 0x8a;
			return;
		}
		dest_buffer = ems_handles[dest_handle].buffer + 0x4000 * dest_seg;
		dest_addr = dest_ofs;
		dest_addr_max = 0x4000;
	}
	for(int i = 0; i < copy_length; i++) {
		if(src_addr < src_addr_max && dest_addr < dest_addr_max) {
			if(REG8(AL) == 0x00) {
				dest_buffer[dest_addr++] = src_buffer[src_addr++];
			} else if(REG8(AL) == 0x01) {
				UINT8 tmp = dest_buffer[dest_addr];
				dest_buffer[dest_addr++] = src_buffer[src_addr];
				src_buffer[src_addr++] = tmp;
			}
		} else {
			REG8(AH) = 0x93;
			return;
		}
	}
	REG8(AH) = 0x80;
}

inline void msdos_int_67h_57h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG8(AL) == 0x00 || REG8(AL) == 0x01) {
		struct {
			UINT16 handle;
			UINT16 page;
			bool mapped;
		} tmp_pages[4];
		
		// unmap pages to copy memory data to ems buffer
		for(int i = 0; i < 4; i++) {
			tmp_pages[i].handle = ems_pages[i].handle;
			tmp_pages[i].page   = ems_pages[i].page;
			tmp_pages[i].mapped = ems_pages[i].mapped;
			ems_unmap_page(i);
		}
		
		// run move/exchange operation
		msdos_int_67h_57h_tmp();
		
		// restore unmapped pages
		for(int i = 0; i < 4; i++) {
			if(tmp_pages[i].mapped) {
				ems_map_page(i, tmp_pages[i].handle, tmp_pages[i].page);
			}
		}
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_58h()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG8(AL) == 0x00) {
		for(int i = 0; i < 4; i++) {
			*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4 * i + 0) = (EMS_TOP + 0x4000 * i) >> 4;
			*(UINT16 *)(mem + SREG_BASE(ES) + REG16(DI) + 4 * i + 2) = i;
		}
		REG8(AH) = 0x00;
		REG16(CX) = 4;
	} else if(REG8(AL) == 0x01) {
		REG8(AH) = 0x00;
		REG16(CX) = 4;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_5ah()
{
	if(!support_ems) {
		REG8(AH) = 0x84;
	} else if(REG16(BX) > MAX_EMS_PAGES) {
		REG8(AH) = 0x87;
	} else if(REG16(BX) > free_ems_pages) {
		REG8(AH) = 0x88;
//	} else if(REG16(BX) == 0) {
//		REG8(AH) = 0x89;
	} else if(REG8(AL) == 0x00 || REG8(AL) == 0x01) {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(!ems_handles[i].allocated) {
				ems_allocate_pages(i, REG16(BX));
				REG8(AH) = 0x00;
				REG16(DX) = i;
				return;
			}
		}
		REG8(AH) = 0x85;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0x8f;
	}
}

inline void msdos_int_67h_deh()
{
	REG8(AH) = 0x84;
}

#ifdef SUPPORT_XMS

inline void msdos_xms_init()
{
	emb_handle_top = (emb_handle_t *)calloc(1, sizeof(emb_handle_t));
	emb_handle_top->address = EMB_TOP;
	emb_handle_top->size_kb = (EMB_END - EMB_TOP) >> 10;
	xms_a20_local_enb_count = 0;
}

inline void msdos_xms_finish()
{
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL;) {
		emb_handle_t *next_handle = emb_handle->next;
		free(emb_handle);
		emb_handle = next_handle;
	}
}

emb_handle_t *msdos_xms_get_emb_handle(int handle)
{
	if(handle != 0) {
		for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
			if(emb_handle->handle == handle) {
				return(emb_handle);
			}
		}
	}
	return(NULL);
}

int msdos_xms_get_unused_emb_handle_id()
{
	for(int handle = 1;; handle++) {
		if(msdos_xms_get_emb_handle(handle) == NULL) {
			return(handle);
		}
	}
	return(0);
}

int msdos_xms_get_unused_emb_handle_count()
{
	int count = 64; //255;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle != 0) {
			if(--count == 1) {
				break;
			}
		}
	}
	return(count);
}

void msdos_xms_split_emb_handle(emb_handle_t *emb_handle, int size_kb)
{
	if(emb_handle->size_kb > size_kb) {
		emb_handle_t *new_handle = (emb_handle_t *)calloc(1, sizeof(emb_handle_t));
		
		new_handle->address = emb_handle->address + size_kb * 1024;
		new_handle->size_kb = emb_handle->size_kb - size_kb;
		emb_handle->size_kb = size_kb;
		
		new_handle->prev = emb_handle;
		new_handle->next = emb_handle->next;
		if(emb_handle->next != NULL) {
			emb_handle->next->prev = new_handle;
		}
		emb_handle->next = new_handle;
	}
}

void msdos_xms_combine_emb_handles(emb_handle_t *emb_handle)
{
	emb_handle_t *next_handle = emb_handle->next;
	
	if(next_handle != NULL) {
		emb_handle->size_kb += next_handle->size_kb;
		
		if(next_handle->next != NULL) {
			next_handle->next->prev = emb_handle;
		}
		emb_handle->next = next_handle->next;
		free(next_handle);
	}
}

emb_handle_t *msdos_xms_alloc_emb_handle(int size_kb)
{
	emb_handle_t *target_handle = NULL;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle == 0 && emb_handle->size_kb >= size_kb) {
			if(target_handle == NULL || target_handle->size_kb > emb_handle->size_kb) {
				target_handle = emb_handle;
			}
		}
	}
	if(target_handle != NULL) {
		if(target_handle->size_kb > size_kb) {
			msdos_xms_split_emb_handle(target_handle, size_kb);
		}
//		target_handle->handle = msdos_xms_get_unused_emb_handle_id();
		return(target_handle);
	}
	return(NULL);
}

void msdos_xms_free_emb_handle(emb_handle_t *emb_handle)
{
	emb_handle_t *prev_handle = emb_handle->prev;
	emb_handle_t *next_handle = emb_handle->next;
	
	if(prev_handle != NULL && prev_handle->handle == 0) {
		msdos_xms_combine_emb_handles(prev_handle);
		emb_handle = prev_handle;
	}
	if(next_handle != NULL && next_handle->handle == 0) {
		msdos_xms_combine_emb_handles(emb_handle);
	}
	emb_handle->handle = 0;
}

inline void msdos_call_xms_00h()
{
#if defined(HAS_I386)
	REG16(AX) = 0x0300; // V3.00 (XMS Version)
	REG16(BX) = 0x035f; // V3.95 (Driver Revision)
#else
	REG16(AX) = 0x0200; // V2.00 (XMS Version)
	REG16(BX) = 0x0270; // V2.70 (Driver Revision)
#endif
//	REG16(DX) = 0x0000; // HMA does not exist
	REG16(DX) = 0x0001; // HMA does exist
}

inline void msdos_call_xms_01h()
{
	if(REG8(AL) == 0x40) {
		// HIMEM.SYS will fail function 01h with error code 91h if AL=40h and
		// DX=KB free extended memory returned by last call of function 08h
		REG16(AX) = 0x0000;
		REG8(BL) = 0x91;
		REG16(DX) = xms_dx_after_call_08h;
	} else if(memcmp(mem + 0x100003, "VDISK", 5) == 0) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x81; // Vdisk was detected
#ifdef SUPPORT_HMA
	} else if(is_hma_used_by_int_2fh) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x90; // HMA does not exist or is not managed by XMS provider
	} else if(is_hma_used_by_xms) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x91; // HMA is already in use
	} else {
		REG16(AX) = 0x0001;
		is_hma_used_by_xms = true;
#else
	} else {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x91; // HMA is already in use
#endif
	}
}

inline void msdos_call_xms_02h()
{
	if(memcmp(mem + 0x100003, "VDISK", 5) == 0) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x81; // Vdisk was detected
#ifdef SUPPORT_HMA
	} else if(is_hma_used_by_int_2fh) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x90; // HMA does not exist or is not managed by XMS provider
	} else if(!is_hma_used_by_xms) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x93; // HMA is not allocated
	} else {
		REG16(AX) = 0x0001;
		is_hma_used_by_xms = false;
		// restore first free mcb in high memory area
		msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
#else
	} else {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x91; // HMA is already in use
#endif
	}
}

inline void msdos_call_xms_03h()
{
	i386_set_a20_line(1);
	REG16(AX) = 0x0001;
	REG8(BL) = 0x00;
}

inline void msdos_call_xms_04h()
{
	i386_set_a20_line(0);
	REG16(AX) = 0x0001;
	REG8(BL) = 0x00;
}

inline void msdos_call_xms_05h()
{
	i386_set_a20_line(1);
	REG16(AX) = 0x0001;
	REG8(BL) = 0x00;
	xms_a20_local_enb_count++;
}

void msdos_call_xms_06h()
{
	if(xms_a20_local_enb_count > 0) {
		xms_a20_local_enb_count--;
	}
	if(xms_a20_local_enb_count == 0) {
		i386_set_a20_line(0);
	}
	if((m_a20_mask >> 20) & 1) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0x94;
	} else {
		REG16(AX) = 0x0001;
		REG8(BL) = 0x00;
	}
}

inline void msdos_call_xms_07h()
{
	REG16(AX) = (m_a20_mask >> 20) & 1;
	REG8(BL) = 0x00;
}

inline void msdos_call_xms_08h()
{
	REG16(AX) = REG16(DX) = 0x0000;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle == 0) {
			if(REG16(AX) < emb_handle->size_kb) {
				REG16(AX) = emb_handle->size_kb;
			}
			REG16(DX) += emb_handle->size_kb;
		}
	}
	
	if(REG16(AX) == 0 && REG16(DX) == 0) {
		REG8(BL) = 0xa0;
	} else {
		REG8(BL) = 0x00;
	}
	xms_dx_after_call_08h = REG16(DX);
}

void msdos_call_xms_09h(int size_kb)
{
	emb_handle_t *emb_handle = msdos_xms_alloc_emb_handle(size_kb);
	
	if(emb_handle != NULL) {
		emb_handle->handle = msdos_xms_get_unused_emb_handle_id();
		
		REG16(AX) = 0x0001;
		REG16(DX) = emb_handle->handle;
		REG8(BL) = 0x00;
	} else {
		REG16(AX) = REG16(DX) = 0x0000;
		REG8(BL) = 0xa0;
	}
}

inline void msdos_call_xms_09h()
{
	msdos_call_xms_09h(REG16(DX));
}

inline void msdos_call_xms_0ah()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(REG16(DX));
	
	if(emb_handle == NULL) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xa2;
	} else if(emb_handle->lock > 0) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xab;
	} else {
		msdos_xms_free_emb_handle(emb_handle);
		
		REG16(AX) = 0x0001;
		REG8(BL) = 0x00;
	}
}

inline void msdos_call_xms_0bh()
{
	UINT32 copy_length = *(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x00);
	UINT16 src_handle  = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x04);
	UINT32 src_addr    = *(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x06);
	UINT16 dest_handle = *(UINT16 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x0a);
	UINT32 dest_addr   = *(UINT32 *)(mem + SREG_BASE(DS) + REG16(SI) + 0x0c);
	
	UINT8 *src_buffer, *dest_buffer;
	UINT32 src_addr_max, dest_addr_max;
	emb_handle_t *emb_handle;
	
	if(src_handle == 0) {
		src_buffer = mem;
		src_addr = (((src_addr >> 16) & 0xffff) << 4) + (src_addr & 0xffff);
		src_addr_max = MAX_MEM;

	} else {
		if((emb_handle = msdos_xms_get_emb_handle(src_handle)) == NULL) {
			REG16(AX) = 0x0000;
			REG8(BL) = 0xa3;
			return;
		}
		src_buffer = mem + emb_handle->address;
		src_addr_max = emb_handle->size_kb * 1024;
	}
	if(dest_handle == 0) {
		dest_buffer = mem;
		dest_addr = (((dest_addr >> 16) & 0xffff) << 4) + (dest_addr & 0xffff);
		dest_addr_max = MAX_MEM;
	} else {
		if((emb_handle = msdos_xms_get_emb_handle(dest_handle)) == NULL) {
			REG16(AX) = 0x0000;
			REG8(BL) = 0xa5;
			return;
		}
		dest_buffer = mem + emb_handle->address;
		dest_addr_max = emb_handle->size_kb * 1024;
	}
	for(int i = 0; i < copy_length; i++) {
		if(src_addr < src_addr_max && dest_addr < dest_addr_max) {
			dest_buffer[dest_addr++] = src_buffer[src_addr++];
		} else {
			break;
		}
	}
	REG16(AX) = 0x0001;
	REG8(BL) = 0x00;
}

inline void msdos_call_xms_0ch()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(REG16(DX));
	
	if(emb_handle == NULL) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xa2;
	} else {
		emb_handle->lock++;
		REG16(AX) = 0x0001;
		REG8(BL) = 0x00;
		REG16(DX) = (emb_handle->address >> 16) & 0xffff;
		REG16(BX) = (emb_handle->address      ) & 0xffff;
	}
}

inline void msdos_call_xms_0dh()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(REG16(DX));
	
	if(emb_handle == NULL) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xa2;
	} else if(!(emb_handle->lock > 0)) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xaa;
	} else {
		emb_handle->lock--;
		REG16(AX) = 0x0001;
		REG8(BL) = 0x00;
	}
}

inline void msdos_call_xms_0eh()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(REG16(DX));
	
	if(emb_handle == NULL) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xa2;
	} else {
		REG16(AX) = 0x0001;
		REG8(BH) = emb_handle->lock;
		REG8(BL) = msdos_xms_get_unused_emb_handle_count();
		REG16(DX) = emb_handle->size_kb;
	}
}

void msdos_call_xms_0fh(int size_kb)
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(REG16(DX));
	
	if(emb_handle == NULL) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xa2;
	} else if(emb_handle->lock > 0) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xab;
	} else {
		if(emb_handle->size_kb < size_kb) {
			if(emb_handle->next != NULL && emb_handle->next->handle == 0 && (emb_handle->size_kb + emb_handle->next->size_kb) >= size_kb) {
				msdos_xms_combine_emb_handles(emb_handle);
				if(emb_handle->size_kb > size_kb) {
					msdos_xms_split_emb_handle(emb_handle, size_kb);
				}
			} else {
				int old_handle = emb_handle->handle;
				int old_size_kb = emb_handle->size_kb;
				UINT8 *buffer = (UINT8 *)malloc(old_size_kb * 1024);
				
				memcpy(buffer, mem + emb_handle->address, old_size_kb * 1024);
				msdos_xms_free_emb_handle(emb_handle);
				
				if((emb_handle = msdos_xms_alloc_emb_handle(size_kb)) == NULL) {
					emb_handle = msdos_xms_alloc_emb_handle(old_size_kb); // should be always successed
				}
				emb_handle->handle = old_handle;
				memcpy(mem + emb_handle->address, buffer, old_size_kb * 1024);
				free(buffer);
			}
		} else if(emb_handle->size_kb > size_kb) {
			msdos_xms_split_emb_handle(emb_handle, size_kb);
		}
		if(emb_handle->size_kb != size_kb) {
			REG16(AX) = 0x0000;
			REG8(BL) = 0xa0;
		} else {
			REG16(AX) = 0x0001;
			REG8(BL) = 0x00;
		}
	}
}

inline void msdos_call_xms_0fh()
{
	msdos_call_xms_0fh(REG16(BX));
}

inline void msdos_call_xms_10h()
{
	int seg;
	
	if((seg = msdos_mem_alloc(UMB_TOP >> 4, REG16(DX), 0)) != -1) {
		REG16(AX) = 0x0001;
		REG16(BX) = seg;
	} else {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xb0;
		REG16(DX) = msdos_mem_get_free(UMB_TOP >> 4, 0);
	}
}

inline void msdos_call_xms_11h()
{
	int mcb_seg = REG16(DX) - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		msdos_mem_free(REG16(DX));
		REG16(AX) = 0x0001;
		REG8(BL) = 0x00;
	} else {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xb2;
	}
}

inline void msdos_call_xms_12h()
{
	int mcb_seg = REG16(DX) - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	int max_paragraphs;
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		if(!msdos_mem_realloc(REG16(DX), REG16(BX), &max_paragraphs)) {
			REG16(AX) = 0x0001;
			REG8(BL) = 0x00;
		} else {
			REG16(AX) = 0x0000;
			REG8(BL) = 0xb0;
			REG16(DX) = max_paragraphs;
		}
	} else {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xb2;
	}
}

#if defined(HAS_I386)

inline void msdos_call_xms_88h()
{
	REG32(EAX) = REG32(EDX) = 0x0000;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle == 0) {
			if(REG32(EAX) < emb_handle->size_kb) {
				REG32(EAX) = emb_handle->size_kb;
			}
			REG32(EDX) += emb_handle->size_kb;
		}
	}
	
	if(REG32(EAX) == 0 && REG32(EDX) == 0) {
		REG8(BL) = 0xa0;
	} else {
		REG8(BL) = 0x00;
	}
	REG32(ECX) = EMB_END - 1;
}

inline void msdos_call_xms_89h()
{
	msdos_call_xms_09h(REG32(EDX));
}

inline void msdos_call_xms_8eh()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(REG16(DX));
	
	if(emb_handle == NULL) {
		REG16(AX) = 0x0000;
		REG8(BL) = 0xa2;
	} else {
		REG16(AX) = 0x0001;
		REG8(BH) = emb_handle->lock;
		REG16(CX) = msdos_xms_get_unused_emb_handle_count();
		REG32(EDX) = emb_handle->size_kb;
	}
}

inline void msdos_call_xms_8fh()
{
	msdos_call_xms_0fh(REG32(EBX));
}

#endif
#endif

UINT16 msdos_get_equipment()
{
	static UINT16 equip = 0;
	
	if(equip == 0) {
#ifdef SUPPORT_FPU
		equip |= (1 << 1);	// 80x87 coprocessor installed
#endif
		equip |= (1 << 2);	// pointing device installed (PS/2)
		equip |= (2 << 4);	// initial video mode (80x25 color)
//		equip |= (1 << 8);	// 0 if DMA installed
		equip |= (2 << 9);	// number of serial ports
		equip |= (3 << 14);	// number of printer ports (NOTE: this number is 3 on Windows 98 SE though only LPT1 exists)
		
		// check only A: and B: if it is floppy drive
		DWORD dwDrives = GetLogicalDrives();
		int n = 0;
		for(int i = 0; i < 2; i++) {
			if(dwDrives & (1 << i)) {
				char volume[] = "A:\\";
				volume[0] = 'A' + i;
				if(GetDriveType(volume) == DRIVE_REMOVABLE) {
					n++;
				}
			}
		}
		if(n != 0) {
			equip |= (1 << 0);	// floppy disk(s) installed
			n--;
			equip |= (n << 6);	// number of floppies installed less 1
		}
//		if(joyGetNumDevs() != 0) {
//			equip |= (1 << 12);	// game port installed
//		}
	}
	return(equip);
}

void msdos_syscall(unsigned num)
{
#ifdef ENABLE_DEBUG_SYSCALL
	if(num == 0x68) {
		// dummy interrupt for EMS (int 67h)
		fprintf(fdebug, "int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
	} else if(num == 0x69) {
		// dummy interrupt for XMS (call far)
		fprintf(fdebug, "call XMS (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
	} else if(num == 0x6a) {
		// dummy interrupt for case map routine pointed in the country info
	} else {
		fprintf(fdebug, "int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
	}
#endif
	ctrl_c_pressed = ctrl_c_detected = false;
	
	switch(num) {
	case 0x00:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
			error("division by zero\n");
		} catch(...) {
			fatalerror("division by zero detected, and failed to terminate current process\n");
		}
		break;
	case 0x04:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
			error("overflow\n");
		} catch(...) {
			fatalerror("overflow detected, and failed to terminate current process\n");
		}
		break;
	case 0x06:
		// NOTE: ish.com has illegal instruction...
		if(!ignore_illegal_insn) {
			try {
				msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
				error("illegal instruction\n");
			} catch(...) {
				fatalerror("illegal instruction detected, and failed to terminate current process\n");
			}
		} else {
#if defined(HAS_I386)
			m_eip++;
#else
			m_pc++;
#endif
		}
		break;
	case 0x08:
//		pcbios_irq0(); // this causes too slow emulation...
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		// EOI
		pic[0].isr &= ~(1 << (num - 0x08));
		pic_update();
		break;
	case 0x10:
		// PC BIOS - Video
		if(!restore_console_on_exit) {
			change_console_size(scr_width, scr_height);
		}
		m_CF = 0;
		switch(REG8(AH)) {
		case 0x00: pcbios_int_10h_00h(); break;
		case 0x01: pcbios_int_10h_01h(); break;
		case 0x02: pcbios_int_10h_02h(); break;
		case 0x03: pcbios_int_10h_03h(); break;
		case 0x05: pcbios_int_10h_05h(); break;
		case 0x06: pcbios_int_10h_06h(); break;
		case 0x07: pcbios_int_10h_07h(); break;
		case 0x08: pcbios_int_10h_08h(); break;
		case 0x09: pcbios_int_10h_09h(); break;
		case 0x0a: pcbios_int_10h_0ah(); break;
		case 0x0b: break;
		case 0x0c: break;
		case 0x0d: break;
		case 0x0e: pcbios_int_10h_0eh(); break;
		case 0x0f: pcbios_int_10h_0fh(); break;
		case 0x10: break;
		case 0x11: pcbios_int_10h_11h(); break;
		case 0x12: pcbios_int_10h_12h(); break;
		case 0x13: pcbios_int_10h_13h(); break;
		case 0x18: pcbios_int_10h_18h(); break;
		case 0x1a: pcbios_int_10h_1ah(); break;
		case 0x1b: REG8(AL) = 0x00; break; // functionality/state information is not supported
		case 0x1c: REG8(AL) = 0x00; break; // save/restore video state is not supported
		case 0x1d: pcbios_int_10h_1dh(); break;
		case 0x1e: REG8(AL) = 0x00; break; // flat-panel functions are not supported
		case 0x1f: REG8(AL) = 0x00; break; // xga functions are not supported
		case 0x4f: pcbios_int_10h_4fh(); break;
		case 0x6f: break;
		case 0x80: m_CF = 1; break; // unknown
		case 0x81: m_CF = 1; break; // unknown
		case 0x82: pcbios_int_10h_82h(); break;
		case 0x83: pcbios_int_10h_83h(); break;
		case 0x8b: break;
		case 0x8c: m_CF = 1; break; // unknown
		case 0x8d: m_CF = 1; break; // unknown
		case 0x8e: m_CF = 1; break; // unknown
		case 0x90: pcbios_int_10h_90h(); break;
		case 0x91: pcbios_int_10h_91h(); break;
		case 0x92: break;
		case 0x93: break;
		case 0xef: pcbios_int_10h_efh(); break;
		case 0xfa: break; // ega register interface library is not installed
		case 0xfe: pcbios_int_10h_feh(); break;
		case 0xff: pcbios_int_10h_ffh(); break;
		default:
			unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			m_CF = 1;
			break;
		}
		break;
	case 0x11:
		// PC BIOS - Get Equipment List
		REG16(AX) = msdos_get_equipment();
		break;
	case 0x12:
		// PC BIOS - Get Memory Size
		REG16(AX) = MEMORY_END / 1024;
		break;
	case 0x13:
		// PC BIOS - Disk
//		fatalerror("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		REG8(AH) = 0xff;
		m_CF = 1;
		break;
	case 0x14:
		// PC BIOS - Serial I/O
		switch(REG8(AH)) {
		case 0x00: pcbios_int_14h_00h(); break;
		case 0x01: pcbios_int_14h_01h(); break;
		case 0x02: pcbios_int_14h_02h(); break;
		case 0x03: pcbios_int_14h_03h(); break;
		case 0x04: pcbios_int_14h_04h(); break;
		case 0x05: pcbios_int_14h_05h(); break;
		default:
			unimplemented_14h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			break;
		}
		break;
	case 0x15:
		// PC BIOS
		m_CF = 0;
		switch(REG8(AH)) {
		case 0x10: pcbios_int_15h_10h(); break;
		case 0x23: pcbios_int_15h_23h(); break;
		case 0x24: pcbios_int_15h_24h(); break;
		case 0x41: break;
		case 0x49: pcbios_int_15h_49h(); break;
		case 0x50: pcbios_int_15h_50h(); break;
		case 0x53: pcbios_int_15h_53h(); break;
		case 0x86: pcbios_int_15h_86h(); break;
		case 0x87: pcbios_int_15h_87h(); break;
		case 0x88: pcbios_int_15h_88h(); break;
		case 0x89: pcbios_int_15h_89h(); break;
		case 0x8a: pcbios_int_15h_8ah(); break;
		case 0xc0: // PS/2 ???
		case 0xc1:
		case 0xc2:
		case 0xc3: // PS50+ ???
		case 0xc4:
			REG8(AH) = 0x86;
			m_CF = 1;
			break;
#if defined(HAS_I386)
		case 0xc9: pcbios_int_15h_c9h(); break;
#endif
		case 0xca: pcbios_int_15h_cah(); break;
		case 0xe8: pcbios_int_15h_e8h(); break;
		default:
			unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG8(AH) = 0x86;
			m_CF = 1;
			break;
		}
		break;
	case 0x16:
		// PC BIOS - Keyboard
		m_CF = 0;
		switch(REG8(AH)) {
		case 0x00: pcbios_int_16h_00h(); break;
		case 0x01: pcbios_int_16h_01h(); break;
		case 0x02: pcbios_int_16h_02h(); break;
		case 0x03: pcbios_int_16h_03h(); break;
		case 0x05: pcbios_int_16h_05h(); break;
		case 0x10: pcbios_int_16h_00h(); break;
		case 0x11: pcbios_int_16h_01h(); break;
		case 0x12: pcbios_int_16h_12h(); break;
		case 0x13: pcbios_int_16h_13h(); break;
		case 0x14: pcbios_int_16h_14h(); break;
		case 0x55: pcbios_int_16h_55h(); break;
		case 0x6f: pcbios_int_16h_6fh(); break;
		case 0xda: break; // unknown
		case 0xff: break; // unknown
		default:
			unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			break;
		}
		break;
	case 0x17:
		// PC BIOS - Printer
//		fatalerror("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		break;
	case 0x1a:
		// PC BIOS - Timer
		m_CF = 0;
		switch(REG8(AH)) {
		case 0x00: pcbios_int_1ah_00h(); break;
		case 0x01: break;
		case 0x02: pcbios_int_1ah_02h(); break;
		case 0x03: break;
		case 0x04: pcbios_int_1ah_04h(); break;
		case 0x05: break;
		case 0x0a: pcbios_int_1ah_0ah(); break;
		case 0x0b: break;
		case 0x35: break; // Word Perfect Third Party Interface?
		case 0x36: break; // Word Perfect Third Party Interface
		case 0x70: break; // SNAP? (Simple Network Application Protocol)
		default:
			unimplemented_1ah("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			break;
		}
		break;
	case 0x20:
		try {
			msdos_process_terminate(SREG(CS), retval, 1);
		} catch(...) {
			fatalerror("failed to terminate the process (PSP=%04X) by int 20h\n", SREG(CS));
		}
		break;
	case 0x21:
		// MS-DOS System Call
		m_CF = 0;
		try {
			switch(REG8(AH)) {
			case 0x00: msdos_int_21h_00h(); break;
			case 0x01: msdos_int_21h_01h(); break;
			case 0x02: msdos_int_21h_02h(); break;
			case 0x03: msdos_int_21h_03h(); break;
			case 0x04: msdos_int_21h_04h(); break;
			case 0x05: msdos_int_21h_05h(); break;
			case 0x06: msdos_int_21h_06h(); break;
			case 0x07: msdos_int_21h_07h(); break;
			case 0x08: msdos_int_21h_08h(); break;
			case 0x09: msdos_int_21h_09h(); break;
			case 0x0a: msdos_int_21h_0ah(); break;
			case 0x0b: msdos_int_21h_0bh(); break;
			case 0x0c: msdos_int_21h_0ch(); break;
			case 0x0d: msdos_int_21h_0dh(); break;
			case 0x0e: msdos_int_21h_0eh(); break;
			case 0x0f: msdos_int_21h_0fh(); break;
			case 0x10: msdos_int_21h_10h(); break;
			case 0x11: msdos_int_21h_11h(); break;
			case 0x12: msdos_int_21h_12h(); break;
			case 0x13: msdos_int_21h_13h(); break;
			case 0x14: msdos_int_21h_14h(); break;
			case 0x15: msdos_int_21h_15h(); break;
			case 0x16: msdos_int_21h_16h(); break;
			case 0x17: msdos_int_21h_17h(); break;
			case 0x18: msdos_int_21h_18h(); break;
			case 0x19: msdos_int_21h_19h(); break;
			case 0x1a: msdos_int_21h_1ah(); break;
			case 0x1b: msdos_int_21h_1bh(); break;
			case 0x1c: msdos_int_21h_1ch(); break;
			case 0x1d: msdos_int_21h_1dh(); break;
			case 0x1e: msdos_int_21h_1eh(); break;
			case 0x1f: msdos_int_21h_1fh(); break;
			case 0x20: msdos_int_21h_20h(); break;
			case 0x21: msdos_int_21h_21h(); break;
			case 0x22: msdos_int_21h_22h(); break;
			case 0x23: msdos_int_21h_23h(); break;
			case 0x24: msdos_int_21h_24h(); break;
			case 0x25: msdos_int_21h_25h(); break;
			case 0x26: msdos_int_21h_26h(); break;
			case 0x27: msdos_int_21h_27h(); break;
			case 0x28: msdos_int_21h_28h(); break;
			case 0x29: msdos_int_21h_29h(); break;
			case 0x2a: msdos_int_21h_2ah(); break;
			case 0x2b: msdos_int_21h_2bh(); break;
			case 0x2c: msdos_int_21h_2ch(); break;
			case 0x2d: msdos_int_21h_2dh(); break;
			case 0x2e: msdos_int_21h_2eh(); break;
			case 0x2f: msdos_int_21h_2fh(); break;
			case 0x30: msdos_int_21h_30h(); break;
			case 0x31: msdos_int_21h_31h(); break;
			case 0x32: msdos_int_21h_32h(); break;
			case 0x33: msdos_int_21h_33h(); break;
			case 0x34: msdos_int_21h_34h(); break;
			case 0x35: msdos_int_21h_35h(); break;
			case 0x36: msdos_int_21h_36h(); break;
			case 0x37: msdos_int_21h_37h(); break;
			case 0x38: msdos_int_21h_38h(); break;
			case 0x39: msdos_int_21h_39h(0); break;
			case 0x3a: msdos_int_21h_3ah(0); break;
			case 0x3b: msdos_int_21h_3bh(0); break;
			case 0x3c: msdos_int_21h_3ch(); break;
			case 0x3d: msdos_int_21h_3dh(); break;
			case 0x3e: msdos_int_21h_3eh(); break;
			case 0x3f: msdos_int_21h_3fh(); break;
			case 0x40: msdos_int_21h_40h(); break;
			case 0x41: msdos_int_21h_41h(0); break;
			case 0x42: msdos_int_21h_42h(); break;
			case 0x43: msdos_int_21h_43h(0); break;
			case 0x44: msdos_int_21h_44h(); break;
			case 0x45: msdos_int_21h_45h(); break;
			case 0x46: msdos_int_21h_46h(); break;
			case 0x47: msdos_int_21h_47h(0); break;
			case 0x48: msdos_int_21h_48h(); break;
			case 0x49: msdos_int_21h_49h(); break;
			case 0x4a: msdos_int_21h_4ah(); break;
			case 0x4b: msdos_int_21h_4bh(); break;
			case 0x4c: msdos_int_21h_4ch(); break;
			case 0x4d: msdos_int_21h_4dh(); break;
			case 0x4e: msdos_int_21h_4eh(); break;
			case 0x4f: msdos_int_21h_4fh(); break;
			case 0x50: msdos_int_21h_50h(); break;
			case 0x51: msdos_int_21h_51h(); break;
			case 0x52: msdos_int_21h_52h(); break;
			// 0x53: translate bios parameter block to drive param bock
			case 0x54: msdos_int_21h_54h(); break;
			case 0x55: msdos_int_21h_55h(); break;
			case 0x56: msdos_int_21h_56h(0); break;
			case 0x57: msdos_int_21h_57h(); break;
			case 0x58: msdos_int_21h_58h(); break;
			case 0x59: msdos_int_21h_59h(); break;
			case 0x5a: msdos_int_21h_5ah(); break;
			case 0x5b: msdos_int_21h_5bh(); break;
			case 0x5c: msdos_int_21h_5ch(); break;
			case 0x5d: msdos_int_21h_5dh(); break;
			// 0x5e: ms-network
			case 0x5f: msdos_int_21h_5fh(); break;
			case 0x60: msdos_int_21h_60h(0); break;
			case 0x61: msdos_int_21h_61h(); break;
			case 0x62: msdos_int_21h_62h(); break;
			case 0x63: msdos_int_21h_63h(); break;
			// 0x64: set device driver lockahead flag
			case 0x65: msdos_int_21h_65h(); break;
			case 0x66: msdos_int_21h_66h(); break;
			case 0x67: msdos_int_21h_67h(); break;
			case 0x68: msdos_int_21h_68h(); break;
			case 0x69: msdos_int_21h_69h(); break;
			case 0x6a: msdos_int_21h_6ah(); break;
			case 0x6b: msdos_int_21h_6bh(); break;
			case 0x6c: msdos_int_21h_6ch(0); break;
			// 0x6d: find first rom program
			// 0x6e: find next rom program
			// 0x6f: get/set rom scan start address
			// 0x70: windows95 get/set internationalization information
			case 0x71:
				// windows95 long filename functions
				switch(REG8(AL)) {
				case 0x0d: msdos_int_21h_710dh(); break;
				case 0x39: msdos_int_21h_39h(1); break;
				case 0x3a: msdos_int_21h_3ah(1); break;
				case 0x3b: msdos_int_21h_3bh(1); break;
				case 0x41: msdos_int_21h_7141h(1); break;
				case 0x43: msdos_int_21h_43h(1); break;
				case 0x47: msdos_int_21h_47h(1); break;
				case 0x4e: msdos_int_21h_714eh(); break;
				case 0x4f: msdos_int_21h_714fh(); break;
				case 0x56: msdos_int_21h_56h(1); break;
				case 0x60: msdos_int_21h_60h(1); break;
				case 0x6c: msdos_int_21h_6ch(1); break;
				case 0xa0: msdos_int_21h_71a0h(); break;
				case 0xa1: msdos_int_21h_71a1h(); break;
				case 0xa6: msdos_int_21h_71a6h(); break;
				case 0xa7: msdos_int_21h_71a7h(); break;
				case 0xa8: msdos_int_21h_71a8h(); break;
				// 0xa9: server create/open file
				case 0xaa: msdos_int_21h_71aah(); break;
				default:
					unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
					REG16(AX) = 0x7100;
					m_CF = 1;
					break;
				}
				break;
			// 0x72: Windows95 beta - LFN FindClose
			case 0x73:
				// windows95 fat32 functions
				switch(REG8(AL)) {
				case 0x00: msdos_int_21h_7300h(); break;
				// 0x01: set drive locking ???
				case 0x02: msdos_int_21h_7302h(); break;
				case 0x03: msdos_int_21h_7303h(); break;
				// 0x04: set dpb to use for formatting
				// 0x05: extended absolute disk read/write
				default:
					unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
					REG16(AX) = 0x7300;
					m_CF = 1;
					break;
				}
				break;
			case 0xdb: msdos_int_21h_dbh(); break;
			case 0xdc: msdos_int_21h_dch(); break;
			default:
				unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
				REG16(AX) = 0x01;
				m_CF = 1;
				break;
			}
		} catch(int error) {
			REG16(AX) = error;
			m_CF = 1;
		} catch(...) {
			REG16(AX) = 0x1f; // general failure
			m_CF = 1;
		}
		if(m_CF) {
			sda_t *sda = (sda_t *)(mem + SDA_TOP);
			sda->extended_error_code = REG16(AX);
			switch(sda->extended_error_code) {
			case  4: // Too many open files
			case  8: // Insufficient memory
				sda->error_class = 1; // Out of resource
				break;
			case  5: // Access denied
				sda->error_class = 3; // Authorization
				break;
			case  7: // Memory control block destroyed
				sda->error_class = 4; // Internal
				break;
			case  2: // File not found
			case  3: // Path not found
			case 15: // Invaid drive specified
			case 18: // No more files
				sda->error_class = 8; // Not found
				break;
			case 32: // Sharing violation
			case 33: // Lock violation
				sda->error_class = 10; // Locked
				break;
//			case 16: // Removal of current directory attempted
			case 19: // Attempted write on protected disk
			case 21: // Drive not ready
//			case 29: // Write failure
//			case 30: // Read failure
//			case 82: // Cannot create subdirectory
				sda->error_class = 11; // Media
				break;
			case 80: // File already exists
				sda->error_class = 12; // Already exist
				break;
			default:
				sda->error_class = 13; // Unknown
				break;
			}
			sda->suggested_action = 1; // Retry
			sda->locus_of_last_error = 1; // Unknown
		}
		if(ctrl_c_checking && ctrl_c_detected) {
			// raise int 23h
#if defined(HAS_I386)
			m_ext = 0; // not an external interrupt
			i386_trap(0x23, 1, 0);
			m_ext = 1;
#else
			PREFIX86(_interrupt)(0x23);
#endif
		}
		break;
	case 0x22:
		fatalerror("int 22h (terminate address)\n");
	case 0x23:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x100, 1);
		} catch(...) {
			fatalerror("failed to terminate the current process by int 23h\n");
		}
		break;
	case 0x24:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		} catch(...) {
			fatalerror("failed to terminate the current process by int 24h\n");
		}
		break;
	case 0x25:
		msdos_int_25h();
		break;
	case 0x26:
		msdos_int_26h();
		break;
	case 0x27:
		try {
			msdos_int_27h();
		} catch(...) {
			fatalerror("failed to terminate the process (PSP=%04X) by int 27h\n", SREG(CS));
		}
		break;
	case 0x28:
		Sleep(10);
		break;
	case 0x29:
		msdos_int_29h();
		break;
	case 0x2e:
		msdos_int_2eh();
		break;
	case 0x2f:
		// multiplex interrupt
		switch(REG8(AH)) {
		case 0x05: msdos_int_2fh_05h(); break;
		case 0x11: msdos_int_2fh_11h(); break;
		case 0x12: msdos_int_2fh_12h(); break;
		case 0x13: msdos_int_2fh_13h(); break;
		case 0x14: msdos_int_2fh_14h(); break;
		case 0x15: msdos_int_2fh_15h(); break;
		case 0x16: msdos_int_2fh_16h(); break;
		case 0x19: msdos_int_2fh_19h(); break;
		case 0x1a: msdos_int_2fh_1ah(); break;
		case 0x40: msdos_int_2fh_40h(); break;
		case 0x43: msdos_int_2fh_43h(); break;
		case 0x46: msdos_int_2fh_46h(); break;
		case 0x48: msdos_int_2fh_48h(); break;
		case 0x4a: msdos_int_2fh_4ah(); break;
		case 0x4b: msdos_int_2fh_4bh(); break;
		case 0x4f: msdos_int_2fh_4fh(); break;
		case 0x55: msdos_int_2fh_55h(); break;
		case 0x80: msdos_int_2fh_80h(); break;
		case 0xad: msdos_int_2fh_adh(); break;
		case 0xae: msdos_int_2fh_aeh(); break;
		case 0xb7: msdos_int_2fh_b7h(); break;
		case 0xd7: msdos_int_2fh_d7h(); break;
		// Installation Check
		case 0x01: // PRINT.COM
		case 0x02: // PC LAN Program Redirector
		case 0x06: // ASSIGN
		case 0x08: // DRIVER.SYS
		case 0x10: // SHARE
		case 0x17: // Clibboard functions
		case 0x1b: // XMA2EMS.SYS
		case 0x23: // DR DOS 5.0 GRAFTABL
		case 0x27: // DR-DOR 6.0 TaskMAX
		case 0x2e: // Novell DOS 7 GRAFTABL
		case 0x45: // PROF.COM
		case 0x51: // ODIHELP.EXE
		case 0x54: // POWER.EXE
		case 0x56: // INTERLNK
		case 0x70: // License Service API
		case 0x7a: // Novell NetWare
		case 0x94: // MICRO.EXE
		case 0xac: // GRAPHICS.COM
		case 0xb0: // GRAFTABLE.COM
		case 0xb8: // NETWORK
		case 0xb9: // RECEIVER.COM
		case 0xbc: // EGA.SYS
		case 0xbf: // PC LAN Program - REDIRIFS.EXE
		case 0xc0: // Novell LSL.COM
		case 0xd2: // PCL-838.EXE
		case 0xd8: // Novell NetWare Lite - CLIENT.EXE
			switch(REG8(AL)) {
			case 0x00:
				// This is not installed
//				REG8(AL) = 0x00;
				break;
			default:
				unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
				REG16(AX) = 0x01;
				m_CF = 1;
				break;
			}
			break;
		default:
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			break;
		}
		break;
	case 0x33:
		switch(REG8(AH)) {
		case 0x00:
			// Mouse
			switch(REG8(AL)) {
			case 0x00: msdos_int_33h_0000h(); break;
			case 0x01: msdos_int_33h_0001h(); break;
			case 0x02: msdos_int_33h_0002h(); break;
			case 0x03: msdos_int_33h_0003h(); break;
			case 0x04: break; // position mouse cursor
			case 0x05: msdos_int_33h_0005h(); break;
			case 0x06: msdos_int_33h_0006h(); break;
			case 0x07: msdos_int_33h_0007h(); break;
			case 0x08: msdos_int_33h_0008h(); break;
			case 0x09: msdos_int_33h_0009h(); break;
			case 0x0a: break; // define text cursor
			case 0x0b: msdos_int_33h_000bh(); break;
			case 0x0c: msdos_int_33h_000ch(); break;
			case 0x0d: break; // light pen emulation on
			case 0x0e: break; // light pen emulation off
			case 0x0f: msdos_int_33h_000fh(); break;
			case 0x10: break; // define screen region for updating
			case 0x11: msdos_int_33h_0011h(); break;
			case 0x12: REG16(AX) = 0xffff; break; // set large graphics cursor block
			case 0x13: break; // define double-speed threshold
			case 0x14: msdos_int_33h_0014h(); break;
			case 0x15: msdos_int_33h_0015h(); break;
			case 0x16: msdos_int_33h_0016h(); break;
			case 0x17: msdos_int_33h_0017h(); break;
			case 0x1a: msdos_int_33h_001ah(); break;
			case 0x1b: msdos_int_33h_001bh(); break;
			case 0x1d: msdos_int_33h_001dh(); break;
			case 0x1e: msdos_int_33h_001eh(); break;
			case 0x21: msdos_int_33h_0021h(); break;
			case 0x22: msdos_int_33h_0022h(); break;
			case 0x23: msdos_int_33h_0023h(); break;
			case 0x24: msdos_int_33h_0024h(); break;
			case 0x26: msdos_int_33h_0026h(); break;
			case 0x2a: msdos_int_33h_002ah(); break;
			case 0x2f: break; // mouse hardware reset
			case 0x31: msdos_int_33h_0031h(); break;
			case 0x32: msdos_int_33h_0032h(); break;
			default:
				unimplemented_33h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
				break;
			}
			break;
		default:
			unimplemented_33h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			break;
		}
		break;
	case 0x68:
		// dummy interrupt for EMS (int 67h)
		switch(REG8(AH)) {
		case 0x40: msdos_int_67h_40h(); break;
		case 0x41: msdos_int_67h_41h(); break;
		case 0x42: msdos_int_67h_42h(); break;
		case 0x43: msdos_int_67h_43h(); break;
		case 0x44: msdos_int_67h_44h(); break;
		case 0x45: msdos_int_67h_45h(); break;
		case 0x46: msdos_int_67h_46h(); break;
		case 0x47: msdos_int_67h_47h(); break;
		case 0x48: msdos_int_67h_48h(); break;
		// 0x49: LIM EMS - Reserved - Get I/O Port Address (Undocumented in EMS 3.2)
		// 0x4a: LIM EMS - Reserved - Get Translation Array (Undocumented in EMS 3.2)
		case 0x4b: msdos_int_67h_4bh(); break;
		case 0x4c: msdos_int_67h_4ch(); break;
		case 0x4d: msdos_int_67h_4dh(); break;
		case 0x4e: msdos_int_67h_4eh(); break;
		case 0x4f: msdos_int_67h_4fh(); break;
		case 0x50: msdos_int_67h_50h(); break;
		case 0x51: msdos_int_67h_51h(); break;
		case 0x52: msdos_int_67h_52h(); break;
		case 0x53: msdos_int_67h_53h(); break;
		case 0x54: msdos_int_67h_54h(); break;
		// 0x55: LIM EMS 4.0 - Alter Page Map And JUMP
		// 0x56: LIM EMS 4.0 - Alter Page Map And CALL
		case 0x57: msdos_int_67h_57h(); break;
		case 0x58: msdos_int_67h_58h(); break;
		// 0x59: LIM EMS 4.0 - Get Expanded Memory Hardware Information (for DOS Kernel)
		case 0x5a: msdos_int_67h_5ah(); break;
		// 0x5b: LIM EMS 4.0 - Alternate Map Register Set (for DOS Kernel)
		// 0x5c: LIM EMS 4.0 - Prepate Expanded Memory Hardware For Warm Boot
		// 0x5d: LIM EMS 4.0 - Enable/Disable OS Function Set Functions (for DOS Kernel)
		// 0x60: EEMS - Get Physical Window Array
		// 0x61: EEMS - Generic Accelerator Card Support
		// 0x68: EEMS - Get Address of All Pge Frames om System
		// 0x69: EEMS - Map Page into Frame
		// 0x6a: EEMS - Page Mapping
		// 0xde: VCPI
		case 0xde: msdos_int_67h_deh(); break;
		default:
			unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
			REG8(AH) = 0x84;
			break;
		}
		break;
#ifdef SUPPORT_XMS
	case 0x69:
		// dummy interrupt for XMS (call far)
		try {
			switch(REG8(AH)) {
			case 0x00: msdos_call_xms_00h(); break;
			case 0x01: msdos_call_xms_01h(); break;
			case 0x02: msdos_call_xms_02h(); break;
			case 0x03: msdos_call_xms_03h(); break;
			case 0x04: msdos_call_xms_04h(); break;
			case 0x05: msdos_call_xms_05h(); break;
			case 0x06: msdos_call_xms_06h(); break;
			case 0x07: msdos_call_xms_07h(); break;
			case 0x08: msdos_call_xms_08h(); break;
			case 0x09: msdos_call_xms_09h(); break;
			case 0x0a: msdos_call_xms_0ah(); break;
			case 0x0b: msdos_call_xms_0bh(); break;
			case 0x0c: msdos_call_xms_0ch(); break;
			case 0x0d: msdos_call_xms_0dh(); break;
			case 0x0e: msdos_call_xms_0eh(); break;
			case 0x0f: msdos_call_xms_0fh(); break;
			case 0x10: msdos_call_xms_10h(); break;
			case 0x11: msdos_call_xms_11h(); break;
			case 0x12: msdos_call_xms_12h(); break;
#if defined(HAS_I386)
			case 0x88: msdos_call_xms_88h(); break;
			case 0x89: msdos_call_xms_89h(); break;
			case 0x8e: msdos_call_xms_8eh(); break;
			case 0x8f: msdos_call_xms_8fh(); break;
#endif
			default:
				unimplemented_xms("call XMS (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
				REG16(AX) = 0x0000;
				REG8(BL) = 0x80; // function not implemented
				break;
			}
		} catch(...) {
			REG16(AX) = 0x0000;
			REG8(BL) = 0x8f; // unrecoverable driver error
		}
		break;
#endif
	case 0x6a:
		// irq12 (mouse)
		mouse_push_ax = REG16(AX);
		mouse_push_bx = REG16(BX);
		mouse_push_cx = REG16(CX);
		mouse_push_dx = REG16(DX);
		mouse_push_si = REG16(SI);
		mouse_push_di = REG16(DI);
		
		if(mouse.active && mouse.call_addr.dw != 0) {
			REG16(AX) = mouse.status_irq;
			REG16(BX) = mouse.get_buttons();
			REG16(CX) = max(mouse.min_position.x, min(mouse.max_position.x, mouse.position.x));
			REG16(DX) = max(mouse.min_position.y, min(mouse.max_position.y, mouse.position.y));
			REG16(SI) = REG16(CX) * mouse.mickey.x / 8;
			REG16(DI) = REG16(DX) * mouse.mickey.y / 8;
			
			mem[0xfffd0 + 0x02] = 0x9a;	// call far
			mem[0xfffd0 + 0x03] = mouse.call_addr.w.l & 0xff;
			mem[0xfffd0 + 0x04] = mouse.call_addr.w.l >> 8;
			mem[0xfffd0 + 0x05] = mouse.call_addr.w.h & 0xff;
			mem[0xfffd0 + 0x06] = mouse.call_addr.w.h >> 8;
		} else {
			mem[0xfffd0 + 0x02] = 0x90;	// nop
			mem[0xfffd0 + 0x03] = 0x90;	// nop
			mem[0xfffd0 + 0x04] = 0x90;	// nop
			mem[0xfffd0 + 0x05] = 0x90;	// nop
			mem[0xfffd0 + 0x06] = 0x90;	// nop
		}
		break;
	case 0x6b:
		// end of irq12 (mouse)
		REG16(AX) = mouse_push_ax;
		REG16(BX) = mouse_push_bx;
		REG16(CX) = mouse_push_cx;
		REG16(DX) = mouse_push_dx;
		REG16(SI) = mouse_push_si;
		REG16(DI) = mouse_push_di;
		
		// EOI
		if((pic[1].isr &= ~(1 << 4)) == 0) {
			pic[0].isr &= ~(1 << 2); // master
		}
		pic_update();
		break;
	case 0x6c:
		// dummy interrupt for case map routine pointed in the country info
		if(REG8(AL) >= 0x80) {
			char tmp[2] = {0};
			tmp[0] = REG8(AL);
			my_strupr(tmp);
			REG8(AL) = tmp[0];
		}
		break;
	case 0x6d:
		// dummy interrupt for font read routine pointed by int 15h, ax=5000h
		REG8(AL) = 0x86; // not supported
		m_CF = 1;
		break;
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
		// EOI
		if((pic[1].isr &= ~(1 << (num - 0x70))) == 0) {
			pic[0].isr &= ~(1 << 2); // master
		}
		pic_update();
		break;
	default:
//		fatalerror("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SI), REG16(DI), SREG(DS), SREG(ES));
		break;
	}
	
	// update cursor position
	if(cursor_moved) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		if(!restore_console_on_exit) {
			scr_top = csbi.srWindow.Top;
		}
		mem[0x450 + mem[0x462] * 2] = csbi.dwCursorPosition.X;
		mem[0x451 + mem[0x462] * 2] = csbi.dwCursorPosition.Y - scr_top;
		cursor_moved = false;
	}
}

// init

int msdos_init(int argc, char *argv[], char *envp[], int standard_env)
{
	// init file handler
	memset(file_handler, 0, sizeof(file_handler));
	msdos_file_handler_open(0, "STDIN", _isatty(0), 0, 0x80d3, 0);
	msdos_file_handler_open(1, "STDOUT", _isatty(1), 1, 0x80d3, 0);
	msdos_file_handler_open(2, "STDERR", _isatty(2), 1, 0x80d3, 0);
#ifdef MAP_AUX_DEVICE_TO_FILE
	if(_open("stdaux.txt", _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 3) {
#else
	if(_open("NUL", _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 3) {
#endif
		msdos_file_handler_open(3, "STDAUX", 0, 2, 0x80c0, 0);
	}
#ifdef MAP_PRN_DEVICE_TO_FILE
	if(_open("stdprn.txt", _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 4) {
#else
	if(_open("NUL", _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 4) {
#endif
		msdos_file_handler_open(4, "STDPRN", 0, 1, 0xa8c0, 0);
	}
	_dup2(0, DUP_STDIN);
	_dup2(1, DUP_STDOUT);
	_dup2(2, DUP_STDERR);
	_dup2(3, DUP_STDAUX);
	_dup2(4, DUP_STDPRN);
	
	// init mouse
	memset(&mouse, 0, sizeof(mouse));
	mouse.max_position.x = 8 * scr_width  - 1;
	mouse.max_position.y = 8 * scr_height - 1;
	mouse.mickey.x = 8;
	mouse.mickey.y = 16;
	
#ifdef SUPPORT_XMS
	// init xms
	msdos_xms_init();
#endif
	
	// init process
	memset(process, 0, sizeof(process));
	
	// init dtainfo
	msdos_dta_info_init();
	
	// init memory
	memset(mem, 0, sizeof(mem));
	
	// bios data area
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hStdout, &csbi);
	CONSOLE_FONT_INFO cfi;
	GetCurrentConsoleFont(hStdout, FALSE, &cfi);
	
	int regen = min(scr_width * scr_height * 2, 0x8000);
	text_vram_top_address = TEXT_VRAM_TOP;
	text_vram_end_address = text_vram_top_address + regen;
	shadow_buffer_top_address = SHADOW_BUF_TOP;
	shadow_buffer_end_address = shadow_buffer_top_address + regen;
	
	if(regen > 0x4000) {
		regen = 0x8000;
		vram_pages = 1;
	} else if(regen > 0x2000) {
		regen = 0x4000;
		vram_pages = 2;
	} else if(regen > 0x1000) {
		regen = 0x2000;
		vram_pages = 4;
	} else {
		regen = 0x1000;
		vram_pages = 8;
	}
	
	*(UINT16 *)(mem + 0x400) = 0x3f8; // com1 port address
	*(UINT16 *)(mem + 0x402) = 0x2f8; // com2 port address
	*(UINT16 *)(mem + 0x404) = 0x3e8; // com3 port address
	*(UINT16 *)(mem + 0x406) = 0x2e8; // com4 port address
	*(UINT16 *)(mem + 0x408) = 0x378; // lpt1 port address
//	*(UINT16 *)(mem + 0x40a) = 0x278; // lpt2 port address
//	*(UINT16 *)(mem + 0x40c) = 0x3bc; // lpt3 port address
	*(UINT16 *)(mem + 0x40e) = EXT_BIOS_TOP >> 4;
	*(UINT16 *)(mem + 0x410) = msdos_get_equipment();
	*(UINT16 *)(mem + 0x413) = MEMORY_END / 1024;
	*(UINT8  *)(mem + 0x449) = 0x03;//0x73;
	*(UINT16 *)(mem + 0x44a) = csbi.dwSize.X;
	*(UINT16 *)(mem + 0x44c) = regen;
	*(UINT16 *)(mem + 0x44e) = 0;
	*(UINT8  *)(mem + 0x450) = csbi.dwCursorPosition.X;
	*(UINT8  *)(mem + 0x451) = csbi.dwCursorPosition.Y - scr_top;
	*(UINT8  *)(mem + 0x460) = 7;
	*(UINT8  *)(mem + 0x461) = 7;
	*(UINT8  *)(mem + 0x462) = 0;
	*(UINT16 *)(mem + 0x463) = 0x3d4;
	*(UINT8  *)(mem + 0x465) = 0x09;
	*(UINT32 *)(mem + 0x46c) = get_ticks_since_midnight(timeGetTime());
	*(UINT8  *)(mem + 0x484) = csbi.srWindow.Bottom - csbi.srWindow.Top;
	*(UINT8  *)(mem + 0x485) = cfi.dwFontSize.Y;
	*(UINT8  *)(mem + 0x487) = 0x60;
	*(UINT8  *)(mem + 0x496) = 0x10; // enhanced keyboard installed
	*(UINT16 *)(mem + EXT_BIOS_TOP) = 1;
	
	// initial screen
	SMALL_RECT rect;
	SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
	ReadConsoleOutput(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
	for(int y = 0, ofs1 = TEXT_VRAM_TOP, ofs2 = SHADOW_BUF_TOP; y < scr_height; y++) {
		for(int x = 0; x < scr_width; x++) {
			mem[ofs1++] = mem[ofs2++] = SCR_BUF(y,x).Char.AsciiChar;
			mem[ofs1++] = mem[ofs2++] = SCR_BUF(y,x).Attributes;
		}
	}
	
	// init mcb
	int seg = MEMORY_TOP >> 4;
	
	// iret table
	// note: int 2eh vector should address the routine in command.com,
	// and some softwares invite (int 2eh vector segment) - 1 must address the mcb of command.com.
	// so move iret table into allocated memory block
	// http://www5c.biglobe.ne.jp/~ecb/assembler2/2_6.html
	msdos_mcb_create(seg++, 'M', -1, IRET_SIZE >> 4);
	IRET_TOP = seg << 4;
	seg += IRET_SIZE >> 4;
	memset(mem + IRET_TOP, 0xcf, IRET_SIZE); // iret
	
	// dummy xms/ems device
	msdos_mcb_create(seg++, 'M', -1, XMS_SIZE >> 4);
	XMS_TOP = seg << 4;
	seg += XMS_SIZE >> 4;
	
	// environment
	msdos_mcb_create(seg++, 'M', -1, ENV_SIZE >> 4);
	int env_seg = seg;
	int ofs = 0;
	char env_msdos_path[ENV_SIZE] = "", env_path[ENV_SIZE] = "", env_temp[ENV_SIZE] = "", *path;
	
	if((path = getenv("MSDOS_PATH")) != NULL) {
		strcpy(env_msdos_path, msdos_get_multiple_short_path(path));
		if(env_msdos_path[0] != '\0') {
			strcat(env_path, env_msdos_path);
		}
	}
	if((path = getenv("PATH")) != NULL) {
		if(env_path[0] != '\0') {
			strcat(env_path, ";");
		}
		strcat(env_path, msdos_get_multiple_short_path(path));
	}
	if((path = getenv("MSDOS_TEMP")) != NULL || (path = getenv("TEMP")) != NULL || (path = getenv("TMP")) != NULL) {
		strcpy(env_temp, msdos_get_multiple_short_path(path));
	}
	if((path = getenv("MSDOS_COMSPEC")) != NULL || (path = msdos_search_command_com(argv[0], env_path)) != NULL) {
		strcpy(comspec_path, msdos_get_multiple_short_path(path));
	}
	for(char **p = envp; p != NULL && *p != NULL; p++) {
		// lower to upper
		char tmp[ENV_SIZE], name[ENV_SIZE];
		strcpy(tmp, *p);
		for(int i = 0;; i++) {
			if(tmp[i] == '=') {
				tmp[i] = '\0';
				sprintf(name, ";%s;", tmp);
				my_strupr(name);
				tmp[i] = '=';
				break;
			} else if(tmp[i] >= 'a' && tmp[i] <= 'z') {
				tmp[i] = (tmp[i] - 'a') + 'A';
			}
		}
		if(strncmp(tmp, "MSDOS_COMSPEC=", 14) == 0) {
			// ignore MSDOS_COMSPEC
		} else if(strncmp(tmp, "MSDOS_TEMP=", 11) == 0) {
			// ignore MSDOS_TEMP
		} else if(standard_env && strstr(";COMSPEC;INCLUDE;LIB;MSDOS_PATH;PATH;PROMPT;TEMP;TMP;TZ;", name) == NULL) {
			// ignore non standard environments
		} else {
			if(strncmp(tmp, "COMSPEC=", 8) == 0) {
				strcpy(tmp, "COMSPEC=C:\\COMMAND.COM");
			} else if(strncmp(tmp, "MSDOS_PATH=",  11) == 0) {
				if(env_msdos_path[0] != '\0') {
					sprintf(tmp, "MSDOS_PATH=%s", env_msdos_path);
				} else {
					sprintf(tmp, "MSDOS_PATH=%s", msdos_get_multiple_short_path(tmp + 11));
				}
			} else if(strncmp(tmp, "PATH=",  5) == 0) {
				if(env_path[0] != '\0') {
					sprintf(tmp, "PATH=%s", env_path);
				} else {
					sprintf(tmp, "PATH=%s", msdos_get_multiple_short_path(tmp + 5));
				}
			} else if(strncmp(tmp, "TEMP=", 5) == 0) {
				if(env_temp[0] != '\0') {
					sprintf(tmp, "TEMP=%s", env_temp);
				} else {
					sprintf(tmp, "TEMP=%s", msdos_get_multiple_short_path(tmp + 5));
				}
			} else if(strncmp(tmp, "TMP=",  4) == 0) {
				if(env_temp[0] != '\0') {
					sprintf(tmp, "TMP=%s", env_temp);
				} else {
					sprintf(tmp, "TMP=%s", msdos_get_multiple_short_path(tmp + 4));
				}
			}
			int len = strlen(tmp);
			if(ofs + len + 1 + (2 + (8 + 1 + 3)) + 2 > ENV_SIZE) {
				fatalerror("too many environments\n");
			}
			memcpy(mem + (seg << 4) + ofs, tmp, len);
			ofs += len + 1;
		}
	}
	seg += (ENV_SIZE >> 4);
	
	// psp
	msdos_mcb_create(seg++, 'M', -1, PSP_SIZE >> 4);
	current_psp = seg;
	msdos_psp_create(seg, seg + (PSP_SIZE >> 4), -1, env_seg);
	seg += (PSP_SIZE >> 4);
	
	// first free mcb in conventional memory
	msdos_mcb_create(seg, 'Z', 0, (MEMORY_END >> 4) - seg - 2);
	first_mcb = seg;
	
	// dummy mcb to link to umb
	msdos_mcb_create((MEMORY_END >> 4) - 1, 'M', -1, (UMB_TOP >> 4) - (MEMORY_END >> 4));
	
	// first free mcb in upper memory block
	msdos_mcb_create(UMB_TOP >> 4, 'Z', 0, (UMB_END >> 4) - (UMB_TOP >> 4) - 1);
	
#ifdef SUPPORT_HMA
	// first free mcb in high memory area
	msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
#endif
	
	// interrupt vector
	for(int i = 0; i < 0x80; i++) {
		*(UINT16 *)(mem + 4 * i + 0) = i;
		*(UINT16 *)(mem + 4 * i + 2) = (IRET_TOP >> 4);
	}
	*(UINT16 *)(mem + 4 * 0x08 + 0) = 0x0010;	// fffd:0010 irq0 (system timer)
	*(UINT16 *)(mem + 4 * 0x08 + 2) = 0xfffd;
	*(UINT16 *)(mem + 4 * 0x22 + 0) = 0x0000;	// ffff:0000 boot
	*(UINT16 *)(mem + 4 * 0x22 + 2) = 0xffff;
	*(UINT16 *)(mem + 4 * 0x67 + 0) = 0x0012;	// xxxx:0012 ems
	*(UINT16 *)(mem + 4 * 0x67 + 2) = XMS_TOP >> 4;
	*(UINT16 *)(mem + 4 * 0x74 + 0) = 0x0000;	// fffd:0000 irq12 (mouse)
	*(UINT16 *)(mem + 4 * 0x74 + 2) = 0xfffd;
	
	// dummy devices (NUL -> CON -> ... -> CONFIG$ -> EMMXXXX0)
	static const struct {
		UINT16 attributes;
		char *dev_name;
	} dummy_devices[] = {
		{0x8013, "CON     "},
		{0x8000, "AUX     "},
		{0xa0c0, "PRN     "},
		{0x8008, "CLOCK$  "},
		{0x8000, "COM1    "},
		{0xa0c0, "LPT1    "},
		{0xa0c0, "LPT2    "},
		{0xa0c0, "LPT3    "},
		{0x8000, "COM2    "},
		{0x8000, "COM3    "},
		{0x8000, "COM4    "},
//		{0xc000, "CONFIG$ "},
		{0xc000, "$IBMADSP"}, // for windows3.1 setup.exe
	};
	static const UINT8 dummy_device_routine[] = {
		// from NUL device of Windows 98 SE
		// or word ptr ES:[BX+03],0100
		0x26, 0x81, 0x4f, 0x03, 0x00, 0x01,
		// retf
		0xcb,
	};
	device_t *last = NULL;
	for(int i = 0; i < 12; i++) {
		device_t *device = (device_t *)(mem + DEVICE_TOP + 22 + 18 * i);
		device->next_driver.w.l = 22 + 18 * (i + 1);
		device->next_driver.w.h = DEVICE_TOP >> 4;
		device->attributes = dummy_devices[i].attributes;
		device->strategy = DEVICE_SIZE - sizeof(dummy_device_routine);
		device->interrupt = DEVICE_SIZE - sizeof(dummy_device_routine) + 6;
		memcpy(device->dev_name, dummy_devices[i].dev_name, 8);
		last = device;
	}
	if(last != NULL) {
		last->next_driver.w.l = 0;
		last->next_driver.w.h = XMS_TOP >> 4;
	}
	memcpy(mem + DEVICE_TOP + DEVICE_SIZE - sizeof(dummy_device_routine), dummy_device_routine, sizeof(dummy_device_routine));
	
	// dos info
	dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
	dos_info->magic_word = 1;
	dos_info->first_mcb = MEMORY_TOP >> 4;
	dos_info->first_dpb.w.l = 0;
	dos_info->first_dpb.w.h = DPB_TOP >> 4;
	dos_info->first_sft.w.l = 0;
	dos_info->first_sft.w.h = SFT_TOP >> 4;
	dos_info->clock_device.w.l = 22 + 18 * 3;	// CLOCK$ is the 3rd device in IO.SYS
	dos_info->clock_device.w.h = DEVICE_TOP >> 4;
	dos_info->con_device.w.l = 22 + 18 * 0;		// CON is the 1st device in IO.SYS
	dos_info->con_device.w.h = DEVICE_TOP >> 4;
	dos_info->max_sector_len = 512;
	dos_info->disk_buf_info.w.l = offsetof(dos_info_t, disk_buf_heads);
	dos_info->disk_buf_info.w.h = DOS_INFO_TOP >> 4;
	dos_info->cds.w.l = 0;
	dos_info->cds.w.h = CDS_TOP >> 4;
	dos_info->fcb_table.w.l = 0;
	dos_info->fcb_table.w.h = FCB_TABLE_TOP >> 4;
	dos_info->last_drive = 'Z' - 'A' + 1;
	dos_info->buffers_x = 20;
	dos_info->buffers_y = 0;
	dos_info->boot_drive = 'C' - 'A' + 1;
	dos_info->nul_device.next_driver.w.l = 22;
	dos_info->nul_device.next_driver.w.h = DEVICE_TOP >> 4;
	dos_info->nul_device.attributes = 0x8004;
	dos_info->nul_device.strategy = DOS_INFO_SIZE - sizeof(dummy_device_routine);
	dos_info->nul_device.interrupt = DOS_INFO_SIZE - sizeof(dummy_device_routine) + 6;
	memcpy(dos_info->nul_device.dev_name, "NUL     ", 8);
	dos_info->disk_buf_heads.w.l = 0;
	dos_info->disk_buf_heads.w.h = DISK_BUF_TOP >> 4;
	dos_info->first_umb_fcb = UMB_TOP >> 4;
	dos_info->first_mcb_2 = MEMORY_TOP >> 4;
	memcpy(mem + DOS_INFO_TOP + DOS_INFO_SIZE - sizeof(dummy_device_routine), dummy_device_routine, sizeof(dummy_device_routine));
	
	char *env;
	if((env = getenv("LASTDRIVE")) != NULL) {
		if(env[0] >= 'A' && env[0] <= 'Z') {
			dos_info->last_drive = env[0] - 'A' + 1;
		} else if(env[0] >= 'a' && env[0] <= 'z') {
			dos_info->last_drive = env[0] - 'a' + 1;
		}
	}
	if((env = getenv("windir")) != NULL) {
		if(env[0] >= 'A' && env[0] <= 'Z') {
			dos_info->boot_drive = env[0] - 'A' + 1;
		} else if(env[0] >= 'a' && env[0] <= 'z') {
			dos_info->boot_drive = env[0] - 'a' + 1;
		}
	}
#if defined(HAS_I386)
	dos_info->i386_or_later = 1;
#else
	dos_info->i386_or_later = 0;
#endif
	dos_info->ext_mem_size = (min(MAX_MEM, 0x4000000) - 0x100000) >> 10;
	
	// ems (int 67h) and xms
	device_t *xms_device = (device_t *)(mem + XMS_TOP);
	xms_device->next_driver.w.l = 0xffff;
	xms_device->next_driver.w.h = 0xffff;
	xms_device->attributes = 0xc000;
	xms_device->strategy = XMS_SIZE - sizeof(dummy_device_routine);
	xms_device->interrupt = XMS_SIZE - sizeof(dummy_device_routine) + 6;
	memcpy(xms_device->dev_name, "EMMXXXX0", 8);
	
	mem[XMS_TOP + 0x12] = 0xcd;	// int 68h (dummy)
	mem[XMS_TOP + 0x13] = 0x68;
	mem[XMS_TOP + 0x14] = 0xcf;	// iret
#ifdef SUPPORT_XMS
	if(support_xms) {
		mem[XMS_TOP + 0x15] = 0xcd;	// int 69h (dummy)
		mem[XMS_TOP + 0x16] = 0x69;
		mem[XMS_TOP + 0x17] = 0xcb;	// retf
	} else
#endif
	mem[XMS_TOP + 0x15] = 0xcb;	// retf
	memcpy(mem + XMS_TOP + XMS_SIZE - sizeof(dummy_device_routine), dummy_device_routine, sizeof(dummy_device_routine));
	
	// irq12 routine (mouse)
	mem[0xfffd0 + 0x00] = 0xcd;	// int 6ah (dummy)
	mem[0xfffd0 + 0x01] = 0x6a;
	mem[0xfffd0 + 0x02] = 0x9a;	// call far mouse
	mem[0xfffd0 + 0x03] = 0xff;
	mem[0xfffd0 + 0x04] = 0xff;
	mem[0xfffd0 + 0x05] = 0xff;
	mem[0xfffd0 + 0x06] = 0xff;
	mem[0xfffd0 + 0x07] = 0xcd;	// int 6bh (dummy)
	mem[0xfffd0 + 0x08] = 0x6b;
	mem[0xfffd0 + 0x09] = 0xcf;	// iret
	
	// case map routine
	mem[0xfffd0 + 0x0a] = 0xcd;	// int 6ch (dummy)
	mem[0xfffd0 + 0x0b] = 0x6c;
	mem[0xfffd0 + 0x0c] = 0xcb;	// retf
	
	// font read routine
	mem[0xfffd0 + 0x0d] = 0xcd;	// int 6dh (dummy)
	mem[0xfffd0 + 0x0e] = 0x6d;
	mem[0xfffd0 + 0x0f] = 0xcb;	// retf
	
	// irq0 routine (system time)
	mem[0xfffd0 + 0x10] = 0xcd;	// int 1ch
	mem[0xfffd0 + 0x11] = 0x1c;
	mem[0xfffd0 + 0x12] = 0xea;	// jmp far (IRET_TOP >> 4):0008
	mem[0xfffd0 + 0x13] = 0x08;
	mem[0xfffd0 + 0x14] = 0x00;
	mem[0xfffd0 + 0x15] = ((IRET_TOP >> 4)     ) & 0xff;
	mem[0xfffd0 + 0x16] = ((IRET_TOP >> 4) >> 8) & 0xff;
	
	// boot routine
	mem[0xffff0] = 0xf4;	// halt
	mem[0xffff1] = 0xcd;	// int 21h
	mem[0xffff2] = 0x21;
	mem[0xffff3] = 0xcb;	// retf
	
	mem[0xffff5] = '0';	// rom date
	mem[0xffff6] = '2';
	mem[0xffff7] = '/';
	mem[0xffff8] = '2';
	mem[0xffff9] = '2';
	mem[0xffffa] = '/';
	mem[0xffffb] = '0';
	mem[0xffffc] = '6';
	mem[0xffffe] = 0xfc;	// machine id
	mem[0xfffff] = 0x00;
	
	// param block
	// + 0: param block (22bytes)
	// +24: fcb1/2 (20bytes)
	// +44: command tail (128bytes)
	param_block_t *param = (param_block_t *)(mem + WORK_TOP);
	param->env_seg = 0;
	param->cmd_line.w.l = 44;
	param->cmd_line.w.h = (WORK_TOP >> 4);
	param->fcb1.w.l = 24;
	param->fcb1.w.h = (WORK_TOP >> 4);
	param->fcb2.w.l = 24;
	param->fcb2.w.h = (WORK_TOP >> 4);
	
	memset(mem + WORK_TOP + 24, 0x20, 20);
	
	cmd_line_t *cmd_line = (cmd_line_t *)(mem + WORK_TOP + 44);
	if(argc > 1) {
		sprintf(cmd_line->cmd, " %s", argv[1]);
		for(int i = 2; i < argc; i++) {
			char tmp[128];
			sprintf(tmp, "%s %s", cmd_line->cmd, argv[i]);
			strcpy(cmd_line->cmd, tmp);
		}
		cmd_line->len = (UINT8)strlen(cmd_line->cmd);
	} else {
		cmd_line->len = 0;
	}
	cmd_line->cmd[cmd_line->len] = 0x0d;
	
	// system file table
	*(UINT32 *)(mem + SFT_TOP + 0) = 0xffffffff;
	*(UINT16 *)(mem + SFT_TOP + 4) = 20;
	
	// disk buffer header (from DOSBox)
	*(UINT16 *)(mem + DISK_BUF_TOP +  0) = 0xffff;		// forward ptr
	*(UINT16 *)(mem + DISK_BUF_TOP +  2) = 0xffff;		// backward ptr
	*(UINT8  *)(mem + DISK_BUF_TOP +  4) = 0xff;		// not in use
	*(UINT8  *)(mem + DISK_BUF_TOP + 10) = 0x01;		// number of FATs
	*(UINT32 *)(mem + DISK_BUF_TOP + 13) = 0xffffffff;	// pointer to DPB
	
	// current directory structure
	msdos_cds_update(_getdrive() - 1);
	
	// fcb table
	*(UINT32 *)(mem + FCB_TABLE_TOP + 0) = 0xffffffff;
	*(UINT16 *)(mem + FCB_TABLE_TOP + 4) = 0;
	
	// error table
	*(UINT8 *)(mem + ERR_TABLE_TOP + 0) = 0xff;
	*(UINT8 *)(mem + ERR_TABLE_TOP + 1) = 0x04;
	*(UINT8 *)(mem + ERR_TABLE_TOP + 2) = 0x00;
	*(UINT8 *)(mem + ERR_TABLE_TOP + 3) = 0x00;
	
	// nls stuff
	msdos_nls_tables_init();
	
	// execute command
	try {
		if(msdos_process_exec(argv[0], param, 0)) {
			fatalerror("'%s' not found\n", argv[0]);
		}
	} catch(...) {
		// we should not reach here :-(
		fatalerror("failed to start '%s' because of unexpected exeption\n", argv[0]);
	}
	retval = 0;
	return(0);
}

#define remove_std_file(path) { \
	int fd = _open(path, _O_RDONLY | _O_BINARY); \
	if(fd != -1) { \
		_lseek(fd, 0, SEEK_END); \
		int size = _tell(fd); \
		_close(fd); \
		if(size == 0) { \
			remove(path); \
		} \
	} \
}

void msdos_finish()
{
	for(int i = 0; i < MAX_FILES; i++) {
		if(file_handler[i].valid) {
			_close(i);
		}
	}
#ifdef MAP_AUX_DEVICE_TO_FILE
	remove_std_file("stdaux.txt");
#endif
#ifdef MAP_PRN_DEVICE_TO_FILE
	remove_std_file("stdprn.txt");
#endif
#ifdef SUPPORT_XMS
	msdos_xms_finish();
#endif
	msdos_dbcs_table_finish();
}

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

void hardware_init()
{
	CPU_INIT_CALL(CPU_MODEL);
	CPU_RESET_CALL(CPU_MODEL);
	m_IF = 1;
#if defined(HAS_I386)
	cpu_type = (REG32(EDX) >> 8) & 0x0f;
	cpu_step = (REG32(EDX) >> 0) & 0x0f;
#endif
	i386_set_a20_line(0);
	
	ems_init();
	dma_init();
	pic_init();
	pio_init();
#ifdef PIT_ALWAYS_RUNNING
	pit_init();
#else
	pit_active = 0;
#endif
	sio_init();
	cmos_init();
	kbd_init();
}

void hardware_finish()
{
#if defined(HAS_I386)
	vtlb_free(m_vtlb);
#endif
	ems_finish();
	sio_finish();
}

void hardware_release()
{
	// release hardware resources when this program will be terminated abnormally
#ifdef EXPORT_DEBUG_TO_FILE
	if(fdebug != NULL) {
		fclose(fdebug);
		fdebug = NULL;
	}
#endif
#if defined(HAS_I386)
	vtlb_free(m_vtlb);
#endif
	ems_release();
	sio_release();
}

void hardware_run()
{
	int ops = 0;
	
#ifdef EXPORT_DEBUG_TO_FILE
	// open debug log file after msdos_init() is done not to use the standard file handlers
	fdebug = fopen("debug.log", "w");
#endif
	while(!m_halted) {
#ifdef ENABLE_DEBUG_DASM
		if(dasm > 0) {
			char buffer[256];
#if defined(HAS_I386)
			UINT32 flags = get_flags();
			UINT32 eip = m_eip;
#else
			UINT32 flags = CompressFlags();
			UINT32 eip = m_pc - SREG_BASE(CS);
#endif
			UINT8 *oprom = mem + SREG_BASE(CS) + eip;
			
#if defined(HAS_I386)
			if(m_operand_size) {
				CPU_DISASSEMBLE_CALL(x86_32);
			} else
#endif
			CPU_DISASSEMBLE_CALL(x86_16);
			
			fprintf(fdebug, "AX=%04X  BX=%04X CX=%04X DX=%04X SP=%04X  BP=%04X  SI=%04X  DI=%04X\nDS=%04X  ES=%04X SS=%04X CS=%04X IP=%04X  FLAG=[%s %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]\n",
			REG16(AX), REG16(BX), REG16(CX), REG16(DX), REG16(SP), REG16(BP), REG16(SI), REG16(DI), SREG(DS), SREG(ES), SREG(SS), SREG(CS), eip,
#if defined(HAS_I386)
			PROTECTED_MODE ? "PE" : "--",
#else
			"--",
#endif
			(flags & 0x40000) ? 'A' : '-',
			(flags & 0x20000) ? 'V' : '-',
			(flags & 0x10000) ? 'R' : '-',
			(flags & 0x04000) ? 'N' : '-',
			(flags & 0x02000) ? '1' : '0',
			(flags & 0x01000) ? '1' : '0',
			(flags & 0x00800) ? 'O' : '-',
			(flags & 0x00400) ? 'D' : '-',
			(flags & 0x00200) ? 'I' : '-',
			(flags & 0x00100) ? 'T' : '-',
			(flags & 0x00080) ? 'S' : '-',
			(flags & 0x00040) ? 'Z' : '-',
			(flags & 0x00010) ? 'A' : '-',
			(flags & 0x00004) ? 'P' : '-',
			(flags & 0x00001) ? 'C' : '-');
			fprintf(fdebug, "%04X:%04X\t%s\n", SREG(CS), (unsigned)eip, buffer);
			dasm--;
		}
#endif
#if defined(HAS_I386)
		m_cycles = 1;
		CPU_EXECUTE_CALL(i386);
#else
		CPU_EXECUTE_CALL(CPU_MODEL);
#endif
#if defined(HAS_I386)
		if(m_eip != m_prev_eip) {
#else
		if(m_pc != m_prevpc) {
#endif
			iops++;
		}
		if(++ops == 16384) {
			hardware_update();
			ops = 0;
		}
	}
#ifdef EXPORT_DEBUG_TO_FILE
	if(fdebug != NULL) {
		fclose(fdebug);
		fdebug = NULL;
	}
#endif
}

void hardware_update()
{
	static UINT32 prev_time = 0;
	UINT32 cur_time = timeGetTime();
	
	if(prev_time != cur_time) {
		// update pit and raise irq0
#ifndef PIT_ALWAYS_RUNNING
		if(pit_active)
#endif
		{
			if(pit_run(0, cur_time)) {
				pic_req(0, 0, 1);
			}
			pit_run(1, cur_time);
			pit_run(2, cur_time);
		}
		
		// update sio and raise irq4/3
		for(int c = 0; c < 4; c++) {
			sio_update(c);
		}
		
		// update keyboard and mouse
		static UINT32 prev_tick = 0;
		UINT32 cur_tick = cur_time / 32;
		
		if(prev_tick != cur_tick) {
			// update keyboard flags
			UINT8 state;
			state  = (GetAsyncKeyState(VK_INSERT  ) & 0x0001) ? 0x80 : 0;
			state |= (GetAsyncKeyState(VK_CAPITAL ) & 0x0001) ? 0x40 : 0;
			state |= (GetAsyncKeyState(VK_NUMLOCK ) & 0x0001) ? 0x20 : 0;
			state |= (GetAsyncKeyState(VK_SCROLL  ) & 0x0001) ? 0x10 : 0;
			state |= (GetAsyncKeyState(VK_MENU    ) & 0x8000) ? 0x08 : 0;
			state |= (GetAsyncKeyState(VK_CONTROL ) & 0x8000) ? 0x04 : 0;
			state |= (GetAsyncKeyState(VK_LSHIFT  ) & 0x8000) ? 0x02 : 0;
			state |= (GetAsyncKeyState(VK_RSHIFT  ) & 0x8000) ? 0x01 : 0;
			mem[0x417] = state;
			state  = (GetAsyncKeyState(VK_INSERT  ) & 0x8000) ? 0x80 : 0;
			state |= (GetAsyncKeyState(VK_CAPITAL ) & 0x8000) ? 0x40 : 0;
			state |= (GetAsyncKeyState(VK_NUMLOCK ) & 0x8000) ? 0x20 : 0;
			state |= (GetAsyncKeyState(VK_SCROLL  ) & 0x8000) ? 0x10 : 0;
//			state |= (GetAsyncKeyState(VK_PAUSE   ) & 0x0001) ? 0x08 : 0;
//			state |= (GetAsyncKeyState(VK_SYSREQ  ) & 0x8000) ? 0x04 : 0;
			state |= (GetAsyncKeyState(VK_LMENU   ) & 0x8000) ? 0x02 : 0;
			state |= (GetAsyncKeyState(VK_LCONTROL) & 0x8000) ? 0x01 : 0;
			mem[0x418] = state;
			
			// update console input if needed
			if(!key_changed || mouse.active) {
				update_console_input();
			}
			
			// raise irq1 if key is pressed/released
			if(key_changed) {
				pic_req(0, 1, 1);
				key_changed = false;
			}
			
			// raise irq12 if mouse status is changed
			if(mouse.status & mouse.call_mask) {
				if(mouse.active) {
					pic_req(1, 4, 1);
					mouse.status_irq = mouse.status & mouse.call_mask;
				}
				mouse.status &= ~mouse.call_mask;
			}
			
			prev_tick = cur_tick;
		}
		
		// update daily timer counter
		pcbios_update_daily_timer_counter(cur_time);
		
		prev_time = cur_time;
	}
}

// ems

void ems_init()
{
	memset(ems_handles, 0, sizeof(ems_handles));
	memset(ems_pages, 0, sizeof(ems_pages));
	free_ems_pages = MAX_EMS_PAGES;
}

void ems_finish()
{
	ems_release();
}

void ems_release()
{
	for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
		if(ems_handles[i].buffer != NULL) {
			free(ems_handles[i].buffer);
			ems_handles[i].buffer = NULL;
		}
	}
}

void ems_allocate_pages(int handle, int pages)
{
	if(pages > 0) {
		ems_handles[handle].buffer = (UINT8 *)calloc(1, 0x4000 * pages);
	} else {
		ems_handles[handle].buffer = NULL;
	}
	ems_handles[handle].pages = pages;
	ems_handles[handle].allocated = true;
	free_ems_pages -= pages;
}

void ems_reallocate_pages(int handle, int pages)
{
	if(ems_handles[handle].allocated) {
		if(ems_handles[handle].pages != pages) {
			UINT8 *new_buffer = NULL;
			
			if(pages > 0) {
				new_buffer = (UINT8 *)calloc(1, 0x4000 * pages);
			}
			if(ems_handles[handle].buffer) {
				if(new_buffer) {
					if(pages > ems_handles[handle].pages) {
						memcpy(new_buffer, ems_handles[handle].buffer, 0x4000 * ems_handles[handle].pages);
					} else {
						memcpy(new_buffer, ems_handles[handle].buffer, 0x4000 * pages);
					}
				}
				free(ems_handles[handle].buffer);
				ems_handles[handle].buffer = NULL;
			}
			free_ems_pages += ems_handles[handle].pages;
			
			ems_handles[handle].buffer = new_buffer;
			ems_handles[handle].pages = pages;
			free_ems_pages -= pages;
		}
	} else {
		ems_allocate_pages(handle, pages);
	}
}

void ems_release_pages(int handle)
{
	if(ems_handles[handle].allocated) {
		if(ems_handles[handle].buffer) {
			free(ems_handles[handle].buffer);
			ems_handles[handle].buffer = NULL;
		}
		free_ems_pages += ems_handles[handle].pages;
		ems_handles[handle].allocated = false;
	}
}

void ems_map_page(int physical, int handle, int logical)
{
	if(ems_pages[physical].mapped) {
		if(ems_pages[physical].handle == handle && ems_pages[physical].page == logical) {
			return;
		}
		ems_unmap_page(physical);
	}
	if(ems_handles[handle].allocated && ems_handles[handle].buffer && logical < ems_handles[handle].pages) {
		memcpy(mem + EMS_TOP + 0x4000 * physical, ems_handles[handle].buffer + 0x4000 * logical, 0x4000);
	}
	ems_pages[physical].handle = handle;
	ems_pages[physical].page = logical;
	ems_pages[physical].mapped = true;
}

void ems_unmap_page(int physical)
{
	if(ems_pages[physical].mapped) {
		int handle = ems_pages[physical].handle;
		int logical = ems_pages[physical].page;
		
		if(ems_handles[handle].allocated && ems_handles[handle].buffer && logical < ems_handles[handle].pages) {
			memcpy(ems_handles[handle].buffer + 0x4000 * logical, mem + EMS_TOP + 0x4000 * physical, 0x4000);
		}
		ems_pages[physical].mapped = false;
	}
}

// dma

void dma_init()
{
	memset(dma, 0, sizeof(dma));
	for(int c = 0; c < 2; c++) {
//		for(int ch = 0; ch < 4; ch++) {
//			dma[c].ch[ch].creg.w = dma[c].ch[ch].bcreg.w = 0xffff;
//		}
		dma_reset(c);
	}
}

void dma_reset(int c)
{
	dma[c].low_high = false;
	dma[c].cmd = dma[c].req = dma[c].tc = 0;
	dma[c].mask = 0xff;
}

void dma_write(int c, UINT32 addr, UINT8 data)
{
	int ch = (addr >> 1) & 3;
	UINT8 bit = 1 << (data & 3);
	
	switch(addr & 0x0f) {
	case 0x00: case 0x02: case 0x04: case 0x06:
		if(dma[c].low_high) {
			dma[c].ch[ch].bareg.b.h = data;
		} else {
			dma[c].ch[ch].bareg.b.l = data;
		}
		dma[c].ch[ch].areg.w = dma[c].ch[ch].bareg.w;
		dma[c].low_high = !dma[c].low_high;
		break;
	case 0x01: case 0x03: case 0x05: case 0x07:
		if(dma[c].low_high) {
			dma[c].ch[ch].bcreg.b.h = data;
		} else {
			dma[c].ch[ch].bcreg.b.l = data;
		}
		dma[c].ch[ch].creg.w = dma[c].ch[ch].bcreg.w;
		dma[c].low_high = !dma[c].low_high;
		break;
	case 0x08:
		// command register
		dma[c].cmd = data;
		break;
	case 0x09:
		// dma[c].request register
		if(data & 4) {
			if(!(dma[c].req & bit)) {
				dma[c].req |= bit;
//				dma_run(c, ch);
			}
		} else {
			dma[c].req &= ~bit;
		}
		break;
	case 0x0a:
		// single mask register
		if(data & 4) {
			dma[c].mask |= bit;
		} else {
			dma[c].mask &= ~bit;
		}
		break;
	case 0x0b:
		// mode register
		dma[c].ch[data & 3].mode = data;
		break;
	case 0x0c:
		dma[c].low_high = false;
		break;
	case 0x0d:
		// clear master
		dma_reset(c);
		break;
	case 0x0e:
		// clear mask register
		dma[c].mask = 0;
		break;
	case 0x0f:
		// all mask register
		dma[c].mask = data & 0x0f;
		break;
	}
}

UINT8 dma_read(int c, UINT32 addr)
{
	int ch = (addr >> 1) & 3;
	UINT8 val = 0xff;
	
	switch(addr & 0x0f) {
	case 0x00: case 0x02: case 0x04: case 0x06:
		if(dma[c].low_high) {
			val = dma[c].ch[ch].areg.b.h;
		} else {
			val = dma[c].ch[ch].areg.b.l;
		}
		dma[c].low_high = !dma[c].low_high;
		return(val);
	case 0x01: case 0x03: case 0x05: case 0x07:
		if(dma[c].low_high) {
			val = dma[c].ch[ch].creg.b.h;
		} else {
			val = dma[c].ch[ch].creg.b.l;
		}
		dma[c].low_high = !dma[c].low_high;
		return(val);
	case 0x08:
		// status register
		val = (dma[c].req << 4) | dma[c].tc;
		dma[c].tc = 0;
		return(val);
	case 0x0d:
		// temporary register (intel 82374 does not support)
		return(dma[c].tmp & 0xff);
	case 0x0f:
		// mask register (intel 82374 does support)
		return(dma[c].mask);
	}
	return(0xff);
}

void dma_page_write(int c, int ch, UINT8 data)
{
	dma[c].ch[ch].pagereg = data;
}

UINT8 dma_page_read(int c, int ch)
{
	return(dma[c].ch[ch].pagereg);
}

void dma_run(int c, int ch)
{
	UINT8 bit = 1 << ch;
	
	if((dma[c].req & bit) && !(dma[c].mask & bit)) {
		// execute dma
		while(dma[c].req & bit) {
			if(ch == 0 && (dma[c].cmd & 0x01)) {
				// memory -> memory
				UINT32 saddr = dma[c].ch[0].areg.w | (dma[c].ch[0].pagereg << 16);
				UINT32 daddr = dma[c].ch[1].areg.w | (dma[c].ch[1].pagereg << 16);
				
				if(c == 0) {
					dma[c].tmp = read_byte(saddr);
					write_byte(daddr, dma[c].tmp);
				} else {
					dma[c].tmp = read_word(saddr << 1);
					write_word(daddr << 1, dma[c].tmp);
				}
				if(!(dma[c].cmd & 0x02)) {
					if(dma[c].ch[0].mode & 0x20) {
						dma[c].ch[0].areg.w--;
						if(dma[c].ch[0].areg.w == 0xffff) {
							dma[c].ch[0].pagereg--;
						}
					} else {
						dma[c].ch[0].areg.w++;
						if(dma[c].ch[0].areg.w == 0) {
							dma[c].ch[0].pagereg++;
						}
					}
				}
				if(dma[c].ch[1].mode & 0x20) {
					dma[c].ch[1].areg.w--;
					if(dma[c].ch[1].areg.w == 0xffff) {
						dma[c].ch[1].pagereg--;
					}
				} else {
					dma[c].ch[1].areg.w++;
					if(dma[c].ch[1].areg.w == 0) {
						dma[c].ch[1].pagereg++;
					}
				}
				
				// check dma condition
				if(dma[c].ch[0].creg.w-- == 0) {
					if(dma[c].ch[0].mode & 0x10) {
						// self initialize
						dma[c].ch[0].areg.w = dma[c].ch[0].bareg.w;
						dma[c].ch[0].creg.w = dma[c].ch[0].bcreg.w;
					} else {
//						dma[c].mask |= bit;
					}
				}
				if(dma[c].ch[1].creg.w-- == 0) {
					// terminal count
					if(dma[c].ch[1].mode & 0x10) {
						// self initialize
						dma[c].ch[1].areg.w = dma[c].ch[1].bareg.w;
						dma[c].ch[1].creg.w = dma[c].ch[1].bcreg.w;
					} else {
						dma[c].mask |= bit;
					}
					dma[c].req &= ~bit;
					dma[c].tc |= bit;
				}
			} else {
				UINT32 addr = dma[c].ch[ch].areg.w | (dma[c].ch[ch].pagereg << 16);
				
				if((dma[c].ch[ch].mode & 0x0c) == 0x00) {
					// verify
				} else if((dma[c].ch[ch].mode & 0x0c) == 0x04) {
					// io -> memory
					if(c == 0) {
						dma[c].tmp = read_io_byte(dma[c].ch[ch].port);
						write_byte(addr, dma[c].tmp);
					} else {
						dma[c].tmp = read_io_word(dma[c].ch[ch].port);
						write_word(addr << 1, dma[c].tmp);
					}
				} else if((dma[c].ch[ch].mode & 0x0c) == 0x08) {
					// memory -> io
					if(c == 0) {
						dma[c].tmp = read_byte(addr);
						write_io_byte(dma[c].ch[ch].port, dma[c].tmp);
					} else {
						dma[c].tmp = read_word(addr << 1);
						write_io_word(dma[c].ch[ch].port, dma[c].tmp);
					}
				}
				if(dma[c].ch[ch].mode & 0x20) {
					dma[c].ch[ch].areg.w--;
					if(dma[c].ch[ch].areg.w == 0xffff) {
						dma[c].ch[ch].pagereg--;
					}
				} else {
					dma[c].ch[ch].areg.w++;
					if(dma[c].ch[ch].areg.w == 0) {
						dma[c].ch[ch].pagereg++;
					}
				}
				
				// check dma condition
				if(dma[c].ch[ch].creg.w-- == 0) {
					// terminal count
					if(dma[c].ch[ch].mode & 0x10) {
						// self initialize
						dma[c].ch[ch].areg.w = dma[c].ch[ch].bareg.w;
						dma[c].ch[ch].creg.w = dma[c].ch[ch].bcreg.w;
					} else {
						dma[c].mask |= bit;
					}
					dma[c].req &= ~bit;
					dma[c].tc |= bit;
				} else if((dma[c].ch[ch].mode & 0xc0) == 0x40) {
					// single mode
					break;
				}
			}
		}
	}
}

// pic

void pic_init()
{
	memset(pic, 0, sizeof(pic));
	pic[0].imr = pic[1].imr = 0xff;
	
	// from bochs bios
	pic_write(0, 0, 0x11);	// icw1 = 11h
	pic_write(0, 1, 0x08);	// icw2 = 08h
	pic_write(0, 1, 0x04);	// icw3 = 04h
	pic_write(0, 1, 0x01);	// icw4 = 01h
	pic_write(0, 1, 0xb8);	// ocw1 = b8h
	pic_write(1, 0, 0x11);	// icw1 = 11h
	pic_write(1, 1, 0x70);	// icw2 = 70h
	pic_write(1, 1, 0x02);	// icw3 = 02h
	pic_write(1, 1, 0x01);	// icw4 = 01h
}

void pic_write(int c, UINT32 addr, UINT8 data)
{
	if(addr & 1) {
		if(pic[c].icw2_r) {
			// icw2
			pic[c].icw2 = data;
			pic[c].icw2_r = 0;
		} else if(pic[c].icw3_r) {
			// icw3
			pic[c].icw3 = data;
			pic[c].icw3_r = 0;
		} else if(pic[c].icw4_r) {
			// icw4
			pic[c].icw4 = data;
			pic[c].icw4_r = 0;
		} else {
			// ocw1
			pic[c].imr = data;
		}
	} else {
		if(data & 0x10) {
			// icw1
			pic[c].icw1 = data;
			pic[c].icw2_r = 1;
			pic[c].icw3_r = (data & 2) ? 0 : 1;
			pic[c].icw4_r = data & 1;
			pic[c].irr = 0;
			pic[c].isr = 0;
			pic[c].imr = 0;
			pic[c].prio = 0;
			if(!(pic[c].icw1 & 1)) {
				pic[c].icw4 = 0;
			}
			pic[c].ocw3 = 0;
		} else if(data & 8) {
			// ocw3
			if(!(data & 2)) {
				data = (data & ~1) | (pic[c].ocw3 & 1);
			}
			if(!(data & 0x40)) {
				data = (data & ~0x20) | (pic[c].ocw3 & 0x20);
			}
			pic[c].ocw3 = data;
		} else {
			// ocw2
			int level = 0;
			if(data & 0x40) {
				level = data & 7;
			} else {
				if(!pic[c].isr) {
					return;
				}
				level = pic[c].prio;
				while(!(pic[c].isr & (1 << level))) {
					level = (level + 1) & 7;
				}
			}
			if(data & 0x80) {
				pic[c].prio = (level + 1) & 7;
			}
			if(data & 0x20) {
				pic[c].isr &= ~(1 << level);
			}
		}
	}
	pic_update();
}

UINT8 pic_read(int c, UINT32 addr)
{
	if(addr & 1) {
		return(pic[c].imr);
	} else {
		// polling mode is not supported...
		//if(pic[c].ocw3 & 4) {
		//	return ???;
		//}
		if(pic[c].ocw3 & 1) {
			return(pic[c].isr);
		} else {
			return(pic[c].irr);
		}
	}
}

void pic_req(int c, int level, int signal)
{
	if(signal) {
		pic[c].irr |= (1 << level);
	} else {
		pic[c].irr &= ~(1 << level);
	}
	pic_update();
}

int pic_ack()
{
	// ack (INTA=L)
	pic[pic_req_chip].isr |= pic_req_bit;
	pic[pic_req_chip].irr &= ~pic_req_bit;
	if(pic_req_chip > 0) {
		// update isr and irr of master
		UINT8 slave = 1 << (pic[pic_req_chip].icw3 & 7);
		pic[pic_req_chip - 1].isr |= slave;
		pic[pic_req_chip - 1].irr &= ~slave;
	}
	//if(pic[pic_req_chip].icw4 & 1) {
		// 8086 mode
		int vector = (pic[pic_req_chip].icw2 & 0xf8) | pic_req_level;
	//} else {
	//	// 8080 mode
	//	UINT16 addr = (UINT16)pic[pic_req_chip].icw2 << 8;
	//	if(pic[pic_req_chip].icw1 & 4) {
	//		addr |= (pic[pic_req_chip].icw1 & 0xe0) | (pic_req_level << 2);
	//	} else {
	//		addr |= (pic[pic_req_chip].icw1 & 0xc0) | (pic_req_level << 3);
	//	}
	//	vector = 0xcd | (addr << 8);
	//}
	if(pic[pic_req_chip].icw4 & 2) {
		// auto eoi
		pic[pic_req_chip].isr &= ~pic_req_bit;
	}
	return(vector);
}

void pic_update()
{
	for(int c = 0; c < 2; c++) {
		UINT8 irr = pic[c].irr;
		if(c + 1 < 2) {
			// this is master
			if(pic[c + 1].irr & (~pic[c + 1].imr)) {
				// request from slave
				irr |= 1 << (pic[c + 1].icw3 & 7);
			}
		}
		irr &= (~pic[c].imr);
		if(!irr) {
			break;
		}
		if(!(pic[c].ocw3 & 0x20)) {
			irr |= pic[c].isr;
		}
		int level = pic[c].prio;
		UINT8 bit = 1 << level;
		while(!(irr & bit)) {
			level = (level + 1) & 7;
			bit = 1 << level;
		}
		if((c + 1 < 2) && (pic[c].icw3 & bit)) {
			// check slave
			continue;
		}
		if(pic[c].isr & bit) {
			break;
		}
		// interrupt request
		pic_req_chip = c;
		pic_req_level = level;
		pic_req_bit = bit;
		i386_set_irq_line(INPUT_LINE_IRQ, HOLD_LINE);
		return;
	}
	i386_set_irq_line(INPUT_LINE_IRQ, CLEAR_LINE);
}

// pio

void pio_init()
{
	memset(pio, 0, sizeof(pio));
	for(int c = 0; c < 2; c++) {
		pio[c].stat = 0xde;
		pio[c].ctrl = 0x0c;
	}
}

void pio_write(int c, UINT32 addr, UINT8 data)
{
	switch(addr & 3) {
	case 0:
		pio[c].data = data;
		break;
	case 2:
		pio[c].ctrl = data;
		break;
	}
}

UINT8 pio_read(int c, UINT32 addr)
{
	switch(addr & 3) {
	case 0:
		return(pio[c].data);
	case 1:
		return(pio[c].stat);
	case 2:
		return(pio[c].ctrl);
	}
	return(0xff);
}

// pit

#define PIT_FREQ 1193182ULL
#define PIT_COUNT_VALUE(n) ((pit[n].count_reg == 0) ? 0x10000 : (pit[n].mode == 3 && pit[n].count_reg == 1) ? 0x10001 : pit[n].count_reg)

void pit_init()
{
	memset(pit, 0, sizeof(pit));
	for(int ch = 0; ch < 3; ch++) {
		pit[ch].count = 0x10000;
		pit[ch].ctrl_reg = 0x34;
		pit[ch].mode = 3;
	}
	
	// from bochs bios
	pit_write(3, 0x34);
	pit_write(0, 0x00);
	pit_write(0, 0x00);
}

void pit_write(int ch, UINT8 val)
{
#ifndef PIT_ALWAYS_RUNNING
	if(!pit_active) {
		pit_active = 1;
		pit_init();
	}
#endif
	switch(ch) {
	case 0:
	case 1:
	case 2:
		// write count register
		if(!pit[ch].low_write && !pit[ch].high_write) {
			if(pit[ch].ctrl_reg & 0x10) {
				pit[ch].low_write = 1;
			}
			if(pit[ch].ctrl_reg & 0x20) {
				pit[ch].high_write = 1;
			}
		}
		if(pit[ch].low_write) {
			pit[ch].count_reg = val;
			pit[ch].low_write = 0;
		} else if(pit[ch].high_write) {
			if((pit[ch].ctrl_reg & 0x30) == 0x20) {
				pit[ch].count_reg = val << 8;
			} else {
				pit[ch].count_reg |= val << 8;
			}
			pit[ch].high_write = 0;
		}
		// start count
		if(!pit[ch].low_write && !pit[ch].high_write) {
			if(pit[ch].mode == 0 || pit[ch].mode == 4 || pit[ch].prev_time == 0) {
				pit[ch].count = PIT_COUNT_VALUE(ch);
				pit[ch].prev_time = timeGetTime();
				pit[ch].expired_time = pit[ch].prev_time + pit_get_expired_time(ch);
			}
		}
		break;
	case 3: // ctrl reg
		if((val & 0xc0) == 0xc0) {
			// i8254 read-back command
			for(ch = 0; ch < 3; ch++) {
				if(!(val & 0x10) && !pit[ch].status_latched) {
					pit[ch].status = pit[ch].ctrl_reg & 0x3f;
					pit[ch].status_latched = 1;
				}
				if(!(val & 0x20) && !pit[ch].count_latched) {
					pit_latch_count(ch);
				}
			}
			break;
		}
		ch = (val >> 6) & 3;
		if(val & 0x30) {
			static int modes[8] = {0, 1, 2, 3, 4, 5, 2, 3};
			pit[ch].mode = modes[(val >> 1) & 7];
			pit[ch].count_latched = 0;
			pit[ch].low_read = pit[ch].high_read = 0;
			pit[ch].low_write = pit[ch].high_write = 0;
			pit[ch].ctrl_reg = val;
			// stop count
			pit[ch].prev_time = pit[ch].expired_time = 0;
			pit[ch].count_reg = 0;
		} else if(!pit[ch].count_latched) {
			pit_latch_count(ch);
		}
		break;
	}
}

UINT8 pit_read(int ch)
{
#ifndef PIT_ALWAYS_RUNNING
	if(!pit_active) {
		pit_active = 1;
		pit_init();
	}
#endif
	switch(ch) {
	case 0:
	case 1:
	case 2:
		if(pit[ch].status_latched) {
			pit[ch].status_latched = 0;
			return(pit[ch].status);
		}
		// if not latched, through current count
		if(!pit[ch].count_latched) {
			if(!pit[ch].low_read && !pit[ch].high_read) {
				pit_latch_count(ch);
			}
		}
		// return latched count
		if(pit[ch].low_read) {
			pit[ch].low_read = 0;
			if(!pit[ch].high_read) {
				pit[ch].count_latched = 0;
			}
			return(pit[ch].latch & 0xff);
		} else if(pit[ch].high_read) {
			pit[ch].high_read = 0;
			pit[ch].count_latched = 0;
			return((pit[ch].latch >> 8) & 0xff);
		}
	}
	return(0xff);
}

int pit_run(int ch, UINT32 cur_time)
{
	if(pit[ch].expired_time != 0 && cur_time >= pit[ch].expired_time) {
		pit[ch].count = PIT_COUNT_VALUE(ch);
		pit[ch].prev_time = pit[ch].expired_time;
		pit[ch].expired_time = pit[ch].prev_time + pit_get_expired_time(ch);
		if(cur_time >= pit[ch].expired_time) {
			pit[ch].prev_time = cur_time;
			pit[ch].expired_time = pit[ch].prev_time + pit_get_expired_time(ch);
		}
		return(1);
	}
	return(0);
}

void pit_latch_count(int ch)
{
	if(pit[ch].expired_time != 0) {
		UINT32 cur_time = timeGetTime();
		pit_run(ch, cur_time);
		UINT32 tmp = (pit[ch].count * (pit[ch].expired_time - cur_time)) / (pit[ch].expired_time - pit[ch].prev_time);
		UINT16 latch = (tmp != 0) ? (UINT16)tmp : 1;
		
		if(pit[ch].prev_latch == latch && pit[ch].expired_time > cur_time) {
			// decrement counter in 1msec period
			if(pit[ch].next_latch == 0) {
				tmp = (pit[ch].count * (pit[ch].expired_time - cur_time - 1)) / (pit[ch].expired_time - pit[ch].prev_time);
				pit[ch].next_latch = (tmp != 0) ? (UINT16)tmp : 1;
			}
			if(pit[ch].latch > pit[ch].next_latch) {
				pit[ch].latch--;
			}
		} else {
			pit[ch].prev_latch = pit[ch].latch = latch;
			pit[ch].next_latch = 0;
		}
	} else {
		pit[ch].latch = (UINT16)pit[ch].count;
		pit[ch].prev_latch = pit[ch].next_latch = 0;
	}
	pit[ch].count_latched = 1;
	if((pit[ch].ctrl_reg & 0x30) == 0x10) {
		// lower byte
		pit[ch].low_read = 1;
		pit[ch].high_read = 0;
	} else if((pit[ch].ctrl_reg & 0x30) == 0x20) {
		// upper byte
		pit[ch].low_read = 0;
		pit[ch].high_read = 1;
	} else {
		// lower -> upper
		pit[ch].low_read = pit[ch].high_read = 1;
	}
}

int pit_get_expired_time(int ch)
{
	pit[ch].accum += 1024ULL * 1000ULL * (UINT64)pit[ch].count / PIT_FREQ;
	UINT64 val = pit[ch].accum >> 10;
	pit[ch].accum -= val << 10;
	return((val != 0) ? val : 1);
}

// sio

void sio_init()
{
	memset(sio, 0, sizeof(sio));
	memset(sio_mt, 0, sizeof(sio_mt));
	
	for(int c = 0; c < 4; c++) {
		sio[c].send_buffer = new FIFO(SIO_BUFFER_SIZE);
		sio[c].recv_buffer = new FIFO(SIO_BUFFER_SIZE);
		
		sio[c].divisor.w = 12;		// 115200Hz / 9600Baud
		sio[c].line_ctrl = 0x03;	// 8bit, stop 1bit, non parity
		sio[c].modem_ctrl = 0x03;	// rts=on, dtr=on
		sio[c].set_rts = sio[c].set_dtr = true;
		sio[c].line_stat_buf = 0x60;	// send/recv buffers are empty
		sio[c].irq_identify = 0x01;	// no pending irq
		
		InitializeCriticalSection(&sio_mt[c].csSendData);
		InitializeCriticalSection(&sio_mt[c].csRecvData);
		InitializeCriticalSection(&sio_mt[c].csLineCtrl);
		InitializeCriticalSection(&sio_mt[c].csLineStat);
		InitializeCriticalSection(&sio_mt[c].csModemCtrl);
		InitializeCriticalSection(&sio_mt[c].csModemStat);
		
		if(sio_port_number[c] != 0) {
			sio[c].channel = c;
			sio_mt[c].hThread = CreateThread(NULL, 0, sio_thread, &sio[c], 0, NULL);
		}
	}
}

void sio_finish()
{
	for(int c = 0; c < 4; c++) {
		if(sio_mt[c].hThread != NULL) {
			WaitForSingleObject(sio_mt[c].hThread, INFINITE);
			CloseHandle(sio_mt[c].hThread);
			sio_mt[c].hThread = NULL;
		}
		DeleteCriticalSection(&sio_mt[c].csSendData);
		DeleteCriticalSection(&sio_mt[c].csRecvData);
		DeleteCriticalSection(&sio_mt[c].csLineCtrl);
		DeleteCriticalSection(&sio_mt[c].csLineStat);
		DeleteCriticalSection(&sio_mt[c].csModemCtrl);
		DeleteCriticalSection(&sio_mt[c].csModemStat);
	}
	sio_release();
}

void sio_release()
{
	for(int c = 0; c < 4; c++) {
		// sio_thread() may access the resources :-(
		if(sio_mt[c].hThread == NULL) {
			if(sio[c].send_buffer != NULL) {
				sio[c].send_buffer->release();
				delete sio[c].send_buffer;
				sio[c].send_buffer = NULL;
			}
			if(sio[c].recv_buffer != NULL) {
				sio[c].recv_buffer->release();
				delete sio[c].recv_buffer;
				sio[c].recv_buffer = NULL;
			}
		}
	}
}

void sio_write(int c, UINT32 addr, UINT8 data)
{
	switch(addr & 7) {
	case 0:
		if(sio[c].selector & 0x80) {
			if(sio[c].divisor.b.l != data) {
				EnterCriticalSection(&sio_mt[c].csLineCtrl);
				sio[c].divisor.b.l = data;
				LeaveCriticalSection(&sio_mt[c].csLineCtrl);
			}
		} else {
			EnterCriticalSection(&sio_mt[c].csSendData);
			sio[c].send_buffer->write(data);
			// transmitter holding/shift registers are not empty
			sio[c].line_stat_buf &= ~0x60;
			LeaveCriticalSection(&sio_mt[c].csSendData);
			
			if(sio[c].irq_enable & 0x02) {
				sio_update_irq(c);
			}
		}
		break;
	case 1:
		if(sio[c].selector & 0x80) {
			if(sio[c].divisor.b.h != data) {
				EnterCriticalSection(&sio_mt[c].csLineCtrl);
				sio[c].divisor.b.h = data;
				LeaveCriticalSection(&sio_mt[c].csLineCtrl);
			}
		} else {
			if(sio[c].irq_enable != data) {
				sio[c].irq_enable = data;
				sio_update_irq(c);
			}
		}
		break;
	case 3:
		{
			UINT8 line_ctrl = data & 0x3f;
			bool set_brk = ((data & 0x40) != 0);
			
			if(sio[c].line_ctrl != line_ctrl) {
				EnterCriticalSection(&sio_mt[c].csLineCtrl);
				sio[c].line_ctrl = line_ctrl;
				LeaveCriticalSection(&sio_mt[c].csLineCtrl);
			}
			if(sio[c].set_brk != set_brk) {
				EnterCriticalSection(&sio_mt[c].csModemCtrl);
				sio[c].set_brk = set_brk;
				LeaveCriticalSection(&sio_mt[c].csModemCtrl);
			}
		}
		sio[c].selector = data;
		break;
	case 4:
		{
			bool set_dtr = ((data & 0x01) != 0);
			bool set_rts = ((data & 0x02) != 0);
			
			if(sio[c].set_dtr != set_dtr || sio[c].set_rts != set_rts) {
//				EnterCriticalSection(&sio_mt[c].csModemCtrl);
				sio[c].set_dtr = set_dtr;
				sio[c].set_rts = set_rts;
//				LeaveCriticalSection(&sio_mt[c].csModemCtrl);
				
				bool state_changed = false;
				
				EnterCriticalSection(&sio_mt[c].csModemStat);
				if(set_dtr) {
					sio[c].modem_stat |= 0x20;	// dsr on
				} else {
					sio[c].modem_stat &= ~0x20;	// dsr off
				}
				if(set_rts) {
					sio[c].modem_stat |= 0x10;	// cts on
				} else {
					sio[c].modem_stat &= ~0x10;	// cts off
				}
				if((sio[c].prev_modem_stat & 0x20) != (sio[c].modem_stat & 0x20)) {
					if(!(sio[c].modem_stat & 0x02)) {
						if(sio[c].irq_enable & 0x08) {
							state_changed = true;
						}
						sio[c].modem_stat |= 0x02;
					}
				}
				if((sio[c].prev_modem_stat & 0x10) != (sio[c].modem_stat & 0x10)) {
					if(!(sio[c].modem_stat & 0x01)) {
						if(sio[c].irq_enable & 0x08) {
							state_changed = true;
						}
						sio[c].modem_stat |= 0x01;
					}
				}
				LeaveCriticalSection(&sio_mt[c].csModemStat);
				
				if(state_changed) {
					sio_update_irq(c);
				}
			}
		}
		sio[c].modem_ctrl = data;
		break;
	case 7:
		sio[c].scratch = data;
		break;
	}
}

UINT8 sio_read(int c, UINT32 addr)
{
	switch(addr & 7) {
	case 0:
		if(sio[c].selector & 0x80) {
			return(sio[c].divisor.b.l);
		} else {
			EnterCriticalSection(&sio_mt[c].csRecvData);
			UINT8 data = sio[c].recv_buffer->read();
			// data is not ready
			sio[c].line_stat_buf &= ~0x01;
			LeaveCriticalSection(&sio_mt[c].csRecvData);
			
			if(sio[c].irq_enable & 0x01) {
				sio_update_irq(c);
			}
			return(data);
		}
	case 1:
		if(sio[c].selector & 0x80) {
			return(sio[c].divisor.b.h);
		} else {
			return(sio[c].irq_enable);
		}
	case 2:
		return(sio[c].irq_identify);
	case 3:
		return(sio[c].selector);
	case 4:
		return(sio[c].modem_ctrl);
	case 5:
		{
			EnterCriticalSection(&sio_mt[c].csLineStat);
			UINT8 val = sio[c].line_stat_err | sio[c].line_stat_buf;
			sio[c].line_stat_err = 0x00;
			LeaveCriticalSection(&sio_mt[c].csLineStat);
			
			bool state_changed = false;
			
			if((sio[c].line_stat_buf & 0x60) == 0x00) {
				EnterCriticalSection(&sio_mt[c].csSendData);
				if(!sio[c].send_buffer->full()) {
					// transmitter holding register will be empty first
					if(sio[c].irq_enable & 0x02) {
						state_changed = true;
					}
					sio[c].line_stat_buf |= 0x20;
				}
				LeaveCriticalSection(&sio_mt[c].csSendData);
			} else if((sio[c].line_stat_buf & 0x60) == 0x20) {
				// transmitter shift register will be empty later
				sio[c].line_stat_buf |= 0x40;
			}
			if(!(sio[c].line_stat_buf & 0x01)) {
				EnterCriticalSection(&sio_mt[c].csRecvData);
				if(!sio[c].recv_buffer->empty()) {
					// data is ready
					if(sio[c].irq_enable & 0x01) {
						state_changed = true;
					}
					sio[c].line_stat_buf |= 0x01;
				}
				LeaveCriticalSection(&sio_mt[c].csRecvData);
			}
			if(state_changed) {
				sio_update_irq(c);
			}
			return(val);
		}
	case 6:
		{
			EnterCriticalSection(&sio_mt[c].csModemStat);
			UINT8 val = sio[c].modem_stat;
			sio[c].modem_stat &= 0xf0;
			sio[c].prev_modem_stat = sio[c].modem_stat;
			LeaveCriticalSection(&sio_mt[c].csModemStat);
			
			if(sio[c].modem_ctrl & 0x10) {
				// loop-back
				val &= 0x0f;
				val |= (sio[c].modem_ctrl & 0x0c) << 4;
				val |= (sio[c].modem_ctrl & 0x01) << 5;
				val |= (sio[c].modem_ctrl & 0x02) << 3;
			}
			return(val);
		}
	case 7:
		return(sio[c].scratch);
	}
	return(0xff);
}

void sio_update(int c)
{
	if((sio[c].line_stat_buf & 0x60) == 0x00) {
		EnterCriticalSection(&sio_mt[c].csSendData);
		if(!sio[c].send_buffer->full()) {
			// transmitter holding/shift registers will be empty
			sio[c].line_stat_buf |= 0x60;
		}
		LeaveCriticalSection(&sio_mt[c].csSendData);
	} else if((sio[c].line_stat_buf & 0x60) == 0x20) {
		// transmitter shift register will be empty
		sio[c].line_stat_buf |= 0x40;
	}
	if(!(sio[c].line_stat_buf & 0x01)) {
		EnterCriticalSection(&sio_mt[c].csRecvData);
		if(!sio[c].recv_buffer->empty()) {
			// data is ready
			sio[c].line_stat_buf |= 0x01;
		}
		LeaveCriticalSection(&sio_mt[c].csRecvData);
	}
	sio_update_irq(c);
}

void sio_update_irq(int c)
{
	int level = -1;
	
	if(sio[c].irq_enable & 0x08) {
		EnterCriticalSection(&sio_mt[c].csModemStat);
		if((sio[c].modem_stat & 0x0f) != 0) {
			level = 0;
		}
		EnterCriticalSection(&sio_mt[c].csModemStat);
	}
	if(sio[c].irq_enable & 0x02) {
		if(sio[c].line_stat_buf & 0x20) {
			level = 1;
		}
	}
	if(sio[c].irq_enable & 0x01) {
		if(sio[c].line_stat_buf & 0x01) {
			level = 2;
		}
	}
	if(sio[c].irq_enable & 0x04) {
		EnterCriticalSection(&sio_mt[c].csLineStat);
		if(sio[c].line_stat_err != 0) {
			level = 3;
		}
		LeaveCriticalSection(&sio_mt[c].csLineStat);
	}
	
	// COM1 and COM3 shares IRQ4, COM2 and COM4 shares IRQ3
	if(level != -1) {
		sio[c].irq_identify = level << 1;
		pic_req(0, (c == 0 || c == 2) ? 4 : 3, 1);
	} else {
		sio[c].irq_identify = 1;
		pic_req(0, (c == 0 || c == 2) ? 4 : 3, 0);
	}
}

DWORD WINAPI sio_thread(void *lpx)
{
	volatile sio_t *p = (sio_t *)lpx;
	sio_mt_t *q = &sio_mt[p->channel];
	
	char name[] = "COM1";
	name[3] = '0' + sio_port_number[p->channel];
	HANDLE hComm = NULL;
	COMMPROP commProp;
	DCB dcb;
	DWORD dwSettableBaud = 0xffb; // 75, 110, 150, 300, 600, 1200, 1800, 2400, 4800, 7200, and 9600bps
	BYTE bytBuffer[SIO_BUFFER_SIZE];
	
	if((hComm = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
		if(GetCommProperties(hComm, &commProp)) {
			dwSettableBaud = commProp.dwSettableBaud;
		}
		EscapeCommFunction(hComm, CLRBREAK);
//		EscapeCommFunction(hComm, SETRTS);
//		EscapeCommFunction(hComm, SETDTR);
		
		while(!m_halted) {
			// setup comm port
			bool comm_state_changed = false;
			
			EnterCriticalSection(&q->csLineCtrl);
			if((p->prev_divisor != p->divisor.w || p->prev_line_ctrl != p->line_ctrl) && p->divisor.w != 0) {
				p->prev_divisor = p->divisor.w;
				p->prev_line_ctrl = p->line_ctrl;
				comm_state_changed = true;
			}
			LeaveCriticalSection(&q->csLineCtrl);
			
			if(comm_state_changed) {
				if(GetCommState(hComm, &dcb)) {
//					dcb.BaudRate = min(9600, 115200 / p->prev_divisor);
					DWORD baud = 115200 / p->prev_divisor;
					dcb.BaudRate = 9600; // default
					
					if((dwSettableBaud & BAUD_075  ) && baud >= 75   ) dcb.BaudRate = 75;
					if((dwSettableBaud & BAUD_110  ) && baud >= 110  ) dcb.BaudRate = 110;
					// 134.5bps is not supported ???
//					if((dwSettableBaud & BAUD_134_5) && baud >= 134.5) dcb.BaudRate = 134.5;
					if((dwSettableBaud & BAUD_150  ) && baud >= 150  ) dcb.BaudRate = 150;
					if((dwSettableBaud & BAUD_300  ) && baud >= 300  ) dcb.BaudRate = 300;
					if((dwSettableBaud & BAUD_600  ) && baud >= 600  ) dcb.BaudRate = 600;
					if((dwSettableBaud & BAUD_1200 ) && baud >= 1200 ) dcb.BaudRate = 1200;
					if((dwSettableBaud & BAUD_1800 ) && baud >= 1800 ) dcb.BaudRate = 1800;
					if((dwSettableBaud & BAUD_2400 ) && baud >= 2400 ) dcb.BaudRate = 2400;
					if((dwSettableBaud & BAUD_4800 ) && baud >= 4800 ) dcb.BaudRate = 4800;
					if((dwSettableBaud & BAUD_7200 ) && baud >= 7200 ) dcb.BaudRate = 7200;
					if((dwSettableBaud & BAUD_9600 ) && baud >= 9600 ) dcb.BaudRate = 9600;
//					if((dwSettableBaud & BAUD_14400) && baud >= 14400) dcb.BaudRate = 14400;
//					if((dwSettableBaud & BAUD_19200) && baud >= 19200) dcb.BaudRate = 19200;
//					if((dwSettableBaud & BAUD_38400) && baud >= 38400) dcb.BaudRate = 38400;
					
					switch(p->prev_line_ctrl & 0x03) {
					case 0x00: dcb.ByteSize = 5; break;
					case 0x01: dcb.ByteSize = 6; break;
					case 0x02: dcb.ByteSize = 7; break;
					case 0x03: dcb.ByteSize = 8; break;
					}
					switch(p->prev_line_ctrl & 0x04) {
					case 0x00: dcb.StopBits = ONESTOPBIT; break;
					case 0x04: dcb.StopBits = (dcb.ByteSize == 5) ? ONE5STOPBITS : TWOSTOPBITS; break;
					}
					switch(p->prev_line_ctrl & 0x38) {
					case 0x08: dcb.Parity = ODDPARITY;   break;
					case 0x18: dcb.Parity = EVENPARITY;  break;
					case 0x28: dcb.Parity = MARKPARITY;  break;
					case 0x38: dcb.Parity = SPACEPARITY; break;
					default:   dcb.Parity = NOPARITY;    break;
					}
					dcb.fBinary = TRUE;
					dcb.fParity = (dcb.Parity != NOPARITY);
					dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = TRUE;
					dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
					dcb.fDsrSensitivity = FALSE;//TRUE;
					dcb.fTXContinueOnXoff = TRUE;
					dcb.fOutX = dcb.fInX = FALSE;
					dcb.fErrorChar = FALSE;
					dcb.fNull = FALSE;
					dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
					dcb.fAbortOnError = FALSE;
					
					SetCommState(hComm, &dcb);
				}
				
				// check again to apply all comm state changes
				Sleep(10);
				continue;
			}
			
			// set comm pins
			bool change_brk = false;
//			bool change_rts = false;
//			bool change_dtr = false;
			
			EnterCriticalSection(&q->csModemCtrl);
			if(p->prev_set_brk != p->set_brk) {
				p->prev_set_brk = p->set_brk;
				change_brk = true;
			}
//			if(p->prev_set_rts != p->set_rts) {
//				p->prev_set_rts = p->set_rts;
//				change_rts = true;
//			}
//			if(p->prev_set_dtr != p->set_dtr) {
//				p->prev_set_dtr = p->set_dtr;
//				change_dtr = true;
//			}
			LeaveCriticalSection(&q->csModemCtrl);
			
			if(change_brk) {
				static UINT32 clear_time = 0;
				if(p->prev_set_brk) {
					EscapeCommFunction(hComm, SETBREAK);
					clear_time = timeGetTime() + 200;
				} else {
					// keep break for at least 200msec
					UINT32 cur_time = timeGetTime();
					if(clear_time > cur_time) {
						Sleep(clear_time - cur_time);
					}
					EscapeCommFunction(hComm, CLRBREAK);
				}
			}
//			if(change_rts) {
//				if(p->prev_set_rts) {
//					EscapeCommFunction(hComm, SETRTS);
//				} else {
//					EscapeCommFunction(hComm, CLRRTS);
//				}
//			}
//			if(change_dtr) {
//				if(p->prev_set_dtr) {
//					EscapeCommFunction(hComm, SETDTR);
//				} else {
//					EscapeCommFunction(hComm, CLRDTR);
//				}
//			}
			
			// get comm pins
			DWORD dwModemStat = 0;
			
			if(GetCommModemStatus(hComm, &dwModemStat)) {
				EnterCriticalSection(&q->csModemStat);
				if(dwModemStat & MS_RLSD_ON) {
					p->modem_stat |= 0x80;
				} else {
					p->modem_stat &= ~0x80;
				}
				if(dwModemStat & MS_RING_ON) {
					p->modem_stat |= 0x40;
				} else {
					p->modem_stat &= ~0x40;
				}
//				if(dwModemStat & MS_DSR_ON) {
//					p->modem_stat |= 0x20;
//				} else {
//					p->modem_stat &= ~0x20;
//				}
//				if(dwModemStat & MS_CTS_ON) {
//					p->modem_stat |= 0x10;
//				} else {
//					p->modem_stat &= ~0x10;
//				}
				if((p->prev_modem_stat & 0x80) != (p->modem_stat & 0x80)) {
					p->modem_stat |= 0x08;
				}
				if((p->prev_modem_stat & 0x40) && !(p->modem_stat & 0x40)) {
					p->modem_stat |= 0x04;
				}
//				if((p->prev_modem_stat & 0x20) != (p->modem_stat & 0x20)) {
//					p->modem_stat |= 0x02;
//				}
//				if((p->prev_modem_stat & 0x10) != (p->modem_stat & 0x10)) {
//					p->modem_stat |= 0x01;
//				}
				LeaveCriticalSection(&q->csModemStat);
			}
			
			// send data
			DWORD dwSend = 0;
			
			EnterCriticalSection(&q->csSendData);
			while(!p->send_buffer->empty()) {
				bytBuffer[dwSend++] = p->send_buffer->read();
			}
			LeaveCriticalSection(&q->csSendData);
			
			if(dwSend != 0) {
				DWORD dwWritten = 0;
				WriteFile(hComm, bytBuffer, dwSend, &dwWritten, NULL);
			}
			
			// get line status and recv data
			DWORD dwLineStat = 0;
			COMSTAT comStat;
			
			if(ClearCommError(hComm, &dwLineStat, &comStat)) {
				EnterCriticalSection(&q->csLineStat);
				if(dwLineStat & CE_BREAK) {
					p->line_stat_err |= 0x10;
				}
				if(dwLineStat & CE_FRAME) {
					p->line_stat_err |= 0x08;
				}
				if(dwLineStat & CE_RXPARITY) {
					p->line_stat_err |= 0x04;
				}
				if(dwLineStat & CE_OVERRUN) {
					p->line_stat_err |= 0x02;
				}
				LeaveCriticalSection(&q->csLineStat);
				
				if(comStat.cbInQue != 0) {
					EnterCriticalSection(&q->csRecvData);
					DWORD dwRecv = min(comStat.cbInQue, p->recv_buffer->remain());
					LeaveCriticalSection(&q->csRecvData);
					
					if(dwRecv != 0) {
						DWORD dwRead = 0;
						if(ReadFile(hComm, bytBuffer, dwRecv, &dwRead, NULL) && dwRead != 0) {
							EnterCriticalSection(&q->csRecvData);
							for(int i = 0; i < dwRead; i++) {
								p->recv_buffer->write(bytBuffer[i]);
							}
							LeaveCriticalSection(&q->csRecvData);
						}
					}
				}
			}
			Sleep(10);
		}
		CloseHandle(hComm);
	}
	return 0;
}

// cmos

void cmos_init()
{
	memset(cmos, 0, sizeof(cmos));
	cmos_addr = 0;
	
	// from DOSBox
	cmos_write(0x0a, 0x26);
	cmos_write(0x0b, 0x02);
	cmos_write(0x0d, 0x80);
}

void cmos_write(int addr, UINT8 val)
{
	cmos[addr & 0x7f] = val;
}

#define CMOS_GET_TIME() { \
	UINT32 cur_sec = timeGetTime() / 1000 ; \
	if(prev_sec != cur_sec) { \
		GetLocalTime(&time); \
		prev_sec = cur_sec; \
	} \
}
#define CMOS_BCD(v) ((cmos[0x0b] & 4) ? (v) : to_bcd(v))

UINT8 cmos_read(int addr)
{
	static SYSTEMTIME time;
	static UINT32 prev_sec = 0;
	
	switch(addr & 0x7f) {
	case 0x00: CMOS_GET_TIME(); return(CMOS_BCD(time.wSecond));
	case 0x02: CMOS_GET_TIME(); return(CMOS_BCD(time.wMinute));
	case 0x04: CMOS_GET_TIME(); return(CMOS_BCD(time.wHour));
	case 0x06: CMOS_GET_TIME(); return(time.wDayOfWeek + 1);
	case 0x07: CMOS_GET_TIME(); return(CMOS_BCD(time.wDay));
	case 0x08: CMOS_GET_TIME(); return(CMOS_BCD(time.wMonth));
	case 0x09: CMOS_GET_TIME(); return(CMOS_BCD(time.wYear));
//	case 0x0a: return((cmos[0x0a] & 0x7f) | ((timeGetTime() % 1000) < 2 ? 0x80 : 0));	// 2msec
	case 0x0a: return((cmos[0x0a] & 0x7f) | ((timeGetTime() % 1000) < 20 ? 0x80 : 0));	// precision of timeGetTime() may not be 1msec
	case 0x15: return((MEMORY_END >> 10) & 0xff);
	case 0x16: return((MEMORY_END >> 18) & 0xff);
	case 0x17: return(((MAX_MEM - 0x100000) >> 10) & 0xff);
	case 0x18: return(((MAX_MEM - 0x100000) >> 18) & 0xff);
	case 0x30: return(((MAX_MEM - 0x100000) >> 10) & 0xff);
	case 0x31: return(((MAX_MEM - 0x100000) >> 18) & 0xff);
	case 0x32: CMOS_GET_TIME(); return(CMOS_BCD(time.wYear / 100));
	}
	return(cmos[addr & 0x7f]);
}

// kbd (a20)

void kbd_init()
{
	kbd_data = kbd_command = 0;
	kbd_status = 0x18;
}

UINT8 kbd_read_data()
{
	kbd_status &= ~1;
	return(kbd_data);
}

void kbd_write_data(UINT8 val)
{
	switch(kbd_command) {
	case 0xd1:
		i386_set_a20_line((val >> 1) & 1);
		break;
	}
	kbd_command = 0;
	kbd_status &= ~8;
}

UINT8 kbd_read_status()
{
	return(kbd_status);
}

void kbd_write_command(UINT8 val)
{
	switch(val) {
	case 0xd0:
		kbd_data = ((m_a20_mask >> 19) & 2) | 1;
		kbd_status |= 1;
		break;
	case 0xdd:
		i386_set_a20_line(0);
		break;
	case 0xdf:
		i386_set_a20_line(1);
		break;
	case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
	case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
		if(!(val & 1)) {
			if((cmos[0x0f] & 0x7f) == 5) {
				// reset pic
				pic_init();
				pic[0].irr = pic[1].irr = 0x00;
				pic[0].imr = pic[1].imr = 0xff;
			}
			CPU_RESET_CALL(CPU_MODEL);
			i386_jmp_far(0x40, 0x67);
		}
		i386_set_a20_line((val >> 1) & 1);
		break;
	}
	kbd_command = val;
	kbd_status |= 8;
}

// vga

UINT8 vga_read_status()
{
	// 60hz
	static const int period[3] = {16, 17, 17};
	static int index = 0;
	UINT32 time = timeGetTime() % period[index];
	
	index = (index + 1) % 3;
	return((time < 4 ? 0x08 : 0) | (time == 0 ? 0 : 0x01));
}

// i/o bus

// this is ugly patch for SW1US.EXE, it sometimes mistakely read/write 01h-10h for serial I/O
//#define SW1US_PATCH

#ifdef ENABLE_DEBUG_IOPORT
UINT8 read_io_byte_debug(offs_t addr);

UINT8 read_io_byte(offs_t addr)
{
	UINT8 val = read_io_byte_debug(addr);
	if(fdebug != NULL) {
		fprintf(fdebug, "inb %04X, %02X\n", addr, val);
	}
	return(val);
}

UINT8 read_io_byte_debug(offs_t addr)
#else
UINT8 read_io_byte(offs_t addr)
#endif
{
	switch(addr) {
#ifdef SW1US_PATCH
	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08:
	case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: case 0x10:
		return(sio_read(0, addr - 1));
#else
	case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		return(dma_read(0, addr));
#endif
	case 0x20: case 0x21:
		return(pic_read(0, addr));
	case 0x40: case 0x41: case 0x42: case 0x43:
		return(pit_read(addr & 0x03));
	case 0x60:
		return(kbd_read_data());
	case 0x61:
		return(system_port);
	case 0x64:
		return(kbd_read_status());
	case 0x71:
		return(cmos_read(cmos_addr));
	case 0x81:
		return(dma_page_read(0, 2));
	case 0x82:
		return(dma_page_read(0, 3));
	case 0x83:
		return(dma_page_read(0, 1));
	case 0x87:
		return(dma_page_read(0, 0));
	case 0x89:
		return(dma_page_read(1, 2));
	case 0x8a:
		return(dma_page_read(1, 3));
	case 0x8b:
		return(dma_page_read(1, 1));
	case 0x8f:
		return(dma_page_read(1, 0));
	case 0x92:
		return((m_a20_mask >> 19) & 2);
	case 0xa0: case 0xa1:
		return(pic_read(1, addr));
	case 0xc0: case 0xc2: case 0xc4: case 0xc6: case 0xc8: case 0xca: case 0xcc: case 0xce:
	case 0xd0: case 0xd2: case 0xd4: case 0xd6: case 0xd8: case 0xda: case 0xdc: case 0xde:
		return(dma_read(1, (addr - 0xc0) >> 1));
//	case 0x278: case 0x279: case 0x27a:
//		return(pio_read(1, addr));
	case 0x2e8: case 0x2e9: case 0x2ea: case 0x2eb: case 0x2ec: case 0x2ed: case 0x2ee: case 0x2ef:
		return(sio_read(3, addr));
	case 0x2f8: case 0x2f9: case 0x2fa: case 0x2fb: case 0x2fc: case 0x2fd: case 0x2fe: case 0x2ff:
		return(sio_read(1, addr));
	case 0x378: case 0x379: case 0x37a:
		return(pio_read(0, addr));
	case 0x3ba: case 0x3da:
		return(vga_read_status());
	case 0x3e8: case 0x3e9: case 0x3ea: case 0x3eb: case 0x3ec: case 0x3ed: case 0x3ee: case 0x3ef:
		return(sio_read(2, addr));
	case 0x3f8: case 0x3f9: case 0x3fa: case 0x3fb: case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
		return(sio_read(0, addr));
	default:
//		error("inb %4x\n", addr);
		break;
	}
	return(0xff);
}

UINT16 read_io_word(offs_t addr)
{
	return(read_io_byte(addr) | (read_io_byte(addr + 1) << 8));
}

UINT32 read_io_dword(offs_t addr)
{
	return(read_io_byte(addr) | (read_io_byte(addr + 1) << 8) | (read_io_byte(addr + 2) << 16) | (read_io_byte(addr + 3) << 24));
}

void write_io_byte(offs_t addr, UINT8 val)
{
#ifdef ENABLE_DEBUG_IOPORT
	if(fdebug != NULL) {
		fprintf(fdebug, "outb %04X, %02X\n", addr, val);
	}
#endif
	switch(addr) {
#ifdef SW1US_PATCH
	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08:
	case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: case 0x10:
		sio_write(0, addr - 1, val);
		break;
#else
	case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		dma_write(0, addr, val);
		break;
#endif
	case 0x20: case 0x21:
		pic_write(0, addr, val);
		break;
	case 0x40: case 0x41: case 0x42: case 0x43:
		pit_write(addr & 0x03, val);
		break;
	case 0x60:
		kbd_write_data(val);
		break;
	case 0x61:
		if((system_port & 3) != 3 && (val & 3) == 3) {
			// beep on
//			MessageBeep(-1);
		} else if((system_port & 3) == 3 && (val & 3) != 3) {
			// beep off
		}
		system_port = val;
		break;
	case 0x64:
		kbd_write_command(val);
		break;
	case 0x70:
		cmos_addr = val;
		break;
	case 0x71:
		cmos_write(cmos_addr, val);
		break;
	case 0x81:
		dma_page_write(0, 2, val);
	case 0x82:
		dma_page_write(0, 3, val);
	case 0x83:
		dma_page_write(0, 1, val);
	case 0x87:
		dma_page_write(0, 0, val);
	case 0x89:
		dma_page_write(1, 2, val);
	case 0x8a:
		dma_page_write(1, 3, val);
	case 0x8b:
		dma_page_write(1, 1, val);
	case 0x8f:
		dma_page_write(1, 0, val);
	case 0x92:
		i386_set_a20_line((val >> 1) & 1);
		break;
	case 0xa0: case 0xa1:
		pic_write(1, addr, val);
		break;
	case 0xc0: case 0xc2: case 0xc4: case 0xc6: case 0xc8: case 0xca: case 0xcc: case 0xce:
	case 0xd0: case 0xd2: case 0xd4: case 0xd6: case 0xd8: case 0xda: case 0xdc: case 0xde:
		dma_write(1, (addr - 0xc0) >> 1, val);
		break;
//	case 0x278: case 0x279: case 0x27a:
//		pio_write(1, addr, val);
//		break;
	case 0x2e8: case 0x2e9: case 0x2ea: case 0x2eb: case 0x2ec: case 0x2ed: case 0x2ee: case 0x2ef:
		sio_write(3, addr, val);
		break;
	case 0x2f8: case 0x2f9: case 0x2fa: case 0x2fb: case 0x2fc: case 0x2fd: case 0x2fe: case 0x2ff:
		sio_write(1, addr, val);
		break;
	case 0x378: case 0x379: case 0x37a:
		pio_write(0, addr, val);
		break;
	case 0x3e8: case 0x3e9: case 0x3ea: case 0x3eb: case 0x3ec: case 0x3ed: case 0x3ee: case 0x3ef:
		sio_write(2, addr, val);
		break;
	case 0x3f8: case 0x3f9: case 0x3fa: case 0x3fb: case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
		sio_write(0, addr, val);
		break;
	default:
//		error("outb %4x,%2x\n", addr, val);
		break;
	}
}

void write_io_word(offs_t addr, UINT16 val)
{
	write_io_byte(addr + 0, (val >> 0) & 0xff);
	write_io_byte(addr + 1, (val >> 8) & 0xff);
}

void write_io_dword(offs_t addr, UINT32 val)
{
	write_io_byte(addr + 0, (val >>  0) & 0xff);
	write_io_byte(addr + 1, (val >>  8) & 0xff);
	write_io_byte(addr + 2, (val >> 16) & 0xff);
	write_io_byte(addr + 3, (val >> 24) & 0xff);
}
