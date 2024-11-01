/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _MSDOS_H_
#define _MSDOS_H_

#include "lang_fr.h"
#include "lang_de.h"
#include "lang_sp.h"
#include "lang_pt.h"
#include "lang_br.h"
#include "lang_jp.h"
#include "lang_ko.h"

#pragma pack(1)
typedef union {
	UINT32 dw;
	struct {
#ifdef __BIG_ENDIAN__
		UINT16 h, l;
#else
		UINT16 l, h;
#endif
	} w;
} PAIR32;
#pragma pack()

#pragma pack(1)
typedef union {
	UINT16 w;
	struct {
#ifdef __BIG_ENDIAN__
		UINT8 h, l;
#else
		UINT8 l, h;
#endif
	} b;
} PAIR16;
#pragma pack()

/* ----------------------------------------------------------------------------
	FIFO buffer
---------------------------------------------------------------------------- */

class FIFO
{
private:
	int size;
	int *buf;
	int cnt, rpt, wpt;
public:
	FIFO(int s) {
		size = s;
		buf = (int *)malloc(size * sizeof(int));
		cnt = rpt = wpt = 0;
	}
	void release()
	{
		if(buf != NULL) {
			free(buf);
			buf = NULL;
		}
	}
	void clear()
	{
		cnt = rpt = wpt = 0;
	}
	void write(int val) {
		if(cnt < size) {
			buf[wpt++] = val;
			if(wpt >= size) {
				wpt = 0;
			}
			cnt++;
		}
	}
	int read() {
		int val = 0;
		if(cnt) {
			val = buf[rpt++];
			if(rpt >= size) {
				rpt = 0;
			}
			cnt--;
		}
		return(val);
	}
	int read_not_remove(int pt) {
		if(pt >= 0 && pt < cnt) {
			pt += rpt;
			if(pt >= size) {
				pt -= size;
			}
			return buf[pt];
		}
		return(0);
	}
	int count() {
		return(cnt);
	}
	int remain() {
		return(size - cnt);
	}
	bool full()
	{
		return(cnt == size);
	}
	bool empty()
	{
		return(cnt == 0);
	}
};

/* ----------------------------------------------------------------------------
	MAME i86/i386
---------------------------------------------------------------------------- */

#if defined(HAS_IA32)
//	#define CPU_MODEL ia32
	#define SUPPORT_FPU
#elif defined(HAS_I86)
	#define CPU_MODEL i8086
#elif defined(HAS_I186)
	#define CPU_MODEL i80186
#elif defined(HAS_V30)
	#define CPU_MODEL v30
#elif defined(HAS_I286)
	#define CPU_MODEL i80286
#elif defined(HAS_I386)
	#define CPU_MODEL i386
#else
//	#if defined(HAS_I386SX)
//		#define CPU_MODEL i386SX
//	#else
		#if defined(HAS_I486)
			#define CPU_MODEL i486
		#else
			#if defined(HAS_PENTIUM)
				#define CPU_MODEL pentium
			#elif defined(HAS_MEDIAGX)
				#define CPU_MODEL mediagx
			#elif defined(HAS_PENTIUM_PRO)
				#define CPU_MODEL pentium_pro
			#elif defined(HAS_PENTIUM_MMX)
				#define CPU_MODEL pentium_mmx
			#elif defined(HAS_PENTIUM2)
				#define CPU_MODEL pentium2
			#elif defined(HAS_PENTIUM3)
				#define CPU_MODEL pentium3
			#elif defined(HAS_PENTIUM4)
				#define CPU_MODEL pentium4
			#endif
//			#define SUPPORT_RDTSC
		#endif
		#define SUPPORT_FPU
//	#endif
#endif

#if !(defined(HAS_I86) || defined(HAS_I186) || defined(HAS_V30) || defined(HAS_I286) || defined(HAS_I386))
	#define HAS_I386
#endif

/* ----------------------------------------------------------------------------
	debugger
---------------------------------------------------------------------------- */

#ifdef USE_DEBUGGER
bool now_debugging = false;
bool now_going = false;
bool now_suspended = false;
bool force_suspend = false;

break_point_t break_point = {0};
break_point_t rd_break_point = {0};
break_point_t wr_break_point = {0};
break_point_t in_break_point = {0};
break_point_t out_break_point = {0};

int_break_point_t int_break_point = {0};

FILE *fp_debugger = NULL;
FILE *fi_debugger = NULL;

// these read/write interfaces do not check break points,
// debugger should use them not to hit any break point mistakely
UINT8 debugger_read_byte(UINT32 byteaddress);
UINT16 debugger_read_word(UINT32 byteaddress);
UINT32 debugger_read_dword(UINT32 byteaddress);
void debugger_write_byte(UINT32 byteaddress, UINT8 data);
void debugger_write_word(UINT32 byteaddress, UINT16 data);
void debugger_write_dword(UINT32 byteaddress, UINT32 data);
UINT8 debugger_read_io_byte(UINT32 addr);
UINT16 debugger_read_io_word(UINT32 addr);
UINT32 debugger_read_io_dword(UINT32 addr);
void debugger_write_io_byte(UINT32 addr, UINT8 val);
void debugger_write_io_word(UINT32 addr, UINT16 val);
void debugger_write_io_dword(UINT32 addr, UINT32 val);
#endif

/* ----------------------------------------------------------------------------
	service thread
---------------------------------------------------------------------------- */

UINT16 CPU_AX_in_service;
UINT16 CPU_CX_in_service;
UINT16 CPU_DX_in_service;
UINT32 CPU_DS_BASE_in_service;

void prepare_service_loop();
void cleanup_service_loop();

BOOL use_service_thread = FALSE;
CRITICAL_SECTION input_crit_sect;
CRITICAL_SECTION key_buf_crit_sect;
CRITICAL_SECTION putch_crit_sect;
bool in_service = false;
bool service_exit = false;
DWORD main_thread_id;

void start_service_loop(LPTHREAD_START_ROUTINE lpStartAddress);
void finish_service_loop();

bool in_service_29h = false;

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

//#define SUPPORT_GRAPHIC_SCREEN

void hardware_init();
void hardware_finish();
void hardware_release();
void hardware_run();
void hardware_run_cpu();
void hardware_update();

// drive

typedef struct drive_param_s {
	int initialized;
	int valid;
	DISK_GEOMETRY geometry;
	
	int is_fdd()
	{
		if(initialized && valid) {
			switch(geometry.MediaType) {
			case F5_1Pt2_512:
			case F3_1Pt44_512:
			case F3_2Pt88_512:
			case F3_20Pt8_512:
			case F3_720_512:
			case F5_360_512:
			case F5_320_512:
			case F5_320_1024:
			case F5_180_512:
			case F5_160_512:
			case F3_120M_512:
			case F3_640_512:
			case F5_640_512:
			case F5_720_512:
			case F3_1Pt2_512:
			case F3_1Pt23_1024:
			case F5_1Pt23_1024:
			case F3_128Mb_512:
			case F3_230Mb_512:
			case F8_256_128:
#ifndef _MSC_VC6
			case F3_200Mb_512:
			case F3_240M_512:
			case F3_32M_512:
#endif
				return(1);
			}
		}
		return(0);
	}
	int head_num()
	{
		if(initialized && valid) {
			switch(geometry.MediaType) {
			case F5_1Pt2_512:
			case F3_1Pt44_512:
			case F3_2Pt88_512:
			case F3_20Pt8_512:
			case F3_720_512:
			case F5_360_512:
			case F5_320_512:
			case F5_320_1024:
//			case F5_180_512:
//			case F5_160_512:
			case F3_120M_512:
			case F3_640_512:
			case F5_640_512:
			case F5_720_512:
			case F3_1Pt2_512:
			case F3_1Pt23_1024:
			case F5_1Pt23_1024:
			case F3_128Mb_512:
			case F3_230Mb_512:
//			case F8_256_128:
#ifndef _MSC_VC6
			case F3_200Mb_512:
			case F3_240M_512:
			case F3_32M_512:
#endif
				return(2);
			default:
				return(1);
			}
		}
		return(0);
	}
} drive_param_t;

drive_param_t drive_params[26] = {0};

// memory

#if defined(HAS_I386)
	#define ADDR_MASK 0xffffffff
	#define MAX_MEM 0x2000000	/* 32MB */
#elif defined(HAS_I286)
	#define ADDR_MASK 0xffffff
	#define MAX_MEM 0x1000000	/* 16MB */
#else
	#define ADDR_MASK 0xfffff
	#define MAX_MEM 0x100000	/* 1MB */
#endif

#if defined(_MSC_VER) && (_MSC_VER > 1200)
__declspec(align(4096))
#endif
UINT8 mem[MAX_MEM + 16]
#ifdef __GNUC__
__attribute__ ((aligned(4096)))
#endif
;

// EMS

#define MAX_EMS_HANDLES 16
#define MAX_EMS_PAGES 2048	/* 32MB */

typedef struct {
	char name[8];
	UINT8* buffer;
	int pages;
	bool allocated;
} ems_handle_t;

typedef struct {
	UINT16 handle;
	UINT16 page;
	bool mapped;
} ems_page_t;

ems_handle_t ems_handles[MAX_EMS_HANDLES + 1] = {0};
ems_page_t ems_pages[4];
int free_ems_pages;

void ems_init();
void ems_finish();
void ems_release();
void ems_allocate_pages(int handle, int pages);
void ems_reallocate_pages(int handle, int pages);
void ems_release_pages(int handle);
void ems_map_page(int physical, int handle, int logical);
void ems_unmap_page(int physical);

// DMA

typedef struct {
	struct {
		PAIR16 areg, creg, bareg, bcreg;
		UINT8 mode;
		UINT8 pagereg;
		UINT32 port;
	} ch[4];
	
	bool low_high;
	UINT8 cmd;
	UINT8 req;
	UINT8 mask;
	UINT8 tc;
	UINT16 tmp;
} dma_t;

dma_t dma[2];

void dma_init();
void dma_reset(int c);
void dma_write(int c, UINT32 addr, UINT8 data);
UINT8 dma_read(int c, UINT32 addr);
void dma_page_write(int c, int ch, UINT8 data);
UINT8 dma_page_read(int c, int ch);
void dma_req(int c, int ch, bool req);
void dma_run(int c, int ch);

// PIC

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

// PIO

typedef struct {
	UINT8 data, stat, ctrl;
	// code conversion
	bool conv_mode;
	bool jis_mode;
	UINT8 sjis_hi;
	UINT8 esc_buf[8];
	UINT32 esc_len;
	// printer to file
	char path[MAX_PATH];
	FILE *fp;
	SYSTEMTIME time;
} pio_t;

pio_t pio[3];

void pio_init();
void pio_finish();
void pio_release();
void pio_write(int c, UINT32 addr, UINT8 data);
UINT8 pio_read(int c, UINT32 addr);
void printer_out(int c, UINT8 data);
void pcbios_printer_out(int c, UINT8 data);

// PIT

#define PIT_ALWAYS_RUNNING
#define PIT_FREQ (UINT64)1193182
#define PIT_COUNT_VALUE(n) ((pit[n].count_reg == 0) ? 0x10000 : (pit[n].mode == 3 && pit[n].count_reg == 1) ? 0x10001 : pit[n].count_reg)

typedef struct {
	INT32 count;
	UINT16 latch;
	UINT16 prev_latch, next_latch;
	UINT16 count_reg;
	UINT8 ctrl_reg;
	int count_latched;
	int low_read, high_read;
	int low_write, high_write;
	int mode;
	int status_latched;
	UINT8 status;
	// constant clock
	UINT32 expired_time;
	UINT32 prev_time;
	UINT64 accum;
} pit_t;

pit_t pit[3];
#ifndef PIT_ALWAYS_RUNNING
int pit_active;
#endif

void pit_init();
void pit_write(int ch, UINT8 val);
UINT8 pit_read(int ch);
int pit_run(int ch, UINT32 cur_time);
void pit_latch_count(int ch);
int pit_get_expired_time(int ch);

UINT8 system_port = 0x0c;
int refresh_count = 0;

// SIO

#define SIO_BUFFER_SIZE 1024

typedef struct {
	int channel;
	
	FIFO *send_buffer;
	FIFO *recv_buffer;
	
	PAIR16 divisor;
	UINT16 prev_divisor;
	UINT8 line_ctrl, prev_line_ctrl;
	UINT8 selector;
	
	UINT8 modem_ctrl;
	bool set_brk, set_rts, set_dtr;
	bool prev_set_brk;
	bool prev_set_rts, prev_set_dtr;
	DWORD set_brk_time;
	
	UINT8 line_stat_buf, line_stat_err;
	UINT8 modem_stat, prev_modem_stat;
	
	UINT8 irq_enable;
	UINT8 irq_identify;
	
	UINT8 scratch;
} sio_t;

typedef struct {
	HANDLE hThread;
	CRITICAL_SECTION csSendData;
	CRITICAL_SECTION csRecvData;
	CRITICAL_SECTION csLineCtrl;
	CRITICAL_SECTION csLineStat;
	CRITICAL_SECTION csSetBreak;
	CRITICAL_SECTION csModemCtrl;
	CRITICAL_SECTION csModemStat;
} sio_mt_t;

sio_t sio[4] = {0};
sio_mt_t sio_mt[4];

void sio_init();
void sio_finish();
void sio_release();
void sio_write(int c, UINT32 addr, UINT8 data);
UINT8 sio_read(int c, UINT32 addr);
void sio_update(int c);
void sio_update_irq(int c);
DWORD WINAPI sio_thread(void *lpx);
bool sio_wait_sending_complete(int c);

// CMOS

UINT8 cmos[128];
UINT8 cmos_addr;

void cmos_init();
void cmos_write(int addr, UINT8 val);
UINT8 cmos_read(int addr);

// keyboard

UINT8 kbd_data;
UINT8 kbd_status;
UINT8 kbd_command;

void kbd_init();
void kbd_reset();
UINT8 kbd_read_data();
void kbd_write_data(UINT8 val);
UINT8 kbd_read_status();
void kbd_write_command(UINT8 val);

// beep

WAVEFORMATEX wfe;
HWAVEOUT hWaveOut;
WAVEHDR whdr[2];
double beep_freq;
bool beep_playing;

void beep_init();
void beep_finish();
void beep_release();
void beep_update();

// CRTC

UINT8 crtc_addr = 0;
UINT8 crtc_regs[16] = {0};
UINT8 crtc_changed[16] = {0};

#ifdef SUPPORT_GRAPHIC_SCREEN
// VRAM
static UINT32 vga_read(UINT32 addr, int size);
static void vga_write(UINT32 addr, UINT32 data, int size);
#endif

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

#if defined(HAS_I386)
#define SUPPORT_VCPI
#endif
#if defined(HAS_I286) || defined(HAS_I386)
#define SUPPORT_XMS
//#define SUPPORT_HMA
#endif
//#define SUPPORT_MSCDEX

#define VECTOR_TOP	0
#define VECTOR_SIZE	0x400
#define BIOS_TOP	(VECTOR_TOP + VECTOR_SIZE)
#define BIOS_SIZE	0x100
#define WORK_TOP	(BIOS_TOP + BIOS_SIZE)
#define WORK_SIZE	0x200
// IO.SYS 0070:0000
#define DEVICE_TOP	(WORK_TOP + WORK_SIZE)
#define DEVICE_SIZE	0x120	/* 22 + 18 * 14 + 7 */
#define DOS_INFO_TOP	(DEVICE_TOP + DEVICE_SIZE)
#define DOS_INFO_SIZE	0x100
//#define EXT_BIOS_TOP	(DOS_INFO_TOP + DOS_INFO_SIZE)
//#define EXT_BIOS_SIZE	0x400
#ifdef EXT_BIOS_TOP
#define DPB_TOP		(EXT_BIOS_TOP + EXT_BIOS_SIZE)
#else
#define DPB_TOP		(DOS_INFO_TOP + DOS_INFO_SIZE)
#endif
#define DPB_SIZE	0x600
#define SFT_TOP		(DPB_TOP + DPB_SIZE)
#define SFT_SIZE	0x4b0	/* 6 + 0x3b * 20 */
#define DISK_BUF_TOP	(SFT_TOP + SFT_SIZE)
#define DISK_BUF_SIZE	0x20
#define CDS_TOP		(DISK_BUF_TOP + DISK_BUF_SIZE)
#define CDS_SIZE	0x8F0	/* 88 * 26 */
#define FCB_TABLE_TOP	(CDS_TOP + CDS_SIZE)
#define FCB_TABLE_SIZE	0x10
#define SDA_TOP		(FCB_TABLE_TOP + FCB_TABLE_SIZE)
#define SDA_SIZE	0xb0
// NLS tables
#define UPPERTABLE_TOP	(SDA_TOP + SDA_SIZE)
#define UPPERTABLE_SIZE	0x82
#define LOWERTABLE_TOP	(UPPERTABLE_TOP + UPPERTABLE_SIZE)
#define LOWERTABLE_SIZE	0x82
#define FILENAME_UPPERTABLE_TOP (LOWERTABLE_TOP + LOWERTABLE_SIZE)
#define FILENAME_UPPERTABLE_SIZE 0x82
#define FILENAME_TERMINATOR_TOP (FILENAME_UPPERTABLE_TOP + FILENAME_UPPERTABLE_SIZE)
#define FILENAME_TERMINATOR_SIZE 0x20	/* requirement: 10 + 14(terminate chars) */
#define COLLATING_TABLE_TOP (FILENAME_TERMINATOR_TOP + FILENAME_TERMINATOR_SIZE)
#define COLLATING_TABLE_SIZE 0x102
#define DBCS_TOP	(COLLATING_TABLE_TOP + COLLATING_TABLE_SIZE)
#define DBCS_TABLE	(DBCS_TOP + 2)
#define DBCS_SIZE	0x10
#define MSDOS_SYSTEM_DATA_END (DBCS_TOP + DBCS_SIZE)
#define MEMORY_TOP	((MSDOS_SYSTEM_DATA_END + 15) & ~15U)
#ifdef SUPPORT_GRAPHIC_SCREEN
#define MEMORY_END	0xa0000
#define VGA_VRAM_TOP	0xa0000
#define VGA_VRAM_END	0xc0000
#else
#define MEMORY_END	0xb0000
#endif
#define MDA_VRAM_TOP	0xb0000
#define TEXT_VRAM_TOP	0xb8000
#define EMS_TOP		0xc0000
#define EMS_SIZE	0x10000
UINT32 UMB_TOP = EMS_TOP; // EMS is disabled
#define UMB_END		0xf8000
#define SHADOW_BUF_TOP	0xf8000
// text VRAM size: 80x25x2 = 4000 = 0fa0h
// fffa0h-fffefh can be used for dummy routines
#define DUMMY_TOP	0xfffc0
//#define EMB_TOP	0x10fff0
#define EMB_TOP		0x110000 // align to 4KB
#define EMB_END		MAX_MEM

UINT32 IRET_TOP = 0;
//#define IRET_SIZE	0x100	// moved into common.h

UINT32 ATOK_TOP = 0;
// ATOK_TOP + 0x000	ATOK5 driver
// ATOK_TOP + 0x012	ATOK5 dummy routine
// ATOK_TOP + 0x015	"ATOK"
// ATOK_TOP + 0x019	ATOK5 driver dummy routine (at ATOK_TOP + ATOK_SIZE - 7)
#define ATOK_SIZE	0x20	/* 18 + 3 + 4 + 7 */

UINT32 XMS_TOP = 0;
// XMS_TOP + 0x000	EMMXXXX0 driver
// XMS_TOP + 0x012	EMS dummy routine
// XMS_TOP + 0x015	XMS dummy routine
// XMS_TOP + 0x018	EMMXXXX0 ioctrl recv buffer
// XMS_TOP + 0x1b9	EMMXXXX0 driver dummy routine (at XMS_TOP + XMS_SIZE - 7)
#define XMS_SIZE	0x1c0	/* 18 + 6 + 413 + 7 */

//#define ENV_SIZE	0x800
#define ENV_SIZE	0x2000
#define PSP_SIZE	0x100
#define PSP_SYSTEM	0x0008

#define MAX_FILES	128
#define MAX_PROCESS	16
#define MAX_DTAINFO	128
#define LFN_DTA_LADDR	0x10FFF0
#define FIND_MAGIC	0x46696e64

#define DUP_STDIN	27
#define DUP_STDOUT	28
#define DUP_STDERR	29
#define DUP_STDAUX	30
#define DUP_STDPRN	31

//#define MAP_AUX_DEVICE_TO_FILE

#pragma pack(1)
typedef struct {
	UINT8 mz;
	UINT16 psp;
	UINT16 paragraphs;
	UINT8 reserved[3];
	char prog_name[8];
} mcb_t;
#pragma pack()

#ifdef SUPPORT_HMA
#pragma pack(1)
typedef struct {
	UINT8 ms[2];
	UINT16 owner;
	UINT16 size;
	UINT16 next;
} hma_mcb_t;
#pragma pack()
#endif

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
//	UINT8 far_call;
//	PAIR32 cpm_entry;
	UINT8 call5[5];
	PAIR32 int_22h;
	PAIR32 int_23h;
	PAIR32 int_24h;
	UINT16 parent_psp;
	UINT8 file_table[20];
	UINT16 env_seg;
	PAIR32 stack;
	UINT16 file_table_size;
	PAIR32 file_table_ptr;
	UINT8 reserved_2[24];
	UINT8 service[3];
	UINT8 reserved_3[2];
	UINT8 ex_fcb[7];
	UINT8 fcb1[16];
	UINT8 fcb2[16];
	UINT8 reserved_4[4];
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
	union {
		UINT8 reserved[8];
		HANDLE handle;
	};
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
	union {
		UINT8 reserved[21];
		struct {
			UINT32 find_magic;
			UINT32 dta_index;
		};
	};
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
	UINT16 info_level;
	UINT32 serial_number;
	char volume_label[11];
	char file_system[8];
} drive_info_t;
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
} ext_space_info_t;
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
	// extended
	UINT16 fat_mirroring;
	UINT16 info_sector;
	UINT16 backup_boot_sector;
	UINT32 first_cluster_sector;
	UINT32 maximum_cluster_num;
	UINT32 fat_sectors;
	UINT32 root_cluster;
	UINT32 free_search_cluster;
} dpb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 bytes_per_sector;
	UINT8 sectors_per_cluster;
	UINT16 reserved_sectors;
	UINT8 fat_num;
	UINT16 root_entries;
	UINT16 total_sectors;
	UINT8 media_type;
	UINT16 sectors_per_fat;
	UINT16 sectors_per_track;
	UINT16 heads_num;
	UINT32 hidden_sectors;
	// extended
	UINT32 ext_total_sectors;
	UINT32 ext_sectors_per_fat;
} bpb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	char path_name[67];
	UINT16 drive_attrib;
	PAIR32 dpb_ptr;
	UINT16 word_1;
	UINT16 word_2;
	UINT16 word_3;
	UINT16 bs_offset;
} cds_t;
#pragma pack()

#pragma pack(1)
typedef struct {
//	UINT16 int21h_5d0ah_si;		// -38
//	UINT16 int21h_5d0ah_ds;		// -36
	// swappable data area
	UINT8 printer_cho_flag;		// -34
	UINT16 int21h_5d0ah_dx;		// -33
	UINT8 switchar;			// -31 current switch character
	UINT8 malloc_strategy;		// -30 current memory allocation strategy
	UINT8 int21h_5d0ah_cl;		// -29
	UINT8 int21h_5e01h_counter;	// -28
	UINT8 int21h_5e01h_name[16];	// -27
	UINT16 offset_lists[5];		// -11
	UINT8 int21h_5d0ah_called;	// -1
	// ----- from DOSBox -----
	UINT8 crit_error_flag;		// 0x00 Critical Error Flag
	UINT8 indos_flag;		// 0x01 InDOS flag (count of active INT 21 calls)
	UINT8 drive_crit_error;		// 0x02 Drive on which current critical error occurred or FFh
	UINT8 locus_of_last_error;	// 0x03 locus of last error
	UINT16 extended_error_code;	// 0x04 extended error code of last error
	UINT8 suggested_action;		// 0x06 suggested action for last error
	UINT8 error_class;		// 0x07 class of last error
	PAIR32 last_error_pointer; 	// 0x08 ES:DI pointer for last error
	PAIR32 current_dta;		// 0x0C current DTA (Disk Transfer Address)
	UINT16 current_psp; 		// 0x10 current PSP
	UINT16 sp_int_23;		// 0x12 stores SP across an INT 23
	UINT16 return_code;		// 0x14 return code from last process termination (zerod after reading with AH=4Dh)
	UINT8 current_drive;		// 0x16 current drive
	UINT8 extended_break_flag; 	// 0x17 extended break flag
	UINT8 fill[2];			// 0x18 flag: code page switching || flag: copy of previous byte in case of INT 24 Abort
} sda_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 reserved_0[4];	// -38
	UINT16 magic_word;	// -34 from DOSBox
	UINT8 reserved_1[30];	// -32
	UINT16 first_mcb;	// -2
	PAIR32 first_dpb;	// +0
	PAIR32 first_sft;	// +4
	PAIR32 clock_device;	// +8
	PAIR32 con_device;	// +12
	UINT16 max_sector_len;	// +16
	PAIR32 disk_buf_info;	// +18 from DOSBox
	PAIR32 cds;		// +22
	PAIR32 fcb_table;	// +26
	UINT8 reserved_2[3];	// +30
	UINT8 last_drive;	// +33
	struct {
		PAIR32 next_driver;	// +34
		UINT16 attributes;	// +38
		UINT16 strategy;	// +40
		UINT16 interrupt;	// +42
		char dev_name[8];	// +44
	} nul_device;
	UINT8 reserved_3[11];	// +52
	UINT16 buffers_x;	// +63
	UINT16 buffers_y;	// +65
	UINT8 boot_drive;	// +67
	UINT8 i386_or_later;	// +68
	UINT16 ext_mem_size;	// +69
	PAIR32 disk_buf_heads;	// +71 from DOSBox
	UINT8 reserved_4[21];	// +75
	UINT8 dos_flag;		// +96
	UINT8 reserved_5[2];	// +97
	UINT8 umb_linked;	// +99 from DOSBox
	UINT8 reserved_6[2];	// +100
	UINT16 first_umb_fcb;	// +102 from DOSBox
	UINT16 first_mcb_2;	// +104 from DOSBox
	UINT8 nul_device_routine[7];
} dos_info_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	PAIR32 next_driver;
	UINT16 attributes;
	UINT16 strategy;
	UINT16 interrupt;
	char dev_name[8];
} device_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 date_format;
	char currency_symbol[5];
	char thou_sep[2];
	char dec_sep[2];
	char date_sep[2];
	char time_sep[2];
	char currency_format;
	char currency_dec_digits;
	char time_format;
	PAIR32 case_map;
	char list_sep[2];
	char reserved[10];
} country_info_t;
#pragma pack()

typedef struct {
	char path[MAX_PATH];
	int valid;
	int id;
	int atty;
	int mode;
	UINT16 info;
	UINT16 psp;
	int sio_port; // 1-4
	int lpt_port; // 1-3
} file_handler_t;

static const struct {
	int mode;
	int in;
	int out;
	const char *str;
} file_mode[] = {
	{ _O_RDONLY | _O_BINARY, 1, 0, "r"  },
	{ _O_WRONLY | _O_BINARY, 0, 1, "w"  },
	{ _O_RDWR   | _O_BINARY, 1, 1, "rw" },
};

typedef struct {
	UINT16 psp;
	char module_dir[MAX_PATH];
//#ifdef USE_DEBUGGER
	char module_path[MAX_PATH];
//#endif
	PAIR32 dta;
	UINT8 switchar;
	UINT8 verify;
	int max_files;
	char volume_label[MAX_PATH];
	bool parent_int_10h_feh_called;
	bool parent_int_10h_ffh_called;
	UINT16 parent_ds;
	UINT16 parent_es;
	struct {
		UINT16 handle;
		UINT16 page;
		bool mapped;
	} ems_pages[4];
	bool ems_pages_stored;
	bool called_by_int2eh;
} process_t;

typedef struct {
	UINT16 psp;
	UINT32 dta;
	UINT8 allowable_mask;
	UINT8 required_mask;
	HANDLE find_handle;
} dtainfo_t;

UINT8 dos_major_version = 7;	// Windows 98 Second Edition
UINT8 dos_minor_version = 10;
UINT8 win_major_version = 4;
UINT8 win_minor_version = 10;

UINT16 first_mcb;
UINT16 current_psp;

int retval = 0;
UINT16 error_code = 0;

file_handler_t file_handler[MAX_FILES];
UINT8 file_buffer[0x100000];

process_t process[MAX_PROCESS];
dtainfo_t dtalist[MAX_DTAINFO];

UINT16 malloc_strategy = 0;

char comspec_path[MAX_PATH] = "C:\\COMMAND.COM";

UINT16 error_table_seg[4];
UINT16 error_table_ofs[4];

void msdos_syscall(unsigned num);
int msdos_init(int argc, char *argv[], char *envp[], int standard_env);
void msdos_finish();

// console

#define SCR_BUF_WIDTH	256
#define SCR_BUF_HEIGHT	128

#define SET_RECT(rect, l, t, r, b) { \
	rect.Left = l; \
	rect.Top = t; \
	rect.Right = r; \
	rect.Bottom = b; \
}

DWORD dwConsoleMode = 0;

UINT input_cp, output_cp;
int multibyte_cp;
bool restore_input_cp = false;
bool restore_output_cp = false;
bool restore_multibyte_cp = false;

CONSOLE_CURSOR_INFO ci_old;
CONSOLE_CURSOR_INFO ci_new;
CONSOLE_FONT_INFOEX fi_new;
int font_width = 10;
int font_height = 18;

CHAR_INFO scr_buf[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
char scr_char[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
WORD scr_attr[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
COORD scr_buf_size;
COORD scr_buf_pos;
int scr_width, scr_height;
int scr_top;
bool restore_console_size = false;
bool restore_console_cursor = false;
bool restore_console_font = false;
bool cursor_moved;
bool cursor_moved_by_crtc;

FIFO *key_buf_char = NULL;
FIFO *key_buf_scan = NULL;
FIFO *key_buf_data = NULL;
bool key_changed = false;
UINT32 key_code = 0;
UINT32 key_recv = 0;

bool pcbios_is_key_buffer_empty();
void pcbios_clear_key_buffer();
void pcbios_set_key_buffer(int key_char, int key_scan);
bool pcbios_get_key_buffer(int *key_char, int *key_scan);

UINT8 ctrl_break_checking = 0x00; // ???
bool ctrl_break_detected = false;
bool ctrl_break_pressed = false;
bool ctrl_c_pressed = false;
bool raise_int_1bh = false;

int active_code_page;
int system_code_page;
int console_code_page;

UINT32 text_vram_top_address;
UINT32 text_vram_end_address;
UINT32 shadow_buffer_top_address;
UINT32 shadow_buffer_end_address;
UINT32 cursor_position_address;
int vram_pages;
bool int_10h_feh_called = false;
bool int_10h_ffh_called = false;

#define MAX_MOUSE_BUTTONS	2

typedef struct mouse_s {
	bool enabled;	// from DOSBox
	bool enabled_ps2;
	int hidden;
	int old_hidden;	// from DOSBox
	struct {
		int x, y;
	} position, prev_position, max_position, min_position, mickey;
	struct {
		bool status;
		int pressed_times;
		int released_times;
		struct {
			int x, y;
		} pressed_position;
		struct {
			int x, y;
		} released_position;
	} buttons[MAX_MOUSE_BUTTONS];
	int get_buttons()
	{
		int val = 0;
		for(int i = 0; i < MAX_MOUSE_BUTTONS; i++) {
			if(buttons[i].status) {
				val |= 1 << i;
			}
		}
		return(val);
	}
	UINT16 status, status_alt;
	UINT16 status_irq, status_irq_alt, status_irq_ps2;
	UINT16 call_mask;
	PAIR32 call_addr, call_addr_alt[8], call_addr_ps2;
	// dummy
	UINT16 sensitivity[3];
	UINT16 display_page;
	UINT16 language;
	UINT16 hot_spot[2];
	UINT16 screen_mask;
	UINT16 cursor_mask;
} mouse_t;

mouse_t mouse;

UINT16 mouse_push_ax;
UINT16 mouse_push_bx;
UINT16 mouse_push_cx;
UINT16 mouse_push_dx;
UINT16 mouse_push_si;
UINT16 mouse_push_di;
UINT16 mouse_push_ds;
UINT16 mouse_push_es;

// HMA

#ifdef SUPPORT_HMA
bool is_hma_used_by_xms = false;
bool is_hma_used_by_int_2fh = false;
#endif

// XMS

#ifdef SUPPORT_XMS
typedef struct emb_handle_s {
	UINT32 handle; // 0=allocated
	UINT32 address;
	int size_kb;
	int lock;
	struct emb_handle_s *prev;
	struct emb_handle_s *next;
} emb_handle_t;

emb_handle_t *emb_handle_top = NULL;
int xms_a20_local_enb_count;
UINT16 xms_dx_after_call_08h = 0;

void msdos_xms_init();
void msdos_xms_finish();
void msdos_xms_release();
emb_handle_t *msdos_xms_get_emb_handle(int handle);
int msdos_xms_get_unused_emb_handle_id();
int msdos_xms_get_unused_emb_handle_count();
void msdos_xms_split_emb_handle(emb_handle_t *emb_handle, int size_kb);
void msdos_xms_combine_emb_handles(emb_handle_t *emb_handle);
emb_handle_t *msdos_xms_alloc_emb_handle(int size_kb);
void msdos_xms_free_emb_handle(emb_handle_t *emb_handle);
#endif

/* ----------------------------------------------------------------------------
	tables
---------------------------------------------------------------------------- */

static const struct {
	UINT16 code;
	const char *message_english;
	const BYTE *message_french;
	const BYTE *message_german;
	const BYTE *message_spanish;
	const BYTE *message_portuguese;
	const BYTE *message_brazilian;
	const BYTE *message_japanese;
	const BYTE *message_korean;
} standard_error_table[] = {
	{0x01,	"Invalid function",
		standard_error_french_01, standard_error_german_01, standard_error_spanish_01, standard_error_portuguese_01, standard_error_brazilian_01, standard_error_japanese_01, standard_error_korean_01},
	{0x02,	"File not found",
		standard_error_french_02, standard_error_german_02, standard_error_spanish_02, standard_error_portuguese_02, standard_error_brazilian_02, standard_error_japanese_02, standard_error_korean_02},
	{0x03,	"Path not found",
		standard_error_french_03, standard_error_german_03, standard_error_spanish_03, standard_error_portuguese_03, standard_error_brazilian_03, standard_error_japanese_03, standard_error_korean_03},
	{0x04,	"Too many open files",
		standard_error_french_04, standard_error_german_04, standard_error_spanish_04, standard_error_portuguese_04, standard_error_brazilian_04, standard_error_japanese_04, standard_error_korean_04},
	{0x05,	"Access denied",
		standard_error_french_05, standard_error_german_05, standard_error_spanish_05, standard_error_portuguese_05, standard_error_brazilian_05, standard_error_japanese_05, standard_error_korean_05},
	{0x06,	"Invalid handle",
		standard_error_french_06, standard_error_german_06, standard_error_spanish_06, standard_error_portuguese_06, standard_error_brazilian_06, standard_error_japanese_06, standard_error_korean_06},
	{0x07,	"Memory control blocks destroyed",
		standard_error_french_07, standard_error_german_07, standard_error_spanish_07, standard_error_portuguese_07, standard_error_brazilian_07, standard_error_japanese_07, standard_error_korean_07},
	{0x08,	"Insufficient memory",
		standard_error_french_08, standard_error_german_08, standard_error_spanish_08, standard_error_portuguese_08, standard_error_brazilian_08, standard_error_japanese_08, standard_error_korean_08},
	{0x09,	"Invalid memory block address",
		standard_error_french_09, standard_error_german_09, standard_error_spanish_09, standard_error_portuguese_09, standard_error_brazilian_09, standard_error_japanese_09, standard_error_korean_09},
	{0x0A,	"Invalid Environment",
		standard_error_french_0A, standard_error_german_0A, standard_error_spanish_0A, standard_error_portuguese_0A, standard_error_brazilian_0A, standard_error_japanese_0A, standard_error_korean_0A},
	{0x0B,	"Invalid format",
		standard_error_french_0B, standard_error_german_0B, standard_error_spanish_0B, standard_error_portuguese_0B, standard_error_brazilian_0B, standard_error_japanese_0B, standard_error_korean_0B},
	{0x0C,	"Invalid function parameter",
		standard_error_french_0C, standard_error_german_0C, standard_error_spanish_0C, standard_error_portuguese_0C, standard_error_brazilian_0C, standard_error_japanese_0C, standard_error_korean_0C},
	{0x0D,	"Invalid data",
		standard_error_french_0D, standard_error_german_0D, standard_error_spanish_0D, standard_error_portuguese_0D, standard_error_brazilian_0D, standard_error_japanese_0D, standard_error_korean_0D},
	{0x0F,	"Invalid drive specification",
		standard_error_french_0F, standard_error_german_0F, standard_error_spanish_0F, standard_error_portuguese_0F, standard_error_brazilian_0F, standard_error_japanese_0F, standard_error_korean_0F},
	{0x10,	"Attempt to remove current directory",
		standard_error_french_10, standard_error_german_10, standard_error_spanish_10, standard_error_portuguese_10, standard_error_brazilian_10, standard_error_japanese_10, standard_error_korean_10},
	{0x11,	"Not same device",
		standard_error_french_11, standard_error_german_11, standard_error_spanish_11, standard_error_portuguese_11, standard_error_brazilian_11, standard_error_japanese_11, standard_error_korean_11},
	{0x12,	"No more files",
		standard_error_french_12, standard_error_german_12, standard_error_spanish_12, standard_error_portuguese_12, standard_error_brazilian_12, standard_error_japanese_12, standard_error_korean_12},
	{0x13,	"Write protect error",
		critical_error_french_00, critical_error_german_00, critical_error_spanish_00, critical_error_portuguese_00, critical_error_brazilian_00, critical_error_japanese_00, critical_error_korean_00},
	{0x14,	"Invalid unit",
		critical_error_french_01, critical_error_german_01, critical_error_spanish_01, critical_error_portuguese_01, critical_error_brazilian_01, critical_error_japanese_01, critical_error_korean_01},
	{0x15,	"Not ready",
		critical_error_french_02, critical_error_german_02, critical_error_spanish_02, critical_error_portuguese_02, critical_error_brazilian_02, critical_error_japanese_02, critical_error_korean_02},
	{0x16,	"Invalid device request",
		critical_error_french_03, critical_error_german_03, critical_error_spanish_03, critical_error_portuguese_03, critical_error_brazilian_03, critical_error_japanese_03, critical_error_korean_03},
	{0x17,	"Data error",
		critical_error_french_04, critical_error_german_04, critical_error_spanish_04, critical_error_portuguese_04, critical_error_brazilian_04, critical_error_japanese_04, critical_error_korean_04},
	{0x18,	"Invalid device request parameters",
		critical_error_french_05, critical_error_german_05, critical_error_spanish_05, critical_error_portuguese_05, critical_error_brazilian_05, critical_error_japanese_05, critical_error_korean_05},
	{0x19,	"Seek error",
		critical_error_french_06, critical_error_german_06, critical_error_spanish_06, critical_error_portuguese_06, critical_error_brazilian_06, critical_error_japanese_06, critical_error_korean_06},
	{0x1A,	"Invalid media type",
		critical_error_french_07, critical_error_german_07, critical_error_spanish_07, critical_error_portuguese_07, critical_error_brazilian_07, critical_error_japanese_07, critical_error_korean_07},
	{0x1B,	"Sector not found",
		critical_error_french_08, critical_error_german_08, critical_error_spanish_08, critical_error_portuguese_08, critical_error_brazilian_08, critical_error_japanese_08, critical_error_korean_08},
	{0x1C,	"Printer out of paper error",
		critical_error_french_09, critical_error_german_09, critical_error_spanish_09, critical_error_portuguese_09, critical_error_brazilian_09, critical_error_japanese_09, critical_error_korean_09},
	{0x1D,	"Write fault error",
		critical_error_french_0A, critical_error_german_0A, critical_error_spanish_0A, critical_error_portuguese_0A, critical_error_brazilian_0A, critical_error_japanese_0A, critical_error_korean_0A},
	{0x1E,	"Read fault error",
		critical_error_french_0B, critical_error_german_0B, critical_error_spanish_0B, critical_error_portuguese_0B, critical_error_brazilian_0B, critical_error_japanese_0B, critical_error_korean_0B},
	{0x1F,	"General failure",
		critical_error_french_0C, critical_error_german_0C, critical_error_spanish_0C, critical_error_portuguese_0C, critical_error_brazilian_0C, critical_error_japanese_0C, critical_error_korean_0C},
	{0x20,	"Sharing violation",
		critical_error_french_0D, critical_error_german_0D, critical_error_spanish_0D, critical_error_portuguese_0D, critical_error_brazilian_0D, critical_error_japanese_0D, critical_error_korean_0D},
	{0x21,	"Lock violation",
		critical_error_french_0E, critical_error_german_0E, critical_error_spanish_0E, critical_error_portuguese_0E, critical_error_brazilian_0E, critical_error_japanese_0E, critical_error_korean_0E},
	{0x22,	"Invalid disk change",
		critical_error_french_0F, critical_error_german_0F, critical_error_spanish_0F, critical_error_portuguese_0F, critical_error_brazilian_0F, critical_error_japanese_0F, critical_error_korean_0F},
	{0x23,	"FCB unavailable",
		critical_error_french_10, critical_error_german_10, critical_error_spanish_10, critical_error_portuguese_10, critical_error_brazilian_10, critical_error_japanese_10, critical_error_korean_10},
	{0x24,	"System resource exhausted",
		critical_error_french_11, critical_error_german_11, critical_error_spanish_11, critical_error_portuguese_11, critical_error_brazilian_11, critical_error_japanese_11, critical_error_korean_11},
	{0x25,	"Code page mismatch",
		critical_error_french_12, critical_error_german_12, critical_error_spanish_12, critical_error_portuguese_12, critical_error_brazilian_12, critical_error_japanese_12, critical_error_korean_12},
	{0x26,	"Out of input",
		critical_error_french_13, critical_error_german_13, critical_error_spanish_13, critical_error_portuguese_13, critical_error_brazilian_13, critical_error_japanese_13, critical_error_korean_13},
	{0x27,	"Insufficient disk space",
		critical_error_french_14, critical_error_german_14, critical_error_spanish_14, critical_error_portuguese_14, critical_error_brazilian_14, critical_error_japanese_14, critical_error_korean_14},
/*
	{0x32,	"Network request not supported", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x33,	"Remote computer not listening", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x34,	"Duplicate name on network", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x35,	"Network name not found", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x36,	"Network busy", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x37,	"Network device no longer exists", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x38,	"Network BIOS command limit exceeded", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x39,	"Network adapter hardware error", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x3A,	"Incorrect response from network", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x3B,	"Unexpected network error", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x3C,	"Incompatible remote adapter", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x3D,	"Print queue full", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x3E,	"Queue not full", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x3F,	"Not enough space to print file", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x40,	"Network name was deleted", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x41,	"Network: Access denied", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x42,	"Network device type incorrect", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x43,	"Network name not found", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x44,	"Network name limit exceeded", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x45,	"Network BIOS session limit exceeded", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x46,	"Temporarily paused", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x47,	"Network request not accepted", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x48,	"Network print/disk redirection paused", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x49,	"Network software not installed", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
	{0x4A,	"Unexpected adapter close", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
*/
	{0x50,	"File exists",
		standard_error_french_50, standard_error_german_50, standard_error_spanish_50, standard_error_portuguese_50, standard_error_brazilian_50, standard_error_japanese_50, standard_error_korean_50},
	{0x52,	"Cannot make directory entry",
		standard_error_french_52, standard_error_german_52, standard_error_spanish_52, standard_error_portuguese_52, standard_error_brazilian_52, standard_error_japanese_52, standard_error_korean_52},
	{0x53,	"Fail on INT 24",
		standard_error_french_53, standard_error_german_53, standard_error_spanish_53, standard_error_portuguese_53, standard_error_brazilian_53, standard_error_japanese_53, standard_error_korean_53},
	{0x54,	"Too many redirections",
		standard_error_french_54, standard_error_german_54, standard_error_spanish_54, standard_error_portuguese_54, standard_error_brazilian_54, standard_error_japanese_54, standard_error_korean_54},
	{0x55,	"Duplicate redirection",
		standard_error_french_55, standard_error_german_55, standard_error_spanish_55, standard_error_portuguese_55, standard_error_brazilian_55, standard_error_japanese_55, standard_error_korean_55},
	{0x56,	"Invalid password",
		standard_error_french_56, standard_error_german_56, standard_error_spanish_56, standard_error_portuguese_56, standard_error_brazilian_56, standard_error_japanese_56, standard_error_korean_56},
	{0x57,	"Invalid parameter",
		standard_error_french_57, standard_error_german_57, standard_error_spanish_57, standard_error_portuguese_57, standard_error_brazilian_57, standard_error_japanese_57, standard_error_korean_57},
	{0x58,	"Network data fault",
		standard_error_french_58, standard_error_german_58, standard_error_spanish_58, standard_error_portuguese_58, standard_error_brazilian_58, standard_error_japanese_58, standard_error_korean_58},
	{0x59,	"Function not supported by network",
		standard_error_french_59, standard_error_german_59, standard_error_spanish_59, standard_error_portuguese_59, standard_error_brazilian_59, standard_error_japanese_59, standard_error_korean_59},
	{0x5A,	"Required system component not installe",
		standard_error_french_5A, standard_error_german_5A, standard_error_spanish_5A, standard_error_portuguese_5A, standard_error_brazilian_5A, standard_error_japanese_5A, standard_error_korean_5A},
//#ifdef SUPPORT_MSCDEX
	{0x64,	"Unknown error",
		standard_error_french_64, standard_error_german_64, standard_error_spanish_64, standard_error_portuguese_64, standard_error_brazilian_64, standard_error_japanese_64, standard_error_korean_64},
	{0x65,	"Not ready",
		standard_error_french_65, standard_error_german_65, standard_error_spanish_65, standard_error_portuguese_65, standard_error_brazilian_65, standard_error_japanese_65, standard_error_korean_65},
	{0x66,	"EMS memory no longer valid",
		standard_error_french_66, standard_error_german_66, standard_error_spanish_66, standard_error_portuguese_66, standard_error_brazilian_66, standard_error_japanese_66, standard_error_korean_66},
	{0x67,	"CDROM not High Sierra or ISO-9660 format",
		standard_error_french_67, standard_error_german_67, standard_error_spanish_67, standard_error_portuguese_67, standard_error_brazilian_67, standard_error_japanese_67, standard_error_korean_67},
	{0x68,	"Door open",
		standard_error_french_68, standard_error_german_68, standard_error_spanish_68, standard_error_portuguese_68, standard_error_brazilian_68, standard_error_japanese_68, standard_error_korean_68},
//#endif
	{0xB0,	"Volume is not locked",
		standard_error_french_B0, standard_error_german_B0, standard_error_spanish_B0, standard_error_portuguese_B0, standard_error_brazilian_B0, standard_error_japanese_B0, standard_error_korean_B0},
	{0xB1,	"Volume is locked in drive",
		standard_error_french_B1, standard_error_german_B1, standard_error_spanish_B1, standard_error_portuguese_B1, standard_error_brazilian_B1, standard_error_japanese_B1, standard_error_korean_B1},
	{0xB2,	"Volume is not removable",
		standard_error_french_B2, standard_error_german_B2, standard_error_spanish_B2, standard_error_portuguese_B2, standard_error_brazilian_B2, standard_error_japanese_B2, standard_error_korean_B2},
	{0xB4,	"Lock count has been exceeded",
		standard_error_french_B4, standard_error_german_B4, standard_error_spanish_B4, standard_error_portuguese_B4, standard_error_brazilian_B4, standard_error_japanese_B4, standard_error_korean_B4},
	{0xB5,	"A valid eject request failed",
		standard_error_french_B5, standard_error_german_B5, standard_error_spanish_B5, standard_error_portuguese_B5, standard_error_brazilian_B5, standard_error_japanese_B5, standard_error_korean_B5},
	{(UINT16)-1,
		"Unknown error",
		unknown_error_french, unknown_error_german, unknown_error_spanish, unknown_error_portuguese, unknown_error_brazilian, unknown_error_japanese, unknown_error_korean},
};

static const struct {
	UINT16 code;
	const char *message_english;
	const BYTE *message_french;
	const BYTE *message_german;
	const BYTE *message_spanish;
	const BYTE *message_portuguese;
	const BYTE *message_brazilian;
	const BYTE *message_japanese;
	const BYTE *message_korean;
} critical_error_table[] = {
	{0x00,	"Write protect error",
		critical_error_french_00, critical_error_german_00, critical_error_spanish_00, critical_error_portuguese_00, critical_error_brazilian_00, critical_error_japanese_00, critical_error_korean_00},
	{0x01,	"Invalid unit",
		critical_error_french_01, critical_error_german_01, critical_error_spanish_01, critical_error_portuguese_01, critical_error_brazilian_01, critical_error_japanese_01, critical_error_korean_01},
	{0x02,	"Not ready",
		critical_error_french_02, critical_error_german_02, critical_error_spanish_02, critical_error_portuguese_02, critical_error_brazilian_02, critical_error_japanese_02, critical_error_korean_02},
	{0x03,	"Invalid device request",
		critical_error_french_03, critical_error_german_03, critical_error_spanish_03, critical_error_portuguese_03, critical_error_brazilian_03, critical_error_japanese_03, critical_error_korean_03},
	{0x04,	"Data error",
		critical_error_french_04, critical_error_german_04, critical_error_spanish_04, critical_error_portuguese_04, critical_error_brazilian_04, critical_error_japanese_04, critical_error_korean_04},
	{0x05,	"Invalid device request parameters",
		critical_error_french_05, critical_error_german_05, critical_error_spanish_05, critical_error_portuguese_05, critical_error_brazilian_05, critical_error_japanese_05, critical_error_korean_05},
	{0x06,	"Seek error",
		critical_error_french_06, critical_error_german_06, critical_error_spanish_06, critical_error_portuguese_06, critical_error_brazilian_06, critical_error_japanese_06, critical_error_korean_06},
	{0x07,	"Invalid media type",
		critical_error_french_07, critical_error_german_07, critical_error_spanish_07, critical_error_portuguese_07, critical_error_brazilian_07, critical_error_japanese_07, critical_error_korean_07},
	{0x08,	"Sector not found",
		critical_error_french_08, critical_error_german_08, critical_error_spanish_08, critical_error_portuguese_08, critical_error_brazilian_08, critical_error_japanese_08, critical_error_korean_08},
	{0x09,	"Printer out of paper error",
		critical_error_french_09, critical_error_german_09, critical_error_spanish_09, critical_error_portuguese_09, critical_error_brazilian_09, critical_error_japanese_09, critical_error_korean_09},
	{0x0A,	"Write fault error",
		critical_error_french_0A, critical_error_german_0A, critical_error_spanish_0A, critical_error_portuguese_0A, critical_error_brazilian_0A, critical_error_japanese_0A, critical_error_korean_0A},
	{0x0B,	"Read fault error",
		critical_error_french_0B, critical_error_german_0B, critical_error_spanish_0B, critical_error_portuguese_0B, critical_error_brazilian_0B, critical_error_japanese_0B, critical_error_korean_0B},
	{0x0C,	"General failure",
		critical_error_french_0C, critical_error_german_0C, critical_error_spanish_0C, critical_error_portuguese_0C, critical_error_brazilian_0C, critical_error_japanese_0C, critical_error_korean_0C},
	{0x0D,	"Sharing violation",
		critical_error_french_0D, critical_error_german_0D, critical_error_spanish_0D, critical_error_portuguese_0D, critical_error_brazilian_0D, critical_error_japanese_0D, critical_error_korean_0D},
	{0x0E,	"Lock violation",
		critical_error_french_0E, critical_error_german_0E, critical_error_spanish_0E, critical_error_portuguese_0E, critical_error_brazilian_0E, critical_error_japanese_0E, critical_error_korean_0E},
	{0x0F,	"Invalid disk change",
		critical_error_french_0F, critical_error_german_0F, critical_error_spanish_0F, critical_error_portuguese_0F, critical_error_brazilian_0F, critical_error_japanese_0F, critical_error_korean_0F},
	{0x10,	"FCB unavailable",
		critical_error_french_10, critical_error_german_10, critical_error_spanish_10, critical_error_portuguese_10, critical_error_brazilian_10, critical_error_japanese_10, critical_error_korean_10},
	{0x11,	"System resource exhausted",
		critical_error_french_11, critical_error_german_11, critical_error_spanish_11, critical_error_portuguese_11, critical_error_brazilian_11, critical_error_japanese_11, critical_error_korean_11},
	{0x12,	"Code page mismatch",
		critical_error_french_12, critical_error_german_12, critical_error_spanish_12, critical_error_portuguese_12, critical_error_brazilian_12, critical_error_japanese_12, critical_error_korean_12},
	{0x13,	"Out of input",
		critical_error_french_13, critical_error_german_13, critical_error_spanish_13, critical_error_portuguese_13, critical_error_brazilian_13, critical_error_japanese_13, critical_error_korean_13},
	{0x14,	"Insufficient disk space",
		critical_error_french_14, critical_error_german_14, critical_error_spanish_14, critical_error_portuguese_14, critical_error_brazilian_14, critical_error_japanese_14, critical_error_korean_04},
	{(UINT16)-1,
		"Critical error",
		critical_error_french, critical_error_german, critical_error_spanish, critical_error_portuguese, critical_error_brazilian, critical_error_japanese, critical_error_korean},
};

static const struct {
	UINT16 code;
	const char *message_english;
	const BYTE *message_french;
	const BYTE *message_german;
	const BYTE *message_spanish;
	const BYTE *message_portuguese;
	const BYTE *message_brazilian;
	const BYTE *message_japanese;
	const BYTE *message_korean;
} param_error_table[] = {
	{0x01,	"Too many parameters",
		param_error_french_01, param_error_german_01, param_error_spanish_01, param_error_portuguese_01, param_error_brazilian_01, param_error_japanese_01, param_error_korean_01},
	{0x02,	"Required parameter missing",
		param_error_french_02, param_error_german_02, param_error_spanish_02, param_error_portuguese_02, param_error_brazilian_02, param_error_japanese_02, param_error_korean_02},
	{0x03,	"Invalid switch",
		param_error_french_03, param_error_german_03, param_error_spanish_03, param_error_portuguese_03, param_error_brazilian_03, param_error_japanese_03, param_error_korean_03},
	{0x04,	"Invalid keyword",
		param_error_french_04, param_error_german_04, param_error_spanish_04, param_error_portuguese_04, param_error_brazilian_04, param_error_japanese_04, param_error_korean_04},
	{0x06,	"Parameter value not in allowed range",
		param_error_french_06, param_error_german_06, param_error_spanish_06, param_error_portuguese_06, param_error_brazilian_06, param_error_japanese_06, param_error_korean_06},
	{0x07,	"Parameter value not allowed",
		param_error_french_07, param_error_german_07, param_error_spanish_07, param_error_portuguese_07, param_error_brazilian_07, param_error_japanese_07, param_error_korean_07},
	{0x08,	"Parameter value not allowed",
		param_error_french_08, param_error_german_08, param_error_spanish_08, param_error_portuguese_08, param_error_brazilian_08, param_error_japanese_08, param_error_korean_08},
	{0x09,	"Parameter format not correct",
		param_error_french_09, param_error_german_09, param_error_spanish_09, param_error_portuguese_09, param_error_brazilian_09, param_error_japanese_09, param_error_korean_09},
	{0x0A,	"Invalid parameter",
		param_error_french_0A, param_error_german_0A, param_error_spanish_0A, param_error_portuguese_0A, param_error_brazilian_0A, param_error_japanese_0A, param_error_korean_0A},
	{0x0B,	"Invalid parameter combination",
		param_error_french_0B, param_error_german_0B, param_error_spanish_0B, param_error_portuguese_0B, param_error_brazilian_0B, param_error_japanese_0B, param_error_korean_0B},
	{(UINT16)-1,
		"Unknown error",
		unknown_error_french, unknown_error_german, unknown_error_spanish, unknown_error_portuguese, unknown_error_brazilian, unknown_error_japanese, unknown_error_korean},
};

static const struct {
	const char *name;
	DWORD lcid;
	const char *std;
	const char *dlt;
} tz_table[] = {
	// https://science.ksc.nasa.gov/software/winvn/userguide/3_1_4.htm
//	0	GMT		Greenwich Mean Time		GMT0
	{"GMT Standard Time",			0x0809, "GMT", "BST"},		// (UTC+00:00) GB London (en-gb)
	{"GMT Standard Time",			0x1809, "GMT", "IST"},		// (UTC+00:00) IE Dublin (en-ie)
	{"GMT Standard Time",			0x0000, "WET", "WES"},		// (UTC+00:00) PT Lisbon
	{"Greenwich Standard Time",		0x0000, "GMT", "GST"},		// (UTC+00:00) IS Reykjavik
//	2	FST	FDT	Fernando De Noronha Std		FST2FDT
	{"Mid-Atlantic Standard Time",		0x0416, "FST", "FDT"},		// (UTC-02:00) BR Noronha (pt-br)
	{"UTC-02",				0x0416, "FST", "FDT"},		// (UTC-02:00) BR Noronha (pt-br)
//	3	BST		Brazil Standard Time		BST3
	{"Bahia Standard Time",			0x0000, "BST", "BDT"},		// (UTC-03:00) BR Bahia
	{"SA Eastern Standard Time",		0x0000, "BST", "BDT"},		// (UTC-03:00) BR Fortaleza
	{"Tocantins Standard Time",		0x0000, "BST", "BDT"},		// (UTC-03:00) BR Palmas
//	3	EST	EDT	Eastern Standard (Brazil)	EST3EDT
	{"E. South America Standard Time",	0x0000, "EST", "EDT"},		// (UTC-03:00) BR Sao Paulo
//	3	GST		Greenland Standard Time		GST3
	{"Greenland Standard Time",		0x0000, "GST", "GDT"},		// (UTC-03:00) GL Godthab
//	3:30	NST	NDT	Newfoundland Standard Time	NST3:30NDT
	{"Newfoundland Standard Time",		0x0000, "NST", "NDT"},		// (UTC-03:30) CA St.Johns
//	4	AST	ADT	Atlantic Standard Time		AST4ADT
	{"Atlantic Standard Time",		0x0000, "AST", "ADT"},		// (UTC-04:00) CA Halifax
//	4	WST	WDT	Western Standard (Brazil)	WST4WDT
	{"Central Brazilian Standard Time",	0x0000, "WST", "WDT"},		// (UTC-04:00) BR Cuiaba
	{"SA Western Standard Time",		0x0000, "WST", "WDT"},		// (UTC-04:00) BR Manaus
//	5	EST	EDT	Eastern Standard Time	 	EST5EDT
	{"Eastern Standard Time",		0x0000, "EST", "EDT"},		// (UTC-05:00) US New York
	{"Eastern Standard Time (Mexico)",	0x0000, "EST", "EDT"},		// (UTC-05:00) MX Cancun
	{"US Eastern Standard Time",		0x0000, "EST", "EDT"},		// (UTC-05:00) US Indianapolis
//	5	CST	CDT	Chile Standard Time		CST5CDT
	{"Pacific SA Standard Time",		0x0000, "CST", "CDT"},		// (UTC-04:00) CL Santiago
//	5	AST	ADT	Acre Standard Time		AST5ADT
	{"SA Pacific Standard Time",		0x0000, "AST", "ADT"},		// (UTC-05:00) BR Rio Branco
//	5	CST	CDT	Cuba Standard Time		CST5CDT
	{"Cuba Standard Time",			0x0000, "CST", "CDT"},		// (UTC-05:00) CU Havana
//	6	CST	CDT	Central Standard Time		CST6CDT
	{"Canada Central Standard Time",	0x0000, "CST", "CDT"},		// (UTC-06:00) CA Regina
	{"Central Standard Time",		0x0000, "CST", "CDT"},		// (UTC-06:00) US Chicago
	{"Central Standard Time (Mexico)",	0x0000, "CST", "CDT"},		// (UTC-06:00) MX Mexico City
//	6	EST	EDT	Easter Island Standard		EST6EDT
	{"Easter Island Standard Time",		0x0000, "EST", "EDT"},		// (UTC-06:00) CL Easter
//	7	MST	MDT	Mountain Standard Time		MST7MDT
	{"Mountain Standard Time",		0x0000, "MST", "MDT"},		// (UTC-07:00) US Denver
	{"Mountain Standard Time (Mexico)",	0x0000, "MST", "MDT"},		// (UTC-07:00) MX Chihuahua
	{"US Mountain Standard Time",		0x0000, "MST", "MDT"},		// (UTC-07:00) US Phoenix
//	8	PST	PDT	Pacific Standard Time		PST8PDT
	{"Pacific Standard Time",		0x0000, "PST", "PDT"},		// (UTC-08:00) US Los Angeles
	{"Pacific Standard Time (Mexico)",	0x0000, "PST", "PDT"},		// (UTC-08:00) MX Tijuana
//	9	AKS	AKD	Alaska Standard Time		AKS9AKD
//	9	YST	YDT	Yukon Standard Time		YST9YST
	{"Alaskan Standard Time",		0x0000, "AKS", "AKD"},		// (UTC-09:00) US Anchorage
//	10	HST	HDT	Hawaii Standard Time		HST10HDT
	{"Aleutian Standard Time",		0x0000, "HST", "HDT"},		// (UTC-10:00) US Aleutian
	{"Hawaiian Standard Time",		0x0000, "HST", "HDT"},		// (UTC-10:00) US Honolulu
//	11	SST		Samoa Standard Time		SST11
	{"Samoa Standard Time",			0x0000, "SST", "SDT"},		// (UTC-11:00) US Samoa
//	-12	NZS	NZD	New Zealand Standard Time	NZS-12NZD
	{"New Zealand Standard Time",		0x0000, "NZS", "NZD"},		// (UTC+12:00) NZ Auckland
//	-10	GST		Guam Standard Time		GST-10
	{"West Pacific Standard Time",		0x0000, "GST", "GDT"},		// (UTC+10:00) GU Guam
//	-10	EAS	EAD	Eastern Australian Standard	EAS-10EAD
	{"AUS Eastern Standard Time",		0x0000, "EAS", "EAD"},		// (UTC+10:00) AU Sydney
	{"E. Australia Standard Time",		0x0000, "EAS", "EAD"},		// (UTC+10:00) AU Brisbane
	{"Tasmania Standard Time",		0x0000, "EAS", "EAD"},		// (UTC+10:00) AU Hobart
//	-9:30	CAS	CAD	Central Australian Standard	CAS-9:30CAD
	{"AUS Central Standard Time",		0x0000, "CAS", "CAD"},		// (UTC+09:30) AU Darwin
	{"Cen. Australia Standard Time",	0x0000, "CAS", "CAD"},		// (UTC+09:30) AU Adelaide
//	-9	JST		Japan Standard Time		JST-9
	{"Tokyo Standard Time",			0x0000, "JST", "JDT"},		// (UTC+09:00) JP Tokyo
//	-9	KST	KDT	Korean Standard Time		KST-9KDT
	{"Korea Standard Time",			0x0000, "KST", "KDT"},		// (UTC+09:00) KR Seoul
	{"North Korea Standard Time",		0x0000, "KST", "KDT"},		// (UTC+09:00) KP Pyongyang
//	-8	HKT		Hong Kong Time			HKT-8
	{"China Standard Time",			0x0C04, "HKT", "HKS"},		// (UTC+08:00) HK Hong Kong (zh-hk)
//	-8	CCT		China Coast Time		CCT-8
	{"China Standard Time",			0x0000, "CCT", "CDT"},		// (UTC+08:00) CN Shanghai
	{"Taipei Standard Time",		0x0000, "CCT", "CDT"},		// (UTC+08:00) TW Taipei
//	-8	SST		Singapore Standard Time		SST-8
	{"Singapore Standard Time",		0x0000, "SST", "SDT"},		// (UTC+08:00) SG Singapore
//	-8	WAS	WAD	Western Australian Standard	WAS-8WAD
	{"Aus Central W. Standard Time",	0x0000, "WAS", "WAD"},		// (UTC+08:45) AU Eucla
	{"W. Australia Standard Time",		0x0000, "WAS", "WAD"},		// (UTC+08:00) AU Perth
//	-7:30	JT		Java Standard Time		JST-7:30
//	-7	NST		North Sumatra Time		NST-7
	{"SE Asia Standard Time",		0x0000, "NST", "NDT"},		// (UTC+07:00) ID Jakarta
//	-5:30	IST		Indian Standard Time		IST-5:30
	{"India Standard Time",			0x0000, "IST", "IDT"},		// (UTC+05:30) IN Calcutta
//	-3:30	IST	IDT	Iran Standard Time		IST-3:30IDT
	{"Iran Standard Time",			0x0000, "IST", "IDT"},		// (UTC+03:30) IR Tehran
//	-3	MSK	MSD	Moscow Winter Time		MSK-3MSD
	{"Belarus Standard Time",		0x0000, "MSK", "MSD"},		// (UTC+03:00) BY Minsk
	{"Russian Standard Time",		0x0000, "MSK", "MSD"},		// (UTC+03:00) RU Moscow
//	-2	EET		Eastern Europe Time		EET-2
	{"E. Europe Standard Time",		0x0000, "EET", "EES"},		// (UTC+02:00) MD Chisinau
	{"FLE Standard Time",			0x0000, "EET", "EES"},		// (UTC+02:00) UA Kiev
	{"GTB Standard Time",			0x0000, "EET", "EES"},		// (UTC+02:00) RO Bucharest
	{"Kaliningrad Standard Time",		0x0000, "EET", "EES"},		// (UTC+02:00) RU Kaliningrad
//	-2	IST	IDT	Israel Standard Time		IST-2IDT
	{"Israel Standard Time",		0x0000, "IST", "IDT"},		// (UTC+02:00) IL Jerusalem
//	-1	MEZ	MES	Middle European Time		MEZ-1MES
//	-1	SWT	SST	Swedish Winter Time		SWT-1SST
//	-1	FWT	FST	French Winter Time		FWT-1FST
//	-1	CET	CES	Central European Time		CET-1CES
	{"Central Europe Standard Time",	0x0000, "CET", "CES"},		// (UTC+01:00) HU Budapest
	{"Central European Standard Time",	0x0000, "CET", "CES"},		// (UTC+01:00) PL Warsaw
	{"Romance Standard Time",		0x0000, "CET", "CES"},		// (UTC+01:00) FR Paris
	{"W. Europe Standard Time",		0x0000, "CET", "CES"},		// (UTC+01:00) DE Berlin
//	-1	WAT	 	West African Time		WAT-1
	{"Namibia Standard Time",		0x0000, "WAT", "WAS"},		// (UTC+01:00) NA Windhoek
	{"W. Central Africa Standard Time",	0x0000, "WAT", "WAS"},		// (UTC+01:00) NG Lagos
//	0	UTC		Universal Coordinated Time	UTC0
	{"UTC",					0x0000, "UTC", ""   },		// (UTC+00:00) GMT+0
	{"UTC-02",				0x0000, "UTC", ""   },		// (UTC-02:00) GMT+2
	{"UTC-08",				0x0000, "UTC", ""   },		// (UTC-08:00) GMT+8
	{"UTC-09",				0x0000, "UTC", ""   },		// (UTC-09:00) GMT+9
	{"UTC-11",				0x0000, "UTC", ""   },		// (UTC-11:00) GMT+11
	{"UTC+12",				0x0000, "UTC", ""   },		// (UTC+12:00) GMT-12
};

static const struct {
	int code;
	USHORT usPrimaryLanguage;
	USHORT usSubLanguage;
} country_table[] = {
	{0x001, LANG_ENGLISH, SUBLANG_ENGLISH_US},				// United States
	{0x002, LANG_FRENCH, SUBLANG_FRENCH_CANADIAN},				// Canadian-French
	{0x004, LANG_ENGLISH, SUBLANG_ENGLISH_CAN},				// Canada (English)
	{0x007, LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA},				// Russia
	{0x014, LANG_ARABIC, SUBLANG_ARABIC_EGYPT},				// Egypt
	{0x01B, LANG_ZULU, SUBLANG_ZULU_SOUTH_AFRICA},				// South Africa
//	{0x01B, LANG_XHOSA, SUBLANG_XHOSA_SOUTH_AFRICA},			// South Africa
//	{0x01B, LANG_AFRIKAANS, SUBLANG_AFRIKAANS_SOUTH_AFRICA},		// South Africa
//	{0x01B, LANG_ENGLISH, SUBLANG_ENGLISH_SOUTH_AFRICA},			// South Africa
//	{0x01B, LANG_SOTHO, SUBLANG_SOTHO_NORTHERN_SOUTH_AFRICA},		// South Africa
//	{0x01B, LANG_TSWANA, SUBLANG_TSWANA_SOUTH_AFRICA},			// South Africa
	{0x01E, LANG_GREEK, SUBLANG_GREEK_GREECE},				// Greece
	{0x01F, LANG_DUTCH, SUBLANG_DUTCH},					// Netherlands
//	{0x01F, LANG_FRISIAN, SUBLANG_FRISIAN_NETHERLANDS},			// Netherlands
	{0x020, LANG_DUTCH, SUBLANG_DUTCH_BELGIAN},				// Belgium
//	{0x020, LANG_FRENCH, SUBLANG_FRENCH_BELGIAN},				// Belgium
	{0x021, LANG_FRENCH, SUBLANG_FRENCH},					// France
	{0x022, LANG_SPANISH, SUBLANG_SPANISH},					// Spain
	{0x023, LANG_BULGARIAN, SUBLANG_BULGARIAN_BULGARIA},			// Bulgaria???
	{0x024, LANG_HUNGARIAN, SUBLANG_HUNGARIAN_HUNGARY},			// Hungary (not supported by DR DOS 5.0)
	{0x027, LANG_ITALIAN, SUBLANG_ITALIAN},					// Italy / San Marino / Vatican City
	{0x028, LANG_ROMANIAN, SUBLANG_ROMANIAN_ROMANIA},			// Romania
	{0x029, LANG_GERMAN, SUBLANG_GERMAN_SWISS},				// Switzerland / Liechtenstein
//	{0x029, LANG_FRENCH, SUBLANG_FRENCH_SWISS},				// Switzerland / Liechtenstein
//	{0x029, LANG_ITALIAN, SUBLANG_ITALIAN_SWISS},				// Switzerland / Liechtenstein
	{0x02A, LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA},				// Czechoslovakia / Tjekia / Slovakia (not supported by DR DOS 5.0)
	{0x02B, LANG_GERMAN, SUBLANG_GERMAN_AUSTRIAN},				// Austria (DR DOS 5.0)
	{0x02C, LANG_ENGLISH, SUBLANG_ENGLISH_UK},				// United Kingdom
	{0x02D, LANG_DANISH, SUBLANG_DANISH_DENMARK},				// Denmark
	{0x02E, LANG_SWEDISH, SUBLANG_SWEDISH},					// Sweden
//	{0x02E, LANG_SAMI, SUBLANG_SAMI_NORTHERN_SWEDEN},			// Sweden
//	{0x02E, LANG_SAMI, SUBLANG_SAMI_LULE_SWEDEN},				// Sweden
//	{0x02E, LANG_SAMI, SUBLANG_SAMI_SOUTHERN_SWEDEN},			// Sweden
	{0x02F, LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL},			// Norway
//	{0x02F, LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK},			// Norway
//	{0x02F, LANG_SAMI, SUBLANG_SAMI_NORTHERN_NORWAY},			// Norway
//	{0x02F, LANG_SAMI, SUBLANG_SAMI_LULE_NORWAY},				// Norway
//	{0x02F, LANG_SAMI, SUBLANG_SAMI_SOUTHERN_NORWAY},			// Norway
	{0x030, LANG_POLISH, SUBLANG_POLISH_POLAND},				// Poland (not supported by DR DOS 5.0)
	{0x031, LANG_GERMAN, SUBLANG_GERMAN},					// Germany
	{0x033, LANG_SPANISH, SUBLANG_SPANISH_PERU},				// Peru
//	{0x033, LANG_QUECHUA, SUBLANG_QUECHUA_PERU},				// Peru
	{0x034, LANG_SPANISH, SUBLANG_SPANISH_MEXICAN},				// Mexico
	{0x035, LANG_SPANISH, SUBLANG_NEUTRAL},					// Cuba
	{0x036, LANG_SPANISH, SUBLANG_SPANISH_ARGENTINA},			// Argentina
	{0x037, LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN},			// Brazil (not supported by DR DOS 5.0)
	{0x038, LANG_SPANISH, SUBLANG_SPANISH_CHILE},				// Chile
	{0x039, LANG_SPANISH, SUBLANG_SPANISH_COLOMBIA},			// Columbia
	{0x03A, LANG_SPANISH, SUBLANG_SPANISH_VENEZUELA},			// Venezuela
	{0x03C, LANG_MALAY, SUBLANG_MALAY_MALAYSIA},				// Malaysia
	{0x03D, LANG_ENGLISH, SUBLANG_ENGLISH_AUS},				// International English / Australia
	{0x03E, LANG_INDONESIAN, SUBLANG_INDONESIAN_INDONESIA},			// Indonesia / East Timor
	{0x03F, LANG_ENGLISH, SUBLANG_ENGLISH_PHILIPPINES},			// Philippines
	{0x040, LANG_ENGLISH, SUBLANG_ENGLISH_NZ},				// New Zealand
	{0x041, LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE},			// Singapore
//	{0x041, LANG_ENGLISH, SUBLANG_ENGLISH_SINGAPORE},			// Singapore
	{0x042, LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL},			// Taiwan???
	{0x051, LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN},				// Japan (DR DOS 5.0, MS-DOS 5.0+)
	{0x052, LANG_KOREAN, SUBLANG_KOREAN},					// South Korea (DR DOS 5.0)
	{0x054, LANG_VIETNAMESE, SUBLANG_VIETNAMESE_VIETNAM},			// Vietnam
	{0x056, LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED},			// China (MS-DOS 5.0+)
	{0x058, LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL},			// Taiwan (MS-DOS 5.0+)
	{0x05A, LANG_TURKISH, SUBLANG_TURKISH_TURKEY},				// Turkey (MS-DOS 5.0+)
	{0x05B, LANG_HINDI, SUBLANG_HINDI_INDIA},				// India
	{0x05C, LANG_URDU, SUBLANG_URDU_PAKISTAN},				// Pakistan
	{0x05D, LANG_PASHTO, SUBLANG_PASHTO_AFGHANISTAN},			// Afghanistan
//	{0x05D, LANG_DARI, SUBLANG_DARI_AFGHANISTAN},				// Afghanistan
	{0x05E, LANG_SINHALESE, SUBLANG_SINHALESE_SRI_LANKA},			// Sri Lanka
//	{0x05E, LANG_TAMIL, SUBLANG_TAMIL_SRI_LANKA},				// Sri Lanka
	{0x062, LANG_PERSIAN, SUBLANG_PERSIAN_IRAN},				// Iran
	{0x070, LANG_BELARUSIAN, SUBLANG_BELARUSIAN_BELARUS},			// Belarus
	{0x0C8, LANG_THAI, SUBLANG_THAI_THAILAND},				// Thailand
	{0x0D4, LANG_ARABIC, SUBLANG_ARABIC_MOROCCO},				// Morocco
	{0x0D5, LANG_ARABIC, SUBLANG_ARABIC_ALGERIA},				// Algeria
	{0x0D8, LANG_ARABIC, SUBLANG_ARABIC_TUNISIA},				// Tunisia
	{0x0DA, LANG_ARABIC, SUBLANG_ARABIC_LIBYA},				// Libya
	{0x0DD, LANG_WOLOF, SUBLANG_WOLOF_SENEGAL},				// Senegal
//	{0x0DD, LANG_PULAR, SUBLANG_PULAR_SENEGAL},				// Senegal
	{0x0EA, LANG_HAUSA, SUBLANG_HAUSA_NIGERIA_LATIN},			// Nigeria
//	{0x0EA, LANG_YORUBA, SUBLANG_YORUBA_NIGERIA},				// Nigeria
//	{0x0EA, LANG_IGBO, SUBLANG_IGBO_NIGERIA},				// Nigeria
	{0x0FB, LANG_AMHARIC, SUBLANG_AMHARIC_ETHIOPIA},			// Ethiopia
//	{0x0FB, LANG_TIGRINYA, SUBLANG_TIGRINYA_ETHIOPIA},			// Ethiopia
	{0x0FE, LANG_SWAHILI, SUBLANG_SWAHILI_KENYA},				// Kenya
	{0x107, LANG_ENGLISH, SUBLANG_ENGLISH_ZIMBABWE},			// Zimbabwe
	{0x10B, LANG_TSWANA, SUBLANG_TSWANA_BOTSWANA},				// Botswana
	{0x12A, LANG_FAEROESE, SUBLANG_FAEROESE_FAROE_ISLANDS},			// Faroe Islands
	{0x12B, LANG_GREENLANDIC, SUBLANG_GREENLANDIC_GREENLAND},		// Greenland
	{0x15F, LANG_PORTUGUESE, SUBLANG_PORTUGUESE},				// Portugal
	{0x160, LANG_LUXEMBOURGISH, SUBLANG_LUXEMBOURGISH_LUXEMBOURG},		// Luxembourg
//	{0x160, LANG_GERMAN, SUBLANG_GERMAN_LUXEMBOURG},			// Luxembourg
//	{0x160, LANG_FRENCH, SUBLANG_FRENCH_LUXEMBOURG},			// Luxembourg
	{0x161, LANG_IRISH, SUBLANG_IRISH_IRELAND},				// Ireland
//	{0x161, LANG_ENGLISH, SUBLANG_ENGLISH_IRELAND},				// Ireland
	{0x162, LANG_ICELANDIC, SUBLANG_ICELANDIC_ICELAND},			// Iceland
	{0x163, LANG_ALBANIAN, SUBLANG_ALBANIAN_ALBANIA},			// Albania
	{0x164, LANG_MALTESE, SUBLANG_MALTESE_MALTA},				// Malta
	{0x166, LANG_FINNISH, SUBLANG_FINNISH_FINLAND},				// Finland
//	{0x166, LANG_SAMI, SUBLANG_SAMI_NORTHERN_FINLAND},			// Finland
//	{0x166, LANG_SAMI, SUBLANG_SAMI_SKOLT_FINLAND},				// Finland
//	{0x166, LANG_SAMI, SUBLANG_SAMI_INARI_FINLAND},				// Finland
	{0x167, LANG_BULGARIAN, SUBLANG_BULGARIAN_BULGARIA},			// Bulgaria
	{0x172, LANG_LITHUANIAN, SUBLANG_LITHUANIAN},				// Lithuania
	{0x173, LANG_LATVIAN, SUBLANG_LATVIAN_LATVIA},				// Latvia
	{0x174, LANG_ESTONIAN, SUBLANG_ESTONIAN_ESTONIA},			// Estonia
	{0x17D, LANG_SERBIAN, SUBLANG_SERBIAN_LATIN},				// Serbia / Montenegro
	{0x180, LANG_SERBIAN, SUBLANG_SERBIAN_CROATIA},				// Croatia???
	{0x181, LANG_SERBIAN, SUBLANG_SERBIAN_CROATIA},				// Croatia
	{0x182, LANG_SLOVENIAN, SUBLANG_SLOVENIAN_SLOVENIA},			// Slovenia
	{0x183, LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN},	// Bosnia-Herzegovina (Latin)
	{0x184, LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC},	// Bosnia-Herzegovina (Cyrillic)
	{0x185, LANG_MACEDONIAN, SUBLANG_MACEDONIAN_MACEDONIA},			// FYR Macedonia
	{0x1A5, LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC},			// Czech Republic
	{0x1A6, LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA},				// Slovakia
	{0x1F5, LANG_ENGLISH, SUBLANG_ENGLISH_BELIZE},				// Belize
	{0x1F6, LANG_SPANISH, SUBLANG_SPANISH_GUATEMALA},			// Guatemala
	{0x1F7, LANG_SPANISH, SUBLANG_SPANISH_EL_SALVADOR},			// El Salvador
	{0x1F8, LANG_SPANISH, SUBLANG_SPANISH_HONDURAS},			// Honduras
	{0x1F9, LANG_SPANISH, SUBLANG_SPANISH_NICARAGUA},			// Nicraragua
	{0x1FA, LANG_SPANISH, SUBLANG_SPANISH_COSTA_RICA},			// Costa Rica
	{0x1FB, LANG_SPANISH, SUBLANG_SPANISH_PANAMA},				// Panama
	{0x24F, LANG_SPANISH, SUBLANG_SPANISH_BOLIVIA},				// Bolivia
//	{0x24F, LANG_QUECHUA, SUBLANG_QUECHUA_BOLIVIA},				// Bolivia
	{0x251, LANG_SPANISH, SUBLANG_SPANISH_ECUADOR},				// Ecuador
//	{0x251, LANG_QUECHUA, SUBLANG_QUECHUA_ECUADOR},				// Ecuador
	{0x253, LANG_SPANISH, SUBLANG_SPANISH_PARAGUAY},			// Paraguay
	{0x256, LANG_SPANISH, SUBLANG_SPANISH_URUGUAY},				// Uruguay
	{0x2A1, LANG_MALAY, SUBLANG_MALAY_BRUNEI_DARUSSALAM},			// Brunei Darussalam
	{0x311, LANG_ARABIC, SUBLANG_NEUTRAL},					// Arabic (Middle East/Saudi Arabia/etc.)
	{0x324, LANG_UKRAINIAN, SUBLANG_UKRAINIAN_UKRAINE},			// Ukraine
	{0x352, LANG_KOREAN, SUBLANG_KOREAN},					// North Korea
	{0x354, LANG_CHINESE, SUBLANG_CHINESE_HONGKONG},			// Hong Kong
	{0x355, LANG_CHINESE, SUBLANG_CHINESE_MACAU},				// Macao
	{0x357, LANG_KHMER, SUBLANG_KHMER_CAMBODIA},				// Cambodia
	{0x370, LANG_BENGALI, SUBLANG_BENGALI_BANGLADESH},			// Bangladesh
	{0x376, LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL},			// Taiwan (DOS 6.22+)
	{0x3C0, LANG_DIVEHI, SUBLANG_DIVEHI_MALDIVES},				// Maldives
	{0x3C1, LANG_ARABIC, SUBLANG_ARABIC_LEBANON},				// Lebanon
	{0x3C2, LANG_ARABIC, SUBLANG_ARABIC_JORDAN},				// Jordan
	{0x3C3, LANG_ARABIC, SUBLANG_ARABIC_SYRIA},				// Syrian Arab Republic
	{0x3C4, LANG_ARABIC, SUBLANG_ARABIC_IRAQ},				// Ireq
	{0x3C5, LANG_ARABIC, SUBLANG_ARABIC_KUWAIT},				// Kuwait
	{0x3C6, LANG_ARABIC, SUBLANG_ARABIC_SAUDI_ARABIA},			// Saudi Arabia
	{0x3C7, LANG_ARABIC, SUBLANG_ARABIC_YEMEN},				// Yemen
	{0x3C8, LANG_ARABIC, SUBLANG_ARABIC_OMAN},				// Oman
	{0x3CB, LANG_ARABIC, SUBLANG_ARABIC_UAE},				// United Arab Emirates
	{0x3CC, LANG_HEBREW, SUBLANG_HEBREW_ISRAEL},				// Israel (Hebrew) (DR DOS 5.0,MS-DOS 5.0+)
	{0x3CD, LANG_ARABIC, SUBLANG_ARABIC_BAHRAIN},				// Bahrain
	{0x3CE, LANG_ARABIC, SUBLANG_ARABIC_QATAR},				// Qatar
	{0x3D0, LANG_MONGOLIAN, SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA},		// Mongolia
//	{0x3D0, LANG_MONGOLIAN, SUBLANG_MONGOLIAN_PRC},				// Mongolia
	{0x3D1, LANG_NEPALI, SUBLANG_NEPALI_NEPAL},				// Nepal
	{-1, 0, 0},
};

#endif
