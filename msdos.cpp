/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#include "msdos.h"

//#define fatal_error(...) { \
//	fprintf(stderr, "error: " __VA_ARGS__); \
//	int fd = _open("dump.bin", _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE); \
//	if(fd != -1) { \
//		_write(fd, mem, /*sizeof(mem)*/0x100000); \
//		_close(fd); \
//	} \
//	exit(1); \
//}
#define fatal_error(...) { \
	fprintf(stderr, "error: " __VA_ARGS__); \
	exit(1); \
}
#define error(...) fprintf(stderr, "error: " __VA_ARGS__)

int main(int argc, char *argv[], char *envp[])
{
	if(argc < 2) {
#ifdef _WIN64
		printf("MS-DOS Player for Win32-x64 console\n\n");
#else
		printf("MS-DOS Player for Win32 console\n\n");
#endif
		printf("Usage: MSDOS (command file) [opions]\n");
		return(EXIT_FAILURE);
	}
	
	hardware_init();
	if(msdos_init(argc - 1, argv + 1, envp)) {
		return(EXIT_FAILURE);
	}
	
	CONSOLE_SCREEN_BUFFER_INFO csbi;
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
	key_buf_cnt = key_buf_set = key_buf_get = 0;
	
	timeBeginPeriod(1);
	hardware_run();
	timeEndPeriod(1);
	
	SetConsoleTextAttribute(hStdout, csbi.wAttributes);
	
	return(msdos_finish());
}

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

// process info

process_t *msdos_process_info_create(uint16 psp_seg)
{
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == 0 || process[i].psp == psp_seg) {
			memset(&process[i], 0, sizeof(process_t));
			process[i].psp = psp_seg;
			return(&process[i]);
		}
	}
	fatal_error("too many processes\n");
	return(NULL);
}

process_t *msdos_process_info_get(uint16 psp_seg)
{
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			return(&process[i]);
		}
	}
	fatal_error("invalid psp address\n");
	return(NULL);
}

// dbcs

void msdos_dbcs_table_update(int page)
{
	uint8 dbcs_data[DBCS_SIZE];
	memset(dbcs_data, 0, sizeof(dbcs_data));
	
	CPINFO info;
	GetCPInfo(page, &info);
	
	if(info.MaxCharSize != 1) {
		for(int i = 0;; i += 2) {
			uint8 lo = info.LeadByte[i + 0];
			uint8 hi = info.LeadByte[i + 1];
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
	
	code_page = page;
}

void msdos_dbcs_table_init()
{
	msdos_dbcs_table_update(_getmbcp());
}

int msdos_lead_byte_check(uint8 code)
{
	uint8 *dbcs_table = mem + DBCS_TABLE;
	
	for(int i = 0;; i += 2) {
		uint8 lo = dbcs_table[i + 0];
		uint8 hi = dbcs_table[i + 1];
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

inline int msdos_drive_number(char *path)
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

void msdos_file_handler_open(int fd, char *path, int atty, int mode, uint16 info, uint16 psp_seg)
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

void msdos_file_handler_dup(int dst, int src, uint16 psp_seg)
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

void msdos_file_handler_close(int fd, uint16 psp_seg)
{
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		psp->file_table[fd] = 0xff;
	}
	file_handler[fd].valid = 0;
}

int msdos_file_attribute_create(uint16 new_attr)
{
	int attr = 0;
	
	if(regs.w[CX] & 0x01) {
		attr |= FILE_ATTRIBUTE_READONLY;
	}
	if(regs.w[CX] & 0x02) {
		attr |= FILE_ATTRIBUTE_HIDDEN;
	}
	if(regs.w[CX] & 0x04) {
		attr |= FILE_ATTRIBUTE_SYSTEM;
	}
	if(regs.w[CX] & 0x20) {
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
	if(key_buf_cnt > 0) {
		return(1);
	} else {
		return(_kbhit());
	}
}

inline int msdos_getch_ex(int echo)
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
	if(key_buf_cnt > 0) {
		int code = key_buf[key_buf_get];
		key_buf_get++;
		key_buf_get &= KEY_BUF_MASK;
		key_buf_cnt--;
		return(code);
	} else {
		// XXX: need to consider function/cursor key
		if(echo) {
			return(_getche());
		} else {
			return(_getch());
		}
	}
}

int msdos_getch()
{
	return(msdos_getch_ex(0));
}

int msdos_getche()
{
	return(msdos_getch_ex(1));
}

int msdos_write(int fd, const void *buffer, unsigned int count)
{
	static int is_cr = 0;
	
	if(fd == 1 && !file_handler[1].atty) {
		// CR+LF -> LF
		uint8 *buf = (uint8 *)buffer;
		for(unsigned int i = 0; i < count; i++) {
			uint8 data = buf[i];
			if(is_cr) {
				if(data != 0x0a) {
					uint8 tmp = 0x0d;
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

void msdos_putch(uint8 data)
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
						if(key_buf_cnt + len <= KEY_BUF_SIZE) {
							for(int i = 0; i < len; i++) {
								key_buf[key_buf_set] = tmp[i];
								key_buf_set++;
								key_buf_set &= KEY_BUF_MASK;
							}
							key_buf_cnt += len;
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

mcb_t *msdos_mcb_create(int mcb_seg, uint8 mz, uint16 psp, uint16 paragraphs)
{
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->mz = mz;
	mcb->psp = psp;
	mcb->paragraphs = paragraphs;
	return(mcb);
}

inline void msdos_mcb_check(mcb_t *mcb)
{
	if(!(mcb->mz == 'M' || mcb->mz == 'Z')) {
		fatal_error("broken mcb\n");
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

psp_t *msdos_psp_create(int psp_seg, uint16 first_mcb, uint16 parent_psp, uint16 env_seg)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	memset(psp, 0, PSP_SIZE);
	psp->exit[0] = 0xcd;
	psp->exit[1] = 0x20;
	psp->first_mcb = first_mcb;
	psp->far_call = 0xea;
	psp->cpm_entry.w.l = 0xfff1;	// int 21h, retf
	psp->cpm_entry.w.h = 0xf000;
	psp->int_22h.dw = *(uint32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(uint32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(uint32 *)(mem + 4 * 0x24);
	psp->parent_psp = parent_psp;
	for(int i = 0; i < 20; i++) {
		if(file_handler[i].valid) {
			psp->file_table[i] = i;
		} else {
			psp->file_table[i] = 0xff;
		}
	}
	psp->env_seg = env_seg;
	psp->stack.w.l = regs.w[SP];
	psp->stack.w.h = sregs[SS];
	psp->service[0] = 0xcd;
	psp->service[1] = 0x21;
	psp->service[2] = 0xcb;
	return(psp);
}

int msdos_process_exec(char *cmd, param_block_t *param, uint8 al)
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
	uint16 cs, ss, ip, sp;
	
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
			int ofs = *(uint16 *)(file_buffer + header->relocation_table + i * 4 + 0);
			int seg = *(uint16 *)(file_buffer + header->relocation_table + i * 4 + 2);
			*(uint16 *)(file_buffer + header_size + (seg << 4) + ofs) += start_seg;
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
	*(uint16 *)(mem + 4 * 0x22 + 0) = PC - base[CS];
	*(uint16 *)(mem + 4 * 0x22 + 2) = sregs[CS];
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
	process->find_handle = INVALID_HANDLE_VALUE;
	
	current_psp = psp_seg;
	
	if(al == 0x00) {
		*(uint16 *)(mem + (ss << 4) + sp) = 0;
		PC = ((cs << 4) + ip) & AMASK;
		
		// registers and segments
		regs.w[AX] = regs.w[BX] = 0x00;
		regs.w[CX] = 0xff;
		regs.w[DX] = psp_seg;
		regs.w[SI] = ip;
		regs.w[DI] = sp;
		regs.w[SP] = sp;
		sregs[DS] = sregs[ES] = psp_seg;
		sregs[CS] = cs;
		sregs[SS] = ss;
		base[DS] = SegBase(DS);
		base[ES] = SegBase(ES);
		base[CS] = SegBase(CS);
		base[SS] = SegBase(SS);
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
	
	*(uint32 *)(mem + 4 * 0x22) = psp->int_22h.dw;
	*(uint32 *)(mem + 4 * 0x23) = psp->int_23h.dw;
	*(uint32 *)(mem + 4 * 0x24) = psp->int_24h.dw;
	
	sregs[CS] = psp->int_22h.w.h;
	base[CS] = SegBase(CS);
	PC = (base[CS] + psp->int_22h.w.l) & AMASK;
	sregs[SS] = psp->stack.w.h;
	base[SS] = SegBase(SS);
	regs.w[SP] = psp->stack.w.l;
	
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
		
		process_t *process = msdos_process_info_get(psp_seg);
		if(process->find_handle != INVALID_HANDLE_VALUE) {
			FindClose(process->find_handle);
			process->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	
	process_t *process = msdos_process_info_get(psp_seg);
	memset(process, 0, sizeof(process_t));
	
	current_psp = psp->parent_psp;
	retval = ret;
}

// drive

int msdos_drive_param_block_update(int drive_num, uint16 *seg, uint16 *ofs, int force_update)
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
			dpb->bytes_per_sector = (uint16)geo.BytesPerSector;
			dpb->highest_sector_num = (uint8)(geo.SectorsPerTrack - 1);
			dpb->highest_cluster_num = (uint16)(geo.TracksPerCylinder * geo.Cylinders.QuadPart) + 1;
			dpb->media_type = geo.MediaType;
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

inline void pcbios_int_15h_87h()
{
#ifdef HAS_I286
	// copy extended memory (from DOSBox)
	int len = regs.w[CX] * 2;
	int ofs = base[ES] + regs.w[SI];
	int src = (*(uint32 *)(mem + ofs + 0x12) & 0xffffff); // + (mem[ofs + 0x16] << 24);
	int dst = (*(uint32 *)(mem + ofs + 0x1a) & 0xffffff); // + (mem[ofs + 0x1e] << 24);
	memcpy(mem + dst, mem + src, len);
	regs.w[AX] = 0x00;
#else
	regs.b[AH] = 0x86;
	CarryVal = 1;
#endif
}

inline void pcbios_int_15h_88h()
{
	regs.w[AX] = ((MAX_MEM - 0x100000) >> 10);
}

inline void pcbios_int_15h_89h()
{
#ifdef HAS_I286
	// switch to protected mode (from DOSBox)
	OUT8(0x20, 0x10);
	OUT8(0x21, regs.b[BH]);
	OUT8(0x21, 0x00);
	OUT8(0xa0, 0x10);
	OUT8(0xa1, regs.b[BL]);
	OUT8(0xa1, 0x00);
	AMASK = 0xffffff;
	int ofs = base[ES] + regs.w[SI];
	gdtr_limit = *(uint16 *)(mem + ofs + 0x08);
	gdtr_base = *(uint32 *)(mem + ofs + 0x08 + 0x02) & 0xffffff;
	idtr_limit = *(uint16 *)(mem + ofs + 0x10);
	idtr_base = *(uint32 *)(mem + ofs + 0x10 + 0x02) & 0xffffff;
	msw |= 1;
	sregs[DS] = 0x18;
	sregs[ES] = 0x20;
	sregs[SS] = 0x28;
	base[DS] = SegBase(DS);
	base[ES] = SegBase(ES);
	base[SS] = SegBase(SS);
	regs.w[SP] += 6;
	flags = 2;
	ExpandFlags(flags);
	regs.w[AX] = 0x00;
	i286_code_descriptor(0x30, regs.w[CX]);
#else
	regs.b[AH] = 0x86;
	CarryVal = 1;
#endif
}

// msdos system call

inline void msdos_int_21h_00h()
{
	msdos_process_terminate(sregs[CS], retval, 1);
}

inline void msdos_int_21h_01h()
{
	regs.b[AL] = msdos_getche();
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_02h()
{
	msdos_putch(regs.b[DL]);
}

inline void msdos_int_21h_03h()
{
	regs.b[AL] = msdos_aux_in();
}

inline void msdos_int_21h_04h()
{
	msdos_aux_out(regs.b[DL]);
}

inline void msdos_int_21h_05h()
{
	msdos_prn_out(regs.b[DL]);
}

inline void msdos_int_21h_06h()
{
	if(regs.b[DL] == 0xff) {
		if(msdos_kbhit()) {
			regs.b[AL] = msdos_getch();
			ZeroVal = 0;
		} else {
			regs.b[AL] = 0;
			ZeroVal = 1;
		}
		Sleep(0);
	} else {
		msdos_putch(regs.b[DL]);
	}
}

inline void msdos_int_21h_07h()
{
	regs.b[AL] = msdos_getch();
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_08h()
{
	regs.b[AL] = msdos_getch();
#ifdef SUPPORT_HARDWARE
	hardware_update();
#endif
}

inline void msdos_int_21h_09h()
{
	char tmp[0x10000];
	memcpy(tmp, mem + base[DS] + regs.w[DX], sizeof(tmp));
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
	int ofs = base[DS] + regs.w[DX];
	int max = mem[ofs] - 1;
	uint8 *buf = mem + ofs + 2;
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
	regs.b[AL] = msdos_kbhit() ? 0xff : 0x00;
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
	
	switch(regs.b[AL]) {
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
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_0dh()
{
}

inline void msdos_int_21h_0eh()
{
	_chdrive(regs.b[DL] + 1);
	regs.b[AL] = 26; // zdrive
}

inline void msdos_int_21h_18h()
{
	regs.b[AL] = 0x00;
}

inline void msdos_int_21h_19h()
{
	regs.b[AL] = _getdrive() - 1;
}

inline void msdos_int_21h_1ah()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	process->dta.w.l = regs.w[DX];
	process->dta.w.h = sregs[DS];
}

inline void msdos_int_21h_1fh()
{
	int drive_num = _getdrive() - 1;
	uint16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		regs.b[AL] = 0;
		sregs[DS] = seg;
		base[DS] = SegBase(DS);
		regs.w[BX] = ofs;
	} else {
		regs.b[AL] = 0xff;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_25h()
{
	*(uint16 *)(mem + 4 * regs.b[AL] + 0) = regs.w[DX];
	*(uint16 *)(mem + 4 * regs.b[AL] + 2) = sregs[DS];
}

inline void msdos_int_21h_26h()
{
	psp_t *psp = (psp_t *)(mem + (regs.w[DX] << 4));
	
	memcpy(mem + (regs.w[DX] << 4), mem + (current_psp << 4), sizeof(psp_t));
	psp->first_mcb = regs.w[DX] + 16;
	psp->int_22h.dw = *(uint32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(uint32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(uint32 *)(mem + 4 * 0x24);
	psp->parent_psp = 0;
}

inline void msdos_int_21h_29h()
{
	int ofs = base[DS] + regs.w[SI];
	char name[MAX_PATH], ext[MAX_PATH];
	uint8 drv = 0;
	char sep_chars[] = ":.;,=+";
	char end_chars[] = "\\<>|/\"[]";
	char spc_chars[] = " \t";
	
	if(regs.b[AL] & 1) {
		ofs += strspn((char *)&mem[ofs], spc_chars);
		if(strchr(sep_chars, mem[ofs]) && mem[ofs] != '\0') {
			ofs++;
		}
	}
	ofs += strspn((char *)&mem[ofs], spc_chars);
	
	if(mem[ofs + 1] == ':') {
		uint8 c = mem[ofs];
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
		uint8 c = mem[ofs];
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
			uint8 c = mem[ofs];
			if(c <= 0x20 || strchr(end_chars, c) || strchr(sep_chars, c)) {
				break;
			} else if(c >= 'a' && c <= 'z') {
				c -= 0x20;
			}
			ofs++;
			ext[i] = c;
		}
	}
	int si = ofs - base[DS];
	int ds = sregs[DS];
	while(si > 0xffff) {
		si -= 0x10;
		ds++;
	}
	regs.w[SI] = si;
	sregs[DS] = ds;
	base[DS] = SegBase(DS);
	
	uint8 *fcb = mem + base[ES] + regs.w[DI];
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
	
	regs.b[AL] = 0x00;
	if(drv == 0 || (drv > 0 && drv <= 26 && (GetLogicalDrives() & ( 1 << (drv - 1) )))) {
		if(memchr(fcb + 1, '?', 8 + 3)) {
			regs.b[AL] = 0x01;
		}
	} else {
		regs.b[AL] = 0xff;
	}
}

inline void msdos_int_21h_2ah()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	regs.w[CX] = sTime.wYear;
	regs.b[DH] = (uint8)sTime.wMonth;
	regs.b[DL] = (uint8)sTime.wDay;
	regs.b[AL] = (uint8)sTime.wDayOfWeek;
}

inline void msdos_int_21h_2bh()
{
	regs.b[AL] = 0x00;
}

inline void msdos_int_21h_2ch()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	regs.b[CH] = (uint8)sTime.wHour;
	regs.b[CL] = (uint8)sTime.wMinute;
	regs.b[DH] = (uint8)sTime.wSecond;
}

inline void msdos_int_21h_2dh()
{
	regs.b[AL] = 0x00;
}

inline void msdos_int_21h_2eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	process->verify = regs.b[AL];
}

inline void msdos_int_21h_2fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	regs.w[BX] = process->dta.w.l;
	sregs[ES] = process->dta.w.h;
	base[ES] = SegBase(ES);
}

inline void msdos_int_21h_30h()
{
	// Version Flag / OEM
	if(regs.b[AL] == 1) {
		regs.b[BH] = 0x00;	// not in ROM
	} else {
		regs.b[BH] = 0xff;	// OEM = Microsoft
	}
	// MS-DOS version (7.00)
	regs.b[AL] = 7;
	regs.b[AH] = 0;
}

inline void msdos_int_21h_31h()
{
	int mcb_seg = current_psp - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->paragraphs = regs.w[DX];
	mcb_seg += mcb->paragraphs + 1;
	msdos_mcb_create(mcb_seg, 'Z', 0, (MEMORY_END >> 4) - mcb_seg - 1);
	
	msdos_process_terminate(current_psp, regs.b[AL], 0);
}

inline void msdos_int_21h_32h()
{
	int drive_num = (regs.b[DL] == 0) ? (_getdrive() - 1) : (regs.b[DL] - 1);
	uint16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		regs.b[AL] = 0;
		sregs[DS] = seg;
		base[DS] = SegBase(DS);
		regs.w[BX] = ofs;
	} else {
		regs.b[AL] = 0xff;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_33h()
{
	char path[MAX_PATH];;
	
	switch(regs.b[AL]) {
	case 0x00:
		regs.b[DL] = 0x00;
		break;
	case 0x01:
		break;
	case 0x05:
		GetSystemDirectory(path, MAX_PATH);
		if(path[0] >= 'a' && path[0] <= 'z') {
			regs.b[DL] = path[0] - 'a' + 1;
		} else {
			regs.b[DL] = path[0] - 'A' + 1;
		}
		break;
	case 0x06:
		// MS-DOS version (7.00)
		regs.b[BL] = 7;
		regs.b[BH] = 0;
		regs.b[DL] = 0;
		regs.b[DH] = 0x10; // in HMA
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_35h()
{
	regs.w[BX] = *(uint16 *)(mem + 4 * regs.b[AL] + 0);
	sregs[ES] = *(uint16 *)(mem + 4 * regs.b[AL] + 2);
	base[ES] = SegBase(ES);
}

inline void msdos_int_21h_36h()
{
	struct _diskfree_t df = {0};
	
	if(_getdiskfree(regs.b[DL], &df) == 0) {
		regs.w[AX] = (uint16)df.sectors_per_cluster;
		regs.w[CX] = (uint16)df.bytes_per_sector;
		regs.w[BX] = (uint16)df.avail_clusters;
		regs.w[DX] = (uint16)df.total_clusters;
	} else {
		regs.w[AX] = 0xffff;
	}
}

inline void msdos_int_21h_37h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	switch(regs.b[AL]) {
	case 0x00:
		regs.b[AL] = 0x00;
		regs.b[DL] = process->switchar;
		break;
	case 0x01:
		regs.b[AL] = 0x00;
		process->switchar = regs.b[DL];
		break;
	default:
		regs.w[AX] = 1;
		break;
	}
}

inline void msdos_int_21h_39h(int lfn)
{
	if(_mkdir(msdos_trimmed_path((char *)(mem + base[DS] + regs.w[DX]), lfn))) {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_3ah(int lfn)
{
	if(_rmdir(msdos_trimmed_path((char *)(mem + base[DS] + regs.w[DX]), lfn))) {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_3bh(int lfn)
{
	if(_chdir(msdos_trimmed_path((char *)(mem + base[DS] + regs.w[DX]), lfn))) {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_3ch()
{
	char *path = msdos_local_file_path((char *)(mem + base[DS] + regs.w[DX]), 0);
	int attr = GetFileAttributes(path);
	int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
	
	if(fd != -1) {
		if(attr == -1) {
			attr = msdos_file_attribute_create(regs.w[CX]) & ~FILE_ATTRIBUTE_READONLY;
		}
		SetFileAttributes(path, attr);
		regs.w[AX] = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
	} else {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_3dh()
{
	char *path = msdos_local_file_path((char *)(mem + base[DS] + regs.w[DX]), 0);
	int mode = regs.b[AL] & 0x03;
	
	if(mode < 0x03) {
		int fd = _open(path, file_mode[mode].mode);
		
		if(fd != -1) {
			regs.w[AX] = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), mode, msdos_drive_number(path), current_psp);
		} else {
			regs.w[AX] = errno;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x0c;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_3eh()
{
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		_close(regs.w[BX]);
		msdos_file_handler_close(regs.w[BX], current_psp);
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_3fh()
{
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		if(file_mode[file_handler[regs.w[BX]].mode].in) {
			if(file_handler[regs.w[BX]].atty) {
				// regs.b[BX] is stdin or is redirected to stdin
				uint8 *buf = mem + base[DS] + regs.w[DX];
				int max = regs.w[CX];
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
				regs.w[AX] = p;
#ifdef SUPPORT_HARDWARE
				hardware_update();
#endif
			} else {
				regs.w[AX] = _read(regs.w[BX], mem + base[DS] + regs.w[DX], regs.w[CX]);
			}
		} else {
			regs.w[AX] = 0x05;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_40h()
{
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		if(file_mode[file_handler[regs.w[BX]].mode].out) {
			if(regs.w[CX]) {
				if(file_handler[regs.w[BX]].atty) {
					// regs.b[BX] is stdout/stderr or is redirected to stdout
					for(int i = 0; i < regs.w[CX]; i++) {
						msdos_putch(mem[base[DS] + regs.w[DX] + i]);
					}
					regs.w[AX] = regs.w[CX];
				} else {
					regs.w[AX] = msdos_write(regs.w[BX], mem + base[DS] + regs.w[DX], regs.w[CX]);
				}
			} else {
				uint32 pos = _tell(regs.w[BX]);
				_lseek(regs.w[BX], 0, SEEK_END);
				uint32 size = _tell(regs.w[BX]);
				regs.w[AX] = 0;
				for(uint32 i = size; i < pos; i++) {
					uint8 tmp = 0;
					regs.w[AX] += msdos_write(regs.w[BX], &tmp, 1);
				}
				_lseek(regs.w[BX], pos, SEEK_SET);
			}
		} else {
			regs.w[AX] = 0x05;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_41h(int lfn)
{
	if(remove(msdos_trimmed_path((char *)(mem + base[DS] + regs.w[DX]), lfn))) {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_42h()
{
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		if(regs.b[AL] < 0x03) {
			static int ptrname[] = { SEEK_SET, SEEK_CUR, SEEK_END };
			_lseek(regs.w[BX], (regs.w[CX] << 16) | regs.w[DX], ptrname[regs.b[AL]]);
			uint32 pos = _tell(regs.w[BX]);
			regs.w[AX] = pos & 0xffff;
			regs.w[DX] = (pos >> 16);
		} else {
			regs.w[AX] = 0x01;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_43h(int lfn)
{
	char *path = msdos_local_file_path((char *)(mem + base[DS] + regs.w[DX]), lfn);
	int attr;
	
	switch(regs.b[AL]) {
	case 0x00:
		if((attr = GetFileAttributes(path)) != -1) {
			regs.w[CX] = 0;
			if(attr & FILE_ATTRIBUTE_READONLY) {
				regs.w[CX] |= 0x01;
			}
			if(attr & FILE_ATTRIBUTE_HIDDEN) {
				regs.w[CX] |= 0x02;
			}
			if(attr & FILE_ATTRIBUTE_SYSTEM) {
				regs.w[CX] |= 0x04;
			}
			if(attr & FILE_ATTRIBUTE_ARCHIVE) {
				regs.w[CX] |= 0x20;
			}
		} else {
			regs.w[AX] = (uint16)GetLastError();
			CarryVal = 1;
		}
		break;
	case 0x01:
		if(SetFileAttributes(path, msdos_file_attribute_create(regs.w[CX])) != 0) {
			regs.w[AX] = (uint16)GetLastError();
			CarryVal = 1;
		}
		break;
	case 0x02:
		break;
	case 0x03:
		regs.w[CX] = 0x00;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_44h()
{
	switch(regs.b[AL]) {
	case 0x00: // get ioctrl data
		regs.w[DX] = file_handler[regs.w[BX]].info;
		break;
	case 0x01: // set ioctrl data
		file_handler[regs.w[BX]].info |= regs.b[DL];
		break;
	case 0x02: // recv from character device
	case 0x03: // send to character device
		regs.w[AX] = 0x06;
		CarryVal = 1;
		break;
	case 0x04: // recv from block device
	case 0x05: // send to block device
		regs.w[AX] = 0x05;
		CarryVal = 1;
		break;
	case 0x06: // get read status
		if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
			if(file_mode[file_handler[regs.w[BX]].mode].in) {
				if(file_handler[regs.w[BX]].atty) {
					regs.b[AL] = msdos_kbhit() ? 0xff : 0x00;
				} else {
					regs.b[AL] = eof(regs.w[BX]) ? 0x00 : 0xff;
				}
			} else {
				regs.b[AL] = 0x00;
			}
		} else {
			regs.w[AX] = 0x06;
			CarryVal = 1;
		}
		break;
	case 0x07: // get write status
		if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
			if(file_mode[file_handler[regs.w[BX]].mode].out) {
				regs.b[AL] = 0xff;
			} else {
				regs.b[AL] = 0x00;
			}
		} else {
			regs.w[AX] = 0x06;
			CarryVal = 1;
		}
		break;
	case 0x08: // check removable drive
		if(regs.b[BL] < ('Z' - 'A' + 1)) {
			uint32 val;
			if(regs.b[BL] == 0) {
				val = GetDriveType(NULL);
			} else if(regs.b[BL] < ('Z' - 'A' + 1)) {
				char tmp[8];
				sprintf(tmp, "%c:\\", 'A' + regs.b[BL] - 1);
				val = GetDriveType(tmp);
			}
			if(val == DRIVE_NO_ROOT_DIR) {
				// no drive
				regs.w[AX] = 0x0f;
				CarryVal = 1;
			} else if(val == DRIVE_REMOVABLE || val == DRIVE_CDROM) {
				// removable drive
				regs.w[AX] = 0x00;
			} else {
				// fixed drive
				regs.w[AX] = 0x01;
			}
		} else {
			// invalid drive number
			regs.w[AX] = 0x0f;
			CarryVal = 1;
		}
		break;
	case 0x09: // check remote drive
		if(regs.b[BL] < ('Z' - 'A' + 1)) {
			uint32 val;
			if(regs.b[BL] == 0) {
				val = GetDriveType(NULL);
			} else if(regs.b[BL] < ('Z' - 'A' + 1)) {
				char tmp[8];
				sprintf(tmp, "%c:\\", 'A' + regs.b[BL] - 1);
				val = GetDriveType(tmp);
			}
			if(val == DRIVE_NO_ROOT_DIR) {
				// no drive
				regs.w[AX] = 0x0f;
				CarryVal = 1;
			} else if(val == DRIVE_REMOTE) {
				// remote drive
				regs.w[DX] = 0x1000;
			} else {
				// local drive
				regs.w[DX] = 0x00;
			}
		} else {
			// invalid drive number
			regs.w[AX] = 0x0f;
			CarryVal = 1;
		}
		break;
	case 0x0b: // set retry count
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_45h()
{
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		int fd = _dup(regs.w[BX]);
		if(fd != -1) {
			regs.w[AX] = fd;
			msdos_file_handler_dup(regs.w[AX], regs.w[BX], current_psp);
		} else {
			regs.w[AX] = errno;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_46h()
{
	if(regs.w[BX] < MAX_FILES && regs.w[CX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		if(_dup2(regs.w[BX], regs.w[CX]) != -1) {
			msdos_file_handler_dup(regs.w[CX], regs.w[BX], current_psp);
		} else {
			regs.w[AX] = errno;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_47h(int lfn)
{
	char path[MAX_PATH];
	
	if(_getdcwd(regs.b[DL], path, MAX_PATH) != NULL) {
		if(path[1] == ':') {
			// the returned path does not include a drive or the initial backslash
			strcpy((char *)(mem + base[DS] + regs.w[SI]), (lfn ? path : msdos_short_path(path)) + 3);
		} else {
			strcpy((char *)(mem + base[DS] + regs.w[SI]), lfn ? path : msdos_short_path(path));
		}
	} else {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_48h()
{
	int seg;
	
	if((seg = msdos_mem_alloc(regs.w[BX], 0)) != -1) {
		regs.w[AX] = seg;
	} else {
		regs.w[AX] = 0x08;
		regs.w[BX] = msdos_mem_get_free(0);
		CarryVal = 1;
	}
}

inline void msdos_int_21h_49h()
{
	msdos_mem_free(sregs[ES]);
}

inline void msdos_int_21h_4ah()
{
	int max_paragraphs;
	
	if(msdos_mem_realloc(sregs[ES], regs.w[BX], &max_paragraphs)) {
		regs.w[AX] = 0x08;
		regs.w[BX] = max_paragraphs;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_4bh()
{
	char *command = (char *)(mem + base[DS] + regs.w[DX]);
	param_block_t *param = (param_block_t *)(mem + base[ES] + regs.w[BX]);
	
	switch(regs.b[AL]) {
	case 0x00:
	case 0x01:
		if(msdos_process_exec(command, param, regs.b[AL])) {
			regs.w[AX] = 0x02;
			CarryVal = 1;
		}
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_4ch()
{
	msdos_process_terminate(current_psp, regs.b[AL], 1);
}

inline void msdos_int_21h_4dh()
{
	regs.w[AX] = retval;
}

inline void msdos_int_21h_4eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_t *find = (find_t *)(mem + (process->dta.w.h << 4) + process->dta.w.l);
	char *path = msdos_trimmed_path((char *)(mem + base[DS] + regs.w[DX]), 0);
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(process->find_handle);
		process->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	process->allowable_mask = regs.b[CL];
	
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
		find->attrib = (uint8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_path(fd.cFileName));
	} else if(process->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		process->allowable_mask &= ~8;
	} else {
		regs.w[AX] = 0x12;	// NOTE: return 0x02 if file path is invalid
		CarryVal = 1;
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
		find->attrib = (uint8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_path(fd.cFileName));
	} else if(process->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		process->allowable_mask &= ~8;
	} else {
		regs.w[AX] = 0x12;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_50h()
{
	current_psp = regs.w[BX];
}

inline void msdos_int_21h_51h()
{
	regs.w[BX] = current_psp;
}

inline void msdos_int_21h_54h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	regs.b[AL] = process->verify;
}

inline void msdos_int_21h_55h()
{
	psp_t *psp = (psp_t *)(mem + (regs.w[DX] << 4));
	
	memcpy(mem + (regs.w[DX] << 4), mem + (current_psp << 4), sizeof(psp_t));
	psp->int_22h.dw = *(uint32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(uint32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(uint32 *)(mem + 4 * 0x24);
	psp->parent_psp = current_psp;
}

inline void msdos_int_21h_56h(int lfn)
{
	char src[MAX_PATH], dst[MAX_PATH];
	strcpy(src, msdos_trimmed_path((char *)(mem + base[DS] + regs.w[DX]), lfn));
	strcpy(dst, msdos_trimmed_path((char *)(mem + base[ES] + regs.w[DI]), lfn));
	
	if(rename(src, dst)) {
		regs.w[AX] = errno;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_57h()
{
	FILETIME time, local;
	
	switch(regs.b[AL]) {
	case 0x00:
		if(GetFileTime((HANDLE)_get_osfhandle(regs.w[BX]), NULL, NULL, &time)) {
			FileTimeToLocalFileTime(&time, &local);
			FileTimeToDosDateTime(&local, &regs.w[DX], &regs.w[CX]);
		} else {
			regs.w[AX] = (uint16)GetLastError();
			CarryVal = 1;
		}
		break;
	case 0x01:
		DosDateTimeToFileTime(regs.w[DX], regs.w[CX], &local);
		LocalFileTimeToFileTime(&local, &time);
		if(!SetFileTime((HANDLE)_get_osfhandle(regs.w[BX]), NULL, NULL, &time)) {
			regs.w[AX] = (uint16)GetLastError();
			CarryVal = 1;
		}
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_5ah()
{
	char *path = (char *)(mem + base[DS] + regs.w[DX]);
	int len = strlen(path);
	char tmp[MAX_PATH];
	
	if(GetTempFileName(path, "TMP", 0, tmp)) {
		int fd = _open(tmp, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		
		SetFileAttributes(tmp, msdos_file_attribute_create(regs.w[CX]) & ~FILE_ATTRIBUTE_READONLY);
		regs.w[AX] = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
		
		strcpy(path, tmp);
		int dx = regs.w[DX] + len;
		int ds = sregs[DS];
		while(dx > 0xffff) {
			dx -= 0x10;
			ds++;
		}
		regs.w[DX] = dx;
		sregs[DS] = ds;
		base[DS] = SegBase(DS);
	} else {
		regs.w[AX] = (uint16)GetLastError();
		CarryVal = 1;
	}
}

inline void msdos_int_21h_5bh()
{
	char *path = msdos_local_file_path((char *)(mem + base[DS] + regs.w[DX]), 0);
	
	if(_access(path, 0) == 0) {
		// already exists
		regs.w[AX] = 0x50;
		CarryVal = 1;
	} else {
		int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		
		if(fd != -1) {
			SetFileAttributes(path, msdos_file_attribute_create(regs.w[CX]) & ~FILE_ATTRIBUTE_READONLY);
			regs.w[AX] = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
		} else {
			regs.w[AX] = errno;
			CarryVal = 1;
		}
	}
}

inline void msdos_int_21h_5ch()
{
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		if(regs.b[AL] == 0 || regs.b[AL] == 1) {
			static int modes[2] = {_LK_LOCK, _LK_UNLCK};
			uint32 pos = _tell(regs.w[BX]);
			_lseek(regs.w[BX], (regs.w[CX] << 16) | regs.w[DX], SEEK_SET);
			if(_locking(regs.w[BX], modes[regs.b[AL]], (regs.w[SI] << 16) | regs.w[DI])) {
				regs.w[AX] = errno;
				CarryVal = 1;
			}
			_lseek(regs.w[BX], pos, SEEK_SET);
#ifdef SUPPORT_HARDWARE
			// some seconds may be passed in _locking()
			hardware_update();
#endif
		} else {
			regs.w[AX] = 0x01;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_60h(int lfn)
{
	if(lfn) {
		char full[MAX_PATH], *name;
		GetFullPathName((char *)(mem + base[DS] + regs.w[SI]), MAX_PATH, full, &name);
		strcpy((char *)(mem + base[ES] + regs.w[DI]), full);
	} else {
		strcpy((char *)(mem + base[ES] + regs.w[DI]), msdos_short_full_path((char *)(mem + base[DS] + regs.w[SI])));
	}
}

inline void msdos_int_21h_62h()
{
	regs.w[BX] = current_psp;
}

inline void msdos_int_21h_63h()
{
	switch(regs.b[AL]) {
	case 0x00:
		sregs[DS] = (DBCS_TABLE >> 4);
		base[DS] = SegBase(DS);
		regs.w[SI] = (DBCS_TABLE & 0x0f);
		regs.b[AL] = 0x00;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_65h()
{
	char tmp[0x10000];
	
	switch(regs.b[AL]) {
	case 0x07:
		*(uint8  *)(mem + base[ES] + regs.w[DI] + 0) = 0x07;
		*(uint16 *)(mem + base[ES] + regs.w[DI] + 1) = (DBCS_TOP & 0x0f);
		*(uint16 *)(mem + base[ES] + regs.w[DI] + 3) = (DBCS_TOP >> 4);
		regs.w[CX] = 0x05;
		break;
	case 0x20:
		sprintf(tmp, "%c", regs.b[DL]);
		msdos_strupr(tmp);
		regs.b[DL] = tmp[0];
		break;
	case 0x21:
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, mem + base[DS] + regs.w[DX], regs.w[CX]);
		msdos_strupr(tmp);
		memcpy(mem + base[DS] + regs.w[DX], tmp, regs.w[CX]);
		break;
	case 0x22:
		msdos_strupr((char *)(mem + base[DS] + regs.w[DX]));
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_6ch(int lfn)
{
	char *path = msdos_local_file_path((char *)(mem + base[DS] + regs.w[SI]), lfn);
	int mode = regs.b[BL] & 0x03;
	
	if(mode < 0x03) {
		if(_access(path, 0) == 0) {
			// file exists
			if(regs.b[DL] & 1) {
				int fd = _open(path, file_mode[mode].mode);
				
				if(fd != -1) {
					regs.w[AX] = fd;
					regs.w[CX] = 1;
					msdos_file_handler_open(fd, path, _isatty(fd), mode, msdos_drive_number(path), current_psp);
				} else {
					regs.w[AX] = errno;
					CarryVal = 1;
				}
			} else if(regs.b[DL] & 2) {
				int attr = GetFileAttributes(path);
				int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
				
				if(fd != -1) {
					if(attr == -1) {
						attr = msdos_file_attribute_create(regs.w[CX]) & ~FILE_ATTRIBUTE_READONLY;
					}
					SetFileAttributes(path, attr);
					regs.w[AX] = fd;
					regs.w[CX] = 3;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
				} else {
					regs.w[AX] = errno;
					CarryVal = 1;
				}
			} else {
				regs.w[AX] = 0x50;
				CarryVal = 1;
			}
		} else {
			// file not exists
			if(regs.b[DL] & 0x10) {
				int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
				
				if(fd != -1) {
					SetFileAttributes(path, msdos_file_attribute_create(regs.w[CX]) & ~FILE_ATTRIBUTE_READONLY);
					regs.w[AX] = fd;
					regs.w[CX] = 2;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
				} else {
					regs.w[AX] = errno;
					CarryVal = 1;
				}
			} else {
				regs.w[AX] = 0x02;
				CarryVal = 1;
			}
		}
	} else {
		regs.w[AX] = 0x0c;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_710dh()
{
	// reset drive
}

inline void msdos_int_21h_714eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + base[ES] + regs.w[DI]);
	char *path = (char *)(mem + base[DS] + regs.w[DX]);
	WIN32_FIND_DATA fd;
	
	if(process->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(process->find_handle);
		process->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	process->allowable_mask = regs.b[CL];
	process->required_mask = regs.b[CH];
	
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
		if(regs.b[SI] == 0) {
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
		regs.w[AX] = 0x12;	// NOTE: return 0x02 if file path is invalid
		CarryVal = 1;
	}
}

inline void msdos_int_21h_714fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + base[ES] + regs.w[DI]);
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
		if(regs.b[SI] == 0) {
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
		regs.w[AX] = 0x12;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_71a0h()
{
	DWORD max_component_len, file_sys_flag;
	
	if(GetVolumeInformation((char *)(mem + base[DS] + regs.w[DX]), NULL, 0, NULL, &max_component_len, &file_sys_flag, (char *)(mem + base[ES] + regs.w[DI]), regs.w[CX])) {
		regs.w[BX] = (uint16)file_sys_flag;
		regs.w[CX] = (uint16)max_component_len;		// 255
		regs.w[DX] = (uint16)max_component_len + 5;	// 260
	} else {
		regs.w[AX] = (uint16)GetLastError();
		CarryVal = 1;
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
	uint8 *buffer = (uint8 *)(mem + base[DS] + regs.w[DX]);
	struct _stat64 status;
	DWORD serial_number = 0;
	
	if(regs.w[BX] < MAX_FILES && file_handler[regs.w[BX]].valid) {
		if(_fstat64(regs.w[BX], &status) == 0) {
			if(file_handler[regs.w[BX]].path[1] == ':') {
				// NOTE: we need to consider the network file path "\\host\share\"
				char volume[] = "A:\\";
				volume[0] = file_handler[regs.w[BX]].path[1];
				GetVolumeInformation(volume, NULL, 0, &serial_number, NULL, NULL, NULL, 0);
			}
			*(uint32 *)(buffer + 0x00) = GetFileAttributes(file_handler[regs.w[BX]].path);
			*(uint32 *)(buffer + 0x04) = (uint32)(status.st_ctime & 0xffffffff);
			*(uint32 *)(buffer + 0x08) = (uint32)((status.st_ctime >> 32) & 0xffffffff);
			*(uint32 *)(buffer + 0x0c) = (uint32)(status.st_atime & 0xffffffff);
			*(uint32 *)(buffer + 0x10) = (uint32)((status.st_atime >> 32) & 0xffffffff);
			*(uint32 *)(buffer + 0x14) = (uint32)(status.st_mtime & 0xffffffff);
			*(uint32 *)(buffer + 0x18) = (uint32)((status.st_mtime >> 32) & 0xffffffff);
			*(uint32 *)(buffer + 0x1c) = serial_number;
			*(uint32 *)(buffer + 0x20) = (uint32)((status.st_size >> 32) & 0xffffffff);
			*(uint32 *)(buffer + 0x24) = (uint32)(status.st_size & 0xffffffff);
			*(uint32 *)(buffer + 0x28) = status.st_nlink;
			// this is dummy id and it will be changed when it is reopend...
			*(uint32 *)(buffer + 0x2c) = 0;
			*(uint32 *)(buffer + 0x30) = file_handler[regs.w[BX]].id;
		} else {
			regs.w[AX] = errno;
			CarryVal = 1;
		}
	} else {
		regs.w[AX] = 0x06;
		CarryVal = 1;
	}
}

inline void msdos_int_21h_71a7h()
{
	switch(regs.b[BL]) {
	case 0x00:
		if(!FileTimeToDosDateTime((FILETIME *)(mem + base[DS] + regs.w[SI]), &regs.w[DX], &regs.w[CX])) {
			regs.w[AX] = (uint16)GetLastError();
			CarryVal = 1;
		}
		break;
	case 0x01:
		// NOTE: we need to check BH that shows 10-millisecond untils past time in CX
		if(!DosDateTimeToFileTime(regs.w[DX], regs.w[CX], (FILETIME *)(mem + base[ES] + regs.w[DI]))) {
			regs.w[AX] = (uint16)GetLastError();
			CarryVal = 1;
		}
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_21h_71a8h()
{
	if(regs.b[DH] == 0) {
		char tmp[MAX_PATH], fcb[MAX_PATH];
		strcpy(tmp, msdos_short_path((char *)(mem + base[DS] + regs.w[SI])));
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
		memcpy((char *)(mem + base[ES] + regs.w[DI]), fcb, 11);
	} else {
		strcpy((char *)(mem + base[ES] + regs.w[DI]), msdos_short_path((char *)(mem + base[DS] + regs.w[SI])));
	}
}

inline void msdos_int_25h()
{
	uint16 seg, ofs;
	
	_pushf();
	
	if(!(regs.b[AL] < 26)) {
		regs.b[AL] = 0x01; // unit unknown
		CarryVal = 1;
	} else if(!msdos_drive_param_block_update(regs.b[AL], &seg, &ofs, 0)) {
		regs.b[AL] = 0x02; // drive not ready
		CarryVal = 1;
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + regs.b[AL]);
		
		HANDLE hFile = CreateFile(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			regs.b[AL] = 0x02; // drive not ready
			CarryVal = 1;
		} else {
			if(SetFilePointer(hFile, regs.w[DX] * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				regs.b[AL] = 0x08; // sector not found
				CarryVal = 1;
			} else {
				DWORD dwSize;
				if(ReadFile(hFile, mem + base[DS] + regs.w[BX], regs.w[CX] * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
					regs.b[AL] = 0x0b; // read error
					CarryVal = 1;
				}
			}
		}
		CloseHandle(hFile);
	}
}

inline void msdos_int_26h()
{
	// this operation may cause serious damage for drives, so always returns error...
	uint16 seg, ofs;
	
	_pushf();
	
	if(!(regs.b[AL] < 26)) {
		regs.b[AL] = 0x01; // unit unknown
		CarryVal = 1;
	} else if(!msdos_drive_param_block_update(regs.b[AL], &seg, &ofs, 0)) {
		regs.b[AL] = 0x02; // drive not ready
		CarryVal = 1;
	} else {
#if 1
		regs.b[AL] = 0x0a; // write error
		CarryVal = 1;
#else
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + regs.b[AL]);
		
		HANDLE hFile = CreateFile(dev, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			regs.b[AL] = 0x02; // drive not ready
			CarryVal = 1;
		} else {
			if(SetFilePointer(hFile, regs.w[DX] * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				regs.b[AL] = 0x08; // sector not found
				CarryVal = 1;
			} else {
				DWORD dwSize;
				if(WriteFile(hFile, mem + base[DS] + regs.w[BX], regs.w[CX] * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
					regs.b[AL] = 0x0a; // write error
					CarryVal = 1;
				}
			}
		}
		CloseHandle(hFile);
#endif
	}
}

inline void msdos_int_27h()
{
	int mcb_seg = sregs[CS] - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->paragraphs = (regs.w[DX] >> 4);
	mcb_seg += mcb->paragraphs + 1;
	msdos_mcb_create(mcb_seg, 'Z', 0, (MEMORY_END >> 4) - mcb_seg - 1);
	
	msdos_process_terminate(sregs[CS], retval, 0);
}

inline void msdos_int_29h()
{
	msdos_putch(regs.b[AL]);
}

inline void msdos_int_2eh()
{
	char tmp[MAX_PATH], command[MAX_PATH], opt[MAX_PATH];
	memset(tmp, 0, sizeof(tmp));
	strcpy(tmp, (char *)(mem + base[DS] + regs.w[SI]));
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
	regs.b[AL] = 0;
}

inline void msdos_int_2fh_16h()
{
	switch(regs.b[AL]) {
	case 0x00:
		// windows 95 is installed
		regs.b[AL] = 4;
		regs.b[AH] = 0;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_2fh_43h()
{
	switch(regs.b[AL]) {
	case 0x00:
		// xms is not installed
		regs.b[AL] = 0;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_2fh_46h()
{
	switch(regs.b[AL]) {
	case 0x80:
		// windows 3.0 is not installed
		regs.w[AX] = 0x80;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_2fh_4ah()
{
	switch(regs.b[AL]) {
	case 0x01:
		// hma is not installed
		regs.w[BX] = 0;
	case 0x02:
		sregs[ES] = 0xffff;
		base[ES] = SegBase(ES);
		regs.w[DI] = 0xffff;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_2fh_4bh()
{
	switch(regs.b[AL]) {
	case 0x01:
	case 0x02:
		// task switcher is not installed
		sregs[ES] = 0;
		base[ES] = SegBase(ES);
		regs.w[DI] = 0;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_2fh_4fh()
{
	switch(regs.b[AL]) {
	case 0x00:
		regs.w[AX] = 0;
		regs.b[DL] = 1;	// major version
		regs.b[DH] = 0;	// minor version
		break;
	case 0x01:
		regs.w[AX] = 0;
		regs.w[BX] = code_page;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
		break;
	}
}

inline void msdos_int_2fh_b7h()
{
	switch(regs.b[AL]) {
	case 0x00:
		// append is not installed
		regs.b[AL] = 0;
		break;
	default:
		regs.w[AX] = 0x01;
		CarryVal = 1;
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
	case 0x15:
		// PC BIOS
		CarryVal = 0;
		switch(regs.b[AH]) {
		case 0x87: pcbios_int_15h_87h(); break;
		case 0x88: pcbios_int_15h_88h(); break;
		case 0x89: pcbios_int_15h_89h(); break;
		default:
			fatal_error("int 15h (ah=%2xh al=%2xh)\n", regs.b[AH], regs.b[AL]);
		}
		break;
	case 0x20:
		msdos_process_terminate(sregs[CS], retval, 1);
		break;
	case 0x21:
		// MS-DOS System Call
		CarryVal = 0;
		switch(regs.b[AH]) {
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
		// 0x11: search first entry with fcb
		// 0x12: search next entry with fcb
		// 0x13: delete file with fcb
		// 0x14: sequential read with fcb
		// 0x15: sequential write with fcb
		// 0x16: create new file with fcb
		// 0x17: rename file with fcb
		case 0x18: msdos_int_21h_18h(); break;
		case 0x19: msdos_int_21h_19h(); break;
		case 0x1a: msdos_int_21h_1ah(); break;
		// 0x1b: get fat table address
		// 0x1c: get fat table for drive
		// 0x1d:
		// 0x1e:
		case 0x1f: msdos_int_21h_1fh(); break;
		// 0x20:
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
		case 0x35: msdos_int_21h_35h(); break;
		case 0x36: msdos_int_21h_36h(); break;
		case 0x37: msdos_int_21h_37h(); break;
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
		case 0x54: msdos_int_21h_54h(); break;
		case 0x55: msdos_int_21h_55h(); break;
		case 0x56: msdos_int_21h_56h(0); break;
		case 0x57: msdos_int_21h_57h(); break;
		case 0x5a: msdos_int_21h_5ah(); break;
		case 0x5b: msdos_int_21h_5bh(); break;
		case 0x5c: msdos_int_21h_5ch(); break;
		// 0x5e: ms-network
		// 0x5f: ms-network
		case 0x60: msdos_int_21h_60h(0); break;
		case 0x62: msdos_int_21h_62h(); break;
		case 0x63: msdos_int_21h_63h(); break;
		case 0x65: msdos_int_21h_65h(); break;
		case 0x6c: msdos_int_21h_6ch(0); break;
		case 0x71:
			// windows95 long filename functions
			switch(regs.b[AL]) {
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
//				fatal_error("int 21h (ah=%2xh al=%2xh)\n", regs.b[AH], regs.b[AL]);
				regs.w[AX] = 0x7100;
				CarryVal = 1;
			}
			break;
		default:
			fatal_error("int 21h (ah=%2xh al=%2xh)\n", regs.b[AH], regs.b[AL]);
		}
		break;
	case 0x22:
		fatal_error("int 22h (terminate address)\n");
	case 0x23:
		msdos_process_terminate(current_psp, (retval & 0xff) | 0x100, 1);
		break;
	case 0x24:
		msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		break;
	case 0x25:
		msdos_int_25h(); break;
	case 0x26:
		msdos_int_26h(); break;
	case 0x27:
		msdos_int_27h(); break;
	case 0x29:
		msdos_int_29h(); break;
	case 0x2e:
		msdos_int_2eh(); break;
	case 0x2f:
		// multiplex interrupt
		CarryVal = 0;
		switch(regs.b[AH]) {
		case 0x16: msdos_int_2fh_16h(); break;
		case 0x43: msdos_int_2fh_43h(); break;
		case 0x46: msdos_int_2fh_46h(); break;
		case 0x4a: msdos_int_2fh_4ah(); break;
		case 0x4b: msdos_int_2fh_4bh(); break;
		case 0x4f: msdos_int_2fh_4fh(); break;
		case 0xb7: msdos_int_2fh_b7h(); break;
		default:
			fatal_error("int 2fh (ah=%2xh al=%2xh)\n", regs.b[AH], regs.b[AL]);
		}
		break;
	}
}

// init

int msdos_init(int argc, char *argv[], char *envp[])
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
		*(uint16 *)(mem + 4 * i + 0) = i;
		*(uint16 *)(mem + 4 * i + 2) = (IRET_TOP >> 4);
	}
	*(uint16 *)(mem + 4 * 0x22 + 0) = 0xfff0;
	*(uint16 *)(mem + 4 * 0x22 + 2) = 0xf000;
	memset(mem + IRET_TOP, 0xcf, IRET_SIZE);
	
	// environment
	int seg = MEMORY_TOP >> 4;
	msdos_mcb_create(seg++, 'M', -1, ENV_SIZE >> 4);
	int env_seg = seg;
	int ofs = 0;
	for(char **p = envp; p != NULL && *p != NULL; p++) {
		// lower to upper
		char tmp[ENV_SIZE];
		strcpy(tmp, *p);
		for(int i = 0;; i++) {
			if(tmp[i] == '=') {
				break;
			} else if(tmp[i] >= 'a' && tmp[i] <= 'z') {
				tmp[i] = tmp[i] - 'a' + 'A';
			}
		}
		int len = strlen(tmp);
		if (ofs + len + 1 + (2 + (8 + 1 + 3)) + 2 > ENV_SIZE) {
			fatal_error("too many environments\n");
		}
		memcpy(mem + (seg << 4) + ofs, tmp, len);
		ofs += len + 1;
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
		cmd_line->len = (uint8)strlen(cmd_line->cmd);
	} else {
		cmd_line->len = 0;
	}
	cmd_line->cmd[cmd_line->len] = 0x0d;
	
	// dbcs table
	msdos_dbcs_table_init();
	
	// execute command
	if(msdos_process_exec(argv[0], param, 0)) {
		fatal_error("'%s' not found\n", argv[0]);
	}
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

int msdos_finish()
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
	
	return(retval);
}

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

void hardware_init()
{
	cpu_init();
#ifdef SUPPORT_HARDWARE
	pic_init();
	//pit_init();
	pit_active = 0;
#endif
}

void hardware_run()
{
	ops = 0;
	
	while(!halt) {
		seg_prefix = 0;
		op(FETCHOP());
#ifdef SUPPORT_HARDWARE
		if(++ops >= 1024) {
			hardware_update();
		}
		if(intstat && IF) {
			int num = pic_ack();
			intstat = 0;
			interrupt(num);
		}
#endif
	}
}

void hardware_update()
{
#ifdef SUPPORT_HARDWARE
	if(pit_active) {
		pit_run();
	}
#endif
	ops = 0;
}

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

void pic_write(int c, uint32 addr, uint8 data)
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

uint8 pic_read(int c, uint32 addr)
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
		uint8 slave = 1 << (pic[pic_req_chip].icw3 & 7);
		pic[pic_req_chip - 1].isr |= slave;
		pic[pic_req_chip - 1].irr &= ~slave;
	}
	//if(pic[pic_req_chip].icw4 & 1) {
		// 8086 mode
		int vector = (pic[pic_req_chip].icw2 & 0xf8) | pic_req_level;
	//} else {
	//	// 8080 mode
	//	uint16 addr = (uint16)pic[pic_req_chip].icw2 << 8;
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
		uint8 irr = pic[c].irr;
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
		uint8 bit = 1 << level;
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
		cpu_interrupt(1);
		return;
	}
	cpu_interrupt(0);
}

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

void pit_write(int ch, uint8 val)
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
				uint8 bit = 2 << ch;
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

uint8 pit_read(int ch)
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
	static uint32 prev_time = 0;
	uint32 cur_time = timeGetTime();
	
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
	int32 tmp = PIT_COUNT_VALUE(ch);
loop:
	if(pit[ch].mode == 3) {
		int32 half = tmp >> 1;
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
	uint32 cur_time = timeGetTime();
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
		uint32 cur_time = timeGetTime();
		if(cur_time > pit[ch].prev_time) {
			uint32 input = PIT_FREQ * (cur_time - pit[ch].prev_time) / 1000;
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
	pit[ch].latch = (uint16)pit[ch].count;
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
		int32 half = PIT_COUNT_VALUE(ch) >> 1;
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
	uint32 val = 1000 * clock / PIT_FREQ;
	
	if(val > 0) {
		return(val);
	} else {
		return(1);
	}
}

// i/o bus

uint8 IN8(uint32 addr)
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
		return((AMASK >> 19) & 2);
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

void OUT8(uint32 addr, uint8 val)
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
			// reset cpu
			cpu_init();
			if(cmos[0x0f] == 5) {
				// reset pic
				pic_init();
				pic[0].irr = pic[1].irr = 0x00;
				pic[0].imr = pic[1].imr = 0xff;
			}
			// jmp far 0040:0067
#ifdef HAS_I286
			i286_code_descriptor(0x40, 0x67);
#else
			sregs[CS] = 0x40;
			base[CS] = SegBase(CS);
			PC = (base[CS] + 0x67) & AMASK;
#endif
		}
		break;
	case 0x70:
		cmos_addr = val;
		break;
	case 0x71:
		cmos[cmos_addr & 0x7] = val;
		break;
	case 0x92:
#ifdef HAS_I286
		if(val & 0x02) {
			AMASK = 0xffffff;
		} else {
			AMASK = 0xfffff;
		}
#endif
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

uint16 IN16(uint32 addr)
{
	return(IN8(addr) | (IN8(addr + 1) << 8));
}

void OUT16(uint32 addr, uint16 val)
{
	OUT8(addr + 0, (val >> 0) & 0xff);
	OUT8(addr + 1, (val >> 8) & 0xff);
}

/* ----------------------------------------------------------------------------
	80286 emulation (based on MAME i86 core)
---------------------------------------------------------------------------- */

// interrupt vector
#define NMI_INT_VECTOR			2
#define ILLEGAL_INSTRUCTION		6
#define GENERAL_PROTECTION_FAULT	0xd

// flags
#define CF	(CarryVal != 0)
#define SF	(SignVal < 0)
#define ZF	(ZeroVal == 0)
#define PF	parity_table[ParityVal]
#define AF	(AuxVal != 0)
#define OF	(OverVal != 0)
#define DF	(DirVal < 0)
#define MD	(MF != 0)
#define PM	(msw & 1)
#define CPL	(sregs[CS] & 3)
#define IOPL	((flags & 0x3000) >> 12)

#define SetTF(x) (TF = (x))
#define SetIF(x) (IF = (x))
#define SetDF(x) (DirVal = (x) ? -1 : 1)
#define SetMD(x) (MF = (x))
#define SetOFW_Add(x, y, z) (OverVal = ((x) ^ (y)) & ((x) ^ (z)) & 0x8000)
#define SetOFB_Add(x, y, z) (OverVal = ((x) ^ (y)) & ((x) ^ (z)) & 0x80)
#define SetOFW_Sub(x, y, z) (OverVal = ((z) ^ (y)) & ((z) ^ (x)) & 0x8000)
#define SetOFB_Sub(x, y, z) (OverVal = ((z) ^ (y)) & ((z) ^ (x)) & 0x80)
#define SetCFB(x) (CarryVal = (x) & 0x100)
#define SetCFW(x) (CarryVal = (x) & 0x10000)
#define SetAF(x, y, z) (AuxVal = ((x) ^ ((y) ^ (z))) & 0x10)
#define SetSF(x) (SignVal = (x))
#define SetZF(x) (ZeroVal = (x))
#define SetPF(x) (ParityVal = (x))
#define SetSZPF_Byte(x) (ParityVal = SignVal = ZeroVal = (int8)(x))
#define SetSZPF_Word(x) (ParityVal = SignVal = ZeroVal = (int16)(x))

// opecodes
#define ADDB(dst, src) { unsigned res = (dst) + (src); SetCFB(res); SetOFB_Add(res, src, dst); SetAF(res, src, dst); SetSZPF_Byte(res); dst = (uint8)res; }
#define ADDW(dst, src) { unsigned res = (dst) + (src); SetCFW(res); SetOFW_Add(res, src, dst); SetAF(res, src, dst); SetSZPF_Word(res); dst = (uint16)res; }
#define SUBB(dst, src) { unsigned res = (dst) - (src); SetCFB(res); SetOFB_Sub(res, src, dst); SetAF(res, src, dst); SetSZPF_Byte(res); dst = (uint8)res; }
#define SUBW(dst, src) { unsigned res = (dst) - (src); SetCFW(res); SetOFW_Sub(res, src, dst); SetAF(res, src, dst); SetSZPF_Word(res); dst = (uint16)res; }
#define ORB(dst, src) dst |= (src); CarryVal = OverVal = AuxVal = 0; SetSZPF_Byte(dst)
#define ORW(dst, src) dst |= (src); CarryVal = OverVal = AuxVal = 0; SetSZPF_Word(dst)
#define ANDB(dst, src) dst &= (src); CarryVal = OverVal = AuxVal = 0; SetSZPF_Byte(dst)
#define ANDW(dst, src) dst &= (src); CarryVal = OverVal = AuxVal = 0; SetSZPF_Word(dst)
#define XORB(dst, src) dst ^= (src); CarryVal = OverVal = AuxVal = 0; SetSZPF_Byte(dst)
#define XORW(dst, src) dst ^= (src); CarryVal = OverVal = AuxVal = 0; SetSZPF_Word(dst)

// memory
#define RegWord(ModRM) regs.w[mod_reg16[ModRM]]
#define RegByte(ModRM) regs.b[mod_reg8[ModRM]]
#define GetRMWord(ModRM) ((ModRM) >= 0xc0 ? regs.w[mod_rm16[ModRM]] : (GetEA(ModRM), RM16(EA)))
#define PutbackRMWord(ModRM, val) { \
	if(ModRM >= 0xc0) { \
		regs.w[mod_rm16[ModRM]] = val; \
	} else { \
		WM16(EA, val); \
	} \
}
#define GetNextRMWord() RM16(EA + 2)
#define GetRMWordOfs(ofs) RM16(EA - EO + (uint16)(EO + (ofs)))
#define GetRMByteOfs(ofs) RM8(EA - EO + (uint16)(EO + (ofs)))
#define PutRMWord(ModRM, val) { \
	if (ModRM >= 0xc0) { \
		regs.w[mod_rm16[ModRM]] = val; \
	} else { \
		GetEA(ModRM); \
		WM16(EA, val); \
	} \
}
#define PutRMWordOfs(ofs, val) WM16(EA - EO + (uint16)(EO + (ofs)), val)
#define PutRMByteOfs(offs, val) WM8(EA - EO + (uint16)(EO + (offs)), val)
#define PutImmRMWord(ModRM) { \
	if (ModRM >= 0xc0) { \
		regs.w[mod_rm16[ModRM]] = FETCH16(); \
	} else { \
		GetEA(ModRM); \
		uint16 val = FETCH16(); \
		WM16(EA , val); \
	} \
}
#define GetRMByte(ModRM) ((ModRM) >= 0xc0 ? regs.b[mod_rm8[ModRM]] : RM8(GetEA(ModRM)))
#define PutRMByte(ModRM, val) { \
	if(ModRM >= 0xc0) { \
		regs.b[mod_rm8[ModRM]] = val; \
	} else { \
		WM8(GetEA(ModRM), val); \
	} \
}
#define PutImmRMByte(ModRM) { \
	if (ModRM >= 0xc0) { \
		regs.b[mod_rm8[ModRM]] = FETCH8(); \
	} else { \
		GetEA(ModRM); \
		WM8(EA , FETCH8()); \
	} \
}
#define PutbackRMByte(ModRM, val) { \
	if (ModRM >= 0xc0) { \
		regs.b[mod_rm8[ModRM]] = val; \
	} else { \
		WM8(EA, val); \
	} \
}
#define DEF_br8(dst, src) \
	unsigned ModRM = FETCHOP(); \
	unsigned src = RegByte(ModRM); \
	unsigned dst = GetRMByte(ModRM)
#define DEF_wr16(dst, src) \
	unsigned ModRM = FETCHOP(); \
	unsigned src = RegWord(ModRM); \
	unsigned dst = GetRMWord(ModRM)
#define DEF_r8b(dst, src) \
	unsigned ModRM = FETCHOP(); \
	unsigned dst = RegByte(ModRM); \
	unsigned src = GetRMByte(ModRM)
#define DEF_r16w(dst, src) \
	unsigned ModRM = FETCHOP(); \
	unsigned dst = RegWord(ModRM); \
	unsigned src = GetRMWord(ModRM)
#define DEF_ald8(dst, src) \
	unsigned src = FETCHOP(); \
	unsigned dst = regs.b[AL]
#define DEF_axd16(dst, src) \
	unsigned src = FETCHOP(); \
	unsigned dst = regs.w[AX]; \
	src += (FETCH8() << 8)

inline uint8 RM8(uint32 addr)
{
	return(mem[addr & AMASK]);
}

inline uint16 RM16(uint32 addr)
{
	return(*(uint16 *)(mem + (addr & AMASK)));
}

inline void WM8(uint32 addr, uint8 val)
{
	mem[addr & AMASK] = val;
}

inline void WM16(uint32 addr, uint16 val)
{
	*(uint16 *)(mem + (addr & AMASK)) = val;
}

inline uint8 RM8(uint32 seg, uint32 ofs)
{
	return(mem[(DefaultBase(seg) + ofs) & AMASK]);
}

inline uint16 RM16(uint32 seg, uint32 ofs)
{
	return(*(uint16 *)(mem + ((DefaultBase(seg) + ofs) & AMASK)));
}

inline void WM8(uint32 seg, uint32 ofs, uint8 val)
{
	mem[(DefaultBase(seg) + ofs) & AMASK] = val;
}

inline void WM16(uint32 seg, uint32 ofs, uint16 val)
{
	*(uint16 *)(mem + ((DefaultBase(seg) + ofs) & AMASK)) = val;
}

inline uint8 FETCHOP()
{
	return(mem[(PC++) & AMASK]);
}

inline uint8 FETCH8()
{
	return(mem[(PC++) & AMASK]);
}

inline uint16 FETCH16()
{
	uint16 val = *(uint16 *)(mem + (PC & AMASK));
	PC += 2;
	return(val);
}

inline void PUSH16(uint16 val)
{
	regs.w[SP] -= 2;
	*(uint16 *)(mem + ((base[SS] + regs.w[SP]) & AMASK)) = val;
}

inline uint16 POP16()
{
	uint16 var = *(uint16 *)(mem + ((base[SS] + regs.w[SP]) & AMASK));
	regs.w[SP] += 2;
	return(var);
}

void interrupt(unsigned num)
{
#ifdef HAS_I286
	if(PM) {
		if((num << 3) >= idtr_limit) { // go into shutdown mode
			return;
		}
		_pushf();
		PUSH16(sregs[CS]);
		PUSH16(PC - base[CS]);
		uint16 word1 = RM16(idtr_base + (num << 3));
		uint16 word2 = RM16(idtr_base + (num << 3) + 2);
		uint16 word3 = RM16(idtr_base + (num << 3) + 4);
		switch(word3 & 0xf00) {
		case 0x500: // task gate
			i286_data_descriptor(CS, word2);
			PC = base[CS] + word1;
			break;
		case 0x600: // interrupt gate
			TF = IF = 0;
			i286_data_descriptor(CS, word2);
			PC = base[CS] + word1;
			break;
		case 0x700: // trap gate
			i286_data_descriptor(CS, word2);
			PC = base[CS] + word1;
			break;
		}
	} else {
#endif
		uint16 ip = PC - base[CS];
		unsigned ofs = RM16(num * 4);
		unsigned seg = RM16(num * 4 + 2);
		_pushf();
		TF = IF = 0;
		PUSH16(sregs[CS]);
		PUSH16(ip);
		sregs[CS] = (uint16)seg;
		base[CS] = SegBase(CS);
		PC = (base[CS] + ofs) & AMASK;
#ifdef HAS_I286
	}
#endif
}

unsigned GetEA(unsigned ModRM)
{
	switch(ModRM) {
	case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:
		EO = (uint16)(regs.w[BX] + regs.w[SI]); EA = DefaultBase(DS) + EO; return EA;
	case 0x01: case 0x09: case 0x11: case 0x19: case 0x21: case 0x29: case 0x31: case 0x39:
		EO = (uint16)(regs.w[BX] + regs.w[DI]); EA = DefaultBase(DS) + EO; return EA;
	case 0x02: case 0x0a: case 0x12: case 0x1a: case 0x22: case 0x2a: case 0x32: case 0x3a:
		EO = (uint16)(regs.w[BP] + regs.w[SI]); EA = DefaultBase(SS) + EO; return EA;
	case 0x03: case 0x0b: case 0x13: case 0x1b: case 0x23: case 0x2b: case 0x33: case 0x3b:
		EO = (uint16)(regs.w[BP] + regs.w[DI]); EA = DefaultBase(SS) + EO; return EA;
	case 0x04: case 0x0c: case 0x14: case 0x1c: case 0x24: case 0x2c: case 0x34: case 0x3c:
		EO = regs.w[SI]; EA = DefaultBase(DS) + EO; return EA;
	case 0x05: case 0x0d: case 0x15: case 0x1d: case 0x25: case 0x2d: case 0x35: case 0x3d:
		EO = regs.w[DI]; EA = DefaultBase(DS) + EO; return EA;
	case 0x06: case 0x0e: case 0x16: case 0x1e: case 0x26: case 0x2e: case 0x36: case 0x3e:
		EO = FETCHOP(); EO += FETCHOP() << 8; EA = DefaultBase(DS) + EO; return EA;
	case 0x07: case 0x0f: case 0x17: case 0x1f: case 0x27: case 0x2f: case 0x37: case 0x3f:
		EO = regs.w[BX]; EA = DefaultBase(DS) + EO; return EA;
	case 0x40: case 0x48: case 0x50: case 0x58: case 0x60: case 0x68: case 0x70: case 0x78:
		EO = (uint16)(regs.w[BX] + regs.w[SI] + (int8)FETCHOP()); EA = DefaultBase(DS) + EO; return EA;
	case 0x41: case 0x49: case 0x51: case 0x59: case 0x61: case 0x69: case 0x71: case 0x79:
		EO = (uint16)(regs.w[BX] + regs.w[DI] + (int8)FETCHOP()); EA = DefaultBase(DS) + EO; return EA;
	case 0x42: case 0x4a: case 0x52: case 0x5a: case 0x62: case 0x6a: case 0x72: case 0x7a:
		EO = (uint16)(regs.w[BP] + regs.w[SI] + (int8)FETCHOP()); EA = DefaultBase(SS) + EO; return EA;
	case 0x43: case 0x4b: case 0x53: case 0x5b: case 0x63: case 0x6b: case 0x73: case 0x7b:
		EO = (uint16)(regs.w[BP] + regs.w[DI] + (int8)FETCHOP()); EA = DefaultBase(SS) + EO; return EA;
	case 0x44: case 0x4c: case 0x54: case 0x5c: case 0x64: case 0x6c: case 0x74: case 0x7c:
		EO = (uint16)(regs.w[SI] + (int8)FETCHOP()); EA = DefaultBase(DS) + EO; return EA;
	case 0x45: case 0x4d: case 0x55: case 0x5d: case 0x65: case 0x6d: case 0x75: case 0x7d:
		EO = (uint16)(regs.w[DI] + (int8)FETCHOP()); EA = DefaultBase(DS) + EO; return EA;
	case 0x46: case 0x4e: case 0x56: case 0x5e: case 0x66: case 0x6e: case 0x76: case 0x7e:
		EO = (uint16)(regs.w[BP] + (int8)FETCHOP()); EA = DefaultBase(SS) + EO; return EA;
	case 0x47: case 0x4f: case 0x57: case 0x5f: case 0x67: case 0x6f: case 0x77: case 0x7f:
		EO = (uint16)(regs.w[BX] + (int8)FETCHOP()); EA = DefaultBase(DS) + EO; return EA;
	case 0x80: case 0x88: case 0x90: case 0x98: case 0xa0: case 0xa8: case 0xb0: case 0xb8:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[BX] + regs.w[SI]; EA = DefaultBase(DS) + (uint16)EO; return EA;
	case 0x81: case 0x89: case 0x91: case 0x99: case 0xa1: case 0xa9: case 0xb1: case 0xb9:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[BX] + regs.w[DI]; EA = DefaultBase(DS) + (uint16)EO; return EA;
	case 0x82: case 0x8a: case 0x92: case 0x9a: case 0xa2: case 0xaa: case 0xb2: case 0xba:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[BP] + regs.w[SI]; EA = DefaultBase(SS) + (uint16)EO; return EA;
	case 0x83: case 0x8b: case 0x93: case 0x9b: case 0xa3: case 0xab: case 0xb3: case 0xbb:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[BP] + regs.w[DI]; EA = DefaultBase(SS) + (uint16)EO; return EA;
	case 0x84: case 0x8c: case 0x94: case 0x9c: case 0xa4: case 0xac: case 0xb4: case 0xbc:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[SI]; EA = DefaultBase(DS) + (uint16)EO; return EA;
	case 0x85: case 0x8d: case 0x95: case 0x9d: case 0xa5: case 0xad: case 0xb5: case 0xbd:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[DI]; EA = DefaultBase(DS) + (uint16)EO; return EA;
	case 0x86: case 0x8e: case 0x96: case 0x9e: case 0xa6: case 0xae: case 0xb6: case 0xbe:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[BP]; EA = DefaultBase(SS) + (uint16)EO; return EA;
	case 0x87: case 0x8f: case 0x97: case 0x9f: case 0xa7: case 0xaf: case 0xb7: case 0xbf:
		EO = FETCHOP(); EO += FETCHOP() << 8; EO += regs.w[BX]; EA = DefaultBase(DS) + (uint16)EO; return EA;
	}
	return 0;
}

#define WRITEABLE(a) (((a) & 0xa) == 2)
#define READABLE(a) ((((a) & 0xa) == 0xa) || (((a) & 8) == 0))

void rotate_shift_byte(unsigned ModRM, unsigned cnt)
{
	unsigned src = GetRMByte(ModRM);
	unsigned dst = src;
	
	if(cnt == 0) {
	} else if(cnt == 1) {
		switch(ModRM & 0x38) {
		case 0x00:	// ROL eb, 1
			CarryVal = src & 0x80;
			dst = (src << 1) + CF;
			PutbackRMByte(ModRM, dst);
			OverVal = (src ^ dst) & 0x80;
			break;
		case 0x08:	// ROR eb, 1
			CarryVal = src & 1;
			dst = ((CF << 8) + src) >> 1;
			PutbackRMByte(ModRM, dst);
			OverVal = (src ^ dst) & 0x80;
			break;
		case 0x10:	// RCL eb, 1
			dst = (src << 1) + CF;
			PutbackRMByte(ModRM, dst);
			SetCFB(dst);
			OverVal = (src ^ dst) & 0x80;
			break;
		case 0x18:	// RCR eb, 1
			dst = ((CF << 8) + src) >> 1;
			PutbackRMByte(ModRM, dst);
			CarryVal = src & 1;
			OverVal = (src ^ dst) & 0x80;
			break;
		case 0x20:	// SHL eb, 1
		case 0x30:
			dst = src << 1;
			PutbackRMByte(ModRM, dst);
			SetCFB(dst);
			OverVal = (src ^ dst) & 0x80;
			AuxVal = 1;
			SetSZPF_Byte(dst);
			break;
		case 0x28:	// SHR eb, 1
			dst = src >> 1;
			PutbackRMByte(ModRM, dst);
			CarryVal = src & 1;
			OverVal = src & 0x80;
			AuxVal = 1;
			SetSZPF_Byte(dst);
			break;
		case 0x38:	// SAR eb, 1
			dst = ((int8)src) >> 1;
			PutbackRMByte(ModRM, dst);
			CarryVal = src & 1;
			OverVal = 0;
			AuxVal = 1;
			SetSZPF_Byte(dst);
			break;
		}
	} else {
		switch(ModRM & 0x38) {
		case 0x00:	// ROL eb, cnt
			for(; cnt > 0; cnt--) {
				CarryVal = dst & 0x80;
				dst = (dst << 1) + CF;
			}
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		case 0x08:	// ROR eb, cnt
			for(; cnt > 0; cnt--) {
				CarryVal = dst & 1;
				dst = (dst >> 1) + (CF << 7);
			}
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		case 0x10:	// RCL eb, cnt
			for(; cnt > 0; cnt--) {
				dst = (dst << 1) + CF;
				SetCFB(dst);
			}
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		case 0x18:	// RCR eb, cnt
			for(; cnt > 0; cnt--) {
				dst = (CF << 8) + dst;
				CarryVal = dst & 1;
				dst >>= 1;
			}
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		case 0x20:
		case 0x30:	// SHL eb, cnt
			dst <<= cnt;
			SetCFB(dst);
			AuxVal = 1;
			SetSZPF_Byte(dst);
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		case 0x28:	// SHR eb, cnt
			dst >>= cnt - 1;
			CarryVal = dst & 1;
			dst >>= 1;
			SetSZPF_Byte(dst);
			AuxVal = 1;
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		case 0x38:	// SAR eb, cnt
			dst = ((int8)dst) >> (cnt - 1);
			CarryVal = dst & 1;
			dst = ((int8)((uint8)dst)) >> 1;
			SetSZPF_Byte(dst);
			AuxVal = 1;
			PutbackRMByte(ModRM, (uint8)dst);
			break;
		}
	}
}

void rotate_shift_word(unsigned ModRM, unsigned cnt)
{
	unsigned src = GetRMWord(ModRM);
	unsigned dst = src;
	
	if(cnt == 0) {
	} else if(cnt == 1) {
		switch(ModRM & 0x38) {
		case 0x00:	// ROL ew, 1
			CarryVal = src & 0x8000;
			dst = (src << 1) + CF;
			PutbackRMWord(ModRM, dst);
			OverVal = (src ^ dst) & 0x8000;
			break;
		case 0x08:	// ROR ew, 1
			CarryVal = src & 1;
			dst = ((CF << 16) + src) >> 1;
			PutbackRMWord(ModRM, dst);
			OverVal = (src ^ dst) & 0x8000;
			break;
		case 0x10:	// RCL ew, 1
			dst = (src << 1) + CF;
			PutbackRMWord(ModRM, dst);
			SetCFW(dst);
			OverVal = (src ^ dst) & 0x8000;
			break;
		case 0x18:	// RCR ew, 1
			dst = ((CF << 16) + src) >> 1;
			PutbackRMWord(ModRM, dst);
			CarryVal = src & 1;
			OverVal = (src ^ dst) & 0x8000;
			break;
		case 0x20:	// SHL ew, 1
		case 0x30:
			dst = src << 1;
			PutbackRMWord(ModRM, dst);
			SetCFW(dst);
			OverVal = (src ^ dst) & 0x8000;
			AuxVal = 1;
			SetSZPF_Word(dst);
			break;
		case 0x28:	// SHR ew, 1
			dst = src >> 1;
			PutbackRMWord(ModRM, dst);
			CarryVal = src & 1;
			OverVal = src & 0x8000;
			AuxVal = 1;
			SetSZPF_Word(dst);
			break;
		case 0x38:	// SAR ew, 1
			dst = ((int16)src) >> 1;
			PutbackRMWord(ModRM, dst);
			CarryVal = src & 1;
			OverVal = 0;
			AuxVal = 1;
			SetSZPF_Word(dst);
			break;
		}
	} else {
		switch(ModRM & 0x38) {
		case 0x00:	// ROL ew, cnt
			for(; cnt > 0; cnt--) {
				CarryVal = dst & 0x8000;
				dst = (dst << 1) + CF;
			}
			PutbackRMWord(ModRM, dst);
			break;
		case 0x08:	// ROR ew, cnt
			for(; cnt > 0; cnt--) {
				CarryVal = dst & 1;
				dst = (dst >> 1) + (CF << 15);
			}
			PutbackRMWord(ModRM, dst);
			break;
		case 0x10:	// RCL ew, cnt
			for(; cnt > 0; cnt--) {
				dst = (dst << 1) + CF;
				SetCFW(dst);
			}
			PutbackRMWord(ModRM, dst);
			break;
		case 0x18:	// RCR ew, cnt
			for(; cnt > 0; cnt--) {
				dst = dst + (CF << 16);
				CarryVal = dst & 1;
				dst >>= 1;
			}
			PutbackRMWord(ModRM, dst);
			break;
		case 0x20:
		case 0x30:	// SHL ew, cnt
			dst <<= cnt;
			SetCFW(dst);
			AuxVal = 1;
			SetSZPF_Word(dst);
			PutbackRMWord(ModRM, dst);
			break;
		case 0x28:	// SHR ew, cnt
			dst >>= cnt - 1;
			CarryVal = dst & 1;
			dst >>= 1;
			SetSZPF_Word(dst);
			AuxVal = 1;
			PutbackRMWord(ModRM, dst);
			break;
		case 0x38:	// SAR ew, cnt
			dst = ((int16)dst) >> (cnt - 1);
			CarryVal = dst & 1;
			dst = ((int16)((uint16)dst)) >> 1;
			SetSZPF_Word(dst);
			AuxVal = 1;
			PutbackRMWord(ModRM, dst);
			break;
		}
	}
}

#ifdef HAS_I286
int i286_selector_okay(uint16 selector)
{
	if(selector & 4) {
		return (selector & ~7) < ldtr_limit;
	} else {
		return (selector & ~7) < gdtr_limit;
	}
}

void i286_data_descriptor(int reg, uint16 selector)
{
	if(PM) {
		if(selector & 4) {
			// local descriptor table
			if(selector > ldtr_limit) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			sregs[reg] = selector;
			limit[reg] = RM16(ldtr_base + (selector & ~7));
			base[reg] = RM16(ldtr_base + (selector & ~7) + 2) | (RM16(ldtr_base + (selector & ~7) + 4) << 16);
			base[reg] &= 0xffffff;
		} else {
			// global descriptor table
			if(!(selector & ~7) || (selector > gdtr_limit)) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			sregs[reg] = selector;
			limit[reg] = RM16(gdtr_base + (selector & ~7));
			base[reg] = RM16(gdtr_base + (selector & ~7) + 2);
			uint16 tmp = RM16(gdtr_base + (selector & ~7) + 4);
			base[reg] |= (tmp & 0xff) << 16;
		}
	} else {
		sregs[reg] = selector;
		base[reg] = selector << 4;
	}
}

void i286_code_descriptor(uint16 selector, uint16 offset)
{
	if(PM) {
		uint16 word1, word2, word3;
		if(selector & 4) {
			// local descriptor table
			if(selector > ldtr_limit) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			word1 = RM16(ldtr_base + (selector & ~7));
			word2 = RM16(ldtr_base + (selector & ~7) + 2);
			word3 = RM16(ldtr_base + (selector & ~7) + 4);
		} else {
			// global descriptor table
			if(!(selector & ~7) || (selector > gdtr_limit)) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			word1 = RM16(gdtr_base + (selector & ~7));
			word2 = RM16(gdtr_base + (selector & ~7) + 2);
			word3 = RM16(gdtr_base + (selector & ~7) + 4);
		}
		if(word3 & 0x1000) {
			sregs[CS] = selector;
			limit[CS] = word1;
			base[CS] = word2 | ((word3 & 0xff) << 16);
			PC = base[CS] + offset;
		} else {
			// systemdescriptor
			switch(word3 & 0xf00) {
			case 0x400: // call gate
				i286_data_descriptor(CS, word2);
				PC = base[CS] + word1;
				break;
			case 0x500: // task gate
				i286_data_descriptor(CS, word2);
				PC = base[CS] + word1;
				break;
			case 0x600: // interrupt gate
				TF = IF = 0;
				i286_data_descriptor(CS, word2);
				PC = base[CS] + word1;
				break;
			case 0x700: // trap gate
				i286_data_descriptor(CS, word2);
				PC = base[CS] + word1;
				break;
			}
		}
	} else {
		sregs[CS] = selector;
		base[CS] = selector << 4;
		PC = base[CS] + offset;
	}
}
#endif

void op(uint8 code)
{
	switch(code) {
	case 0x00: _add_br8(); break;
	case 0x01: _add_wr16(); break;
	case 0x02: _add_r8b(); break;
	case 0x03: _add_r16w(); break;
	case 0x04: _add_ald8(); break;
	case 0x05: _add_axd16(); break;
	case 0x06: _push_es(); break;
	case 0x07: _pop_es(); break;
	case 0x08: _or_br8(); break;
	case 0x09: _or_wr16(); break;
	case 0x0a: _or_r8b(); break;
	case 0x0b: _or_r16w(); break;
	case 0x0c: _or_ald8(); break;
	case 0x0d: _or_axd16(); break;
	case 0x0e: _push_cs(); break;
	case 0x0f: _op0f(); break;
	case 0x10: _adc_br8(); break;
	case 0x11: _adc_wr16(); break;
	case 0x12: _adc_r8b(); break;
	case 0x13: _adc_r16w(); break;
	case 0x14: _adc_ald8(); break;
	case 0x15: _adc_axd16(); break;
	case 0x16: _push_ss(); break;
	case 0x17: _pop_ss(); break;
	case 0x18: _sbb_br8(); break;
	case 0x19: _sbb_wr16(); break;
	case 0x1a: _sbb_r8b(); break;
	case 0x1b: _sbb_r16w(); break;
	case 0x1c: _sbb_ald8(); break;
	case 0x1d: _sbb_axd16(); break;
	case 0x1e: _push_ds(); break;
	case 0x1f: _pop_ds(); break;
	case 0x20: _and_br8(); break;
	case 0x21: _and_wr16(); break;
	case 0x22: _and_r8b(); break;
	case 0x23: _and_r16w(); break;
	case 0x24: _and_ald8(); break;
	case 0x25: _and_axd16(); break;
	case 0x26: _es(); break;
	case 0x27: _daa(); break;
	case 0x28: _sub_br8(); break;
	case 0x29: _sub_wr16(); break;
	case 0x2a: _sub_r8b(); break;
	case 0x2b: _sub_r16w(); break;
	case 0x2c: _sub_ald8(); break;
	case 0x2d: _sub_axd16(); break;
	case 0x2e: _cs(); break;
	case 0x2f: _das(); break;
	case 0x30: _xor_br8(); break;
	case 0x31: _xor_wr16(); break;
	case 0x32: _xor_r8b(); break;
	case 0x33: _xor_r16w(); break;
	case 0x34: _xor_ald8(); break;
	case 0x35: _xor_axd16(); break;
	case 0x36: _ss(); break;
	case 0x37: _aaa(); break;
	case 0x38: _cmp_br8(); break;
	case 0x39: _cmp_wr16(); break;
	case 0x3a: _cmp_r8b(); break;
	case 0x3b: _cmp_r16w(); break;
	case 0x3c: _cmp_ald8(); break;
	case 0x3d: _cmp_axd16(); break;
	case 0x3e: _ds(); break;
	case 0x3f: _aas(); break;
	case 0x40: _inc_ax(); break;
	case 0x41: _inc_cx(); break;
	case 0x42: _inc_dx(); break;
	case 0x43: _inc_bx(); break;
	case 0x44: _inc_sp(); break;
	case 0x45: _inc_bp(); break;
	case 0x46: _inc_si(); break;
	case 0x47: _inc_di(); break;
	case 0x48: _dec_ax(); break;
	case 0x49: _dec_cx(); break;
	case 0x4a: _dec_dx(); break;
	case 0x4b: _dec_bx(); break;
	case 0x4c: _dec_sp(); break;
	case 0x4d: _dec_bp(); break;
	case 0x4e: _dec_si(); break;
	case 0x4f: _dec_di(); break;
	case 0x50: _push_ax(); break;
	case 0x51: _push_cx(); break;
	case 0x52: _push_dx(); break;
	case 0x53: _push_bx(); break;
	case 0x54: _push_sp(); break;
	case 0x55: _push_bp(); break;
	case 0x56: _push_si(); break;
	case 0x57: _push_di(); break;
	case 0x58: _pop_ax(); break;
	case 0x59: _pop_cx(); break;
	case 0x5a: _pop_dx(); break;
	case 0x5b: _pop_bx(); break;
	case 0x5c: _pop_sp(); break;
	case 0x5d: _pop_bp(); break;
	case 0x5e: _pop_si(); break;
	case 0x5f: _pop_di(); break;
	case 0x60: _pusha(); break;
	case 0x61: _popa(); break;
	case 0x62: _bound(); break;
	case 0x63: _arpl(); break;
	case 0x64: _repc(0); break;
	case 0x65: _repc(1); break;
	case 0x66: _invalid(); break;
	case 0x67: _invalid(); break;
	case 0x68: _push_d16(); break;
	case 0x69: _imul_d16(); break;
	case 0x6a: _push_d8(); break;
	case 0x6b: _imul_d8(); break;
	case 0x6c: _insb(); break;
	case 0x6d: _insw(); break;
	case 0x6e: _outsb(); break;
	case 0x6f: _outsw(); break;
	case 0x70: _jo(); break;
	case 0x71: _jno(); break;
	case 0x72: _jb(); break;
	case 0x73: _jnb(); break;
	case 0x74: _jz(); break;
	case 0x75: _jnz(); break;
	case 0x76: _jbe(); break;
	case 0x77: _jnbe(); break;
	case 0x78: _js(); break;
	case 0x79: _jns(); break;
	case 0x7a: _jp(); break;
	case 0x7b: _jnp(); break;
	case 0x7c: _jl(); break;
	case 0x7d: _jnl(); break;
	case 0x7e: _jle(); break;
	case 0x7f: _jnle(); break;
	case 0x80: _op80(); break;
	case 0x81: _op81(); break;
	case 0x82: _op82(); break;
	case 0x83: _op83(); break;
	case 0x84: _test_br8(); break;
	case 0x85: _test_wr16(); break;
	case 0x86: _xchg_br8(); break;
	case 0x87: _xchg_wr16(); break;
	case 0x88: _mov_br8(); break;
	case 0x89: _mov_wr16(); break;
	case 0x8a: _mov_r8b(); break;
	case 0x8b: _mov_r16w(); break;
	case 0x8c: _mov_wsreg(); break;
	case 0x8d: _lea(); break;
	case 0x8e: _mov_sregw(); break;
	case 0x8f: _popw(); break;
	case 0x90: _nop(); break;
	case 0x91: _xchg_axcx(); break;
	case 0x92: _xchg_axdx(); break;
	case 0x93: _xchg_axbx(); break;
	case 0x94: _xchg_axsp(); break;
	case 0x95: _xchg_axbp(); break;
	case 0x96: _xchg_axsi(); break;
	case 0x97: _xchg_axdi(); break;
	case 0x98: _cbw(); break;
	case 0x99: _cwd(); break;
	case 0x9a: _call_far(); break;
	case 0x9b: _wait(); break;
	case 0x9c: _pushf(); break;
	case 0x9d: _popf(); break;
	case 0x9e: _sahf(); break;
	case 0x9f: _lahf(); break;
	case 0xa0: _mov_aldisp(); break;
	case 0xa1: _mov_axdisp(); break;
	case 0xa2: _mov_dispal(); break;
	case 0xa3: _mov_dispax(); break;
	case 0xa4: _movsb(); break;
	case 0xa5: _movsw(); break;
	case 0xa6: _cmpsb(); break;
	case 0xa7: _cmpsw(); break;
	case 0xa8: _test_ald8(); break;
	case 0xa9: _test_axd16(); break;
	case 0xaa: _stosb(); break;
	case 0xab: _stosw(); break;
	case 0xac: _lodsb(); break;
	case 0xad: _lodsw(); break;
	case 0xae: _scasb(); break;
	case 0xaf: _scasw(); break;
	case 0xb0: _mov_ald8(); break;
	case 0xb1: _mov_cld8(); break;
	case 0xb2: _mov_dld8(); break;
	case 0xb3: _mov_bld8(); break;
	case 0xb4: _mov_ahd8(); break;
	case 0xb5: _mov_chd8(); break;
	case 0xb6: _mov_dhd8(); break;
	case 0xb7: _mov_bhd8(); break;
	case 0xb8: _mov_axd16(); break;
	case 0xb9: _mov_cxd16(); break;
	case 0xba: _mov_dxd16(); break;
	case 0xbb: _mov_bxd16(); break;
	case 0xbc: _mov_spd16(); break;
	case 0xbd: _mov_bpd16(); break;
	case 0xbe: _mov_sid16(); break;
	case 0xbf: _mov_did16(); break;
	case 0xc0: _rotshft_bd8(); break;
	case 0xc1: _rotshft_wd8(); break;
	case 0xc2: _ret_d16(); break;
	case 0xc3: _ret(); break;
	case 0xc4: _les_dw(); break;
	case 0xc5: _lds_dw(); break;
	case 0xc6: _mov_bd8(); break;
	case 0xc7: _mov_wd16(); break;
	case 0xc8: _enter(); break;
	case 0xc9: _leav(); break;	// _leave()
	case 0xca: _retf_d16(); break;
	case 0xcb: _retf(); break;
	case 0xcc: _int3(); break;
	case 0xcd: _int(); break;
	case 0xce: _into(); break;
	case 0xcf: _iret(); break;
	case 0xd0: _rotshft_b(); break;
	case 0xd1: _rotshft_w(); break;
	case 0xd2: _rotshft_bcl(); break;
	case 0xd3: _rotshft_wcl(); break;
	case 0xd4: _aam(); break;
	case 0xd5: _aad(); break;
	case 0xd6: _setalc(); break;
	case 0xd7: _xlat(); break;
	case 0xd8: _escape(); break;
	case 0xd9: _escape(); break;
	case 0xda: _escape(); break;
	case 0xdb: _escape(); break;
	case 0xdc: _escape(); break;
	case 0xdd: _escape(); break;
	case 0xde: _escape(); break;
	case 0xdf: _escape(); break;
	case 0xe0: _loopne(); break;
	case 0xe1: _loope(); break;
	case 0xe2: _loop(); break;
	case 0xe3: _jcxz(); break;
	case 0xe4: _inal(); break;
	case 0xe5: _inax(); break;
	case 0xe6: _outal(); break;
	case 0xe7: _outax(); break;
	case 0xe8: _call_d16(); break;
	case 0xe9: _jmp_d16(); break;
	case 0xea: _jmp_far(); break;
	case 0xeb: _jmp_d8(); break;
	case 0xec: _inaldx(); break;
	case 0xed: _inaxdx(); break;
	case 0xee: _outdxal(); break;
	case 0xef: _outdxax(); break;
	case 0xf0: _lock(); break;
	case 0xf1: _invalid(); break;
	case 0xf2: _rep(0); break;	// repne
	case 0xf3: _rep(1); break;	// repe
	case 0xf4: _hlt(); break;
	case 0xf5: _cmc(); break;
	case 0xf6: _opf6(); break;
	case 0xf7: _opf7(); break;
	case 0xf8: _clc(); break;
	case 0xf9: _stc(); break;
	case 0xfa: _cli(); break;
	case 0xfb: _sti(); break;
	case 0xfc: _cld(); break;
	case 0xfd: _std(); break;
	case 0xfe: _opfe(); break;
	case 0xff: _opff(); break;
	}
};

inline void _add_br8()	// Opcode 0x00
{
	DEF_br8(dst, src);
	ADDB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _add_wr16()	// Opcode 0x01
{
	DEF_wr16(dst, src);
	ADDW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _add_r8b()	// Opcode 0x02
{
	DEF_r8b(dst, src);
	ADDB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _add_r16w()	// Opcode 0x03
{
	DEF_r16w(dst, src);
	ADDW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _add_ald8()	// Opcode 0x04
{
	DEF_ald8(dst, src);
	ADDB(dst, src);
	regs.b[AL] = dst;
}

inline void _add_axd16()	// Opcode 0x05
{
	DEF_axd16(dst, src);
	ADDW(dst, src);
	regs.w[AX] = dst;
}

inline void _push_es()	// Opcode 0x06
{
	PUSH16(sregs[ES]);
}

inline void _pop_es()	// Opcode 0x07
{
#ifdef HAS_I286
	uint16 tmp = POP16();
	i286_data_descriptor(ES, tmp);
#else
	sregs[ES] = POP16();
	base[ES] = SegBase(ES);
#endif
}

inline void _or_br8()	// Opcode 0x08
{
	DEF_br8(dst, src);
	ORB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _or_wr16()	// Opcode 0x09
{
	DEF_wr16(dst, src);
	ORW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _or_r8b()	// Opcode 0x0a
{
	DEF_r8b(dst, src);
	ORB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _or_r16w()	// Opcode 0x0b
{
	DEF_r16w(dst, src);
	ORW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _or_ald8()	// Opcode 0x0c
{
	DEF_ald8(dst, src);
	ORB(dst, src);
	regs.b[AL] = dst;
}

inline void _or_axd16()	// Opcode 0x0d
{
	DEF_axd16(dst, src);
	ORW(dst, src);
	regs.w[AX] = dst;
}

inline void _push_cs()	// Opcode 0x0e
{
	PUSH16(sregs[CS]);
}

inline void _op0f()
{
#ifdef HAS_I286
	unsigned next = FETCHOP();
	uint16 ModRM, tmp;
	uint32 addr;
	
	switch(next) {
	case 0:
		ModRM = FETCHOP();
		switch(ModRM & 0x38) {
		case 0:	// sldt
			if(!PM) {
				interrupt(ILLEGAL_INSTRUCTION);
			}
			PutRMWord(ModRM, ldtr_sel);
			break;
		case 8:	// str
			if(!PM) {
				interrupt(ILLEGAL_INSTRUCTION);
			}
			PutRMWord(ModRM, tr_sel);
			break;
		case 0x10:	// lldt
			if(!PM) {
				interrupt(ILLEGAL_INSTRUCTION);
			}
			if(PM && (CPL != 0)) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			ldtr_sel = GetRMWord(ModRM);
			if((ldtr_sel & ~7) >= gdtr_limit) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			ldtr_limit = RM16(gdtr_base + (ldtr_sel & ~7));
			ldtr_base = RM16(gdtr_base + (ldtr_sel & ~7) + 2) | (RM16(gdtr_base + (ldtr_sel & ~7) + 4) << 16);
			ldtr_base &= 0xffffff;
			break;
		case 0x18:	// ltr
			if(!PM) {
				interrupt(ILLEGAL_INSTRUCTION);
			}
			if(CPL!= 0) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			tr_sel = GetRMWord(ModRM);
			if((tr_sel & ~7) >= gdtr_limit) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			tr_limit = RM16(gdtr_base + (tr_sel & ~7));
			tr_base = RM16(gdtr_base + (tr_sel & ~7) + 2) | (RM16(gdtr_base + (tr_sel & ~7) + 4) << 16);
			tr_base &= 0xffffff;
			break;
		case 0x20:	// verr
			if(!PM) {
				interrupt(ILLEGAL_INSTRUCTION);
			}
			tmp = GetRMWord(ModRM);
			if(tmp & 4) {
				ZeroVal = (((tmp & ~7) < ldtr_limit) && READABLE(RM8(ldtr_base + (tmp & ~7) + 5)));
			} else {
				ZeroVal = (((tmp & ~7) < gdtr_limit) && READABLE(RM8(gdtr_base + (tmp & ~7) + 5)));
			}
			break;
		case 0x28: // verw
			if(!PM) {
				interrupt(ILLEGAL_INSTRUCTION);
			}
			tmp = GetRMWord(ModRM);
			if(tmp & 4) {
				ZeroVal = (((tmp & ~7) < ldtr_limit) && WRITEABLE(RM8(ldtr_base + (tmp & ~7) + 5)));
			} else {
				ZeroVal = (((tmp & ~7) < gdtr_limit) && WRITEABLE(RM8(gdtr_base + (tmp & ~7) + 5)));
			}
			break;
		default:
			interrupt(ILLEGAL_INSTRUCTION);
			break;
		}
		break;
	case 1:
		ModRM = FETCHOP();
		switch(ModRM & 0x38) {
		case 0:	// sgdt
			PutRMWord(ModRM, gdtr_limit);
			PutRMWordOfs(2, gdtr_base & 0xffff);
			PutRMByteOfs(4, gdtr_base >> 16);
			break;
		case 8:	// sidt
			PutRMWord(ModRM, idtr_limit);
			PutRMWordOfs(2, idtr_base & 0xffff);
			PutRMByteOfs(4, idtr_base >> 16);
			break;
		case 0x10:	// lgdt
			if(PM && (CPL!= 0)) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			gdtr_limit = GetRMWord(ModRM);
			gdtr_base = GetRMWordOfs(2) | (GetRMByteOfs(4) << 16);
			break;
		case 0x18:	// lidt
			if(PM && (CPL!= 0)) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			idtr_limit = GetRMWord(ModRM);
			idtr_base = GetRMWordOfs(2) | (GetRMByteOfs(4) << 16);
			break;
		case 0x20:	// smsw
			PutRMWord(ModRM, msw);
			break;
		case 0x30:	// lmsw
			if(PM && (CPL!= 0)) {
				interrupt(GENERAL_PROTECTION_FAULT);
			}
			msw = (msw & 1) | GetRMWord(ModRM);
			break;
		default:
			interrupt(ILLEGAL_INSTRUCTION);
			break;
		}
		break;
	case 2:	// LAR
		ModRM = FETCHOP();
		tmp = GetRMWord(ModRM);
		ZeroVal = i286_selector_okay(tmp);
		if(ZeroVal) {
			RegWord(ModRM) = tmp;
		}
		break;
	case 3:	// LSL
		if(!PM) {
			interrupt(ILLEGAL_INSTRUCTION);
		}
		ModRM = FETCHOP();
		tmp = GetRMWord(ModRM);
		ZeroVal = i286_selector_okay(tmp);
		if(ZeroVal) {
			if(tmp & 4) {
				addr = ldtr_base + (tmp & ~7);
			} else {
				addr = gdtr_base + (tmp & ~7);
			}
			RegWord(ModRM) = RM16(addr);
		}
		break;
	case 6:	// clts
		if(PM && (CPL!= 0)) {
			interrupt(GENERAL_PROTECTION_FAULT);
		}
		msw = ~8;
		break;
	default:
		interrupt(ILLEGAL_INSTRUCTION);
		break;
	}
#elif defined(HAS_V30)
	unsigned code = FETCH8();
	unsigned ModRM, tmp1, tmp2;
	
	switch(code) {
	case 0x10:	// 0F 10 47 30 - TEST1 [bx+30h], cl
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = regs.b[CL] & 7;
		ZeroVal = tmp1 & bytes[tmp2] ? 1 : 0;
		// SetZF(tmp1 & (1 << tmp2));
		break;
	case 0x11:	// 0F 11 47 30 - TEST1 [bx+30h], cl
		ModRM = FETCH8();
		// tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = regs.b[CL] & 0xf;
		ZeroVal = tmp1 & bytes[tmp2] ? 1 : 0;
		// SetZF(tmp1 & (1 << tmp2));
		break;
	case 0x12:	// 0F 12 [mod:000:r/m] - CLR1 reg/m8, cl
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = regs.b[CL] & 7;
		tmp1 &= ~(bytes[tmp2]);
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x13:	// 0F 13 [mod:000:r/m] - CLR1 reg/m16, cl
		ModRM = FETCH8();
		// tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = regs.b[CL] & 0xf;
		tmp1 &= ~(bytes[tmp2]);
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x14:	// 0F 14 47 30 - SET1 [bx+30h], cl
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = regs.b[CL] & 7;
		tmp1 |= (bytes[tmp2]);
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x15:	// 0F 15 C6 - SET1 si, cl
		ModRM = FETCH8();
		// tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = regs.b[CL] & 0xf;
		tmp1 |= (bytes[tmp2]);
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x16:	// 0F 16 C6 - NOT1 si, cl
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = regs.b[CL] & 7;
		if(tmp1 & bytes[tmp2]) {
			tmp1 &= ~(bytes[tmp2]);
		} else {
			tmp1 |= (bytes[tmp2]);
		}
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x17:	// 0F 17 C6 - NOT1 si, cl
		ModRM = FETCH8();
		// tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = regs.b[CL] & 0xf;
		if(tmp1 & bytes[tmp2]) {
			tmp1 &= ~(bytes[tmp2]);
		} else {
			tmp1 |= (bytes[tmp2]);
		}
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x18:	// 0F 18 XX - TEST1 [bx+30h], 07
		ModRM = FETCH8();
		// tmp1 = GetRMByte(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 0xf;
		ZeroVal = tmp1 & (bytes[tmp2]) ? 1 : 0;
		// SetZF(tmp1 & (1 << tmp2));
		break;
	case 0x19:	// 0F 19 XX - TEST1 [bx+30h], 07
		ModRM = FETCH8();
		// tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 0xf;
		ZeroVal = tmp1 & (bytes[tmp2]) ? 1 : 0;
		// SetZF(tmp1 & (1 << tmp2));
		break;
	case 0x1a:	// 0F 1A 06 - CLR1 si, cl
		ModRM = FETCH8();
		// tmp1 = GetRMByte(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 7;
		tmp1 &= ~(bytes[tmp2]);
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x1B:	// 0F 1B 06 - CLR1 si, cl
		ModRM = FETCH8();
		// tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 0xf;
		tmp1 &= ~(bytes[tmp2]);
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x1C:	// 0F 1C 47 30 - SET1 [bx+30h], cl
		ModRM = FETCH8();
		// tmp1 = GetRMByte(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 7;
		tmp1 |= (bytes[tmp2]);
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x1D:	// 0F 1D C6 - SET1 si, cl
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 0xf;
		tmp1 |= (bytes[tmp2]);
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x1e:	// 0F 1e C6 - NOT1 si, 07
		ModRM = FETCH8();
		// tmp1 = GetRMByte(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 7;
		if(tmp1 & bytes[tmp2]) {
			tmp1 &= ~(bytes[tmp2]);
		} else {
			tmp1 |= (bytes[tmp2]);
		}
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x1f:	// 0F 1f C6 - NOT1 si, 07
		ModRM = FETCH8();
		//tmp1 = GetRMWord(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.w[mod_rm16[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM16(EA);
		}
		tmp2 = FETCH8();
		tmp2 &= 0xf;
		if(tmp1 & bytes[tmp2]) {
			tmp1 &= ~(bytes[tmp2]);
		} else {
			tmp1 |= (bytes[tmp2]);
		}
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x20:	// 0F 20 59 - add4s
		{
			// length in words !
			int cnt = (regs.b[CL] + 1) / 2;
			unsigned di = regs.w[DI];
			unsigned si = regs.w[SI];
			
			ZeroVal = 1;
			CarryVal = 0;	// NOT ADC
			for(int i = 0; i < cnt; i++) {
				tmp1 = RM8(DS, si);
				tmp2 = RM8(ES, di);
				int v1 = (tmp1 >> 4) * 10 + (tmp1 & 0xf);
				int v2 = (tmp2 >> 4) * 10 + (tmp2 & 0xf);
				int result = v1 + v2 + CarryVal;
				CarryVal = result > 99 ? 1 : 0;
				result = result % 100;
				v1 = ((result / 10) << 4) | (result % 10);
				WM8(ES, di, v1);
				if(v1) {
					ZeroVal = 0;
				}
				si++;
				di++;
			}
			OverVal = CarryVal;
		}
		break;
	case 0x22:	// 0F 22 59 - sub4s
		{
			int cnt = (regs.b[CL] + 1) / 2;
			unsigned di = regs.w[DI];
			unsigned si = regs.w[SI];
			
			ZeroVal = 1;
			CarryVal = 0;	// NOT ADC
			for(int i = 0; i < cnt; i++) {
				tmp1 = RM8(ES, di);
				tmp2 = RM8(DS, si);
				int v1 = (tmp1 >> 4) * 10 + (tmp1 & 0xf);
				int v2 = (tmp2 >> 4) * 10 + (tmp2 & 0xf), result;
				if(v1 < (v2 + CarryVal)) {
					v1 += 100;
					result = v1 - (v2 + CarryVal);
					CarryVal = 1;
				} else {
					result = v1 - (v2 + CarryVal);
					CarryVal = 0;
				}
				v1 = ((result / 10) << 4) | (result % 10);
				WM8(ES, di, v1);
				if(v1) {
					ZeroVal = 0;
				}
				si++;
				di++;
			}
			OverVal = CarryVal;
		}
		break;
	case 0x25:
		break;
	case 0x26:	// 0F 22 59 - cmp4s
		{
			int cnt = (regs.b[CL] + 1) / 2;
			unsigned di = regs.w[DI];
			unsigned si = regs.w[SI];
			
			ZeroVal = 1;
			CarryVal = 0;	// NOT ADC
			for(int i = 0; i < cnt; i++) {
				tmp1 = RM8(ES, di);
				tmp2 = RM8(DS, si);
				int v1 = (tmp1 >> 4) * 10 + (tmp1 & 0xf);
				int v2 = (tmp2 >> 4) * 10 + (tmp2 & 0xf), result;
				if(v1 < (v2 + CarryVal)) {
					v1 += 100;
					result = v1 - (v2 + CarryVal);
					CarryVal = 1;
				} else {
					result = v1 - (v2 + CarryVal);
					CarryVal = 0;
				}
				v1 = ((result / 10) << 4) | (result % 10);
				// WM8(ES, di, v1);	// no store, only compare
				if(v1) {
					ZeroVal = 0;
				}
				si++;
				di++;
			}
			OverVal = CarryVal;
		}
		break;
	case 0x28:	// 0F 28 C7 - ROL4 bh
		ModRM = FETCH8();
		// tmp1 = GetRMByte(ModRM);
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp1 <<= 4;
		tmp1 |= regs.b[AL] & 0xf;
		regs.b[AL] = (regs.b[AL] & 0xf0) | ((tmp1 >> 8) & 0xf);
		tmp1 &= 0xff;
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x29:	// 0F 29 C7 - ROL4 bx
		ModRM = FETCH8();
		break;
	case 0x2A:	// 0F 2a c2 - ROR4 bh
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		tmp2 = (regs.b[AL] & 0xf) << 4;
		regs.b[AL] = (regs.b[AL] & 0xf0) | (tmp1 & 0xf);
		tmp1 = tmp2 | (tmp1 >> 4);
		PutbackRMByte(ModRM, tmp1);
		break;
	case 0x2B:	// 0F 2b c2 - ROR4 bx
		ModRM = FETCH8();
		break;
	case 0x2D:	// 0Fh 2Dh < 1111 1RRR>
		ModRM = FETCH8();
		break;
	case 0x31:	// 0F 31 [mod:reg:r/m] - INS reg8, reg8 or INS reg8, imm4
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		break;
	case 0x33:	// 0F 33 [mod:reg:r/m] - EXT reg8, reg8 or EXT reg8, imm4
		ModRM = FETCH8();
		if(ModRM >= 0xc0) {
			tmp1 = regs.b[mod_rm8[ModRM]];
		} else {
			GetEA(ModRM);
			tmp1 = RM8(EA);
		}
		break;
	case 0x91:
		break;
	case 0x94:
		ModRM = FETCH8();
		break;
	case 0x95:
		ModRM = FETCH8();
		break;
	case 0xbe:
		break;
	case 0xe0:
		ModRM = FETCH8();
		break;
	case 0xf0:
		ModRM = FETCH8();
		break;
	case 0xff:	// 0F ff imm8 - BRKEM
		ModRM = FETCH8();
		interrupt(ModRM);
		break;
	}
#else
	_invalid();
#endif
}

inline void _adc_br8()	// Opcode 0x10
{
	DEF_br8(dst, src);
	src += CF;
	ADDB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _adc_wr16()	// Opcode 0x11
{
	DEF_wr16(dst, src);
	src += CF;
	ADDW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _adc_r8b()	// Opcode 0x12
{
	DEF_r8b(dst, src);
	src += CF;
	ADDB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _adc_r16w()	// Opcode 0x13
{
	DEF_r16w(dst, src);
	src += CF;
	ADDW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _adc_ald8()	// Opcode 0x14
{
	DEF_ald8(dst, src);
	src += CF;
	ADDB(dst, src);
	regs.b[AL] = dst;
}

inline void _adc_axd16()	// Opcode 0x15
{
	DEF_axd16(dst, src);
	src += CF;
	ADDW(dst, src);
	regs.w[AX] = dst;
}

inline void _push_ss()	// Opcode 0x16
{
	PUSH16(sregs[SS]);
}

inline void _pop_ss()	// Opcode 0x17
{
#ifdef HAS_I286
	uint16 tmp = POP16();
	i286_data_descriptor(SS, tmp);
#else
	sregs[SS] = POP16();
	base[SS] = SegBase(SS);
#endif
	op(FETCHOP());
}

inline void _sbb_br8()	// Opcode 0x18
{
	DEF_br8(dst, src);
	src += CF;
	SUBB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _sbb_wr16()	// Opcode 0x19
{
	DEF_wr16(dst, src);
	src += CF;
	SUBW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _sbb_r8b()	// Opcode 0x1a
{
	DEF_r8b(dst, src);
	src += CF;
	SUBB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _sbb_r16w()	// Opcode 0x1b
{
	DEF_r16w(dst, src);
	src += CF;
	SUBW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _sbb_ald8()	// Opcode 0x1c
{
	DEF_ald8(dst, src);
	src += CF;
	SUBB(dst, src);
	regs.b[AL] = dst;
}

inline void _sbb_axd16()	// Opcode 0x1d
{
	DEF_axd16(dst, src);
	src += CF;
	SUBW(dst, src);
	regs.w[AX] = dst;
}

inline void _push_ds()	// Opcode 0x1e
{
	PUSH16(sregs[DS]);
}

inline void _pop_ds()	// Opcode 0x1f
{
#ifdef HAS_I286
	uint16 tmp = POP16();
	i286_data_descriptor(DS, tmp);
#else
	sregs[DS] = POP16();
	base[DS] = SegBase(DS);
#endif
}

inline void _and_br8()	// Opcode 0x20
{
	DEF_br8(dst, src);
	ANDB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _and_wr16()	// Opcode 0x21
{
	DEF_wr16(dst, src);
	ANDW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _and_r8b()	// Opcode 0x22
{
	DEF_r8b(dst, src);
	ANDB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _and_r16w()	// Opcode 0x23
{
	DEF_r16w(dst, src);
	ANDW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _and_ald8()	// Opcode 0x24
{
	DEF_ald8(dst, src);
	ANDB(dst, src);
	regs.b[AL] = dst;
}

inline void _and_axd16()	// Opcode 0x25
{
	DEF_axd16(dst, src);
	ANDW(dst, src);
	regs.w[AX] = dst;
}

inline void _es()	// Opcode 0x26
{
	seg_prefix = 1;
	prefix_base = base[ES];
	op(FETCHOP());
}

inline void _daa()	// Opcode 0x27
{
	if(AF || ((regs.b[AL] & 0xf) > 9)) {
		int tmp = regs.b[AL] + 6;
		regs.b[AL] = tmp;
		AuxVal = 1;
		CarryVal |= tmp & 0x100;
	}
	if(CF || (regs.b[AL] > 0x9f)) {
		regs.b[AL] += 0x60;
		CarryVal = 1;
	}
	SetSZPF_Byte(regs.b[AL]);
}

inline void _sub_br8()	// Opcode 0x28
{
	DEF_br8(dst, src);
	SUBB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _sub_wr16()	// Opcode 0x29
{
	DEF_wr16(dst, src);
	SUBW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _sub_r8b()	// Opcode 0x2a
{
	DEF_r8b(dst, src);
	SUBB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _sub_r16w()	// Opcode 0x2b
{
	DEF_r16w(dst, src);
	SUBW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _sub_ald8()	// Opcode 0x2c
{
	DEF_ald8(dst, src);
	SUBB(dst, src);
	regs.b[AL] = dst;
}

inline void _sub_axd16()	// Opcode 0x2d
{
	DEF_axd16(dst, src);
	SUBW(dst, src);
	regs.w[AX] = dst;
}

inline void _cs()	// Opcode 0x2e
{
	seg_prefix = 1;
	prefix_base = base[CS];
	op(FETCHOP());
}

inline void _das()	// Opcode 0x2f
{
	uint8 tmpAL = regs.b[AL];
	if(AF || ((regs.b[AL] & 0xf) > 9)) {
		int tmp = regs.b[AL] - 6;
		regs.b[AL] = tmp;
		AuxVal = 1;
		CarryVal |= tmp & 0x100;
	}
	if(CF || (tmpAL > 0x9f)) {
		regs.b[AL] -= 0x60;
		CarryVal = 1;
	}
	SetSZPF_Byte(regs.b[AL]);
}

inline void _xor_br8()	// Opcode 0x30
{
	DEF_br8(dst, src);
	XORB(dst, src);
	PutbackRMByte(ModRM, dst);
}

inline void _xor_wr16()	// Opcode 0x31
{
	DEF_wr16(dst, src);
	XORW(dst, src);
	PutbackRMWord(ModRM, dst);
}

inline void _xor_r8b()	// Opcode 0x32
{
	DEF_r8b(dst, src);
	XORB(dst, src);
	RegByte(ModRM) = dst;
}

inline void _xor_r16w()	// Opcode 0x33
{
	DEF_r16w(dst, src);
	XORW(dst, src);
	RegWord(ModRM) = dst;
}

inline void _xor_ald8()	// Opcode 0x34
{
	DEF_ald8(dst, src);
	XORB(dst, src);
	regs.b[AL] = dst;
}

inline void _xor_axd16()	// Opcode 0x35
{
	DEF_axd16(dst, src);
	XORW(dst, src);
	regs.w[AX] = dst;
}

inline void _ss()	// Opcode 0x36
{
	seg_prefix = 1;
	prefix_base = base[SS];
	op(FETCHOP());
}

inline void _aaa()	// Opcode 0x37
{
	uint8 ALcarry = 1;
	if(regs.b[AL]>0xf9) {
		ALcarry = 2;
	}
	if(AF || ((regs.b[AL] & 0xf) > 9)) {
		regs.b[AL] += 6;
		regs.b[AH] += ALcarry;
		AuxVal = 1;
		CarryVal = 1;
	} else {
		AuxVal = 0;
		CarryVal = 0;
	}
	regs.b[AL] &= 0x0F;
}

inline void _cmp_br8()	// Opcode 0x38
{
	DEF_br8(dst, src);
	SUBB(dst, src);
}

inline void _cmp_wr16()	// Opcode 0x39
{
	DEF_wr16(dst, src);
	SUBW(dst, src);
}

inline void _cmp_r8b()	// Opcode 0x3a
{
	DEF_r8b(dst, src);
	SUBB(dst, src);
}

inline void _cmp_r16w()	// Opcode 0x3b
{
	DEF_r16w(dst, src);
	SUBW(dst, src);
}

inline void _cmp_ald8()	// Opcode 0x3c
{
	DEF_ald8(dst, src);
	SUBB(dst, src);
}

inline void _cmp_axd16()	// Opcode 0x3d
{
	DEF_axd16(dst, src);
	SUBW(dst, src);
}

inline void _ds()	// Opcode 0x3e
{
	seg_prefix = 1;
	prefix_base = base[DS];
	op(FETCHOP());
}

inline void _aas()	// Opcode 0x3f
{
	uint8 ALcarry = 1;
	if(regs.b[AL]>0xf9) {
		ALcarry = 2;
	}
	if(AF || ((regs.b[AL] & 0xf) > 9)) {
		regs.b[AL] -= 6;
		regs.b[AH] -= 1;
		AuxVal = 1;
		CarryVal = 1;
	} else {
		AuxVal = 0;
		CarryVal = 0;
	}
	regs.b[AL] &= 0x0F;
}

#define IncWordReg(reg) { \
	unsigned tmp = (unsigned)regs.w[reg]; \
	unsigned tmp1 = tmp + 1; \
	SetOFW_Add(tmp1, tmp, 1); \
	SetAF(tmp1, tmp, 1); \
	SetSZPF_Word(tmp1); \
	regs.w[reg] = tmp1; \
}

inline void _inc_ax()	// Opcode 0x40
{
	IncWordReg(AX);
}

inline void _inc_cx()	// Opcode 0x41
{
	IncWordReg(CX);
}

inline void _inc_dx()	// Opcode 0x42
{
	IncWordReg(DX);
}

inline void _inc_bx()	// Opcode 0x43
{
	IncWordReg(BX);
}

inline void _inc_sp()	// Opcode 0x44
{
	IncWordReg(SP);
}

inline void _inc_bp()	// Opcode 0x45
{
	IncWordReg(BP);
}

inline void _inc_si()	// Opcode 0x46
{
	IncWordReg(SI);
}

inline void _inc_di()	// Opcode 0x47
{
	IncWordReg(DI);
}

#define DecWordReg(reg) { \
	unsigned tmp = (unsigned)regs.w[reg]; \
	unsigned tmp1 = tmp - 1; \
	SetOFW_Sub(tmp1, 1, tmp); \
	SetAF(tmp1, tmp, 1); \
	SetSZPF_Word(tmp1); \
	regs.w[reg] = tmp1; \
}

inline void _dec_ax()	// Opcode 0x48
{
	DecWordReg(AX);
}

inline void _dec_cx()	// Opcode 0x49
{
	DecWordReg(CX);
}

inline void _dec_dx()	// Opcode 0x4a
{
	DecWordReg(DX);
}

inline void _dec_bx()	// Opcode 0x4b
{
	DecWordReg(BX);
}

inline void _dec_sp()	// Opcode 0x4c
{
	DecWordReg(SP);
}

inline void _dec_bp()	// Opcode 0x4d
{
	DecWordReg(BP);
}

inline void _dec_si()	// Opcode 0x4e
{
	DecWordReg(SI);
}

inline void _dec_di()	// Opcode 0x4f
{
	DecWordReg(DI);
}

inline void _push_ax()	// Opcode 0x50
{
	PUSH16(regs.w[AX]);
}

inline void _push_cx()	// Opcode 0x51
{
	PUSH16(regs.w[CX]);
}

inline void _push_dx()	// Opcode 0x52
{
	PUSH16(regs.w[DX]);
}

inline void _push_bx()	// Opcode 0x53
{
	PUSH16(regs.w[BX]);
}

inline void _push_sp()	// Opcode 0x54
{
#ifdef HAS_I286
	PUSH16(regs.w[SP]);
#else
	PUSH16(regs.w[SP] - 2);
#endif
}

inline void _push_bp()	// Opcode 0x55
{
	PUSH16(regs.w[BP]);
}

inline void _push_si()	// Opcode 0x56
{
	PUSH16(regs.w[SI]);
}

inline void _push_di()	// Opcode 0x57
{
	PUSH16(regs.w[DI]);
}

inline void _pop_ax()	// Opcode 0x58
{
	regs.w[AX] = POP16();
}

inline void _pop_cx()	// Opcode 0x59
{
	regs.w[CX] = POP16();
}

inline void _pop_dx()	// Opcode 0x5a
{
	regs.w[DX] = POP16();
}

inline void _pop_bx()	// Opcode 0x5b
{
	regs.w[BX] = POP16();
}

inline void _pop_sp()	// Opcode 0x5c
{
	regs.w[SP] = POP16();
}

inline void _pop_bp()	// Opcode 0x5d
{
	regs.w[BP] = POP16();
}

inline void _pop_si()	// Opcode 0x5e
{
	regs.w[SI] = POP16();
}

inline void _pop_di()	// Opcode 0x5f
{
	regs.w[DI] = POP16();
}

inline void _pusha()	// Opcode 0x60
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned tmp = regs.w[SP];
	PUSH16(regs.w[AX]);
	PUSH16(regs.w[CX]);
	PUSH16(regs.w[DX]);
	PUSH16(regs.w[BX]);
	PUSH16(tmp);
	PUSH16(regs.w[BP]);
	PUSH16(regs.w[SI]);
	PUSH16(regs.w[DI]);
#else
	_invalid();
#endif
}

inline void _popa()	// Opcode 0x61
{
#if defined(HAS_I286) || defined(HAS_V30)
	regs.w[DI] = POP16();
	regs.w[SI] = POP16();
	regs.w[BP] = POP16();
	unsigned tmp = POP16();
	regs.w[BX] = POP16();
	regs.w[DX] = POP16();
	regs.w[CX] = POP16();
	regs.w[AX] = POP16();
#else
	_invalid();
#endif
}

inline void _bound()	// Opcode 0x62
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned ModRM = FETCHOP();
	int low = (int16)GetRMWord(ModRM);
	int high = (int16)GetNextRMWord();
	int tmp = (int16)RegWord(ModRM);
	if(tmp < low || tmp>high) {
		PC-= 2;
		interrupt(5);
	}
#else
	_invalid();
#endif
}

inline void _arpl()	// Opcode 0x63
{
#ifdef HAS_I286
	if(PM) {
		uint16 ModRM = FETCHOP();
		uint16 tmp = GetRMWord(ModRM);
		ZeroVal = i286_selector_okay(RegWord(ModRM)) && i286_selector_okay(RegWord(ModRM)) && ((tmp & 3) < (RegWord(ModRM) & 3));
		if(ZeroVal) {
			PutbackRMWord(ModRM, (tmp & ~3) | (RegWord(ModRM) & 3));
		}
	} else
		interrupt(ILLEGAL_INSTRUCTION);
#else
	_invalid();
#endif
}

#if 0
inline void _brkn()	// Opcode 0x63 BRKN - Break to Native Mode
{
	unsigned vector = FETCH8();
}
#endif

inline void _repc(int flagval)
{
#ifdef HAS_V30
	unsigned next = FETCHOP();
	unsigned cnt = regs.w[CX];
	
	switch(next) {
	case 0x26:	// ES:
		seg_prefix = 1;
		prefix_base = base[ES];
		_repc(flagval);
		break;
	case 0x2e:	// CS:
		seg_prefix = 1;
		prefix_base = base[CS];
		_repc(flagval);
		break;
	case 0x36:	// SS:
		seg_prefix = 1;
		prefix_base = base[SS];
		_repc(flagval);
		break;
	case 0x3e:	// DS:
		seg_prefix = 1;
		prefix_base = base[DS];
		_repc(flagval);
		break;
	case 0x6c:	// REP INSB
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_insb();
		}
		regs.w[CX] = cnt;
		break;
	case 0x6d:	// REP INSW
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_insw();
		}
		regs.w[CX] = cnt;
		break;
	case 0x6e:	// REP OUTSB
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_outsb();
		}
		regs.w[CX] = cnt;
		break;
	case 0x6f:	// REP OUTSW
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_outsw();
		}
		regs.w[CX] = cnt;
		break;
	case 0xa4:	// REP MOVSB
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_movsb();
		}
		regs.w[CX] = cnt;
		break;
	case 0xa5:	// REP MOVSW
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_movsw();
		}
		regs.w[CX] = cnt;
		break;
	case 0xa6:	// REP(N)E CMPSB
		for(ZeroVal = !flagval; (ZF == flagval) && (CF == flagval) && (cnt > 0); cnt--) {
			_cmpsb();
		}
		regs.w[CX] = cnt;
		break;
	case 0xa7:	// REP(N)E CMPSW
		for(ZeroVal = !flagval; (ZF == flagval) && (CF == flagval) && (cnt > 0); cnt--) {
			_cmpsw();
		}
		regs.w[CX] = cnt;
		break;
	case 0xaa:	// REP STOSB
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_stosb();
		}
		regs.w[CX] = cnt;
		break;
	case 0xab:	// REP STOSW
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_stosw();
		}
		regs.w[CX] = cnt;
		break;
	case 0xac:	// REP LODSB
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_lodsb();
		}
		regs.w[CX] = cnt;
		break;
	case 0xad:	// REP LODSW
		for(; (CF == flagval) && (cnt > 0); cnt--) {
			_lodsw();
		}
		regs.w[CX] = cnt;
		break;
	case 0xae:	// REP(N)E SCASB
		for(ZeroVal = !flagval; (ZF == flagval) && (CF == flagval) && (cnt > 0); cnt--) {
			_scasb();
		}
		regs.w[CX] = cnt;
		break;
	case 0xaf:	// REP(N)E SCASW
		for(ZeroVal = !flagval; (ZF == flagval) && (CF == flagval) && (cnt > 0); cnt--) {
			_scasw();
		}
		regs.w[CX] = cnt;
		break;
	default:
		op(next);
	}
#else
	_invalid();
#endif
}

inline void _push_d16()	// Opcode 0x68
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned tmp = FETCH8();
	tmp += FETCH8() << 8;
	PUSH16(tmp);
#else
	_invalid();
#endif
}

inline void _imul_d16()	// Opcode 0x69
{
#if defined(HAS_I286) || defined(HAS_V30)
	DEF_r16w(dst, src);
	unsigned src2 = FETCH8();
	src += (FETCH8() << 8);
	dst = (int32)((int16)src) * (int32)((int16)src2);
	CarryVal = OverVal = (((int32)dst) >> 15 != 0) && (((int32)dst) >> 15 != -1);
	RegWord(ModRM) = (uint16)dst;
#else
	_invalid();
#endif
}

inline void _push_d8()	// Opcode 0x6a
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned tmp = (uint16)((int16)((int8)FETCH8()));
	PUSH16(tmp);
#else
	_invalid();
#endif
}

inline void _imul_d8()	// Opcode 0x6b
{
#if defined(HAS_I286) || defined(HAS_V30)
	DEF_r16w(dst, src);
	unsigned src2 = (uint16)((int16)((int8)FETCH8()));
	dst = (int32)((int16)src) * (int32)((int16)src2);
	CarryVal = OverVal = (((int32)dst) >> 15 != 0) && (((int32)dst) >> 15 != -1);
	RegWord(ModRM) = (uint16)dst;
#else
	_invalid();
#endif
}

inline void _insb()	// Opcode 0x6c
{
#if defined(HAS_I286) || defined(HAS_V30)
	WM8(ES, regs.w[DI], IN8(regs.w[DX]));
	regs.w[DI] += DirVal;
#else
	_invalid();
#endif
}

inline void _insw()	// Opcode 0x6d
{
#if defined(HAS_I286) || defined(HAS_V30)
	WM16(ES, regs.w[DI], IN16(regs.w[DX]));
	regs.w[DI] += 2 * DirVal;
#else
	_invalid();
#endif
}

inline void _outsb()	// Opcode 0x6e
{
#if defined(HAS_I286) || defined(HAS_V30)
	OUT8(regs.w[DX], RM8(DS, regs.w[SI]));
	regs.w[SI] += DirVal; // GOL 11/27/01
#else
	_invalid();
#endif
}

inline void _outsw()	// Opcode 0x6f
{
#if defined(HAS_I286) || defined(HAS_V30)
	OUT16(regs.w[DX], RM16(DS, regs.w[SI]));
	regs.w[SI] += 2 * DirVal;
#else
	_invalid();
#endif
}

inline void _jo()	// Opcode 0x70
{
	int tmp = (int)((int8)FETCH8());
	if(OF) {
		PC += tmp;
	}
}

inline void _jno()	// Opcode 0x71
{
	int tmp = (int)((int8)FETCH8());
	if(!OF) {
		PC += tmp;
	}
}

inline void _jb()	// Opcode 0x72
{
	int tmp = (int)((int8)FETCH8());
	if(CF) {
		PC += tmp;
	}
}

inline void _jnb()	// Opcode 0x73
{
	int tmp = (int)((int8)FETCH8());
	if(!CF) {
		PC += tmp;
	}
}

inline void _jz()	// Opcode 0x74
{
	int tmp = (int)((int8)FETCH8());
	if(ZF) {
		PC += tmp;
	}
}

inline void _jnz()	// Opcode 0x75
{
	int tmp = (int)((int8)FETCH8());
	if(!ZF) {
		PC += tmp;
	}
}

inline void _jbe()	// Opcode 0x76
{
	int tmp = (int)((int8)FETCH8());
	if(CF || ZF) {
		PC += tmp;
	}
}

inline void _jnbe()	// Opcode 0x77
{
	int tmp = (int)((int8)FETCH8());
	if(!(CF || ZF)) {
		PC += tmp;
	}
}

inline void _js()	// Opcode 0x78
{
	int tmp = (int)((int8)FETCH8());
	if(SF) {
		PC += tmp;
	}
}

inline void _jns()	// Opcode 0x79
{
	int tmp = (int)((int8)FETCH8());
	if(!SF) {
		PC += tmp;
	}
}

inline void _jp()	// Opcode 0x7a
{
	int tmp = (int)((int8)FETCH8());
	if(PF) {
		PC += tmp;
	}
}

inline void _jnp()	// Opcode 0x7b
{
	int tmp = (int)((int8)FETCH8());
	if(!PF) {
		PC += tmp;
	}
}

inline void _jl()	// Opcode 0x7c
{
	int tmp = (int)((int8)FETCH8());
	if((SF!= OF) && !ZF) {
		PC += tmp;
	}
}

inline void _jnl()	// Opcode 0x7d
{
	int tmp = (int)((int8)FETCH8());
	if(ZF || (SF == OF)) {
		PC += tmp;
	}
}

inline void _jle()	// Opcode 0x7e
{
	int tmp = (int)((int8)FETCH8());
	if(ZF || (SF!= OF)) {
		PC += tmp;
	}
}

inline void _jnle()	// Opcode 0x7f
{
	int tmp = (int)((int8)FETCH8());
	if((SF == OF) && !ZF) {
		PC += tmp;
	}
}

inline void _op80()	// Opcode 0x80
{
	unsigned ModRM = FETCHOP();
	unsigned dst = GetRMByte(ModRM);
	unsigned src = FETCH8();
	
	switch(ModRM & 0x38) {
	case 0x00:	// ADD eb, d8
		ADDB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x08:	// OR eb, d8
		ORB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x10:	// ADC eb, d8
		src += CF;
		ADDB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x18:	// SBB eb, b8
		src += CF;
		SUBB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x20:	// AND eb, d8
		ANDB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x28:	// SUB eb, d8
		SUBB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x30:	// XOR eb, d8
		XORB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x38:	// CMP eb, d8
		SUBB(dst, src);
		break;
	}
}

inline void _op81()	// Opcode 0x81
{
	unsigned ModRM = FETCH8();
	unsigned dst = GetRMWord(ModRM);
	unsigned src = FETCH8();
	src += (FETCH8() << 8);
	
	switch(ModRM & 0x38) {
	case 0x00:	// ADD ew, d16
		ADDW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x08:	// OR ew, d16
		ORW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x10:	// ADC ew, d16
		src += CF;
		ADDW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x18:	// SBB ew, d16
		src += CF;
		SUBW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x20:	// AND ew, d16
		ANDW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x28:	// SUB ew, d16
		SUBW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x30:	// XOR ew, d16
		XORW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x38:	// CMP ew, d16
		SUBW(dst, src);
		break;
	}
}

inline void _op82()	// Opcode 0x82
{
	unsigned ModRM = FETCH8();
	unsigned dst = GetRMByte(ModRM);
	unsigned src = FETCH8();
	
	switch(ModRM & 0x38) {
	case 0x00:	// ADD eb, d8
		ADDB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x08:	// OR eb, d8
		ORB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x10:	// ADC eb, d8
		src += CF;
		ADDB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x18:	// SBB eb, d8
		src += CF;
		SUBB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x20:	// AND eb, d8
		ANDB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x28:	// SUB eb, d8
		SUBB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x30:	// XOR eb, d8
		XORB(dst, src);
		PutbackRMByte(ModRM, dst);
		break;
	case 0x38:	// CMP eb, d8
		SUBB(dst, src);
		break;
	}
}

inline void _op83()	// Opcode 0x83
{
	unsigned ModRM = FETCH8();
	unsigned dst = GetRMWord(ModRM);
	unsigned src = (uint16)((int16)((int8)FETCH8()));
	
	switch(ModRM & 0x38) {
	case 0x00:	// ADD ew, d16
		ADDW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x08:	// OR ew, d16
		ORW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x10:	// ADC ew, d16
		src += CF;
		ADDW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x18:	// SBB ew, d16
		src += CF;
		SUBW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x20:	// AND ew, d16
		ANDW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x28:	// SUB ew, d16
		SUBW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x30:	// XOR ew, d16
		XORW(dst, src);
		PutbackRMWord(ModRM, dst);
		break;
	case 0x38:	// CMP ew, d16
		SUBW(dst, src);
		break;
	}
}

inline void _test_br8()	// Opcode 0x84
{
	DEF_br8(dst, src);
	ANDB(dst, src);
}

inline void _test_wr16()	// Opcode 0x85
{
	DEF_wr16(dst, src);
	ANDW(dst, src);
}

inline void _xchg_br8()	// Opcode 0x86
{
	DEF_br8(dst, src);
	RegByte(ModRM) = dst;
	PutbackRMByte(ModRM, src);
}

inline void _xchg_wr16()	// Opcode 0x87
{
	DEF_wr16(dst, src);
	RegWord(ModRM) = dst;
	PutbackRMWord(ModRM, src);
}

inline void _mov_br8()	// Opcode 0x88
{
	unsigned ModRM = FETCH8();
	uint8 src = RegByte(ModRM);
	PutRMByte(ModRM, src);
}

inline void _mov_wr16()	// Opcode 0x89
{
	unsigned ModRM = FETCH8();
	uint16 src = RegWord(ModRM);
	PutRMWord(ModRM, src);
}

inline void _mov_r8b()	// Opcode 0x8a
{
	unsigned ModRM = FETCH8();
	uint8 src = GetRMByte(ModRM);
	RegByte(ModRM) = src;
}

inline void _mov_r16w()	// Opcode 0x8b
{
	unsigned ModRM = FETCH8();
	uint16 src = GetRMWord(ModRM);
	RegWord(ModRM) = src;
}

inline void _mov_wsreg()	// Opcode 0x8c
{
	unsigned ModRM = FETCH8();
#ifdef HAS_I286
	if(ModRM & 0x20) {
		interrupt(ILLEGAL_INSTRUCTION);
		return;
	}
#else
	if(ModRM & 0x20) {
		return;
	}
#endif
	PutRMWord(ModRM, sregs[(ModRM & 0x38) >> 3]);
}

inline void _lea()	// Opcode 0x8d
{
	unsigned ModRM = FETCH8();
	GetEA(ModRM);
	RegWord(ModRM) = EO;
}

inline void _mov_sregw()	// Opcode 0x8e
{
	unsigned ModRM = FETCH8();
	uint16 src = GetRMWord(ModRM);
	
#ifdef HAS_I286
	switch(ModRM & 0x38) {
	case 0x00:	// mov es, ew
		i286_data_descriptor(ES, src);
		break;
	case 0x08:	// mov cs, ew
		break;
	case 0x10:	// mov ss, ew
		i286_data_descriptor(SS, src);
		op(FETCHOP());
		break;
	case 0x18:	// mov ds, ew
		i286_data_descriptor(DS, src);
		break;
	}
#else
	switch(ModRM & 0x38) {
	case 0x00:	// mov es, ew
		sregs[ES] = src;
		base[ES] = SegBase(ES);
		break;
	case 0x08:	// mov cs, ew
		break;
	case 0x10:	// mov ss, ew
		sregs[SS] = src;
		base[SS] = SegBase(SS);
		op(FETCHOP());
		break;
	case 0x18:	// mov ds, ew
		sregs[DS] = src;
		base[DS] = SegBase(DS);
		break;
	}
#endif
}

inline void _popw()	// Opcode 0x8f
{
	unsigned ModRM = FETCH8();
	uint16 tmp = POP16();
	PutRMWord(ModRM, tmp);
}

#define XchgAXReg(reg) { \
	uint16 tmp = regs.w[reg]; \
	regs.w[reg] = regs.w[AX]; \
	regs.w[AX] = tmp; \
}

inline void _nop()	// Opcode 0x90
{
}

inline void _xchg_axcx()	// Opcode 0x91
{
	XchgAXReg(CX);
}

inline void _xchg_axdx()	// Opcode 0x92
{
	XchgAXReg(DX);
}

inline void _xchg_axbx()	// Opcode 0x93
{
	XchgAXReg(BX);
}

inline void _xchg_axsp()	// Opcode 0x94
{
	XchgAXReg(SP);
}

inline void _xchg_axbp()	// Opcode 0x95
{
	XchgAXReg(BP);
}

inline void _xchg_axsi()	// Opcode 0x96
{
	XchgAXReg(SI);
}

inline void _xchg_axdi()	// Opcode 0x97
{
	XchgAXReg(DI);
}

inline void _cbw()	// Opcode 0x98
{
	regs.b[AH] = (regs.b[AL] & 0x80) ? 0xff : 0;
}

inline void _cwd()	// Opcode 0x99
{
	regs.w[DX] = (regs.b[AH] & 0x80) ? 0xffff : 0;
}

inline void _call_far()	// Opcode 0x9a
{
	unsigned tmp1 = FETCH8();
	tmp1 += FETCH8() << 8;
	unsigned tmp2 = FETCH8();
	tmp2 += FETCH8() << 8;
	uint16 ip = PC - base[CS];
	PUSH16(sregs[CS]);
	PUSH16(ip);
#ifdef HAS_I286
	i286_code_descriptor(tmp2, tmp1);
#else
	sregs[CS] = (uint16)tmp2;
	base[CS] = SegBase(CS);
	PC = (base[CS] + (uint16)tmp1) & AMASK;
#endif
}

inline void _wait()	// Opcode 0x9b
{
}

inline void _pushf()	// Opcode 0x9c
{
	unsigned tmp = CompressFlags();
#ifdef HAS_I286
	PUSH16(tmp & ~0xf000);
#else
	PUSH16(tmp | 0xf000);
#endif
}

inline void _popf()	// Opcode 0x9d
{
	unsigned tmp = POP16();
	ExpandFlags(tmp);
	if(TF) {
		op(FETCHOP());
		interrupt(1);
	}
}

inline void _sahf()	// Opcode 0x9e
{
	unsigned tmp = (CompressFlags() & 0xff00) | (regs.b[AH] & 0xd5);
	ExpandFlags(tmp);
}

inline void _lahf()	// Opcode 0x9f
{
	regs.b[AH] = CompressFlags() & 0xff;
}

inline void _mov_aldisp()	// Opcode 0xa0
{
	unsigned addr = FETCH8();
	addr += FETCH8() << 8;
	regs.b[AL] = RM8(DS, addr);
}

inline void _mov_axdisp()	// Opcode 0xa1
{
	unsigned addr = FETCH8();
	addr += FETCH8() << 8;
	regs.b[AL] = RM8(DS, addr);
	regs.b[AH] = RM8(DS, addr + 1);
}

inline void _mov_dispal()	// Opcode 0xa2
{
	unsigned addr = FETCH8();
	addr += FETCH8() << 8;
	WM8(DS, addr, regs.b[AL]);
}

inline void _mov_dispax()	// Opcode 0xa3
{
	unsigned addr = FETCH8();
	addr += FETCH8() << 8;
	WM8(DS, addr, regs.b[AL]);
	WM8(DS, addr + 1, regs.b[AH]);
}

inline void _movsb()	// Opcode 0xa4
{
	uint8 tmp = RM8(DS, regs.w[SI]);
	WM8(ES, regs.w[DI], tmp);
	regs.w[DI] += DirVal;
	regs.w[SI] += DirVal;
}

inline void _movsw()	// Opcode 0xa5
{
	uint16 tmp = RM16(DS, regs.w[SI]);
	WM16(ES, regs.w[DI], tmp);
	regs.w[DI] += 2 * DirVal;
	regs.w[SI] += 2 * DirVal;
}

inline void _cmpsb()	// Opcode 0xa6
{
	unsigned dst = RM8(ES, regs.w[DI]);
	unsigned src = RM8(DS, regs.w[SI]);
	SUBB(src, dst);
	regs.w[DI] += DirVal;
	regs.w[SI] += DirVal;
}

inline void _cmpsw()	// Opcode 0xa7
{
	unsigned dst = RM16(ES, regs.w[DI]);
	unsigned src = RM16(DS, regs.w[SI]);
	SUBW(src, dst);
	regs.w[DI] += 2 * DirVal;
	regs.w[SI] += 2 * DirVal;
}

inline void _test_ald8()	// Opcode 0xa8
{
	DEF_ald8(dst, src);
	ANDB(dst, src);
}

inline void _test_axd16()	// Opcode 0xa9
{
	DEF_axd16(dst, src);
	ANDW(dst, src);
}

inline void _stosb()	// Opcode 0xaa
{
	WM8(ES, regs.w[DI], regs.b[AL]);
	regs.w[DI] += DirVal;
}

inline void _stosw()	// Opcode 0xab
{
	WM8(ES, regs.w[DI], regs.b[AL]);
	WM8(ES, regs.w[DI] + 1, regs.b[AH]);
	regs.w[DI] += 2 * DirVal;
}

inline void _lodsb()	// Opcode 0xac
{
	regs.b[AL] = RM8(DS, regs.w[SI]);
	regs.w[SI] += DirVal;
}

inline void _lodsw()	// Opcode 0xad
{
	regs.w[AX] = RM16(DS, regs.w[SI]);
	regs.w[SI] += 2 * DirVal;
}

inline void _scasb()	// Opcode 0xae
{
	unsigned src = RM8(ES, regs.w[DI]);
	unsigned dst = regs.b[AL];
	SUBB(dst, src);
	regs.w[DI] += DirVal;
}

inline void _scasw()	// Opcode 0xaf
{
	unsigned src = RM16(ES, regs.w[DI]);
	unsigned dst = regs.w[AX];
	SUBW(dst, src);
	regs.w[DI] += 2 * DirVal;
}

inline void _mov_ald8()	// Opcode 0xb0
{
	regs.b[AL] = FETCH8();
}

inline void _mov_cld8()	// Opcode 0xb1
{
	regs.b[CL] = FETCH8();
}

inline void _mov_dld8()	// Opcode 0xb2
{
	regs.b[DL] = FETCH8();
}

inline void _mov_bld8()	// Opcode 0xb3
{
	regs.b[BL] = FETCH8();
}

inline void _mov_ahd8()	// Opcode 0xb4
{
	regs.b[AH] = FETCH8();
}

inline void _mov_chd8()	// Opcode 0xb5
{
	regs.b[CH] = FETCH8();
}

inline void _mov_dhd8()	// Opcode 0xb6
{
	regs.b[DH] = FETCH8();
}

inline void _mov_bhd8()	// Opcode 0xb7
{
	regs.b[BH] = FETCH8();
}

inline void _mov_axd16()	// Opcode 0xb8
{
	regs.b[AL] = FETCH8();
	regs.b[AH] = FETCH8();
}

inline void _mov_cxd16()	// Opcode 0xb9
{
	regs.b[CL] = FETCH8();
	regs.b[CH] = FETCH8();
}

inline void _mov_dxd16()	// Opcode 0xba
{
	regs.b[DL] = FETCH8();
	regs.b[DH] = FETCH8();
}

inline void _mov_bxd16()	// Opcode 0xbb
{
	regs.b[BL] = FETCH8();
	regs.b[BH] = FETCH8();
}

inline void _mov_spd16()	// Opcode 0xbc
{
	regs.b[SPL] = FETCH8();
	regs.b[SPH] = FETCH8();
}

inline void _mov_bpd16()	// Opcode 0xbd
{
	regs.b[BPL] = FETCH8();
	regs.b[BPH] = FETCH8();
}

inline void _mov_sid16()	// Opcode 0xbe
{
	regs.b[SIL] = FETCH8();
	regs.b[SIH] = FETCH8();
}

inline void _mov_did16()	// Opcode 0xbf
{
	regs.b[DIL] = FETCH8();
	regs.b[DIH] = FETCH8();
}

inline void _rotshft_bd8()	// Opcode 0xc0
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned ModRM = FETCH8();
	unsigned cnt = FETCH8();
	rotate_shift_byte(ModRM, cnt);
#else
	_invalid();
#endif
}

inline void _rotshft_wd8()	// Opcode 0xc1
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned ModRM = FETCH8();
	unsigned cnt = FETCH8();
	rotate_shift_word(ModRM, cnt);
#else
	_invalid();
#endif
}

inline void _ret_d16()	// Opcode 0xc2
{
	unsigned cnt = FETCH8();
	cnt += FETCH8() << 8;
	PC = POP16();
	PC = (PC + base[CS]) & AMASK;
	regs.w[SP] += cnt;
}

inline void _ret()	// Opcode 0xc3
{
	PC = POP16();
	PC = (PC + base[CS]) & AMASK;
}

inline void _les_dw()	// Opcode 0xc4
{
	unsigned ModRM = FETCH8();
	uint16 tmp = GetRMWord(ModRM);
	RegWord(ModRM) = tmp;
#ifdef HAS_I286
	i286_data_descriptor(ES, GetNextRMWord());
#else
	sregs[ES] = GetNextRMWord();
	base[ES] = SegBase(ES);
#endif
}

inline void _lds_dw()	// Opcode 0xc5
{
	unsigned ModRM = FETCH8();
	uint16 tmp = GetRMWord(ModRM);
	RegWord(ModRM) = tmp;
#ifdef HAS_I286
	i286_data_descriptor(DS, GetNextRMWord());
#else
	sregs[DS] = GetNextRMWord();
	base[DS] = SegBase(DS);
#endif
}

inline void _mov_bd8()	// Opcode 0xc6
{
	unsigned ModRM = FETCH8();
	PutImmRMByte(ModRM);
}

inline void _mov_wd16()	// Opcode 0xc7
{
	unsigned ModRM = FETCH8();
	PutImmRMWord(ModRM);
}

inline void _enter()	// Opcode 0xc8
{
#if defined(HAS_I286) || defined(HAS_V30)
	unsigned nb = FETCH8();
	nb += FETCH8() << 8;
	unsigned level = FETCH8();
	PUSH16(regs.w[BP]);
	regs.w[BP] = regs.w[SP];
	regs.w[SP] -= nb;
	for(unsigned i = 1; i < level; i++) {
		PUSH16(RM16(SS, regs.w[BP]-i*2));
	}
	if(level) {
		PUSH16(regs.w[BP]);
	}
#else
	_invalid();
#endif
}

inline void _leav()	// Opcode 0xc9
{
#if defined(HAS_I286) || defined(HAS_V30)
	regs.w[SP] = regs.w[BP];
	regs.w[BP] = POP16();
#else
	_invalid();
#endif
}

inline void _retf_d16()	// Opcode 0xca
{
	unsigned cnt = FETCH8();
	cnt += FETCH8() << 8;
#ifdef HAS_I286
	{
		int tmp1 = POP16();
		int tmp2 = POP16();
		i286_code_descriptor(tmp2, tmp1);
	}
#else
	PC = POP16();
	sregs[CS] = POP16();
	base[CS] = SegBase(CS);
	PC = (PC + base[CS]) & AMASK;
#endif
	regs.w[SP] += cnt;
}

inline void _retf()	// Opcode 0xcb
{
#ifdef HAS_I286
	{
		int tmp1 = POP16();
		int tmp2 = POP16();
		i286_code_descriptor(tmp2, tmp1);
	}
#else
	PC = POP16();
	sregs[CS] = POP16();
	base[CS] = SegBase(CS);
	PC = (PC + base[CS]) & AMASK;
#endif
}

inline void _int3()	// Opcode 0xcc
{
	interrupt(3);
}

inline void _int()	// Opcode 0xcd
{
	unsigned num = FETCH8();
	interrupt(num);
}

inline void _into()	// Opcode 0xce
{
	if(OF) {
		interrupt(4);
	}
}

inline void _iret()	// Opcode 0xcf
{
	uint32 old = PC - 1;
#ifdef HAS_I286
	{
		int tmp1 = POP16();
		int tmp2 = POP16();
		i286_code_descriptor(tmp2, tmp1);
	}
#else
	PC = POP16();
	sregs[CS] = POP16();
	base[CS] = SegBase(CS);
	PC = (PC + base[CS]) & AMASK;
#endif
	_popf();
	// MS-DOS system call
	if(IRET_TOP <= old && old < (IRET_TOP + IRET_SIZE)) {
		msdos_syscall(old - IRET_TOP);
	}
}

inline void _rotshft_b()	// Opcode 0xd0
{
	rotate_shift_byte(FETCHOP(), 1);
}

inline void _rotshft_w()	// Opcode 0xd1
{
	rotate_shift_word(FETCHOP(), 1);
}

inline void _rotshft_bcl()	// Opcode 0xd2
{
	rotate_shift_byte(FETCHOP(), regs.b[CL]);
}

inline void _rotshft_wcl()	// Opcode 0xd3
{
	rotate_shift_word(FETCHOP(), regs.b[CL]);
}

inline void _aam()	// Opcode 0xd4
{
	unsigned mult = FETCH8();
	if(mult == 0) {
		interrupt(0);
	} else {
		regs.b[AH] = regs.b[AL] / mult;
		regs.b[AL] %= mult;
		SetSZPF_Word(regs.w[AX]);
	}
}

inline void _aad()	// Opcode 0xd5
{
	unsigned mult = FETCH8();
	regs.b[AL] = regs.b[AH] * mult + regs.b[AL];
	regs.b[AH] = 0;
	SetZF(regs.b[AL]);
	SetPF(regs.b[AL]);
	SignVal = 0;
}

inline void _setalc()	// Opcode 0xd6
{
#ifdef HAS_V30
	regs.b[AL] = (CF) ? 0xff : 0x00;
#else
	_invalid();
#endif
}

inline void _xlat()	// Opcode 0xd7
{
	unsigned dest = regs.w[BX] + regs.b[AL];
	regs.b[AL] = RM8(DS, dest);
}

inline void _escape()	// Opcodes 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde and 0xdf
{
	unsigned ModRM = FETCH8();
	GetRMByte(ModRM);
}

inline void _loopne()	// Opcode 0xe0
{
	int disp = (int)((int8)FETCH8());
	unsigned tmp = regs.w[CX] - 1;
	regs.w[CX] = tmp;
	if(!ZF && tmp) {
		PC += disp;
	}
}

inline void _loope()	// Opcode 0xe1
{
	int disp = (int)((int8)FETCH8());
	unsigned tmp = regs.w[CX] - 1;
	regs.w[CX] = tmp;
	if(ZF && tmp) {
		PC += disp;
	}
}

inline void _loop()	// Opcode 0xe2
{
	int disp = (int)((int8)FETCH8());
	unsigned tmp = regs.w[CX] - 1;
	regs.w[CX] = tmp;
	if(tmp) {
		PC += disp;
	}
}

inline void _jcxz()	// Opcode 0xe3
{
	int disp = (int)((int8)FETCH8());
	if(regs.w[CX] == 0) {
		PC += disp;
	}
}

inline void _inal()	// Opcode 0xe4
{
	unsigned port = FETCH8();
	regs.b[AL] = IN8(port);
}

inline void _inax()	// Opcode 0xe5
{
	unsigned port = FETCH8();
	regs.w[AX] = IN16(port);
}

inline void _outal()	// Opcode 0xe6
{
	unsigned port = FETCH8();
	OUT8(port, regs.b[AL]);
}

inline void _outax()	// Opcode 0xe7
{
	unsigned port = FETCH8();
	OUT16(port, regs.w[AX]);
}

inline void _call_d16()	// Opcode 0xe8
{
	uint16 tmp = FETCH16();
	uint16 ip = PC - base[CS];
	PUSH16(ip);
	ip += tmp;
	PC = (ip + base[CS]) & AMASK;
}

inline void _jmp_d16()	// Opcode 0xe9
{
	uint16 tmp = FETCH16();
	uint16 ip = PC - base[CS] + tmp;
	PC = (ip + base[CS]) & AMASK;
}

inline void _jmp_far()	// Opcode 0xea
{
	unsigned tmp1 = FETCH8();
	tmp1 += FETCH8() << 8;
	unsigned tmp2 = FETCH8();
	tmp2 += FETCH8() << 8;
#ifdef HAS_I286
	i286_code_descriptor(tmp2, tmp1);
#else
	sregs[CS] = (uint16)tmp2;
	base[CS] = SegBase(CS);
	PC = (base[CS] + tmp1) & AMASK;
#endif
}

inline void _jmp_d8()	// Opcode 0xeb
{
	int tmp = (int)((int8)FETCH8());
	PC += tmp;
}

inline void _inaldx()	// Opcode 0xec
{
	regs.b[AL] = IN8(regs.w[DX]);
}

inline void _inaxdx()	// Opcode 0xed
{
	unsigned port = regs.w[DX];
	regs.w[AX] = IN16(port);
}

inline void _outdxal()	// Opcode 0xee
{
	OUT8(regs.w[DX], regs.b[AL]);
}

inline void _outdxax()	// Opcode 0xef
{
	unsigned port = regs.w[DX];
	OUT16(port, regs.w[AX]);
}

inline void _lock()	// Opcode 0xf0
{
	op(FETCHOP());
}

inline void _rep(int flagval)
{
	unsigned next = FETCHOP();
	unsigned cnt = regs.w[CX];
	
	switch(next) {
	case 0x26:	// ES:
		seg_prefix = 1;
		prefix_base = base[ES];
		_rep(flagval);
		break;
	case 0x2e:	// CS:
		seg_prefix = 1;
		prefix_base = base[CS];
		_rep(flagval);
		break;
	case 0x36:	// SS:
		seg_prefix = 1;
		prefix_base = base[SS];
		_rep(flagval);
		break;
	case 0x3e:	// DS:
		seg_prefix = 1;
		prefix_base = base[DS];
		_rep(flagval);
		break;
#ifndef HAS_I86
	case 0x6c:	// REP INSB
		for(; cnt > 0; cnt--) {
			WM8(ES, regs.w[DI], IN8(regs.w[DX]));
			regs.w[DI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0x6d:	// REP INSW
		for(; cnt > 0; cnt--) {
			WM16(ES, regs.w[DI], IN16(regs.w[DX]));
			regs.w[DI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0x6e:	// REP OUTSB
		for(; cnt > 0; cnt--) {
			OUT8(regs.w[DX], RM8(DS, regs.w[SI]));
			regs.w[SI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0x6f:	// REP OUTSW
		for(; cnt > 0; cnt--) {
			OUT16(regs.w[DX], RM16(DS, regs.w[SI]));
			regs.w[SI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
#endif
	case 0xa4:	// REP MOVSB
		for(; cnt > 0; cnt--) {
			uint8 tmp = RM8(DS, regs.w[SI]);
			WM8(ES, regs.w[DI], tmp);
			regs.w[DI] += DirVal;
			regs.w[SI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xa5:	// REP MOVSW
		for(; cnt > 0; cnt--) {
			uint16 tmp = RM16(DS, regs.w[SI]);
			WM16(ES, regs.w[DI], tmp);
			regs.w[DI] += 2 * DirVal;
			regs.w[SI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xa6:	// REP(N)E CMPSB
		for(ZeroVal = !flagval; (ZF == flagval) && (cnt > 0); cnt--) {
			unsigned dst = RM8(ES, regs.w[DI]);
			unsigned src = RM8(DS, regs.w[SI]);
			SUBB(src, dst);
			regs.w[DI] += DirVal;
			regs.w[SI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xa7:	// REP(N)E CMPSW
		for(ZeroVal = !flagval; (ZF == flagval) && (cnt > 0); cnt--) {
			unsigned dst = RM16(ES, regs.w[DI]);
			unsigned src = RM16(DS, regs.w[SI]);
			SUBW(src, dst);
			regs.w[DI] += 2 * DirVal;
			regs.w[SI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xaa:	// REP STOSB
		for(; cnt > 0; cnt--) {
			WM8(ES, regs.w[DI], regs.b[AL]);
			regs.w[DI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xab:	// REP STOSW
		for(; cnt > 0; cnt--) {
			WM16(ES, regs.w[DI], regs.w[AX]);
			regs.w[DI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xac:	// REP LODSB
		for(; cnt > 0; cnt--) {
			regs.b[AL] = RM8(DS, regs.w[SI]);
			regs.w[SI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xad:	// REP LODSW
		for(; cnt > 0; cnt--) {
			regs.w[AX] = RM16(DS, regs.w[SI]);
			regs.w[SI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xae:	// REP(N)E SCASB
		for(ZeroVal = !flagval; (ZF == flagval) && (cnt > 0); cnt--) {
			unsigned src = RM8(ES, regs.w[DI]);
			unsigned dst = regs.b[AL];
			SUBB(dst, src);
			regs.w[DI] += DirVal;
		}
		regs.w[CX] = cnt;
		break;
	case 0xaf:	// REP(N)E SCASW
		for(ZeroVal = !flagval; (ZF == flagval) && (cnt > 0); cnt--) {
			unsigned src = RM16(ES, regs.w[DI]);
			unsigned dst = regs.w[AX];
			SUBW(dst, src);
			regs.w[DI] += 2 * DirVal;
		}
		regs.w[CX] = cnt;
		break;
	default:
		op(next);
	}
}

inline void _hlt()	// Opcode 0xf4
{
	halt = 1;
}

inline void _cmc()	// Opcode 0xf5
{
	CarryVal = !CF;
}

inline void _opf6()	// Opecode 0xf6
{
	unsigned ModRM = FETCH8();
	unsigned tmp1 = (unsigned)GetRMByte(ModRM), tmp2;
	
	switch(ModRM & 0x38) {
	case 0x00:	// TEST Eb, data8
	case 0x08:	// ???
		tmp1 &= FETCH8();
		CarryVal = OverVal = AuxVal = 0;
		SetSZPF_Byte(tmp1);
		break;
	case 0x10:	// NOT Eb
		PutbackRMByte(ModRM, ~tmp1);
		break;
	case 0x18:	// NEG Eb
		tmp2 = 0;
		SUBB(tmp2, tmp1);
		PutbackRMByte(ModRM, tmp2);
		break;
	case 0x20:	// MUL AL, Eb
		{
			tmp2 = regs.b[AL];
			SetSF((int8)tmp2);
			SetPF(tmp2);
			uint16 result = (uint16)tmp2 * tmp1;
			regs.w[AX] = (uint16)result;
			SetZF(regs.w[AX]);
			CarryVal = OverVal = (regs.b[AH] != 0);
		}
		break;
	case 0x28:	// IMUL AL, Eb
		{
			tmp2 = (unsigned)regs.b[AL];
			SetSF((int8)tmp2);
			SetPF(tmp2);
			int16 result = (int16)((int8)tmp2) * (int16)((int8)tmp1);
			regs.w[AX] = (uint16)result;
			SetZF(regs.w[AX]);
			CarryVal = OverVal = (result >> 7 != 0) && (result >> 7 != -1);
		}
		break;
	case 0x30:	// DIV AL, Ew
		if(tmp1) {
			uint16 result = regs.w[AX];
			if((result / tmp1) > 0xff) {
				interrupt(0);
			} else {
				regs.b[AH] = result % tmp1;
				regs.b[AL] = result / tmp1;
			}
		} else
			interrupt(0);
		break;
	case 0x38:	// IDIV AL, Ew
		if(tmp1) {
			int16 result = regs.w[AX];
			tmp2 = result % (int16)((int8)tmp1);
			if((result /= (int16)((int8)tmp1)) > 0xff) {
				interrupt(0);
			} else {
				regs.b[AL] = (uint8)result;
				regs.b[AH] = tmp2;
			}
		} else
			interrupt(0);
		break;
	}
}

inline void _opf7()
{
	// Opcode 0xf7
	unsigned ModRM = FETCH8();
	unsigned tmp1 = GetRMWord(ModRM), tmp2;
	
	switch(ModRM & 0x38) {
	case 0x00:	// TEST Ew, data16
	case 0x08:	// ???
		tmp2 = FETCH8();
		tmp2 += FETCH8() << 8;
		tmp1 &= tmp2;
		CarryVal = OverVal = AuxVal = 0;
		SetSZPF_Word(tmp1);
		break;
	case 0x10:	// NOT Ew
		tmp1 = ~tmp1;
		PutbackRMWord(ModRM, tmp1);
		break;
	case 0x18:	// NEG Ew
		tmp2 = 0;
		SUBW(tmp2, tmp1);
		PutbackRMWord(ModRM, tmp2);
		break;
	case 0x20:	// MUL AX, Ew
		{
			tmp2 = regs.w[AX];
			SetSF((int16)tmp2);
			SetPF(tmp2);
			uint32 result = (uint32)tmp2 * tmp1;
			regs.w[AX] = (uint16)result;
			result >>= 16;
			regs.w[DX] = result;
			SetZF(regs.w[AX] | regs.w[DX]);
			CarryVal = OverVal = (regs.w[DX] != 0);
		}
		break;
	case 0x28:	// IMUL AX, Ew
		{
			tmp2 = regs.w[AX];
			SetSF((int16)tmp2);
			SetPF(tmp2);
			int32 result = (int32)((int16)tmp2) * (int32)((int16)tmp1);
			CarryVal = OverVal = (result >> 15 != 0) && (result >> 15 != -1);
			regs.w[AX] = (uint16)result;
			result = (uint16)(result >> 16);
			regs.w[DX] = result;
			SetZF(regs.w[AX] | regs.w[DX]);
		}
		break;
	case 0x30:	// DIV AX, Ew
		if(tmp1) {
			uint32 result = (regs.w[DX] << 16) + regs.w[AX];
			tmp2 = result % tmp1;
			if((result / tmp1) > 0xffff) {
				interrupt(0);
			} else {
				regs.w[DX] = tmp2;
				result /= tmp1;
				regs.w[AX] = result;
			}
		} else {
			interrupt(0);
		}
		break;
	case 0x38:	// IDIV AX, Ew
		if(tmp1) {
			int32 result = (regs.w[DX] << 16) + regs.w[AX];
			tmp2 = result % (int32)((int16)tmp1);
			if((result /= (int32)((int16)tmp1)) > 0xffff) {
				interrupt(0);
			} else {
				regs.w[AX] = result;
				regs.w[DX] = tmp2;
			}
		} else {
			interrupt(0);
		}
		break;
	}
}

inline void _clc()	// Opcode 0xf8
{
	CarryVal = 0;
}

inline void _stc()	// Opcode 0xf9
{
	CarryVal = 1;
}

inline void _cli()	// Opcode 0xfa
{
	SetIF(0);
}

inline void _sti()	// Opcode 0xfb
{
	SetIF(1);
	op(FETCHOP());
}

inline void _cld()	// Opcode 0xfc
{
	SetDF(0);
}

inline void _std()	// Opcode 0xfd
{
	SetDF(1);
}

inline void _opfe()	// Opcode 0xfe
{
	unsigned ModRM = FETCH8();
	unsigned tmp1 = GetRMByte(ModRM), tmp2;
	if((ModRM & 0x38) == 0) {
		// INC eb
		tmp2 = tmp1 + 1;
		SetOFB_Add(tmp2, tmp1, 1);
	} else {
		// DEC eb
		tmp2 = tmp1 - 1;
		SetOFB_Sub(tmp2, 1, tmp1);
	}
	SetAF(tmp2, tmp1, 1);
	SetSZPF_Byte(tmp2);
	PutbackRMByte(ModRM, (uint8)tmp2);
}

inline void _opff()	// Opcode 0xff
{
	unsigned ModRM = FETCHOP(), tmp1, tmp2;
	uint16 ip;
	
	switch(ModRM & 0x38) {
	case 0x00:	// INC ew
		tmp1 = GetRMWord(ModRM);
		tmp2 = tmp1 + 1;
		SetOFW_Add(tmp2, tmp1, 1);
		SetAF(tmp2, tmp1, 1);
		SetSZPF_Word(tmp2);
		PutbackRMWord(ModRM, (uint16)tmp2);
		break;
	case 0x08:	// DEC ew
		tmp1 = GetRMWord(ModRM);
		tmp2 = tmp1 - 1;
		SetOFW_Sub(tmp2, 1, tmp1);
		SetAF(tmp2, tmp1, 1);
		SetSZPF_Word(tmp2);
		PutbackRMWord(ModRM, (uint16)tmp2);
		break;
	case 0x10:	// CALL ew
		tmp1 = GetRMWord(ModRM);
		ip = PC - base[CS];
		PUSH16(ip);
		PC = (base[CS] + (uint16)tmp1) & AMASK;
		break;
	case 0x18:	// CALL FAR ea
		tmp1 = sregs[CS];
		tmp2 = GetRMWord(ModRM);
		ip = PC - base[CS];
		PUSH16(tmp1);
		PUSH16(ip);
#ifdef HAS_I286
		i286_code_descriptor(GetNextRMWord(), tmp2);
#else
		sregs[CS] = GetNextRMWord();
		base[CS] = SegBase(CS);
		PC = (base[CS] + tmp2) & AMASK;
#endif
		break;
	case 0x20:	// JMP ea
		ip = GetRMWord(ModRM);
		PC = (base[CS] + ip) & AMASK;
		break;
	case 0x28:	// JMP FAR ea
#ifdef HAS_I286
		tmp1 = GetRMWord(ModRM);
		i286_code_descriptor(GetNextRMWord(), tmp1);
#else
		PC = GetRMWord(ModRM);
		sregs[CS] = GetNextRMWord();
		base[CS] = SegBase(CS);
		PC = (PC + base[CS]) & AMASK;
#endif
		break;
	case 0x30:	// PUSH ea
		tmp1 = GetRMWord(ModRM);
		PUSH16(tmp1);
		break;
	case 0x38:	// invalid ???
		break;
	}
}

inline void _invalid()
{
#ifdef HAS_I286
	interrupt(ILLEGAL_INSTRUCTION);
#else
	halt = 1;
#endif
}

void cpu_init()
{
	for(int i = 0; i < 8; i++) {
		regs.w[i] = 0;
	}
	memset(sregs, 0, sizeof(sregs));
	memset(limit, 0, sizeof(limit));
	memset(base, 0, sizeof(base));
	EA = 0;
	EO = 0;
	gdtr_base = idtr_base = ldtr_base = tr_base = 0;
	gdtr_limit = idtr_limit = ldtr_limit = tr_limit = 0;
	ldtr_sel = tr_sel = 0;
	AuxVal = OverVal = SignVal = ZeroVal = CarryVal = 0;
	DirVal = 1;
	ParityVal = TF = IF = MF = 0;
	intstat = halt = 0;
	
	sregs[CS] = 0xf000;
	limit[CS] = limit[SS] = limit[DS] = limit[ES] = 0xffff;
	base[CS] = SegBase(CS);
	idtr_limit = 0x3ff;
	PC = 0xffff0 & AMASK;
#ifdef HAS_I286
	AMASK = 0xfffff;
	msw = 0xfff0;
	flags = 2;
#else
	msw = flags = 0;
#endif
	ExpandFlags(flags);
#ifdef HAS_V30
	SetMD(1);
#endif
}

void cpu_interrupt(int status)
{
	intstat = status;
}
