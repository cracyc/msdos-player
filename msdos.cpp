/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#include "msdos.h"

#ifdef _MSC_VER
#pragma warning( disable : 4018 )
#pragma warning( disable : 4065 )
#pragma warning( disable : 4146 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4267 )
#endif

#define fatalerror(...) { \
	fprintf(stderr, __VA_ARGS__); \
	exit(1); \
}
#define error(...) fprintf(stderr, "error: " __VA_ARGS__)

/* ----------------------------------------------------------------------------
	MAME i386
---------------------------------------------------------------------------- */

//#define SUPPORT_DISASSEMBLER

#define CPU_MODEL i386
//#define CPU_MODEL i486
//#define CPU_MODEL pentium
//#define CPU_MODEL mediagx
//#define CPU_MODEL pentium_pro
//#define CPU_MODEL pentium_mmx
//#define CPU_MODEL pentium2
//#define CPU_MODEL pentium3
//#define CPU_MODEL pentium4

#define LSB_FIRST

#ifndef INLINE
#define INLINE inline
#endif
#define U64(v) UINT64(v)

//#define logerror(...) fprintf(stderr, __VA_ARGS__)
#define logerror(...)
//#define popmessage(...) fprintf(stderr, __VA_ARGS__)
#define popmessage(...)

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
/* src/emu/devcpu.h */

// CPU interface functions
#define CPU_INIT_NAME(name)			cpu_init_##name
#define CPU_INIT(name)				void* CPU_INIT_NAME(name)()
#define CPU_INIT_CALL(name)			CPU_INIT_NAME(name)()

#define CPU_RESET_NAME(name)			cpu_reset_##name
#define CPU_RESET(name)				void CPU_RESET_NAME(name)(i386_state *cpustate)
#define CPU_RESET_CALL(name)			CPU_RESET_NAME(name)(cpustate)

#define CPU_EXECUTE_NAME(name)			cpu_execute_##name
#define CPU_EXECUTE(name)			void CPU_EXECUTE_NAME(name)(i386_state *cpustate)
#define CPU_EXECUTE_CALL(name)			CPU_EXECUTE_NAME(name)(cpustate)

#define CPU_DISASSEMBLE_NAME(name)		cpu_disassemble_##name
#define CPU_DISASSEMBLE(name)			int CPU_DISASSEMBLE_NAME(name)(char *buffer, offs_t eip, const UINT8 *oprom)
#define CPU_DISASSEMBLE_CALL(name)		CPU_DISASSEMBLE_NAME(name)(buffer, eip, oprom)

/*****************************************************************************/
/* src/emu/memory.h */

// offsets and addresses are 32-bit (for now...)
typedef UINT32	offs_t;

// read accessors
UINT8 read_byte(offs_t byteaddress)
{
	if(byteaddress < MAX_MEM) {
		return mem[byteaddress];
	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
		return read_byte(byteaddress & 0xfffff);
	}
	return 0;
}

UINT16 read_word(offs_t byteaddress)
{
	if(byteaddress < MAX_MEM - 1) {
		return *(UINT16 *)(mem + byteaddress);
	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
		return read_word(byteaddress & 0xfffff);
	}
	return 0;
}

UINT32 read_dword(offs_t byteaddress)
{
	if(byteaddress < MAX_MEM - 3) {
		return *(UINT32 *)(mem + byteaddress);
	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
		return read_dword(byteaddress & 0xfffff);
	}
	return 0;
}

// write accessors
void write_byte(offs_t byteaddress, UINT8 data)
{
	if(byteaddress < MAX_MEM) {
		if(byteaddress >= tvram_base_address && byteaddress < tvram_base_address + 4000) {
			if(int_10h_feh_called && !int_10h_ffh_called && mem[byteaddress] != data) {
				COORD co;
				DWORD num;
				
				co.X = ((byteaddress - tvram_base_address) >> 1) % 80;
				co.Y = ((byteaddress - tvram_base_address) >> 1) / 80;
				
				if(byteaddress & 1) {
					scr_attr[0] = data;
					WriteConsoleOutputAttribute(hStdout, scr_attr, 1, co, &num);
				} else {
					scr_char[0] = data;
					WriteConsoleOutputCharacter(hStdout, scr_char, 1, co, &num);
				}
			}
		}
		mem[byteaddress] = data;
	}
}

void write_word(offs_t byteaddress, UINT16 data)
{
	if(byteaddress < MAX_MEM - 1) {
		if(byteaddress >= tvram_base_address && byteaddress < tvram_base_address + 4000) {
			if(int_10h_feh_called && !int_10h_ffh_called && *(UINT16 *)(mem + byteaddress) != data) {
				if(byteaddress & 1) {
					write_byte(byteaddress    , data     );
					write_byte(byteaddress + 1, data >> 8);
				} else {
					COORD co;
					DWORD num;
					
					co.X = ((byteaddress - tvram_base_address) >> 1) % 80;
					co.Y = ((byteaddress - tvram_base_address) >> 1) / 80;
					
					scr_char[0] = data;
					scr_attr[0] = data >> 8;
					
					WriteConsoleOutputCharacter(hStdout, scr_char, 1, co, &num);
					WriteConsoleOutputAttribute(hStdout, scr_attr, 1, co, &num);
				}
				return;
			}
		}
		*(UINT16 *)(mem + byteaddress) = data;
	}
}

void write_dword(offs_t byteaddress, UINT32 data)
{
	if(byteaddress < MAX_MEM - 3) {
		if(byteaddress >= tvram_base_address && byteaddress < tvram_base_address + 4000) {
			if(int_10h_feh_called && !int_10h_ffh_called && *(UINT32 *)(mem + byteaddress) != data) {
				if(byteaddress & 1) {
					write_byte(byteaddress    , data      );
					write_byte(byteaddress + 1, data >>  8);
					write_byte(byteaddress + 2, data >> 16);
					write_byte(byteaddress + 3, data >> 24);
				} else {
					COORD co;
					DWORD num;
					
					co.X = ((byteaddress - tvram_base_address) >> 1) % 80;
					co.Y = ((byteaddress - tvram_base_address) >> 1) / 80;
					
					scr_char[0] = data;
					scr_attr[0] = data >> 8;
					scr_char[1] = data >> 16;
					scr_attr[1] = data >> 24;
					
					WriteConsoleOutputCharacter(hStdout, scr_char, 2, co, &num);
					WriteConsoleOutputAttribute(hStdout, scr_attr, 2, co, &num);
				}
				return;
			}
		}
		*(UINT32 *)(mem + byteaddress) = data;
	}
}

// accessor methods for reading decrypted data
#define read_decrypted_byte read_byte
#define read_decrypted_word read_word
#define read_decrypted_dword read_dword

UINT8 read_io_byte(offs_t byteaddress);
UINT16 read_io_word(offs_t byteaddress);
UINT32 read_io_dword(offs_t byteaddress);

void write_io_byte(offs_t byteaddress, UINT8 data);
void write_io_word(offs_t byteaddress, UINT16 data);
void write_io_dword(offs_t byteaddress, UINT32 data);

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
/* src/emu/didisasm.h */

// Disassembler constants
const UINT32 DASMFLAG_SUPPORTED     = 0x80000000;   // are disassembly flags supported?
const UINT32 DASMFLAG_STEP_OUT      = 0x40000000;   // this instruction should be the end of a step out sequence
const UINT32 DASMFLAG_STEP_OVER     = 0x20000000;   // this instruction should be stepped over by setting a breakpoint afterwards
const UINT32 DASMFLAG_OVERINSTMASK  = 0x18000000;   // number of extra instructions to skip when stepping over
const UINT32 DASMFLAG_OVERINSTSHIFT = 27;           // bits to shift after masking to get the value
const UINT32 DASMFLAG_LENGTHMASK    = 0x0000ffff;   // the low 16-bits contain the actual length

/*****************************************************************************/
/* src/osd/osdcomm.h */

/* Highly useful macro for compile-time knowledge of an array size */
#define ARRAY_LENGTH(x)     (sizeof(x) / sizeof(x[0]))

#include "softfloat/softfloat.c"
#include "i386/i386.c"
#ifdef SUPPORT_DISASSEMBLER
#include "i386/i386dasm.c"
bool dasm = false;
#endif

i386_state *cpustate;
int cpu_type, cpu_step;

void i386_jmp_far(UINT16 selector, UINT32 address)
{
	if(PROTECTED_MODE && !V8086_MODE) {
		i386_protected_mode_jump(cpustate, selector, address, 1, cpustate->operand_size);
	} else {
		cpustate->sreg[CS].selector = selector;
		cpustate->performed_intersegment_jump = 1;
		i386_load_segment_descriptor(cpustate, CS);
		cpustate->eip = address;
		CHANGE_PC(cpustate, cpustate->eip);
	}
}

/* ----------------------------------------------------------------------------
	main
---------------------------------------------------------------------------- */

int main(int argc, char *argv[], char *envp[])
{
	int standard_env = (argc > 1 && _stricmp(argv[1], "-e") == 0);
	
	if(argc < 2 + standard_env) {
#ifdef _WIN64
		fprintf(stderr, "MS-DOS Player for Win32-x64 console\n\n");
#else
		fprintf(stderr, "MS-DOS Player for Win32 console\n\n");
#endif
		fprintf(stderr, "Usage: MSDOS [-e] (command file) [opions]\n");
		return(EXIT_FAILURE);
	}
	
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hStdout, &csbi);
	
	for(int y = 0; y < SCR_BUF_SIZE; y++) {
		for(int x = 0; x < 80; x++) {
			scr_buf[y][x].Char.AsciiChar = ' ';
			scr_buf[y][x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		}
	}
	scr_buf_size.X = 80;
	scr_buf_size.Y = SCR_BUF_SIZE;
	scr_buf_pos.X = scr_buf_pos.Y = 0;
	scr_width = csbi.dwSize.X;
	scr_height = csbi.dwSize.Y;
	cursor_moved = false;
	
	key_buf_char = new FIFO();
	key_buf_scan = new FIFO();
	
	hardware_init();
	
	if(msdos_init(argc - (standard_env + 1), argv + (standard_env + 1), envp, standard_env)) {
		retval = EXIT_FAILURE;
	} else {
		timeBeginPeriod(1);
		hardware_run();
		msdos_finish();
		timeEndPeriod(1);
	}
	hardware_finish();
	
	delete key_buf_char;
	delete key_buf_scan;
	
	SetConsoleTextAttribute(hStdout, csbi.wAttributes);
	
	return(retval);
}

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

void update_key_buffer()
{
	DWORD dwRead;
	INPUT_RECORD ir[16];
	
	if(ReadConsoleInputA(hStdin, ir, 16, &dwRead)) {
		for(int i = 0; i < dwRead; i++) {
			if((ir[i].EventType & KEY_EVENT) && ir[i].Event.KeyEvent.bKeyDown) {
				if(ir[i].Event.KeyEvent.uChar.AsciiChar == 0) {
					// ignore shift, ctrl and alt keys
					if(ir[i].Event.KeyEvent.wVirtualScanCode != 0x1d &&
					   ir[i].Event.KeyEvent.wVirtualScanCode != 0x2a &&
					   ir[i].Event.KeyEvent.wVirtualScanCode != 0x36 &&
					   ir[i].Event.KeyEvent.wVirtualScanCode != 0x38) {
						key_buf_char->write(0x00);
						key_buf_scan->write(ir[i].Event.KeyEvent.dwControlKeyState & ENHANCED_KEY ? 0xe0 : 0x00);
						key_buf_char->write(0x00);
						key_buf_scan->write(ir[i].Event.KeyEvent.wVirtualScanCode & 0xff);
					}
				} else {
					key_buf_char->write(ir[i].Event.KeyEvent.uChar.AsciiChar & 0xff);
					key_buf_scan->write(ir[i].Event.KeyEvent.wVirtualScanCode & 0xff);
				}
			}
		}
	}
	if(key_buf_char->count() == 0) {
		Sleep(10);
	}
}

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

void msdos_dbcs_table_init()
{
	system_code_page = active_code_page = _getmbcp();
	msdos_dbcs_table_update();
}

void msdos_dbcs_table_finish()
{
	if(active_code_page != system_code_page) {
		_setmbcp(system_code_page);
	}
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

char *msdos_strupr(char *str)
{
	char *src = str;
	
	while(*src != 0) {
		if(msdos_lead_byte_check(*src)) {
			src += 2;
		} else if(*src >= 'a' && *src <= 'z') {
			*src++ -= 0x20;
		} else {
			src++;
		}
	}
	return(str);
}

// file control

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
	return(tmp);
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
	char *ext = strstr(path, ".");
	
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
	
	GetShortPathName(path, tmp, MAX_PATH);
	msdos_strupr(tmp);
	return(tmp);
}

char *msdos_short_full_path(char *path)
{
	static char tmp[MAX_PATH];
	char full[MAX_PATH], *name;
	
	GetFullPathName(path, MAX_PATH, full, &name);
	GetShortPathName(full, tmp, MAX_PATH);
	msdos_strupr(tmp);
	return(tmp);
}

char *msdos_short_full_dir(char *path)
{
	static char tmp[MAX_PATH];
	char full[MAX_PATH], *name;
	
	GetFullPathName(path, MAX_PATH, full, &name);
	name[-1] = '\0';
	GetShortPathName(full, tmp, MAX_PATH);
	msdos_strupr(tmp);
	return(tmp);
}

char *msdos_local_file_path(char *path, int lfn)
{
	char *trimmed = msdos_trimmed_path(path, lfn);
	
	if(_access(trimmed, 0) != 0) {
		process_t *process = msdos_process_info_get(current_psp);
		static char tmp[MAX_PATH];
		
		sprintf(tmp, "%s\\%s", process->module_dir, trimmed);
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	return(trimmed);
}

int msdos_drive_number(char *path)
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
	msdos_strupr(tmp);
	return(tmp);
}

void msdos_file_handler_open(int fd, char *path, int atty, int mode, UINT16 info, UINT16 psp_seg)
{
	static int id = 0;
	char full[MAX_PATH], *name;
	
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		psp->file_table[fd] = fd;
	}
	if(GetFullPathName(path, MAX_PATH, full, &name) != 0) {
		strcpy(file_handler[fd].path, full);
	} else {
		strcpy(file_handler[fd].path, path);
	}
	file_handler[fd].valid = 1;
	file_handler[fd].id = id++;	// dummy id for int 21h ax=71a6h
	file_handler[fd].atty = atty;
	file_handler[fd].mode = mode;
	file_handler[fd].info = info;
	file_handler[fd].psp = psp_seg;
}

void msdos_file_handler_dup(int dst, int src, UINT16 psp_seg)
{
	if(psp_seg && dst < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		psp->file_table[dst] = dst;
	}
	strcpy(file_handler[dst].path, file_handler[src].path);
	file_handler[dst].valid = 1;
	file_handler[dst].id = file_handler[src].id;
	file_handler[dst].atty = file_handler[src].atty;
	file_handler[dst].mode = file_handler[src].mode;
	file_handler[dst].info = file_handler[src].info;
	file_handler[dst].psp = psp_seg;
}

void msdos_file_handler_close(int fd, UINT16 psp_seg)
{
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		psp->file_table[fd] = 0xff;
	}
	file_handler[fd].valid = 0;
}

int msdos_file_attribute_create(UINT16 new_attr)
{
	int attr = 0;
	
	if(REG16(CX) & 0x01) {
		attr |= FILE_ATTRIBUTE_READONLY;
	}
	if(REG16(CX) & 0x02) {
		attr |= FILE_ATTRIBUTE_HIDDEN;
	}
	if(REG16(CX) & 0x04) {
		attr |= FILE_ATTRIBUTE_SYSTEM;
	}
	if(REG16(CX) & 0x20) {
		attr |= FILE_ATTRIBUTE_ARCHIVE;
	}
	return(attr);
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

void msdos_putch(UINT8 data);

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
}

int msdos_kbhit()
{
	msdos_stdio_reopen();
	
	if(!file_handler[0].atty) {
		// stdin is redirected to file
		return(eof(0) == 0);
	}
	
	// check keyboard status
	if(key_buf_char->count() != 0) {
		return(1);
	} else {
		return(_kbhit());
	}
}

int msdos_getch_ex(int echo)
{
	static char prev = 0;
	
	msdos_stdio_reopen();
	
	if(!file_handler[0].atty) {
		// stdin is redirected to file
retry:
		char data;
		if(_read(0, &data, 1) == 1) {
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
	while(key_buf_char->count() == 0) {
		update_key_buffer();
	}
	int key_char = key_buf_char->read();
	int key_scan = key_buf_scan->read();
	
	if(echo && key_char) {
		msdos_putch(key_char);
	}
	return key_char ? key_char : key_scan;
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
	static char tmp[64];
	
	msdos_stdio_reopen();
	
	if(!file_handler[1].atty) {
		// stdout is redirected to file
		msdos_write(1, &data, 1);
		return;
	}
	
	// output to console
	tmp[p++] = data;
	
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
			co.Y = tmp[2] - 0x20;
			SetConsoleCursorPosition(hStdout, co);
			mem[0x450 + mem[0x462] * 2] = co.X;
			mem[0x451 + mem[0x462] * 2] = co.Y;
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
				SET_RECT(rect, 0, 0, csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
				WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
				co.X = co.Y = 0;
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
					co.Y -= param[0];
				} else if(data == 'B') {
					co.Y += param[0];
				} else if(data == 'C') {
					co.X += param[0];
				} else if(data == 'D') {
					co.X -= param[0];
				} else if(data == 'H' || data == 'f') {
					co.X = param[1] - 1;
					co.Y = param[0] - 1;
				} else if(data == 'J') {
					SMALL_RECT rect;
					if(param[0] == 0) {
						SET_RECT(rect, co.X, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
						if(co.Y < csbi.dwSize.Y - 1) {
							SET_RECT(rect, 0, co.Y + 1, csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
							WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
						}
					} else if(param[0] == 1) {
						if(co.Y > 0) {
							SET_RECT(rect, 0, 0, csbi.dwSize.X - 1, co.Y - 1);
							WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
						}
						SET_RECT(rect, 0, co.Y, co.X, co.Y);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 2) {
						SET_RECT(rect, 0, 0, csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
						co.X = co.Y = 0;
					}
				} else if(data == 'K') {
					SMALL_RECT rect;
					if(param[0] == 0) {
						SET_RECT(rect, co.X, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 1) {
						SET_RECT(rect, 0, co.Y, co.X, co.Y);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 2) {
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					}
				} else if(data == 'L') {
					SMALL_RECT rect;
					SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
					ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					SET_RECT(rect, 0, co.Y + param[0], csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
					WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					// clear buffer
					for(int y = 0; y < SCR_BUF_SIZE; y++) {
						for(int x = 0; x < 80; x++) {
							scr_buf[y][x].Char.AsciiChar = ' ';
							scr_buf[y][x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
						}
					}
					SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, co.Y + param[0] - 1);
					WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					co.X = 0;
				} else if(data == 'M') {
					SMALL_RECT rect;
					if(co.Y + param[0] > csbi.dwSize.Y - 1) {
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
					} else {
						SET_RECT(rect, 0, co.Y + param[0], csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
						ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.dwSize.Y - 1);
						WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
						// clear buffer
						for(int y = 0; y < SCR_BUF_SIZE; y++) {
							for(int x = 0; x < 80; x++) {
								scr_buf[y][x].Char.AsciiChar = ' ';
								scr_buf[y][x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
							}
						}
					}
					co.X = 0;
				} else if(data == 'h') {
					if(tmp[2] == '>' && tmp[3] == '5') {
						CONSOLE_CURSOR_INFO cur;
						GetConsoleCursorInfo(hStdout, &cur);
						if(cur.bVisible) {
							cur.bVisible = FALSE;
							GetConsoleCursorInfo(hStdout, &cur);
						}
					}
				} else if(data == 'l') {
					if(tmp[2] == '>' && tmp[3] == '5') {
						CONSOLE_CURSOR_INFO cur;
						GetConsoleCursorInfo(hStdout, &cur);
						if(!cur.bVisible) {
							cur.bVisible = TRUE;
							GetConsoleCursorInfo(hStdout, &cur);
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
			if(co.Y < 0) {
				co.Y = 0;
			} else if(co.Y >= csbi.dwSize.Y) {
				co.Y = csbi.dwSize.Y - 1;
			}
			if(co.X != csbi.dwCursorPosition.X || co.Y != csbi.dwCursorPosition.Y) {
				SetConsoleCursorPosition(hStdout, co);
				mem[0x450 + mem[0x462] * 2] = co.X;
				mem[0x451 + mem[0x462] * 2] = co.Y;
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
	tmp[p++] = '\0';
	p = 0;
	printf("%s", tmp);
	cursor_moved = true;
}

int msdos_aux_in()
{
#ifdef SUPPORT_AUX_PRN
	if(file_handler[3].valid && !eof(3)) {
		char data = 0;
		_read(3, &data, 1);
		return(data);
	} else {
		return(EOF);
	}
#else
	return(0);
#endif
}

void msdos_aux_out(char data)
{
#ifdef SUPPORT_AUX_PRN
	if(file_handler[3].valid) {
		msdos_write(3, &data, 1);
	}
#endif
}

void msdos_prn_out(char data)
{
#ifdef SUPPORT_AUX_PRN
	if(file_handler[4].valid) {
		msdos_write(4, &data, 1);
	}
#endif
}

// memory control

mcb_t *msdos_mcb_create(int mcb_seg, UINT8 mz, UINT16 psp, UINT16 paragraphs)
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
		fatalerror("broken mcb\n");
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
		mcb->paragraphs += 1 + next_mcb->paragraphs;
	}
}

int msdos_mem_alloc(int paragraphs, int new_process)
{
	int mcb_seg = current_psp - 1;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		
		if(!new_process || mcb->mz == 'Z') {
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
		*max_paragraphs = mcb->paragraphs;
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

int msdos_mem_get_free(int new_process)
{
	int mcb_seg = current_psp - 1;
	int max_paragraphs = 0;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		
		if(!new_process || mcb->mz == 'Z') {
			if(mcb->psp == 0 && mcb->paragraphs > max_paragraphs) {
				max_paragraphs = mcb->paragraphs;
			}
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(max_paragraphs);
}

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
		char *n = strtok(src, "=");
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
		char *n = strtok(src, "=");
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

psp_t *msdos_psp_create(int psp_seg, UINT16 first_mcb, UINT16 parent_psp, UINT16 env_seg)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	memset(psp, 0, PSP_SIZE);
	psp->exit[0] = 0xcd;
	psp->exit[1] = 0x20;
	psp->first_mcb = first_mcb;
	psp->far_call = 0xea;
	psp->cpm_entry.w.l = 0xfff1;	// int 21h, retf
	psp->cpm_entry.w.h = 0xf000;
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = parent_psp;
	for(int i = 0; i < 20; i++) {
		if(file_handler[i].valid) {
			psp->file_table[i] = i;
		} else {
			psp->file_table[i] = 0xff;
		}
	}
	psp->env_seg = env_seg;
	psp->stack.w.l = REG16(SP);
	psp->stack.w.h = cpustate->sreg[SS].selector;
	psp->service[0] = 0xcd;
	psp->service[1] = 0x21;
	psp->service[2] = 0xcb;
	return(psp);
}

int msdos_process_exec(char *cmd, param_block_t *param, UINT8 al)
{
	// load command file
	int fd = -1;
	int dos_command = 0;
	char command[MAX_PATH], path[MAX_PATH], opt[MAX_PATH], *name;
	
	strcpy(command, cmd);
	int opt_ofs = (param->cmd_line.w.h << 4) + param->cmd_line.w.l;
	int opt_len = mem[opt_ofs];
	memset(opt, 0, sizeof(opt));
	memcpy(opt, mem + opt_ofs + 1, opt_len);
	
	// check command.com
	GetFullPathName(command, MAX_PATH, path, &name);
	if(_stricmp(name, "COMMAND.COM") == 0 || _stricmp(name, "CMD.EXE") == 0) {
		for(int i = 0; i < opt_len; i++) {
			if(opt[i] == ' ') {
				continue;
			}
			if(opt[i] == '/' && (opt[i + 1] == 'c' || opt[i + 1] == 'C') && opt[i + 2] == ' ') {
				dos_command = 1;
				for(int j = i + 3; j < opt_len; j++) {
					if(opt[j] == ' ') {
						continue;
					}
					char *token = strtok(opt + j, " ");
					strcpy(command, token);
					char tmp[MAX_PATH];
					strcpy(tmp, token + strlen(token) + 1);
					strcpy(opt, tmp);
					opt_len = strlen(opt);
					mem[opt_ofs] = opt_len;
					strcpy((char *)(mem + opt_ofs + 1), opt);
					break;
				}
			}
			break;
		}
	}
	
	// load command file
	strcpy(path, command);
	if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
		sprintf(path, "%s.COM", command);
		if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
			sprintf(path, "%s.EXE", command);
			if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
				// search path in parent environments
				psp_t *parent_psp = (psp_t *)(mem + (current_psp << 4));
				char *env = msdos_env_get(parent_psp->env_seg, "PATH");
				
				if(env != NULL) {
					char env_path[1024];
					strcpy(env_path, env);
					char *token = strtok(env_path, ";");
					
					while(token != NULL) {
						sprintf(path, "%s\\%s", token, command);
						if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
							break;
						}
						sprintf(path, "%s\\%s.COM", token, command);
						if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
							break;
						}
						sprintf(path, "%s\\%s.EXE", token, command);
						if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
							break;
						}
						token = strtok(NULL, ";");
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
	int env_seg, psp_seg;
	
	if((env_seg = msdos_mem_alloc(ENV_SIZE >> 4, 1)) == -1) {
		return(-1);
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
	int paragraphs, free_paragraphs = msdos_mem_get_free(1);
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
		if((psp_seg = msdos_mem_alloc(paragraphs, 1)) == -1) {
			msdos_mem_free(env_seg);
			return(-1);
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
		if((psp_seg = msdos_mem_alloc(paragraphs, 1)) == -1) {
			msdos_mem_free(env_seg);
			return(-1);
		}
		int start_seg = psp_seg + (PSP_SIZE >> 4);
		memcpy(mem + (start_seg << 4), file_buffer, 0x10000 - PSP_SIZE);
		// segments
		cs = ss = psp_seg;
		ip = 0x100;
		sp = 0xfffe;
	}
	
	// create psp
	*(UINT16 *)(mem + 4 * 0x22 + 0) = cpustate->eip;
	*(UINT16 *)(mem + 4 * 0x22 + 2) = cpustate->sreg[CS].selector;
	psp_t *psp = msdos_psp_create(psp_seg, psp_seg + paragraphs, current_psp, env_seg);
	memcpy(psp->fcb1, mem + (param->fcb1.w.h << 4) + param->fcb1.w.l, sizeof(psp->fcb1));
	memcpy(psp->fcb2, mem + (param->fcb2.w.h << 4) + param->fcb2.w.l, sizeof(psp->fcb2));
	memcpy(psp->buffer, mem + (param->cmd_line.w.h << 4) + param->cmd_line.w.l, sizeof(psp->buffer));
	
	mcb_t *mcb_env = (mcb_t *)(mem + ((env_seg - 1) << 4));
	mcb_t *mcb_psp = (mcb_t *)(mem + ((psp_seg - 1) << 4));
	mcb_psp->psp = mcb_env->psp = psp_seg;
	
	// process info
	process_t *process = msdos_process_info_create(psp_seg);
	strcpy(process->module_dir, msdos_short_full_dir(path));
	process->dta.w.l = 0x80;
	process->dta.w.h = psp_seg;
	process->switchar = '/';
	process->max_files = 20;
	process->find_handle = INVALID_HANDLE_VALUE;
	process->parent_int_10h_feh_called = int_10h_feh_called;
	process->parent_int_10h_ffh_called = int_10h_ffh_called;
	
	current_psp = psp_seg;
	
	if(al == 0x00) {
		int_10h_feh_called = int_10h_ffh_called = false;
		
		// registers and segments
		REG16(AX) = REG16(BX) = 0x00;
		REG16(CX) = 0xff;
		REG16(DX) = psp_seg;
		REG16(SI) = ip;
		REG16(DI) = sp;
		REG16(SP) = sp;
		cpustate->sreg[DS].selector = cpustate->sreg[ES].selector = psp_seg;
		cpustate->sreg[SS].selector = ss;
		i386_load_segment_descriptor(cpustate, DS);
		i386_load_segment_descriptor(cpustate, ES);
		i386_load_segment_descriptor(cpustate, SS);
		
		*(UINT16 *)(mem + (ss << 4) + sp) = 0;
		i386_jmp_far(cs, ip);
	} else if(al == 0x01) {
		// copy ss:sp and cs:ip to param block
		param->sp = sp;
		param->ss = ss;
		param->ip = ip;
		param->cs = cs;
	}
	return(0);
}

void msdos_process_terminate(int psp_seg, int ret, int mem_free)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	*(UINT32 *)(mem + 4 * 0x22) = psp->int_22h.dw;
	*(UINT32 *)(mem + 4 * 0x23) = psp->int_23h.dw;
	*(UINT32 *)(mem + 4 * 0x24) = psp->int_24h.dw;
	
	cpustate->sreg[SS].selector = psp->stack.w.h;
	i386_load_segment_descriptor(cpustate, SS);
	REG16(SP) = psp->stack.w.l;
	i386_jmp_far(psp->int_22h.w.h, psp->int_22h.w.l);
	
	process_t *process = msdos_process_info_get(psp_seg);
	int_10h_feh_called = process->parent_int_10h_feh_called;
	int_10h_ffh_called = process->parent_int_10h_ffh_called;
	
	if(mem_free) {
		int mcb_seg = psp->env_seg - 1;
		msdos_mcb_create(mcb_seg, 'Z', 0, (MEMORY_END >> 4) - mcb_seg - 1);
		
		for(int i = 0; i < MAX_FILES; i++) {
			if(file_handler[i].valid && file_handler[i].psp == psp_seg) {
				_close(i);
				msdos_file_handler_close(i, psp_seg);
			}
		}
		msdos_stdio_reopen();
		
		if(process->find_handle != INVALID_HANDLE_VALUE) {
			FindClose(process->find_handle);
			process->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	
	memset(process, 0, sizeof(process_t));
	
	current_psp = psp->parent_psp;
	retval = ret;
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
	
	HANDLE hFile = CreateFile(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile != INVALID_HANDLE_VALUE) {
		DISK_GEOMETRY geo;
		DWORD dwSize;
		if(DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo, sizeof(geo), &dwSize, NULL)) {
			dpb->bytes_per_sector = (UINT16)geo.BytesPerSector;
			dpb->highest_sector_num = (UINT8)(geo.SectorsPerTrack - 1);
			dpb->highest_cluster_num = (UINT16)(geo.TracksPerCylinder * geo.Cylinders.QuadPart + 1);
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
		dpb->free_clusters = 0xffff;
		CloseHandle(hFile);
	}
	return(res);
}

// pc bios

int get_tvram_address(int page)
{
	if(/*mem[0x449] == 0x03 || */mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		return TVRAM_TOP;
	} else {
		return TVRAM_TOP + 0x1000 * (page & 7);
	}
}

inline int get_tvram_address(int page, int x, int y)
{
	return get_tvram_address(page) + (x + y * 80) * 2;
}

inline void pcbios_int_10h_00h()
{
	mem[0x449] = REG8(AL) & 0x7f;
	tvram_base_address = get_tvram_address(mem[0x462]);
	
	if(REG8(AL) & 0x80) {
		mem[0x487] |= 0x80;
	} else {
		for(int y = 0, ofs = get_tvram_address(mem[0x462]); y < 25; y++) {
			for(int x = 0; x < 80; x++) {
				scr_buf[y][x].Char.AsciiChar = ' ';
				scr_buf[y][x].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
				mem[ofs++] = 0x20;
				mem[ofs++] = 0x07;
			}
		}
		SMALL_RECT rect;
		SET_RECT(rect, 0, 0, 79, 24);
		WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
		mem[0x487] &= ~0x80;
	}
}

inline void pcbios_int_10h_01h()
{
	 mem[0x460] = REG8(CL);
	 mem[0x461] = REG8(CH);
}

inline void pcbios_int_10h_02h()
{
	if(mem[0x462] == REG8(BH)) {
		COORD co;
		co.X = REG8(DL);
		co.Y = REG8(DH);
		SetConsoleCursorPosition(hStdout, co);
	}
	mem[0x450 + (REG8(BH) & 7) * 2] = REG8(DL);
	mem[0x451 + (REG8(BH) & 7) * 2] = REG8(DH);
}

inline void pcbios_int_10h_03h()
{
	REG8(DL) = mem[0x450 + (REG8(BH) & 7) * 2];
	REG8(DH) = mem[0x451 + (REG8(BH) & 7) * 2];
	REG8(CL) = mem[0x460];
	REG8(CH) = mem[0x461];
}

inline void pcbios_int_10h_05h()
{
	if(mem[0x462] != REG8(BH)) {
		SMALL_RECT rect;
		SET_RECT(rect, 0, 0, 79, 24);
		ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
		
		for(int y = 0, ofs = get_tvram_address(mem[0x462]); y < 25; y++) {
			for(int x = 0; x < 80; x++) {
				mem[ofs++] = scr_buf[y][x].Char.AsciiChar;
				mem[ofs++] = scr_buf[y][x].Attributes;
			}
		}
		for(int y = 0, ofs = get_tvram_address(REG8(BH)); y < 25; y++) {
			for(int x = 0; x < 80; x++) {
				scr_buf[y][x].Char.AsciiChar = mem[ofs++];
				scr_buf[y][x].Attributes = mem[ofs++];
			}
		}
		WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
		
		COORD co;
		co.X = mem[0x450 + (REG8(BH) & 7) * 2];
		co.Y = mem[0x451 + (REG8(BH) & 7) * 2];
		SetConsoleCursorPosition(hStdout, co);
	}
	mem[0x462] = REG8(BH) & 7;
	mem[0x44e] = 0;
	mem[0x44f] = REG8(BH) << 4;
	tvram_base_address = get_tvram_address(mem[0x462]);
}

inline void pcbios_int_10h_06h()
{
	SMALL_RECT rect;
	SET_RECT(rect, 0, 0, 79, 24);
	ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
	
	if(REG8(AL) == 0) {
		for(int y = REG8(CH); y <= REG8(DH); y++) {
			for(int x = REG8(CL), ofs = get_tvram_address(mem[0x462], REG8(CL), y); x <= REG8(DL); x++) {
				scr_buf[y][x].Char.AsciiChar = ' ';
				scr_buf[y][x].Attributes = REG8(BH);
				mem[ofs++] = scr_buf[y][x].Char.AsciiChar;
				mem[ofs++] = scr_buf[y][x].Attributes;
			}
		}
	} else {
		ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
		for(int y = REG8(CH), y2 = REG8(CH) + REG8(AL); y <= REG8(DH); y++, y2++) {
			for(int x = REG8(CL), ofs = get_tvram_address(mem[0x462], REG8(CL), y); x <= REG8(DL); x++) {
				if(y2 <= REG8(DH) && y2 >= 0 && y2 < SCR_BUF_SIZE) {
					scr_buf[y][x] = scr_buf[y2][x];
				} else {
					scr_buf[y][x].Char.AsciiChar = ' ';
					scr_buf[y][x].Attributes = REG8(BH);
				}
				mem[ofs++] = scr_buf[y][x].Char.AsciiChar;
				mem[ofs++] = scr_buf[y][x].Attributes;
			}
		}
	}
	WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
}

inline void pcbios_int_10h_07h()
{
	SMALL_RECT rect;
	SET_RECT(rect, 0, 0, 79, 24);
	ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
	
	if(REG8(AL) == 0) {
		for(int y = REG8(CH); y <= REG8(DH); y++) {
			for(int x = REG8(CL), ofs = get_tvram_address(mem[0x462], REG8(CL), y); x <= REG8(DL); x++) {
				scr_buf[y][x].Char.AsciiChar = ' ';
				scr_buf[y][x].Attributes = REG8(BH);
				mem[ofs++] = scr_buf[y][x].Char.AsciiChar;
				mem[ofs++] = scr_buf[y][x].Attributes;
			}
		}
	} else {
		ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
		for(int y = REG8(DH), y2 = REG8(DH) - REG8(AL); y >= REG8(CH); y--, y2--) {
			for(int x = REG8(CL), ofs = get_tvram_address(mem[0x462], REG8(CL), y); x <= REG8(DL); x++) {
				if(y2 >= REG8(CH) && y2 >= 0 && y2 < SCR_BUF_SIZE) {
					scr_buf[y][x] = scr_buf[y2][x];
				} else {
					scr_buf[y][x].Char.AsciiChar = ' ';
					scr_buf[y][x].Attributes = REG8(BH);
				}
				mem[ofs++] = scr_buf[y][x].Char.AsciiChar;
				mem[ofs++] = scr_buf[y][x].Attributes;
			}
		}
	}
	WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
}

inline void pcbios_int_10h_08h()
{
	COORD co;
	DWORD num;
	
	co.X = mem[0x450 + (REG8(BH) & 7) * 2];
	co.Y = mem[0x451 + (REG8(BH) & 7) * 2];
	
	if(mem[0x462] == REG8(BH)) {
		ReadConsoleOutputCharacter(hStdout, scr_char, 1, co, &num);
		ReadConsoleOutputAttribute(hStdout, scr_attr, 1, co, &num);
		REG8(AL) = scr_char[0];
		REG8(AH) = scr_attr[0];
	} else {
		REG16(AX) = *(UINT16 *)(mem + get_tvram_address(REG8(BH), co.X, co.Y));
	}
}

inline void pcbios_int_10h_09h()
{
	COORD co;
	DWORD num;
	
	co.X = mem[0x450 + (REG8(BH) & 7) * 2];
	co.Y = mem[0x451 + (REG8(BH) & 7) * 2];
	
	if(mem[0x462] == REG8(BH)) {
		for(int i = 0; i < REG16(CX) && i < 80 * 25; i++) {
			scr_char[i] = REG8(AL);
			scr_attr[i] = REG8(BL);
		}
		WriteConsoleOutputCharacter(hStdout, scr_char, REG16(CX), co, &num);
		WriteConsoleOutputAttribute(hStdout, scr_attr, REG16(CX), co, &num);
	} else {
		for(int i = 0, dest = get_tvram_address(REG8(BH), co.X, co.Y); i < REG16(CX); i++) {
			mem[dest++] = REG8(AL);
			mem[dest++] = REG8(BL);
			if(++co.X == 80) {
				if(++co.Y == 25) {
					break;
				}
				co.X = 0;
			}
		}
	}
}

inline void pcbios_int_10h_0ah()
{
	COORD co;
	DWORD num;
	
	co.X = mem[0x450 + (REG8(BH) & 7) * 2];
	co.Y = mem[0x451 + (REG8(BH) & 7) * 2];
	
	if(mem[0x462] == REG8(BH)) {
		for(int i = 0; i < REG16(CX) && i < 80 * 25; i++) {
			scr_char[i] = REG8(AL);
//			scr_attr[i] = REG8(BL);
		}
		WriteConsoleOutputCharacter(hStdout, scr_char, REG16(CX), co, &num);
//		WriteConsoleOutputAttribute(hStdout, scr_attr, REG16(CX), co, &num);
	} else {
		for(int i = 0, dest = get_tvram_address(REG8(BH), co.X, co.Y); i < REG16(CX); i++) {
			mem[dest++] = REG8(AL);
//			mem[dest++] = REG8(BL);
			dest++;
			if(++co.X == 80) {
				if(++co.Y == 25) {
					break;
				}
				co.X = 0;
			}
		}
	}
}

inline void pcbios_int_10h_0eh()
{
	msdos_putch(REG8(AL));
}

inline void pcbios_int_10h_0fh()
{
	REG8(AL) = mem[0x449];
	REG8(AH) = mem[0x44a];
	REG8(BH) = mem[0x462];
}

inline void pcbios_int_10h_13h()
{
	int ofs = cpustate->sreg[ES].base + REG16(BP);
	COORD co;
	DWORD num;
	
	co.X = REG8(DL);
	co.Y = REG8(DH);
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		if(mem[0x462] == REG8(BH)) {
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			SetConsoleCursorPosition(hStdout, co);
			
			if(csbi.wAttributes != REG8(BL)) {
				SetConsoleTextAttribute(hStdout, REG8(BL));
			}
			for(int i = 0; i < REG16(CX); i++) {
				msdos_putch(mem[ofs++]);
			}
			if(csbi.wAttributes != REG8(BL)) {
				SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			}
			if(REG8(AL) == 0x00) {
				co.X = mem[0x450 + (REG8(BH) & 7) * 2];
				co.Y = mem[0x451 + (REG8(BH) & 7) * 2];
				SetConsoleCursorPosition(hStdout, co);
			} else {
				cursor_moved = true;
			}
		} else {
			cpustate->CF = 1;
		}
		break;
	case 0x02:
	case 0x03:
		if(mem[0x462] == REG8(BH)) {
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			SetConsoleCursorPosition(hStdout, co);
			
			WORD wAttributes = csbi.wAttributes;
			for(int i = 0; i < REG16(CX); i++, ofs += 2) {
				if(wAttributes != mem[ofs + 1]) {
					SetConsoleTextAttribute(hStdout, mem[ofs + 1]);
					wAttributes = mem[ofs + 1];
				}
				msdos_putch(mem[ofs]);
			}
			if(csbi.wAttributes != wAttributes) {
				SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			}
			if(REG8(AL) == 0x02) {
				co.X = mem[0x450 + (REG8(BH) & 7) * 2];
				co.Y = mem[0x451 + (REG8(BH) & 7) * 2];
				SetConsoleCursorPosition(hStdout, co);
			} else {
				cursor_moved = true;
			}
		} else {
			cpustate->CF = 1;
		}
		break;
	case 0x10:
	case 0x11:
		if(mem[0x462] == REG8(BH)) {
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
			for(int i = 0, src = get_tvram_address(REG8(BH), co.X, co.Y); i < REG16(CX); i++) {
				mem[ofs++] = mem[src++];
				mem[ofs++] = mem[src++];
				if(REG8(AL) == 0x11) {
					mem[ofs++] = 0;
					mem[ofs++] = 0;
				}
				if(++co.X == 80) {
					if(++co.Y == 25) {
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
			for(int i = 0; i < REG16(CX) && i < 80 * 25; i++) {
				scr_char[i] = mem[ofs++];
				scr_attr[i] = mem[ofs++];
				if(REG8(AL) == 0x21) {
					ofs += 2;
				}
			}
			WriteConsoleOutputCharacter(hStdout, scr_char, REG16(CX), co, &num);
			WriteConsoleOutputAttribute(hStdout, scr_attr, REG16(CX), co, &num);
		} else {
			for(int i = 0, dest = get_tvram_address(REG8(BH), co.X, co.Y); i < REG16(CX); i++) {
				mem[dest++] = mem[ofs++];
				mem[dest++] = mem[ofs++];
				if(REG8(AL) == 0x21) {
					ofs += 2;
				}
				if(++co.X == 80) {
					if(++co.Y == 25) {
						break;
					}
					co.X = 0;
				}
			}
		}
		break;
	default:
		cpustate->CF = 1;
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
		cpustate->CF = 1;
		break;
	}
}

inline void pcbios_int_10h_82h()
{
	static UINT8 mode = 0;
	
	switch(REG8(AL)) {
	case 0:
		if(REG8(BL) != 0xff) {
			mode = REG8(BL);
		}
		REG8(AL) = mode;
		break;
	default:
		cpustate->CF = 1;
		break;
	}
}

inline void pcbios_int_10h_feh()
{
	if(mem[0x449] == 0x03 || mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		cpustate->sreg[ES].selector = (TVRAM_TOP >> 4);
		i386_load_segment_descriptor(cpustate, ES);
		REG16(DI) = (TVRAM_TOP & 0x0f);
	}
	int_10h_feh_called = true;
}

inline void pcbios_int_10h_ffh()
{
	if(mem[0x449] == 0x03 || mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		COORD co;
		DWORD num;
		
		co.X = (REG16(DI) >> 1) % 80;
		co.Y = (REG16(DI) >> 1) / 80;
		for(int i = 0, ofs = get_tvram_address(0, co.X, co.Y); i < REG16(CX) && i < 80 * 25; i++) {
			scr_char[i] = mem[ofs++];
			scr_attr[i] = mem[ofs++];
		}
		WriteConsoleOutputCharacter(hStdout, scr_char, REG16(CX), co, &num);
		WriteConsoleOutputAttribute(hStdout, scr_attr, REG16(CX), co, &num);
	}
	int_10h_ffh_called = true;
}

inline void pcbios_int_15h_23h()
{
	switch(REG8(AL)) {
	case 0:
		REG8(CL) = cmos[0x2d];
		REG8(CH) = cmos[0x2e];
		break;
	case 1:
		cmos[0x2d] = REG8(CL);
		cmos[0x2e] = REG8(CH);
		break;
	default:
		REG8(AH) = 0x86;
		cpustate->CF = 1;
		break;
	}
}

inline void pcbios_int_15h_24h()
{
	switch(REG8(AL)) {
	case 0:
		i386_set_a20_line(cpustate, 0);
		REG8(AH) = 0;
		break;
	case 1:
		i386_set_a20_line(cpustate, 1);
		REG8(AH) = 0;
		break;
	case 2:
		REG8(AH) = 0;
		REG8(AL) = (cpustate->a20_mask >> 20) & 1;
		REG16(CX) = 0;
		break;
	case 3:
		REG16(AX) = 0;
		REG16(BX) = 0;
		break;
	}
}

inline void pcbios_int_15h_49h()
{
	REG8(AL) = 0;
	REG8(BL) = 0;	// DOS/V;
}

inline void pcbios_int_15h_87h()
{
#if 1
	// copy extended memory (from DOSBox)
	int len = REG16(CX) * 2;
	int ofs = cpustate->sreg[ES].base + REG16(SI);
	int src = (*(UINT32 *)(mem + ofs + 0x12) & 0xffffff); // + (mem[ofs + 0x16] << 24);
	int dst = (*(UINT32 *)(mem + ofs + 0x1a) & 0xffffff); // + (mem[ofs + 0x1e] << 24);
	memcpy(mem + dst, mem + src, len);
	REG16(AX) = 0x00;
#else
	REG8(AH) = 0x86;
	cpustate->CF = 1;
#endif
}

inline void pcbios_int_15h_88h()
{
	REG16(AX) = ((min(MAX_MEM, 0x1000000) - 0x100000) >> 10);
}

inline void pcbios_int_15h_89h()
{
#if 1
	// switch to protected mode (from DOSBox)
	write_io_byte(0x20, 0x10);
	write_io_byte(0x21, REG8(BH));
	write_io_byte(0x21, 0x00);
	write_io_byte(0xa0, 0x10);
	write_io_byte(0xa1, REG8(BL));
	write_io_byte(0xa1, 0x00);
	i386_set_a20_line(cpustate, 1);
	int ofs = cpustate->sreg[ES].base + REG16(SI);
	cpustate->gdtr.limit = *(UINT16 *)(mem + ofs + 0x08);
	cpustate->gdtr.base = *(UINT32 *)(mem + ofs + 0x08 + 0x02) & 0xffffff;
	cpustate->idtr.limit = *(UINT16 *)(mem + ofs + 0x10);
	cpustate->idtr.base = *(UINT32 *)(mem + ofs + 0x10 + 0x02) & 0xffffff;
	cpustate->cr[0] |= 1;
	cpustate->sreg[DS].selector = 0x18;
	cpustate->sreg[ES].selector = 0x20;
	cpustate->sreg[SS].selector = 0x28;
	i386_load_segment_descriptor(cpustate, DS);
	i386_load_segment_descriptor(cpustate, ES);
	i386_load_segment_descriptor(cpustate, SS);
	REG16(SP) += 6;
	set_flags(cpustate, 0);	// ???
	REG16(AX) = 0x00;
	i386_jmp_far(0x30, REG16(CX));
#else
	REG8(AH) = 0x86;
	cpustate->CF = 1;
#endif
}

inline void pcbios_int_15h_c9h()
{
	REG8(AH) = 0x00;
	REG8(CH) = cpu_type;
	REG8(CL) = cpu_step;
}

inline void pcbios_int_15h_cah()
{
	switch(REG8(AL)) {
	case 0:
		if(REG8(BL) > 0x3f) {
			REG8(AH) = 0x03;
			cpustate->CF = 1;
		} else if(REG8(BL) < 0x0e) {
			REG8(AH) = 0x04;
			cpustate->CF = 1;
		} else {
			REG8(CL) = cmos[REG8(BL)];
		}
		break;
	case 1:
		if(REG8(BL) > 0x3f) {
			REG8(AH) = 0x03;
			cpustate->CF = 1;
		} else if(REG8(BL) < 0x0e) {
			REG8(AH) = 0x04;
			cpustate->CF = 1;
		} else {
			cmos[REG8(BL)] = REG8(CL);
		}
		break;
	default:
		REG8(AH) = 0x86;
		cpustate->CF = 1;
		break;
	}
}

UINT32 get_key_code()
{
	UINT32 code = 0;
	
	if(key_buf_char->count() == 0) {
		update_key_buffer();
	}
	if(key_buf_char->count() != 0) {
		code = key_buf_char->read() | (key_buf_scan->read() << 8);
	}
	if(key_buf_char->count() != 0) {
		code |= (key_buf_char->read() << 16) | (key_buf_scan->read() << 24);
	}
	return code;
}

UINT32 key_code = 0;

inline void pcbios_int_16h_00h()
{
	while(key_code == 0) {
		key_code = get_key_code();
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
	if(key_code == 0) {
		key_code = get_key_code();
	}
	if(key_code != 0) {
		if((key_code & 0xffff) == 0x0000 || (key_code & 0xffff) == 0xe000) {
			if(REG8(AH) == 0x11) {
				key_code = ((key_code >> 8) & 0xff) | ((key_code >> 16) & 0xff00);
			} else {
				key_code = ((key_code >> 16) & 0xff00);
			}
		}
	}
	if(key_code != 0) {
		REG16(AX) = key_code & 0xffff;
	}
	cpustate->ZF = (key_code == 0);
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
		cpustate->CF = 1;
		break;
	}
}

inline void pcbios_int_16h_05h()
{
	_ungetch(REG8(CL));
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
		cpustate->CF = 1;
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
		cpustate->CF = 1;
		break;
	}
}

inline void pcbios_int_1ah_00h()
{
	static WORD prev_day = 0;
	SYSTEMTIME time;
	
	GetLocalTime(&time);
	unsigned __int64 msec = ((time.wHour * 60 + time.wMinute) * 60 + time.wSecond) * 1000 + time.wMilliseconds;
	unsigned __int64 tick = msec * 0x1800b0 / 24 / 60 / 60 / 1000;
	REG16(CX) = (tick >> 16) & 0xffff;
	REG16(DX) = (tick      ) & 0xffff;
	REG8(AL) = (prev_day != 0 && prev_day != time.wDay) ? 1 : 0;
	prev_day = time.wDay;
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
	msdos_process_terminate(cpustate->sreg[CS].selector, retval, 1);
}

inline void msdos_int_21h_01h()
{
	REG8(AL) = msdos_getche();
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_02h()
{
	msdos_putch(REG8(DL));
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
			cpustate->ZF = 0;
		} else {
			REG8(AL) = 0;
			cpustate->ZF = 1;
			Sleep(10);
		}
	} else {
		msdos_putch(REG8(DL));
	}
}

inline void msdos_int_21h_07h()
{
	REG8(AL) = msdos_getch();
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_08h()
{
	REG8(AL) = msdos_getch();
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_09h()
{
	char tmp[0x10000];
	memcpy(tmp, mem + cpustate->sreg[DS].base + REG16(DX), sizeof(tmp));
	tmp[sizeof(tmp) - 1] = '\0';
	int len = strlen(strtok(tmp, "$"));
	
	if(file_handler[1].valid && !file_handler[1].atty) {
		// stdout is redirected to file
		msdos_write(1, tmp, len);
	} else {
		for(int i = 0; i < len; i++) {
			msdos_putch(tmp[i]);
		}
	}
}

inline void msdos_int_21h_0ah()
{
	int ofs = cpustate->sreg[DS].base + REG16(DX);
	int max = mem[ofs] - 1;
	UINT8 *buf = mem + ofs + 2;
	int chr, p = 0;
	
	while((chr = msdos_getch()) != 0x0d) {
		if(chr == 0x08) {
			// back space
			if(p > 0) {
				p--;
				msdos_putch(chr);
				msdos_putch(' ');
				msdos_putch(chr);
			}
		} else if(p < max) {
			buf[p++] = chr;
			msdos_putch(chr);
		}
	}
	buf[p] = 0x0d;
	mem[ofs + 1] = p;
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_0bh()
{
	if(msdos_kbhit()) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0x00;
		Sleep(10);
	}
}

inline void msdos_int_21h_0ch()
{
	// clear key buffer
	if(file_handler[0].valid && !file_handler[0].atty) {
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
		REG16(AX) = 0x01;
		cpustate->CF = 1;
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
	}
	REG8(AL) = 26; // zdrive
}

inline void msdos_int_21h_11h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + cpustate->sreg[DS].base + REG16(DX));
	fcb_t *fcb = (fcb_t *)(mem + cpustate->sreg[DS].base + REG16(DX) + (ext_fcb->flag == 0xff ? 7 : 0));
	
	process_t *process = msdos_process_info_get(current_psp);
	ext_fcb_t *ext_find = (ext_fcb_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l);
	find_fcb_t *find = (find_fcb_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l + (ext_fcb->flag == 0xff ? 7 : 0));
	char *path = msdos_fcb_path(fcb);
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(process->find_handle);
		process->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	process->allowable_mask = (ext_fcb->flag == 0xff) ? ext_fcb->attribute : 0x20;
	
	if((process->find_handle = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, process->allowable_mask, 0)) {
			if(!FindNextFile(process->find_handle, &fd)) {
				FindClose(process->find_handle);
				process->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_path(fd.cFileName));
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
	} else if(process->allowable_mask & 8) {
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
		process->allowable_mask &= ~8;
		REG8(AL) = 0x00;
	} else {
		REG8(AL) = 0xff;
	}
}

inline void msdos_int_21h_12h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + cpustate->sreg[DS].base + REG16(DX));
	fcb_t *fcb = (fcb_t *)(mem + cpustate->sreg[DS].base + REG16(DX) + (ext_fcb->flag == 0xff ? 7 : 0));
	
	process_t *process = msdos_process_info_get(current_psp);
	ext_fcb_t *ext_find = (ext_fcb_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l);
	find_fcb_t *find = (find_fcb_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l + (ext_fcb->flag == 0xff ? 7 : 0));
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFile(process->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, process->allowable_mask, 0)) {
				if(!FindNextFile(process->find_handle, &fd)) {
					FindClose(process->find_handle);
					process->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(process->find_handle);
			process->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_path(fd.cFileName));
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
	} else if(process->allowable_mask & 8) {
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
		process->allowable_mask &= ~8;
		REG8(AL) = 0x00;
	} else {
		REG8(AL) = 0xff;
	}
}

inline void msdos_int_21h_13h()
{
	if(remove(msdos_fcb_path((fcb_t *)(mem + cpustate->sreg[DS].base + REG16(DX))))) {
		REG8(AL) = 0xff;
	} else {
		REG8(AL) = 0x00;
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
	process->dta.w.h = cpustate->sreg[DS].selector;
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
		*(UINT8 *)(mem + cpustate->sreg[DS].base + REG16(BX)) = dpb->media_type;
	} else {
		REG8(AL) = 0xff;
		cpustate->CF = 1;
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
		*(UINT8 *)(mem + cpustate->sreg[DS].base + REG16(BX)) = dpb->media_type;
	} else {
		REG8(AL) = 0xff;
		cpustate->CF = 1;
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
		cpustate->sreg[DS].selector = seg;
		i386_load_segment_descriptor(cpustate, DS);
		REG16(BX) = ofs;
	} else {
		REG8(AL) = 0xff;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_20h()
{
	REG8(AL) = 0;
}

inline void msdos_int_21h_25h()
{
	*(UINT16 *)(mem + 4 * REG8(AL) + 0) = REG16(DX);
	*(UINT16 *)(mem + 4 * REG8(AL) + 2) = cpustate->sreg[DS].selector;
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

inline void msdos_int_21h_29h()
{
	int ofs = cpustate->sreg[DS].base + REG16(SI);
	char name[MAX_PATH], ext[MAX_PATH];
	UINT8 drv = 0;
	char sep_chars[] = ":.;,=+";
	char end_chars[] = "\\<>|/\"[]";
	char spc_chars[] = " \t";
	
	if(REG8(AL) & 1) {
		ofs += strspn((char *)&mem[ofs], spc_chars);
		if(strchr(sep_chars, mem[ofs]) && mem[ofs] != '\0') {
			ofs++;
		}
	}
	ofs += strspn((char *)&mem[ofs], spc_chars);
	
	if(mem[ofs + 1] == ':') {
		UINT8 c = mem[ofs];
		if(c <= 0x20 || strchr(end_chars, c) || strchr(sep_chars, c)) {
			; // invalid drive letter
		} else {
			if(c >= 'a' && c <= 'z') {
				drv = c - 'a' + 1;
			} else {
				drv = c - 'A' + 1;
			}
			ofs += 2;
		}
	}
	memset(name, 0x20, sizeof(name));
	memset(ext, 0x20, sizeof(ext));
	for(int i = 0; i < MAX_PATH; i++) {
		UINT8 c = mem[ofs];
		if(c <= 0x20 || strchr(end_chars, c) || strchr(sep_chars, c)) {
			break;
		} else if(c >= 'a' && c <= 'z') {
			c -= 0x20;
		}
		ofs++;
		name[i] = c;
	}
	if(mem[ofs] == '.') {
		ofs++;
		for(int i = 0; i < MAX_PATH; i++) {
			UINT8 c = mem[ofs];
			if(c <= 0x20 || strchr(end_chars, c) || strchr(sep_chars, c)) {
				break;
			} else if(c >= 'a' && c <= 'z') {
				c -= 0x20;
			}
			ofs++;
			ext[i] = c;
		}
	}
	int si = ofs - cpustate->sreg[DS].base;
	int ds = cpustate->sreg[DS].selector;
	while(si > 0xffff) {
		si -= 0x10;
		ds++;
	}
	REG16(SI) = si;
	cpustate->sreg[DS].selector = ds;
	i386_load_segment_descriptor(cpustate, DS);
	
	UINT8 *fcb = mem + cpustate->sreg[ES].base + REG16(DI);
	fcb[0] = drv;
	memcpy(fcb + 1, name, 8);
	int found_star = 0;
	for(int i = 1; i < 1 + 8; i++) {
		if(fcb[i] == '*') {
			found_star = 1;
		}
		if(found_star) {
			fcb[i] = '?';
		}
	}
	memcpy(fcb + 9, ext, 3);
	found_star = 0;
	for(int i = 9; i < 9 + 3; i++) {
		if(fcb[i] == '*') {
			found_star = 1;
		}
		if(found_star) {
			fcb[i] = '?';
		}
	}
	
	REG8(AL) = 0x00;
	if(drv == 0 || (drv > 0 && drv <= 26 && (GetLogicalDrives() & ( 1 << (drv - 1) )))) {
		if(memchr(fcb + 1, '?', 8 + 3)) {
			REG8(AL) = 0x01;
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
	REG8(AL) = 0x00;
}

inline void msdos_int_21h_2ch()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	REG8(CH) = (UINT8)sTime.wHour;
	REG8(CL) = (UINT8)sTime.wMinute;
	REG8(DH) = (UINT8)sTime.wSecond;
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
	cpustate->sreg[ES].selector = process->dta.w.h;
	i386_load_segment_descriptor(cpustate, ES);
}

inline void msdos_int_21h_30h()
{
	// Version Flag / OEM
	if(REG8(AL) == 1) {
		REG8(BH) = 0x00;	// not in ROM
	} else {
		REG8(BH) = 0xff;	// OEM = Microsoft
	}
	// MS-DOS version (7.00)
	REG8(AL) = 7;
	REG8(AH) = 0;
}

inline void msdos_int_21h_31h()
{
	int mcb_seg = current_psp - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->paragraphs = REG16(DX);
	mcb_seg += mcb->paragraphs + 1;
	msdos_mcb_create(mcb_seg, 'Z', 0, (MEMORY_END >> 4) - mcb_seg - 1);
	
	msdos_process_terminate(current_psp, REG8(AL) | 0x300, 0);
}

inline void msdos_int_21h_32h()
{
	int drive_num = (REG8(DL) == 0) ? (_getdrive() - 1) : (REG8(DL) - 1);
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		REG8(AL) = 0;
		cpustate->sreg[DS].selector = seg;
		i386_load_segment_descriptor(cpustate, DS);
		REG16(BX) = ofs;
	} else {
		REG8(AL) = 0xff;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_33h()
{
	static UINT8 state = 0x00;
	char path[MAX_PATH];
	
	switch(REG8(AL)) {
	case 0x00:
		REG8(DL) = state;
		break;
	case 0x01:
		state = REG8(DL);
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
		// MS-DOS version (7.00)
		REG8(BL) = 7;
		REG8(BH) = 0;
		REG8(DL) = 0;
		REG8(DH) = 0x10; // in HMA
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_35h()
{
	REG16(BX) = *(UINT16 *)(mem + 4 * REG8(AL) + 0);
	cpustate->sreg[ES].selector = *(UINT16 *)(mem + 4 * REG8(AL) + 2);
	i386_load_segment_descriptor(cpustate, ES);
}

inline void msdos_int_21h_36h()
{
	struct _diskfree_t df = {0};
	
	if(_getdiskfree(REG8(DL), &df) == 0) {
		REG16(AX) = (UINT16)df.sectors_per_cluster;
		REG16(CX) = (UINT16)df.bytes_per_sector;
		REG16(BX) = (UINT16)df.avail_clusters;
		REG16(DX) = (UINT16)df.total_clusters;
	} else {
		REG16(AX) = 0xffff;
	}
}

inline void msdos_int_21h_37h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	switch(REG8(AL)) {
	case 0x00:
		REG8(AL) = 0x00;
		REG8(DL) = process->switchar;
		break;
	case 0x01:
		REG8(AL) = 0x00;
		process->switchar = REG8(DL);
		break;
	default:
		REG16(AX) = 1;
		break;
	}
}

inline void msdos_int_21h_39h(int lfn)
{
	if(_mkdir(msdos_trimmed_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_3ah(int lfn)
{
	if(_rmdir(msdos_trimmed_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_3bh(int lfn)
{
	if(_chdir(msdos_trimmed_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_3ch()
{
	char *path = msdos_local_file_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), 0);
	int attr = GetFileAttributes(path);
	int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
	
	if(fd != -1) {
		if(attr == -1) {
			attr = msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY;
		}
		SetFileAttributes(path, attr);
		REG16(AX) = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
	} else {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_3dh()
{
	char *path = msdos_local_file_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), 0);
	int mode = REG8(AL) & 0x03;
	
	if(mode < 0x03) {
		int fd = _open(path, file_mode[mode].mode);
		
		if(fd != -1) {
			REG16(AX) = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), mode, msdos_drive_number(path), current_psp);
		} else {
			REG16(AX) = errno;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x0c;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_3eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		_close(REG16(BX));
		msdos_file_handler_close(REG16(BX), current_psp);
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_3fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		if(file_mode[file_handler[REG16(BX)].mode].in) {
			if(file_handler[REG16(BX)].atty) {
				// BX is stdin or is redirected to stdin
				UINT8 *buf = mem + cpustate->sreg[DS].base + REG16(DX);
				int max = REG16(CX);
				int p = 0;
				
				while(max > p) {
					int chr = msdos_getch();
					
					if(chr == 0x0d) {
						// carriage return
						buf[p++] = 0x0d;
						if(max > p) {
							buf[p++] = 0x0a;
						}
						break;
					} else if(chr == 0x08) {
						// back space
						if(p > 0) {
							p--;
							msdos_putch(chr);
							msdos_putch(' ');
							msdos_putch(chr);
						}
					} else {
						buf[p++] = chr;
						msdos_putch(chr);
					}
				}
				REG16(AX) = p;
#ifdef SUPPORT_HARDWARE
				hardware_update();
#endif
			} else {
				REG16(AX) = _read(REG16(BX), mem + cpustate->sreg[DS].base + REG16(DX), REG16(CX));
			}
		} else {
			REG16(AX) = 0x05;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_40h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		if(file_mode[file_handler[REG16(BX)].mode].out) {
			if(REG16(CX)) {
				if(file_handler[REG16(BX)].atty) {
					// BX is stdout/stderr or is redirected to stdout
					for(int i = 0; i < REG16(CX); i++) {
						msdos_putch(mem[cpustate->sreg[DS].base + REG16(DX) + i]);
					}
					REG16(AX) = REG16(CX);
				} else {
					REG16(AX) = msdos_write(REG16(BX), mem + cpustate->sreg[DS].base + REG16(DX), REG16(CX));
				}
			} else {
				UINT32 pos = _tell(REG16(BX));
				_lseek(REG16(BX), 0, SEEK_END);
				UINT32 size = _tell(REG16(BX));
				REG16(AX) = 0;
				for(UINT32 i = size; i < pos; i++) {
					UINT8 tmp = 0;
					REG16(AX) += msdos_write(REG16(BX), &tmp, 1);
				}
				_lseek(REG16(BX), pos, SEEK_SET);
			}
		} else {
			REG16(AX) = 0x05;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_41h(int lfn)
{
	if(remove(msdos_trimmed_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), lfn))) {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_42h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		if(REG8(AL) < 0x03) {
			static int ptrname[] = { SEEK_SET, SEEK_CUR, SEEK_END };
			_lseek(REG16(BX), (REG16(CX) << 16) | REG16(DX), ptrname[REG8(AL)]);
			UINT32 pos = _tell(REG16(BX));
			REG16(AX) = pos & 0xffff;
			REG16(DX) = (pos >> 16);
		} else {
			REG16(AX) = 0x01;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_43h(int lfn)
{
	char *path = msdos_local_file_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), lfn);
	int attr;
	
	switch(REG8(AL)) {
	case 0x00:
		if((attr = GetFileAttributes(path)) != -1) {
			REG16(CX) = 0;
			if(attr & FILE_ATTRIBUTE_READONLY) {
				REG16(CX) |= 0x01;
			}
			if(attr & FILE_ATTRIBUTE_HIDDEN) {
				REG16(CX) |= 0x02;
			}
			if(attr & FILE_ATTRIBUTE_SYSTEM) {
				REG16(CX) |= 0x04;
			}
			if(attr & FILE_ATTRIBUTE_ARCHIVE) {
				REG16(CX) |= 0x20;
			}
		} else {
			REG16(AX) = (UINT16)GetLastError();
			cpustate->CF = 1;
		}
		break;
	case 0x01:
		if(SetFileAttributes(path, msdos_file_attribute_create(REG16(CX))) != 0) {
			REG16(AX) = (UINT16)GetLastError();
			cpustate->CF = 1;
		}
		break;
	case 0x02:
		break;
	case 0x03:
		REG16(CX) = 0x00;
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_44h()
{
	switch(REG8(AL)) {
	case 0x00: // get ioctrl data
		REG16(DX) = file_handler[REG16(BX)].info;
		break;
	case 0x01: // set ioctrl data
		file_handler[REG16(BX)].info |= REG8(DL);
		break;
	case 0x02: // recv from character device
	case 0x03: // send to character device
		REG16(AX) = 0x06;
		cpustate->CF = 1;
		break;
	case 0x04: // recv from block device
	case 0x05: // send to block device
		REG16(AX) = 0x05;
		cpustate->CF = 1;
		break;
	case 0x06: // get read status
		{
			process_t *process = msdos_process_info_get(current_psp);
			
			if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
				if(file_mode[file_handler[REG16(BX)].mode].in) {
					if(file_handler[REG16(BX)].atty) {
						REG8(AL) = msdos_kbhit() ? 0xff : 0x00;
					} else {
						REG8(AL) = eof(REG16(BX)) ? 0x00 : 0xff;
					}
				} else {
					REG8(AL) = 0x00;
				}
			} else {
				REG16(AX) = 0x06;
				cpustate->CF = 1;
			}
		}
		break;
	case 0x07: // get write status
		{
			process_t *process = msdos_process_info_get(current_psp);
			
			if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
				if(file_mode[file_handler[REG16(BX)].mode].out) {
					REG8(AL) = 0xff;
				} else {
					REG8(AL) = 0x00;
				}
			} else {
				REG16(AX) = 0x06;
				cpustate->CF = 1;
			}
		}
		break;
	case 0x08: // check removable drive
		if(REG8(BL) < ('Z' - 'A' + 1)) {
			UINT32 val;
			if(REG8(BL) == 0) {
				val = GetDriveType(NULL);
			} else if(REG8(BL) < ('Z' - 'A' + 1)) {
				char tmp[8];
				sprintf(tmp, "%c:\\", 'A' + REG8(BL) - 1);
				val = GetDriveType(tmp);
			}
			if(val == DRIVE_NO_ROOT_DIR) {
				// no drive
				REG16(AX) = 0x0f;
				cpustate->CF = 1;
			} else if(val == DRIVE_REMOVABLE || val == DRIVE_CDROM) {
				// removable drive
				REG16(AX) = 0x00;
			} else {
				// fixed drive
				REG16(AX) = 0x01;
			}
		} else {
			// invalid drive number
			REG16(AX) = 0x0f;
			cpustate->CF = 1;
		}
		break;
	case 0x09: // check remote drive
		if(REG8(BL) < ('Z' - 'A' + 1)) {
			UINT32 val;
			if(REG8(BL) == 0) {
				val = GetDriveType(NULL);
			} else if(REG8(BL) < ('Z' - 'A' + 1)) {
				char tmp[8];
				sprintf(tmp, "%c:\\", 'A' + REG8(BL) - 1);
				val = GetDriveType(tmp);
			}
			if(val == DRIVE_NO_ROOT_DIR) {
				// no drive
				REG16(AX) = 0x0f;
				cpustate->CF = 1;
			} else if(val == DRIVE_REMOTE) {
				// remote drive
				REG16(DX) = 0x1000;
			} else {
				// local drive
				REG16(DX) = 0x00;
			}
		} else {
			// invalid drive number
			REG16(AX) = 0x0f;
			cpustate->CF = 1;
		}
		break;
	case 0x0b: // set retry count
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_45h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		int fd = _dup(REG16(BX));
		if(fd != -1) {
			REG16(AX) = fd;
			msdos_file_handler_dup(REG16(AX), REG16(BX), current_psp);
		} else {
			REG16(AX) = errno;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_46h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && REG16(CX) < process->max_files && file_handler[REG16(BX)].valid) {
		if(_dup2(REG16(BX), REG16(CX)) != -1) {
			msdos_file_handler_dup(REG16(CX), REG16(BX), current_psp);
		} else {
			REG16(AX) = errno;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_47h(int lfn)
{
	char path[MAX_PATH];
	
	if(_getdcwd(REG8(DL), path, MAX_PATH) != NULL) {
		if(path[1] == ':') {
			// the returned path does not include a drive or the initial backslash
			strcpy((char *)(mem + cpustate->sreg[DS].base + REG16(SI)), (lfn ? path : msdos_short_path(path)) + 3);
		} else {
			strcpy((char *)(mem + cpustate->sreg[DS].base + REG16(SI)), lfn ? path : msdos_short_path(path));
		}
	} else {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_48h()
{
	int seg;
	
	if((seg = msdos_mem_alloc(REG16(BX), 0)) != -1) {
		REG16(AX) = seg;
	} else {
		REG16(AX) = 0x08;
		REG16(BX) = msdos_mem_get_free(0);
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_49h()
{
	msdos_mem_free(cpustate->sreg[ES].selector);
}

inline void msdos_int_21h_4ah()
{
	int max_paragraphs;
	
	if(msdos_mem_realloc(cpustate->sreg[ES].selector, REG16(BX), &max_paragraphs)) {
		REG16(AX) = 0x08;
		REG16(BX) = max_paragraphs;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_4bh()
{
	char *command = (char *)(mem + cpustate->sreg[DS].base + REG16(DX));
	param_block_t *param = (param_block_t *)(mem + cpustate->sreg[ES].base + REG16(BX));
	
	switch(REG8(AL)) {
	case 0x00:
	case 0x01:
		if(msdos_process_exec(command, param, REG8(AL))) {
			REG16(AX) = 0x02;
			cpustate->CF = 1;
		}
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
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
	find_t *find = (find_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l);
	char *path = msdos_trimmed_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), 0);
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(process->find_handle);
		process->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	process->allowable_mask = REG8(CL);
	
	if((process->find_handle = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, process->allowable_mask, 0)) {
			if(!FindNextFile(process->find_handle, &fd)) {
				FindClose(process->find_handle);
				process->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = (UINT8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_path(fd.cFileName));
		REG16(AX) = 0;
	} else if(process->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		process->allowable_mask &= ~8;
		REG16(AX) = 0;
	} else {
		REG16(AX) = 0x12;	// NOTE: return 0x02 if file path is invalid
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_4fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_t *find = (find_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l);
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFile(process->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, process->allowable_mask, 0)) {
				if(!FindNextFile(process->find_handle, &fd)) {
					FindClose(process->find_handle);
					process->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(process->find_handle);
			process->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = (UINT8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_path(fd.cFileName));
		REG16(AX) = 0;
	} else if(process->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		process->allowable_mask &= ~8;
		REG16(AX) = 0;
	} else {
		REG16(AX) = 0x12;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_50h()
{
	current_psp = REG16(BX);
}

inline void msdos_int_21h_51h()
{
	REG16(BX) = current_psp;
}

inline void msdos_int_21h_52h()
{
	cpustate->sreg[ES].selector = (DOS_INFO_BASE >> 4);
	i386_load_segment_descriptor(cpustate, ES);
	REG16(BX) = (DOS_INFO_BASE & 0x0f);
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
	strcpy(src, msdos_trimmed_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), lfn));
	strcpy(dst, msdos_trimmed_path((char *)(mem + cpustate->sreg[ES].base + REG16(DI)), lfn));
	
	if(rename(src, dst)) {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_57h()
{
	FILETIME time, local;
	
	switch(REG8(AL)) {
	case 0x00:
		if(GetFileTime((HANDLE)_get_osfhandle(REG16(BX)), NULL, NULL, &time)) {
			FileTimeToLocalFileTime(&time, &local);
			FileTimeToDosDateTime(&local, &REG16(DX), &REG16(CX));
		} else {
			REG16(AX) = (UINT16)GetLastError();
			cpustate->CF = 1;
		}
		break;
	case 0x01:
		DosDateTimeToFileTime(REG16(DX), REG16(CX), &local);
		LocalFileTimeToFileTime(&local, &time);
		if(!SetFileTime((HANDLE)_get_osfhandle(REG16(BX)), NULL, NULL, &time)) {
			REG16(AX) = (UINT16)GetLastError();
			cpustate->CF = 1;
		}
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_58h()
{
	switch(REG8(AL)) {
	case 0x00:
		REG16(AX) = 0x00;
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_5ah()
{
	char *path = (char *)(mem + cpustate->sreg[DS].base + REG16(DX));
	int len = strlen(path);
	char tmp[MAX_PATH];
	
	if(GetTempFileName(path, "TMP", 0, tmp)) {
		int fd = _open(tmp, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		
		SetFileAttributes(tmp, msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY);
		REG16(AX) = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
		
		strcpy(path, tmp);
		int dx = REG16(DX) + len;
		int ds = cpustate->sreg[DS].selector;
		while(dx > 0xffff) {
			dx -= 0x10;
			ds++;
		}
		REG16(DX) = dx;
		cpustate->sreg[DS].selector = ds;
		i386_load_segment_descriptor(cpustate, DS);
	} else {
		REG16(AX) = (UINT16)GetLastError();
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_5bh()
{
	char *path = msdos_local_file_path((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), 0);
	
	if(_access(path, 0) == 0) {
		// already exists
		REG16(AX) = 0x50;
		cpustate->CF = 1;
	} else {
		int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		
		if(fd != -1) {
			SetFileAttributes(path, msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY);
			REG16(AX) = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
		} else {
			REG16(AX) = errno;
			cpustate->CF = 1;
		}
	}
}

inline void msdos_int_21h_5ch()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		if(REG8(AL) == 0 || REG8(AL) == 1) {
			static int modes[2] = {_LK_LOCK, _LK_UNLCK};
			UINT32 pos = _tell(REG16(BX));
			_lseek(REG16(BX), (REG16(CX) << 16) | REG16(DX), SEEK_SET);
			if(_locking(REG16(BX), modes[REG8(AL)], (REG16(SI) << 16) | REG16(DI))) {
				REG16(AX) = errno;
				cpustate->CF = 1;
			}
			_lseek(REG16(BX), pos, SEEK_SET);
#ifdef SUPPORT_HARDWARE
			// some seconds may be passed in _locking()
			hardware_update();
#endif
		} else {
			REG16(AX) = 0x01;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_60h(int lfn)
{
	if(lfn) {
		char full[MAX_PATH], *name;
		GetFullPathName((char *)(mem + cpustate->sreg[DS].base + REG16(SI)), MAX_PATH, full, &name);
		strcpy((char *)(mem + cpustate->sreg[ES].base + REG16(DI)), full);
	} else {
		strcpy((char *)(mem + cpustate->sreg[ES].base + REG16(DI)), msdos_short_full_path((char *)(mem + cpustate->sreg[DS].base + REG16(SI))));
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
		cpustate->sreg[DS].selector = (DBCS_TABLE >> 4);
		i386_load_segment_descriptor(cpustate, DS);
		REG16(SI) = (DBCS_TABLE & 0x0f);
		REG8(AL) = 0x00;
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_65h()
{
	char tmp[0x10000];
	
	switch(REG8(AL)) {
	case 0x07:
		*(UINT8  *)(mem + cpustate->sreg[ES].base + REG16(DI) + 0) = 0x07;
		*(UINT16 *)(mem + cpustate->sreg[ES].base + REG16(DI) + 1) = (DBCS_TOP & 0x0f);
		*(UINT16 *)(mem + cpustate->sreg[ES].base + REG16(DI) + 3) = (DBCS_TOP >> 4);
		REG16(CX) = 0x05;
		break;
	case 0x20:
		sprintf(tmp, "%c", REG8(DL));
		msdos_strupr(tmp);
		REG8(DL) = tmp[0];
		break;
	case 0x21:
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, mem + cpustate->sreg[DS].base + REG16(DX), REG16(CX));
		msdos_strupr(tmp);
		memcpy(mem + cpustate->sreg[DS].base + REG16(DX), tmp, REG16(CX));
		break;
	case 0x22:
		msdos_strupr((char *)(mem + cpustate->sreg[DS].base + REG16(DX)));
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
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
			msdos_dbcs_table_update();
			REG16(AX) = 0xeb41;
		} else {
			REG16(AX) = 0x25;
			cpustate->CF = 1;
		}
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
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
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_68h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		// fflush(_fdopen(REG16(BX), ""));
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
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
	char *path = msdos_local_file_path((char *)(mem + cpustate->sreg[DS].base + REG16(SI)), lfn);
	int mode = REG8(BL) & 0x03;
	
	if(mode < 0x03) {
		if(_access(path, 0) == 0) {
			// file exists
			if(REG8(DL) & 1) {
				int fd = _open(path, file_mode[mode].mode);
				
				if(fd != -1) {
					REG16(AX) = fd;
					REG16(CX) = 1;
					msdos_file_handler_open(fd, path, _isatty(fd), mode, msdos_drive_number(path), current_psp);
				} else {
					REG16(AX) = errno;
					cpustate->CF = 1;
				}
			} else if(REG8(DL) & 2) {
				int attr = GetFileAttributes(path);
				int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
				
				if(fd != -1) {
					if(attr == -1) {
						attr = msdos_file_attribute_create(REG16(CX)) & ~FILE_ATTRIBUTE_READONLY;
					}
					SetFileAttributes(path, attr);
					REG16(AX) = fd;
					REG16(CX) = 3;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
				} else {
					REG16(AX) = errno;
					cpustate->CF = 1;
				}
			} else {
				REG16(AX) = 0x50;
				cpustate->CF = 1;
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
				} else {
					REG16(AX) = errno;
					cpustate->CF = 1;
				}
			} else {
				REG16(AX) = 0x02;
				cpustate->CF = 1;
			}
		}
	} else {
		REG16(AX) = 0x0c;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_710dh()
{
	// reset drive
}

inline void msdos_int_21h_714eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + cpustate->sreg[ES].base + REG16(DI));
	char *path = (char *)(mem + cpustate->sreg[DS].base + REG16(DX));
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(process->find_handle);
		process->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	process->allowable_mask = REG8(CL);
	process->required_mask = REG8(CH);
	
	if((process->find_handle = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, process->allowable_mask, process->required_mask)) {
			if(!FindNextFile(process->find_handle, &fd)) {
				FindClose(process->find_handle);
				process->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(process->find_handle != INVALID_HANDLE_VALUE) {
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
		strcpy(find->short_name, msdos_short_path(fd.cFileName));
	} else if(process->allowable_mask & 8) {
		// volume label
		find->attrib = 8;
		find->size_hi = find->size_lo = 0;
		strcpy(find->full_name, process->volume_label);
		strcpy(find->short_name, msdos_short_volume_label(process->volume_label));
		process->allowable_mask &= ~8;
	} else {
		REG16(AX) = 0x12;	// NOTE: return 0x02 if file path is invalid
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_714fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + cpustate->sreg[ES].base + REG16(DI));
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFile(process->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, process->allowable_mask, process->required_mask)) {
				if(!FindNextFile(process->find_handle, &fd)) {
					FindClose(process->find_handle);
					process->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(process->find_handle);
			process->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(process->find_handle != INVALID_HANDLE_VALUE) {
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
		strcpy(find->short_name, msdos_short_path(fd.cFileName));
	} else if(process->allowable_mask & 8) {
		// volume label
		find->attrib = 8;
		find->size_hi = find->size_lo = 0;
		strcpy(find->full_name, process->volume_label);
		strcpy(find->short_name, msdos_short_volume_label(process->volume_label));
		process->allowable_mask &= ~8;
	} else {
		REG16(AX) = 0x12;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_71a0h()
{
	DWORD max_component_len, file_sys_flag;
	
	if(GetVolumeInformation((char *)(mem + cpustate->sreg[DS].base + REG16(DX)), NULL, 0, NULL, &max_component_len, &file_sys_flag, (char *)(mem + cpustate->sreg[ES].base + REG16(DI)), REG16(CX))) {
		REG16(BX) = (UINT16)file_sys_flag;
		REG16(CX) = (UINT16)max_component_len;		// 255
		REG16(DX) = (UINT16)max_component_len + 5;	// 260
	} else {
		REG16(AX) = (UINT16)GetLastError();
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_71a1h()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_t *find = (find_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l);
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(process->find_handle);
		process->find_handle = INVALID_HANDLE_VALUE;
	}
}

inline void msdos_int_21h_71a6h()
{
	process_t *process = msdos_process_info_get(current_psp);
	UINT8 *buffer = (UINT8 *)(mem + cpustate->sreg[DS].base + REG16(DX));
	struct _stat64 status;
	DWORD serial_number = 0;
	
	if(REG16(BX) < process->max_files && file_handler[REG16(BX)].valid) {
		if(_fstat64(REG16(BX), &status) == 0) {
			if(file_handler[REG16(BX)].path[1] == ':') {
				// NOTE: we need to consider the network file path "\\host\share\"
				char volume[] = "A:\\";
				volume[0] = file_handler[REG16(BX)].path[1];
				GetVolumeInformation(volume, NULL, 0, &serial_number, NULL, NULL, NULL, 0);
			}
			*(UINT32 *)(buffer + 0x00) = GetFileAttributes(file_handler[REG16(BX)].path);
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
			// this is dummy id and it will be changed when it is reopend...
			*(UINT32 *)(buffer + 0x2c) = 0;
			*(UINT32 *)(buffer + 0x30) = file_handler[REG16(BX)].id;
		} else {
			REG16(AX) = errno;
			cpustate->CF = 1;
		}
	} else {
		REG16(AX) = 0x06;
		cpustate->CF = 1;
	}
}

inline void msdos_int_21h_71a7h()
{
	switch(REG8(BL)) {
	case 0x00:
		if(!FileTimeToDosDateTime((FILETIME *)(mem + cpustate->sreg[DS].base + REG16(SI)), &REG16(DX), &REG16(CX))) {
			REG16(AX) = (UINT16)GetLastError();
			cpustate->CF = 1;
		}
		break;
	case 0x01:
		// NOTE: we need to check BH that shows 10-millisecond untils past time in CX
		if(!DosDateTimeToFileTime(REG16(DX), REG16(CX), (FILETIME *)(mem + cpustate->sreg[ES].base + REG16(DI)))) {
			REG16(AX) = (UINT16)GetLastError();
			cpustate->CF = 1;
		}
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_21h_71a8h()
{
	if(REG8(DH) == 0) {
		char tmp[MAX_PATH], fcb[MAX_PATH];
		strcpy(tmp, msdos_short_path((char *)(mem + cpustate->sreg[DS].base + REG16(SI))));
		memset(fcb, 0x20, sizeof(fcb));
		int len = strlen(tmp);
		int pos = 0;
		for(int i = 0; i < len; i++) {
			if(tmp[i] == '.') {
				pos = 8;
			} else {
				if(msdos_lead_byte_check(tmp[i])) {
					fcb[pos++] = tmp[i++];
				}
				fcb[pos++] = tmp[i];
			}
		}
		memcpy((char *)(mem + cpustate->sreg[ES].base + REG16(DI)), fcb, 11);
	} else {
		strcpy((char *)(mem + cpustate->sreg[ES].base + REG16(DI)), msdos_short_path((char *)(mem + cpustate->sreg[DS].base + REG16(SI))));
	}
}

inline void msdos_int_21h_7303h()
{
	char *path = (char *)(mem + cpustate->sreg[DS].base + REG16(DX));
	ext_free_space_t *space = (ext_free_space_t *)(mem + cpustate->sreg[ES].base + REG16(DI));
	DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
	
	if(GetDiskFreeSpace(path, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters)) {
		space->size_of_structure = sizeof(ext_free_space_t);
		space->structure_version = 0;
		space->sectors_per_cluster = sectors_per_cluster;
		space->bytes_per_sector = bytes_per_sector;
		space->available_clusters_on_drive = free_clusters;
		space->total_clusters_on_drive = total_clusters;
		space->available_sectors_on_drive = sectors_per_cluster * free_clusters;
		space->total_sectors_on_drive = sectors_per_cluster * total_clusters;
		space->available_allocation_units = free_clusters;	// ???
		space->total_allocation_units = total_clusters;		// ???
	} else {
		REG16(AX) = errno;
		cpustate->CF = 1;
	}
}

inline void msdos_int_25h()
{
	UINT16 seg, ofs;
	DWORD dwSize;
	
	I386OP(pushf)(cpustate);
	
	if(!(REG8(AL) < 26)) {
		REG8(AL) = 0x01; // unit unknown
		cpustate->CF = 1;
	} else if(!msdos_drive_param_block_update(REG8(AL), &seg, &ofs, 0)) {
		REG8(AL) = 0x02; // drive not ready
		cpustate->CF = 1;
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + REG8(AL));
		
		HANDLE hFile = CreateFile(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			REG8(AL) = 0x02; // drive not ready
			cpustate->CF = 1;
		} else {
			if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
				REG8(AL) = 0x02; // drive not ready
				cpustate->CF = 1;
			} else if(SetFilePointer(hFile, REG16(DX) * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				REG8(AL) = 0x08; // sector not found
				cpustate->CF = 1;
			} else if(ReadFile(hFile, mem + cpustate->sreg[DS].base + REG16(BX), REG16(CX) * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
				REG8(AL) = 0x0b; // read error
				cpustate->CF = 1;
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
	
	I386OP(pushf)(cpustate);
	
	if(!(REG8(AL) < 26)) {
		REG8(AL) = 0x01; // unit unknown
		cpustate->CF = 1;
	} else if(!msdos_drive_param_block_update(REG8(AL), &seg, &ofs, 0)) {
		REG8(AL) = 0x02; // drive not ready
		cpustate->CF = 1;
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + REG8(AL));
		
		if(dpb->media_type == 0xf8) {
			// this drive is not a floppy
			REG8(AL) = 0x02; // drive not ready
			cpustate->CF = 1;
		} else {
			HANDLE hFile = CreateFile(dev, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
			if(hFile == INVALID_HANDLE_VALUE) {
				REG8(AL) = 0x02; // drive not ready
				cpustate->CF = 1;
			} else {
				if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
					REG8(AL) = 0x02; // drive not ready
					cpustate->CF = 1;
				} else if(SetFilePointer(hFile, REG16(DX) * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
					REG8(AL) = 0x08; // sector not found
					cpustate->CF = 1;
				} else if(WriteFile(hFile, mem + cpustate->sreg[DS].base + REG16(BX), REG16(CX) * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
					REG8(AL) = 0x0a; // write error
					cpustate->CF = 1;
				}
				CloseHandle(hFile);
			}
		}
	}
}

inline void msdos_int_27h()
{
	int mcb_seg = cpustate->sreg[CS].selector - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->paragraphs = (REG16(DX) >> 4);
	mcb_seg += mcb->paragraphs + 1;
	msdos_mcb_create(mcb_seg, 'Z', 0, (MEMORY_END >> 4) - mcb_seg - 1);
	
	msdos_process_terminate(cpustate->sreg[CS].selector, retval | 0x300, 0);
}

inline void msdos_int_29h()
{
	msdos_putch(REG8(AL));
}

inline void msdos_int_2eh()
{
	char tmp[MAX_PATH], command[MAX_PATH], opt[MAX_PATH];
	memset(tmp, 0, sizeof(tmp));
	strcpy(tmp, (char *)(mem + cpustate->sreg[DS].base + REG16(SI)));
	char *token = strtok(tmp, " ");
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
	
	msdos_process_exec(command, param, 0);
	REG8(AL) = 0;
}

inline void msdos_int_2fh_4ah()
{
	switch(REG8(AL)) {
	case 0x01:
	case 0x02:
		// hma is not installed
		REG16(BX) = 0;
		cpustate->sreg[ES].selector = 0xffff;
		i386_load_segment_descriptor(cpustate, ES);
		REG16(DI) = 0xffff;
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

inline void msdos_int_2fh_4fh()
{
	switch(REG8(AL)) {
	case 0x00:
		REG16(AX) = 0;
		REG8(DL) = 1;	// major version
		REG8(DH) = 0;	// minor version
		break;
	case 0x01:
		REG16(AX) = 0;
		REG16(BX) = active_code_page;
		break;
	default:
		REG16(AX) = 0x01;
		cpustate->CF = 1;
		break;
	}
}

void msdos_syscall(unsigned num)
{
	switch(num) {
	case 0x00:
		error("division by zero\n");
		msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		break;
	case 0x04:
		error("overflow\n");
		msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		break;
	case 0x06:
		// NOTE: ish.com has illegal instruction...
//		error("illegal instruction\n");
//		msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		break;
	case 0x10:
		// PC BIOS
		if(scr_width != 80 || scr_height != 25) {
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			SMALL_RECT rect;
			COORD co;
			
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			if(csbi.dwCursorPosition.Y > 24) {
				SET_RECT(rect, 0, 0, scr_width - 1, scr_height - 1);
				ReadConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
				for(int y = 0, y2 = csbi.dwCursorPosition.Y - 24; y < 25; y++, y2++) {
					for(int x = 0; x < 80; x++) {
						scr_buf[y][x] = scr_buf[y2][x];
					}
				}
				WriteConsoleOutput(hStdout, &scr_buf[0][0], scr_buf_size, scr_buf_pos, &rect);
				
				co.X = csbi.dwCursorPosition.X;
				co.Y = 24;
				SetConsoleCursorPosition(hStdout, co);
				cursor_moved = true;
			}
			SET_RECT(rect, 0, 0, 79, 24);
			co.X = 80;
			co.Y = 25;
			SetConsoleWindowInfo(hStdout, TRUE, &rect);
			SetConsoleScreenBufferSize(hStdout, co);
			scr_width = 80;
			scr_height = 25;
		}
		cpustate->CF = 0;
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
//		case 0x0d: break;
		case 0x0e: pcbios_int_10h_0eh(); break;
		case 0x0f: pcbios_int_10h_0fh(); break;
		case 0x10: break;
//		case 0x11: break;
		case 0x12: break;
		case 0x13: pcbios_int_10h_13h(); break;
		case 0x1d: pcbios_int_10h_1dh(); break;
		case 0x82: pcbios_int_10h_82h(); break;
		case 0xfe: pcbios_int_10h_feh(); break;
		case 0xff: pcbios_int_10h_ffh(); break;
		default:
			fatalerror("int 10h (ah=%2xh al=%2xh bl=%2xh)\n", REG8(AH), REG8(AL), REG8(BL));
			break;
		}
		break;
	case 0x11:
		// PC BIOS
		REG16(AX) = 0x20;
		break;
	case 0x12:
		// PC BIOS
		REG16(AX) = MEMORY_END / 1024;
		break;
	case 0x13:
		// PC BIOS
		fatalerror("int 13h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
		break;
	case 0x14:
		// PC BIOS
		fatalerror("int 14h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
		break;
	case 0x15:
		// PC BIOS
		cpustate->CF = 0;
		switch(REG8(AH)) {
		case 0x23: pcbios_int_15h_23h(); break;
		case 0x24: pcbios_int_15h_24h(); break;
		case 0x49: pcbios_int_15h_49h(); break;
		case 0x87: pcbios_int_15h_87h(); break;
		case 0x88: pcbios_int_15h_88h(); break;
		case 0x89: pcbios_int_15h_89h(); break;
		case 0xc9: pcbios_int_15h_c9h(); break;
		case 0xca: pcbios_int_15h_cah(); break;
		default:
//			fatalerror("int 15h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
			REG8(AH)=0x86;
			cpustate->CF = 1;
			break;
		}
		break;
	case 0x16:
		// PC BIOS
		cpustate->CF = 0;
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
		default:
			fatalerror("int 16h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
			break;
		}
		break;
	case 0x17:
		// PC BIOS
		fatalerror("int 17h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
		break;
	case 0x18:
		// PC BIOS
		fatalerror("int 18h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
		break;
	case 0x1a:
		// PC BIOS
		cpustate->CF = 0;
		switch(REG8(AH)) {
		case 0x00: pcbios_int_1ah_00h(); break;
		case 0x01: break;
		case 0x02: pcbios_int_1ah_02h(); break;
		case 0x03: break;
		case 0x04: pcbios_int_1ah_04h(); break;
		case 0x05: break;
		case 0x0a: pcbios_int_1ah_0ah(); break;
		case 0x0b: break;
		default:
			fatalerror("int 1ah (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
			break;
		}
		break;
	case 0x20:
		msdos_process_terminate(cpustate->sreg[CS].selector, retval, 1);
		break;
	case 0x21:
		// MS-DOS System Call
		cpustate->CF = 0;
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
		// 0x0f: open file with fcb
		// 0x10: close file with fcb
		case 0x11: msdos_int_21h_11h(); break;
		case 0x12: msdos_int_21h_12h(); break;
		case 0x13: msdos_int_21h_13h(); break;
		// 0x14: sequential read with fcb
		// 0x15: sequential write with fcb
		// 0x16: create new file with fcb
		// 0x17: rename file with fcb
		case 0x18: msdos_int_21h_18h(); break;
		case 0x19: msdos_int_21h_19h(); break;
		case 0x1a: msdos_int_21h_1ah(); break;
		case 0x1b: msdos_int_21h_1bh(); break;
		case 0x1c: msdos_int_21h_1ch(); break;
		case 0x1d: msdos_int_21h_1dh(); break;
		case 0x1e: msdos_int_21h_1eh(); break;
		case 0x1f: msdos_int_21h_1fh(); break;
		case 0x20: msdos_int_21h_20h(); break;
		// 0x21: random read with fcb
		// 0x22: randome write with fcb
		// 0x23: get file size with fcb
		// 0x24: set relative record field with fcb
		case 0x25: msdos_int_21h_25h(); break;
		case 0x26: msdos_int_21h_26h(); break;
		// 0x27: random block read with fcb
		// 0x28: random block write with fcb
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
		// 0x34: get address of indos flag
		case 0x35: msdos_int_21h_35h(); break;
		case 0x36: msdos_int_21h_36h(); break;
		case 0x37: msdos_int_21h_37h(); break;
		// 0x38: get country-specific information
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
		// 0x59: get extended error information
		case 0x5a: msdos_int_21h_5ah(); break;
		case 0x5b: msdos_int_21h_5bh(); break;
		case 0x5c: msdos_int_21h_5ch(); break;
		// 0x5e: ms-network
		// 0x5f: ms-network
		case 0x60: msdos_int_21h_60h(0); break;
		case 0x61: msdos_int_21h_61h(); break;
		case 0x62: msdos_int_21h_62h(); break;
		case 0x63: msdos_int_21h_63h(); break;
		// 0x64: set device driver lockahead flag
		case 0x65: msdos_int_21h_65h(); break;
		case 0x66: msdos_int_21h_66h(); break;
		case 0x67: msdos_int_21h_67h(); break;
		case 0x68: msdos_int_21h_68h(); break;
		// 0x69 get/set disk serial number
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
			case 0x41: msdos_int_21h_41h(1); break;
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
			// 0xaa: create/terminate SUBST
			default:
//				fatalerror("int 21h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
				REG16(AX) = 0x7100;
				cpustate->CF = 1;
			}
			break;
		// 0x72: Windows95 beta - LFN FindClose
		case 0x73:
			// windows95 fat32 functions
			switch(REG8(AL)) {
			// 0x00: drive locking ???
			// 0x01: drive locking ???
			// 0x02: get extended dpb
			case 0x03: msdos_int_21h_7303h(); break;
			// 0x04: set dpb to use for formatting
			default:
//				fatalerror("int 21h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
				REG16(AX) = 0x7200;
				cpustate->CF = 1;
			}
			break;
		default:
			fatalerror("int 21h (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
		}
		break;
	case 0x22:
		fatalerror("int 22h (terminate address)\n");
	case 0x23:
		msdos_process_terminate(current_psp, (retval & 0xff) | 0x100, 1);
		break;
	case 0x24:
		msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		break;
	case 0x25:
		msdos_int_25h();
		break;
	case 0x26:
		msdos_int_26h();
		break;
	case 0x27:
		msdos_int_27h();
		break;
	case 0x29:
		msdos_int_29h();
		break;
	case 0x2e:
		msdos_int_2eh();
		break;
	case 0x2f:
		// multiplex interrupt
		cpustate->CF = 0;
		switch(REG8(AH)) {
		case 0x4a: msdos_int_2fh_4ah(); break;
		case 0x4f: msdos_int_2fh_4fh(); break;
		default:
//			fatalerror("int 2fh (ah=%2xh al=%2xh)\n", REG8(AH), REG8(AL));
			break;
		}
		break;
//	default:
//		fatalerror("int %2xh (ah=%2xh al=%2xh)\n", num, REG8(AH), REG8(AL));
	}
	
	// update cursor position
	if(cursor_moved) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(hStdout, &csbi);
		mem[0x450 + mem[0x462] * 2] = csbi.dwCursorPosition.X;
		mem[0x451 + mem[0x462] * 2] = csbi.dwCursorPosition.Y;
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
#ifdef SUPPORT_AUX_PRN
	if(_open("stdaux.txt", _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 3) {
		msdos_file_handler_open(3, 0, 2, 0x80c0, 0);
	}
	if(_open("stdprn.txt", _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 4) {
		msdos_file_handler_open(4, 0, 1, 0xa8c0, 0);
	}
#endif
	_dup2(0, DUP_STDIN);
	_dup2(1, DUP_STDOUT);
	_dup2(2, DUP_STDERR);
	
	// init process
	memset(process, 0, sizeof(process));
	
	// init memory
	memset(mem, 0, sizeof(mem));
	for(int i = 0; i < 0x100; i++) {
		*(UINT16 *)(mem + 4 * i + 0) = i;
		*(UINT16 *)(mem + 4 * i + 2) = (IRET_TOP >> 4);
	}
	*(UINT16 *)(mem + 4 * 0x22 + 0) = 0xfff0;
	*(UINT16 *)(mem + 4 * 0x22 + 2) = 0xf000;
	memset(mem + IRET_TOP, 0xcf, IRET_SIZE);
	
	// bios data area
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hStdout, &csbi);
	
	*(UINT8  *)(mem + 0x411) = 0x20;
	*(UINT16 *)(mem + 0x410) = 0x20;
	*(UINT16 *)(mem + 0x413) = MEMORY_END / 1024;
	*(UINT8  *)(mem + 0x449) = 0x03;//0x73;
	*(UINT16 *)(mem + 0x44a) = 80;
	*(UINT16 *)(mem + 0x44c) = 4096;
	*(UINT16 *)(mem + 0x44e) = 0;
	*(UINT8  *)(mem + 0x450) = csbi.dwCursorPosition.X;
	*(UINT8  *)(mem + 0x451) = csbi.dwCursorPosition.Y;
	*(UINT8  *)(mem + 0x460) = 7;
	*(UINT8  *)(mem + 0x461) = 7;
	*(UINT8  *)(mem + 0x462) = 0;
	*(UINT16 *)(mem + 0x463) = 0x3d4;
	*(UINT8  *)(mem + 0x484) = 24;
	*(UINT8  *)(mem + 0x485) = 19;
	*(UINT8  *)(mem + 0x487) = 0;	// is this okay?
	
	// dos info
	dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
	dos_info->first_mcb = MEMORY_TOP >> 4;
	dos_info->first_dpb.w.l = 0;
	dos_info->first_dpb.w.h = DPB_TOP >> 4;
	dos_info->last_drive = 'Z' - 'A' + 1;
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
	
	// environment
	int seg = MEMORY_TOP >> 4;
	msdos_mcb_create(seg++, 'M', -1, ENV_SIZE >> 4);
	int env_seg = seg;
	int ofs = 0;
	for(char **p = envp; p != NULL && *p != NULL; p++) {
		// lower to upper
		char tmp[ENV_SIZE], name[ENV_SIZE];
		strcpy(tmp, *p);
		for(int i = 0;; i++) {
			if(tmp[i] == '=') {
				tmp[i] = '\0';
				sprintf(name, ";%s;", tmp);
				tmp[i] = '=';
				break;
			} else if(tmp[i] >= 'a' && tmp[i] <= 'z') {
				tmp[i] = tmp[i] - 'a' + 'A';
			}
		}
		if(!(standard_env && strstr(";COMSPEC;INCLUDE;LIB;PATH;PROMPT;TEMP;TMP;TZ;", name) == NULL)) {
			int len = strlen(tmp);
			if (ofs + len + 1 + (2 + (8 + 1 + 3)) + 2 > ENV_SIZE) {
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
	psp_t *psp = msdos_psp_create(seg, seg + (PSP_SIZE >> 4), -1, env_seg);
	seg += (PSP_SIZE >> 4);
	
	// first mcb
	msdos_mcb_create(seg, 'Z', 0, (MEMORY_END >> 4) - seg - 1);
	
	// boot
	mem[0xffff0] = 0xf4;	// halt
	mem[0xffff1] = 0xcd;	// int 21h
	mem[0xffff2] = 0x21;
	mem[0xffff3] = 0xcb;	// retf
	
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
	
	// dbcs table
	msdos_dbcs_table_init();
	
	// execute command
	if(msdos_process_exec(argv[0], param, 0)) {
		fatalerror("'%s' not found\n", argv[0]);
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
#ifdef SUPPORT_AUX_PRN
	remove_std_file("stdaux.txt");
	remove_std_file("stdprn.txt");
#endif
	msdos_dbcs_table_finish();
}

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

void hardware_init()
{
	cpustate = (i386_state *)CPU_INIT_CALL(CPU_MODEL);
	CPU_RESET_CALL(CPU_MODEL);
	cpu_type = (REG32(EDX) >> 8) & 0x0f;
	cpu_step = (REG32(EDX) >> 0) & 0x0f;
	i386_set_a20_line(cpustate, 0);
#ifdef SUPPORT_HARDWARE
	pic_init();
	//pit_init();
	pit_active = 0;
#endif
}

void hardware_finish()
{
	free(cpustate);
}

void hardware_run()
{
	int ops = 0;
	
	while(!cpustate->halted) {
#ifdef SUPPORT_DISASSEMBLER
		if(dasm) {
			char buffer[256];
			UINT64 eip = cpustate->eip;
			UINT8 *oprom = mem + cpustate->sreg[CS].base + cpustate->eip;
			
			if(cpustate->operand_size) {
				CPU_DISASSEMBLE_CALL(x86_32);
			} else {
				CPU_DISASSEMBLE_CALL(x86_16);
			}
			fprintf(stderr, "%04x:%04x\t%s\n", cpustate->sreg[CS].selector, cpustate->eip, buffer);
		}
#endif
		cpustate->cycles = 1;
		CPU_EXECUTE_CALL(i386);
#ifdef SUPPORT_HARDWARE
		if(++ops == 1024) {
			hardware_update();
			ops = 0;
		}
#endif
	}
}

#ifdef SUPPORT_HARDWARE
void hardware_update()
{
	if(pit_active) {
		pit_run();
	}
}
#endif

// pic

void pic_init()
{
	for(int c = 0; c < 2; c++) {
		pic[c].imr = 0xff;
		pic[c].irr = pic[c].isr = pic[c].prio = 0;
		pic[c].icw1 = pic[c].icw2 = pic[c].icw3 = pic[c].icw4 = 0;
		pic[c].ocw3 = 0;
		pic[c].icw2_r = pic[c].icw3_r = pic[c].icw4_r = 0;
	}
	
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
		i386_set_irq_line(cpustate, INPUT_LINE_IRQ, HOLD_LINE);
		return;
	}
	i386_set_irq_line(cpustate, INPUT_LINE_IRQ, CLEAR_LINE);
};

// pit

#define PIT_FREQ 1193182
#define PIT_COUNT_VALUE(n) ((pit[n].count_reg == 0) ? 0x10000 : (pit[n].mode == 3 && pit[n].count_reg == 1) ? 0x10001 : pit[n].count_reg)

void pit_init()
{
	for(int ch = 0; ch < 3; ch++) {
		pit[ch].prev_out = 1;
		//pit[ch].gate = 1;
		pit[ch].count = 0x10000;
		pit[ch].count_reg = 0;
		pit[ch].ctrl_reg = 0x34;
		pit[ch].mode = 3;
		pit[ch].count_latched = 0;
		pit[ch].low_read = pit[ch].high_read = 0;
		pit[ch].low_write = pit[ch].high_write = 0;
		pit[ch].delay = 0;
		pit[ch].start = 0;
		pit[ch].null_count = 1;
		pit[ch].status_latched = 0;
	}
	
	// from bochs bios
	pit_write(3, 0x34);
	pit_write(0, 0x00);
	pit_write(0, 0x00);
}

void pit_write(int ch, UINT8 val)
{
	if(!pit_active) {
		pit_active = 1;
		pit_init();
	}
	
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
		pit[ch].null_count = 1;
		// set signal
		if(pit[ch].mode == 0) {
			pit_set_signal(ch, 0);
		} else {
			pit_set_signal(ch, 1);
		}
		// start count
		if(pit[ch].mode == 0 || pit[ch].mode == 4) {
			// restart with new count
			pit_stop_count(ch);
			pit[ch].delay = 1;
			pit_start_count(ch);
		} else if(pit[ch].mode == 2 || pit[ch].mode == 3) {
			// start with new pit after the current count is finished
			if(!pit[ch].start) {
				pit[ch].delay = 1;
				pit_start_count(ch);
			}
		}
		break;
	case 3: // ctrl reg
		if((val & 0xc0) == 0xc0) {
			// i8254 read-back command
			for(ch = 0; ch < 3; ch++) {
				UINT8 bit = 2 << ch;
				if(!(val & 0x10) && !pit[ch].status_latched) {
					pit[ch].status = pit[ch].ctrl_reg & 0x3f;
					if(pit[ch].prev_out) {
						pit[ch].status |= 0x80;
					}
					if(pit[ch].null_count) {
						pit[ch].status |= 0x40;
					}
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
			// set signal
			if(pit[ch].mode == 0) {
				pit_set_signal(ch, 0);
			} else {
				pit_set_signal(ch, 1);
			}
			// stop count
			pit_stop_count(ch);
			pit[ch].count_reg = 0;
			pit[ch].null_count = 1;
		} else if(!pit[ch].count_latched) {
			pit_latch_count(ch);
		}
		break;
	}
}

UINT8 pit_read(int ch)
{
	if(!pit_active) {
		pit_active = 1;
		pit_init();
	}
	
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

void pit_run()
{
	static UINT32 prev_time = 0;
	UINT32 cur_time = timeGetTime();
	
	if(prev_time == cur_time) {
		return;
	}
	prev_time = cur_time;
	
	for(int ch = 0; ch < 3; ch++) {
		if(pit[ch].start && cur_time >= pit[ch].expired_time) {
			pit_input_clock(ch, pit[ch].input_clk);
			
			// next expired time
			if(pit[ch].start) {
				pit[ch].input_clk = pit[ch].delay ? 1 : pit_get_next_count(ch);
				pit[ch].prev_time = pit[ch].expired_time;
				pit[ch].expired_time += pit_get_expired_time(pit[ch].input_clk);
				if(pit[ch].expired_time <= cur_time) {
					pit[ch].prev_time = cur_time;
					pit[ch].expired_time = cur_time + pit_get_expired_time(pit[ch].input_clk);
				}
			}
		}
	}
}

void pit_input_clock(int ch, int clock)
{
	if(!(pit[ch].start && clock)) {
		return;
	}
	if(pit[ch].delay) {
		clock -= 1;
		pit[ch].delay = 0;
		pit[ch].count = PIT_COUNT_VALUE(ch);
		pit[ch].null_count = 0;
	}
	
	// update pit
	pit[ch].count -= clock;
	INT32 tmp = PIT_COUNT_VALUE(ch);
loop:
	if(pit[ch].mode == 3) {
		INT32 half = tmp >> 1;
		if(pit[ch].count > half) {
			pit_set_signal(ch, 1);
		} else {
			pit_set_signal(ch, 0);
		}
	} else {
		if(pit[ch].count <= 1) {
			if(pit[ch].mode == 2 || pit[ch].mode == 4 || pit[ch].mode == 5) {
				pit_set_signal(ch, 0);
			}
		}
		if(pit[ch].count <= 0) {
			pit_set_signal(ch, 1);
		}
	}
	if(pit[ch].count <= 0) {
		if(pit[ch].mode == 0 || pit[ch].mode == 2 || pit[ch].mode == 3) {
			pit[ch].count += tmp;
			pit[ch].null_count = 0;
			goto loop;
		} else {
			pit[ch].start = 0;
			pit[ch].count = 0x10000;
		}
	}
}

void pit_start_count(int ch)
{
	if(pit[ch].low_write || pit[ch].high_write) {
		return;
	}
	//if(!pit[ch].gate) {
	//	return;
	//}
	pit[ch].start = 1;
	
	// next expired time
	pit[ch].input_clk = pit[ch].delay ? 1 : pit_get_next_count(ch);
	UINT32 cur_time = timeGetTime();
	pit[ch].prev_time = cur_time;
	pit[ch].expired_time = cur_time + pit_get_expired_time(pit[ch].input_clk);
}

void pit_stop_count(int ch)
{
	pit[ch].start = 0;
}

void pit_latch_count(int ch)
{
	if(pit[ch].start) {
		// update pit
		UINT32 cur_time = timeGetTime();
		if(cur_time > pit[ch].prev_time) {
			UINT32 input = PIT_FREQ * (cur_time - pit[ch].prev_time) / 1000;
			pit_input_clock(ch, input);
			
			if(pit[ch].input_clk <= input) {
				// next expired time
				if(pit[ch].start) {
					pit[ch].input_clk = pit[ch].delay ? 1 : pit_get_next_count(ch);
					pit[ch].prev_time = pit[ch].expired_time;
					pit[ch].expired_time += pit_get_expired_time(pit[ch].input_clk);
					if(pit[ch].expired_time <= cur_time) {
						pit[ch].prev_time = cur_time;
						pit[ch].expired_time = cur_time + pit_get_expired_time(pit[ch].input_clk);
					}
				}
			} else {
				pit[ch].input_clk -= input;
				pit[ch].prev_time = cur_time;
			}
		}
	}
	// latch pit
	pit[ch].latch = (UINT16)pit[ch].count;
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
		pit[ch].low_read = pit[ch].low_read = 1;
	}
}

void pit_set_signal(int ch, int signal)
{
	int prev = pit[ch].prev_out;
	pit[ch].prev_out = signal;
	
	if(prev && !signal) {
		// H->L
		if(ch == 0) {
			pic_req(0, 0, 0);
		}
	} else if(!prev && signal) {
		// L->H
		if(ch == 0) {
			pic_req(0, 0, 1);
		}
	}
}

int pit_get_next_count(int ch)
{
	if(pit[ch].mode == 2 || pit[ch].mode == 4 || pit[ch].mode == 5) {
		if(pit[ch].count > 1) {
			return(pit[ch].count - 1);
		} else {
			return(1);
		}
	}
	if(pit[ch].mode == 3) {
		INT32 half = PIT_COUNT_VALUE(ch) >> 1;
		if(pit[ch].count > half) {
			return(pit[ch].count - half);
		} else {
			return(pit[ch].count);
		}
	}
	return(pit[ch].count);
}

int pit_get_expired_time(int clock)
{
	UINT32 val = 1000 * clock / PIT_FREQ;
	
	if(val > 0) {
		return(val);
	} else {
		return(1);
	}
}

// i/o bus

UINT8 read_io_byte(offs_t addr)
{
	switch(addr) {
#ifdef SUPPORT_HARDWARE
	case 0x20:
	case 0x21:
		return(pic_read(0, addr));
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
		return(pit_read(addr & 0x03));
#endif
	case 0x71:
		return(cmos[cmos_addr & 0x7f]);
	case 0x92:
		return((cpustate->a20_mask >> 19) & 2);
#ifdef SUPPORT_HARDWARE
	case 0xa0:
	case 0xa1:
		return(pic_read(1, addr));
#endif
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
	switch(addr) {
#ifdef SUPPORT_HARDWARE
	case 0x20:
	case 0x21:
		pic_write(0, addr, val);
		break;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
		pit_write(addr & 0x03, val);
		break;
#endif
	case 0x64:
		if(val == 0xfe) {
			if(cmos[0x0f] == 5) {
				// reset pic
				pic_init();
				pic[0].irr = pic[1].irr = 0x00;
				pic[0].imr = pic[1].imr = 0xff;
			}
			CPU_RESET_CALL(CPU_MODEL);
			i386_jmp_far(0x40, 0x67);
		}
		break;
	case 0x70:
		cmos_addr = val;
		break;
	case 0x71:
		cmos[cmos_addr & 7] = val;
		break;
	case 0x92:
		i386_set_a20_line(cpustate, val & 2);
		break;
#ifdef SUPPORT_HARDWARE
	case 0xa0:
	case 0xa1:
		pic_write(1, addr, val);
		break;
#endif
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
