/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _MSDOS_H_
#define _MSDOS_H_

#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <sys/locking.h>
#include <mbctype.h>
#include <direct.h>
#include <errno.h>

// variable scope of 'for' loop for microsoft visual c++ 6.0 and embedded visual c++ 4.0
#if (defined(_MSC_VER) && (_MSC_VER == 1200)) || defined(_WIN32_WCE)
#define for if(0);else for
#endif
// disable warnings C4189, C4995 and C4996 for microsoft visual c++ 2005
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning( disable : 4819 )
#pragma warning( disable : 4995 )
#pragma warning( disable : 4996 )
#endif
// compat for mingw32 headers
#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif

// type definition
#ifndef UINT8
typedef unsigned char UINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int UINT32;
#endif
#ifndef INT8
typedef signed char INT8;
#endif
#ifndef INT16
typedef signed short INT16;
#endif
#ifndef INT32
typedef signed int INT32;
#endif

#pragma pack(1)
typedef union {
	UINT32 dw;
	struct {
		UINT16 l, h;
	} w;
} PAIR32;
#pragma pack()

/* ----------------------------------------------------------------------------
	FIFO buffer
---------------------------------------------------------------------------- */

#define MAX_FIFO	256

class FIFO
{
private:
	int buf[MAX_FIFO];
	int cnt, rpt, wpt;
public:
	FIFO() {
		cnt = rpt = wpt = 0;
	}
	void write(int val) {
		if(cnt < MAX_FIFO) {
			buf[wpt++] = val;
			if(wpt >= MAX_FIFO) {
				wpt = 0;
			}
			cnt++;
		}
	}
	int read() {
		int val = 0;
		if(cnt) {
			val = buf[rpt++];
			if(rpt >= MAX_FIFO) {
				rpt = 0;
			}
			cnt--;
		}
		return val;
	}
	int read_not_remove() {
		int val = 0;
		if(cnt) {
			val = buf[rpt];
		}
		return val;
	}
	int count() {
		return cnt;
	}
};

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

#define VECTOR_TOP	0
#define VECTOR_SIZE	0x400
#define BIOS_TOP	(VECTOR_TOP + VECTOR_SIZE)
#define BIOS_SIZE	0x100
#define WORK_TOP	(BIOS_TOP + BIOS_SIZE)
#define WORK_SIZE	0x300
#define DPB_TOP		(WORK_TOP + WORK_SIZE)
#define DPB_SIZE	0x400
#define IRET_TOP	(DPB_TOP + DPB_SIZE)
#define IRET_SIZE	0x100
#define DOS_INFO_TOP	(IRET_TOP + IRET_SIZE)
#define DOS_INFO_BASE	(DOS_INFO_TOP + 12)
#define DOS_INFO_SIZE	0x100
#define DBCS_TOP	(DOS_INFO_TOP + DOS_INFO_SIZE)
#define DBCS_TABLE	(DBCS_TOP + 2)
#define DBCS_SIZE	16
#define MEMORY_TOP	(DBCS_TOP + DBCS_SIZE)
//#define MEMORY_END	0xffff0
#define MEMORY_END	0xf8000
#define TVRAM_TOP	0xf8000

//#define ENV_SIZE	0x800
#define ENV_SIZE	0x2000
#define PSP_SIZE	0x100

#define MAX_FILES	128
#define MAX_PROCESS	16

#define DUP_STDIN	29
#define DUP_STDOUT	30
#define DUP_STDERR	31

//#define SUPPORT_AUX_PRN

#pragma pack(1)
typedef struct {
	UINT8 mz;
	UINT16 psp;
	UINT16 paragraphs;
} mcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 env_seg;
	PAIR32 cmd_line;
	PAIR32 fcb1;
	PAIR32 fcb2;
	UINT16 sp;
	UINT16 ss;
	UINT16 ip;
	UINT16 cs;
} param_block_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 len;
	char cmd[127];
} cmd_line_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 exit[2];
	UINT16 first_mcb;
	UINT8 reserved_1;
	UINT8 far_call;
	PAIR32 cpm_entry;
	PAIR32 int_22h;
	PAIR32 int_23h;
	PAIR32 int_24h;
	UINT16 parent_psp;
	UINT8 file_table[20];
	UINT16 env_seg;
	PAIR32 stack;
	UINT8 reserved_2[30];
	UINT8 service[3];
	UINT8 reserved_3[2];
	UINT8 ex_fcb[7];
	UINT8 fcb1[16];
	UINT8 fcb2[20];
	UINT8 buffer[128];
} psp_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 mz;
	UINT16 extra_bytes;
	UINT16 pages;
	UINT16 relocations;
	UINT16 header_size;
	UINT16 min_alloc;
	UINT16 max_alloc;
	UINT16 init_ss;
	UINT16 init_sp;
	UINT16 check_sum;
	UINT16 init_ip;
	UINT16 init_cs;
	UINT16 relocation_table;
	UINT16 overlay;
} exe_header_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 flag;
	UINT8 reserved[5];
	UINT8 attribute;
} ext_fcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 drive;
	UINT8 file_name[8 + 3];
	UINT16 current_block;
	UINT16 record_size;
	UINT32 file_size;
	UINT16 date;
	UINT16 time;
	UINT8 reserved[8];
	UINT8 cur_record;
	UINT32 rand_record;
} fcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 drive;
	UINT8 file_name[8 + 3];
	UINT8 attribute;
	UINT8 nt_res;
	UINT8 create_time_ms;
	UINT16 creation_time;
	UINT16 creation_date;
	UINT16 last_access_date;
	UINT16 cluster_hi;
	UINT16 last_write_time;
	UINT16 last_write_date;
	UINT16 cluster_lo;
	UINT32 file_size;
} find_fcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 reserved[21];
	UINT8 attrib;
	UINT16 time;
	UINT16 date;
	UINT32 size;
	char name[13];
} find_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT32 attrib;
	PAIR32 ctime_lo;
	PAIR32 ctime_hi;
	PAIR32 atime_lo;
	PAIR32 atime_hi;
	PAIR32 mtime_lo;
	PAIR32 mtime_hi;
	UINT32 size_hi;
	UINT32 size_lo;
	UINT8 reserved[8];
	char full_name[260];
	char short_name[14];
} find_lfn_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 size_of_structure;
	UINT16 structure_version;
	UINT32 sectors_per_cluster;
	UINT32 bytes_per_sector;
	UINT32 available_clusters_on_drive;
	UINT32 total_clusters_on_drive;
	UINT32 available_sectors_on_drive;
	UINT32 total_sectors_on_drive;
	UINT32 available_allocation_units;
	UINT32 total_allocation_units;
	UINT8 reserved[8];
} ext_free_space_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 drive_num;
	UINT8 unit_num;
	UINT16 bytes_per_sector;
	UINT8 highest_sector_num;
	UINT8 shift_count;
	UINT16 reserved_sectors;
	UINT8 fat_num;
	UINT16 root_entries;
	UINT16 first_data_sector;
	UINT16 highest_cluster_num;
	UINT16 sectors_per_fat;
	UINT16 first_dir_sector;
	UINT32 device_driver_header;
	UINT8 media_type;
	UINT8 drive_accessed;
	UINT16 next_dpb_ofs;
	UINT16 next_dpb_seg;
	UINT16 first_free_cluster;
	UINT16 free_clusters;
} dpb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 reserved_1[10];
	UINT16 first_mcb;	// -2
	PAIR32 first_dpb;	// +0
	UINT8 reserved_2[29];
	UINT8 last_drive;	// +33
	UINT8 reserved_3[33];
	UINT8 boot_drive;	// +67
} dos_info_t;
#pragma pack()

typedef struct {
	char path[MAX_PATH];
	int valid;
	int id;
	int atty;
	int mode;
	UINT16 info;
	UINT16 psp;
} file_handler_t;

static const struct {
	int mode;
	int in;
	int out;
} file_mode[] = {
	{ _O_RDONLY | _O_BINARY, 1, 0 },
	{ _O_WRONLY | _O_BINARY, 0, 1 },
	{ _O_RDWR   | _O_BINARY, 1, 1 },
};

typedef struct {
	UINT16 psp;
	char module_dir[MAX_PATH];
	PAIR32 dta;
	UINT8 switchar;
	UINT8 verify;
	int max_files;
	HANDLE find_handle;
	UINT8 allowable_mask;
	UINT8 required_mask;
	char volume_label[MAX_PATH];
	bool parent_int_10h_feh_called;
	bool parent_int_10h_ffh_called;
} process_t;

UINT16 current_psp;

int retval = 0;

file_handler_t file_handler[MAX_FILES];
UINT8 file_buffer[0x100000];

process_t process[MAX_PROCESS];

void msdos_syscall(unsigned num);
int msdos_init(int argc, char *argv[], char *envp[], int standard_env);
void msdos_finish();

// console

#define SCR_BUF_SIZE	1200

#define SET_RECT(rect, l, t, r, b) { \
	rect.Left = l; \
	rect.Top = t; \
	rect.Right = r; \
	rect.Bottom = b; \
}

HANDLE hStdin;
HANDLE hStdout;
CHAR_INFO scr_buf[SCR_BUF_SIZE][80];
char scr_char[80 * 25];
WORD scr_attr[80 * 25];
COORD scr_buf_size;
COORD scr_buf_pos;
int scr_width, scr_height;
bool cursor_moved;

FIFO *key_buf_char;
FIFO *key_buf_scan;

int active_code_page;
int system_code_page;

UINT32 tvram_base_address = TVRAM_TOP;
bool int_10h_feh_called = false;
bool int_10h_ffh_called = false;

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

//#define SUPPORT_HARDWARE

void hardware_init();
void hardware_finish();
void hardware_run();
#ifdef SUPPORT_HARDWARE
void hardware_update();
#endif

// memory

#define MAX_MEM 0x1000000
UINT8 mem[MAX_MEM];

// cmos

UINT8 cmos[128];
UINT8 cmos_addr;

// pic

typedef struct {
	UINT8 imr, isr, irr, prio;
	UINT8 icw1, icw2, icw3, icw4;
	UINT8 ocw3;
	UINT8 icw2_r, icw3_r, icw4_r;
} pic_t;

pic_t pic[2];
int pic_req_chip, pic_req_level;
UINT8 pic_req_bit;

void pic_init();
void pic_write(int c, UINT32 addr, UINT8 data);
UINT8 pic_read(int c, UINT32 addr);
void pic_req(int c, int level, int signal);
int pic_ack();
void pic_update();

// pit

typedef struct {
	int prev_out;
	//int gate;
	INT32 count;
	UINT16 latch;
	UINT16 count_reg;
	UINT8 ctrl_reg;
	int count_latched;
	int low_read, high_read;
	int low_write, high_write;
	int mode;
	int delay;
	int start;
	int null_count;
	int status_latched;
	UINT8 status;
	// constant clock
	UINT32 input_clk;
	UINT32 expired_time;
	UINT32 prev_time;
} pit_t;

pit_t pit[3];
int pit_active;

void pit_init();
void pit_write(int ch, UINT8 val);
UINT8 pit_read(int ch);
void pit_run();
void pit_input_clock(int ch, int clock);
void pit_start_count(int ch);
void pit_stop_count(int ch);
void pit_latch_count(int ch);
void pit_set_signal(int ch, int signal);
int pit_get_next_count(int ch);
int pit_get_expired_time(int clock);

#endif
