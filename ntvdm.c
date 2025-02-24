/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2025.01.11-
*/

#include <windows.h>
#include <direct.h>
#include <mbstring.h>
#include "ntvdm.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma warning( disable : 4996 )
#endif

#ifndef MB_CANCELTRYCONTINUE
#define MB_CANCELTRYCONTINUE 0x00000006L
#endif

#ifndef IDTRYAGAIN
#define IDTRYAGAIN 10
#endif

#ifndef IDCONTINUE
#define IDCONTINUE 11
#endif

static VDD_FUNC_TABLE func;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);
	}
	return TRUE;
}

// NTVDM Registers

__declspec(dllexport) void WINAPI SetFuncTable(PVDD_FUNC_TABLE ptr)
{
	func = *ptr;
}

__declspec(dllexport) BYTE WINAPI getAL()
{
	return func.getAL();
}

__declspec(dllexport) BYTE WINAPI getAH()
{
	return func.getAH();
}

__declspec(dllexport) WORD WINAPI getAX()
{
	return func.getAX();
}

__declspec(dllexport) DWORD WINAPI getEAX()
{
	return func.getEAX();
}

__declspec(dllexport) BYTE WINAPI getBL()
{
	return func.getBL();
}

__declspec(dllexport) BYTE WINAPI getBH()
{
	return func.getBH();
}

__declspec(dllexport) WORD WINAPI getBX()
{
	return func.getBX();
}

__declspec(dllexport) DWORD WINAPI getEBX()
{
	return func.getEBX();
}

__declspec(dllexport) BYTE WINAPI getCL()
{
	return func.getCL();
}

__declspec(dllexport) BYTE WINAPI getCH()
{
	return func.getCH();
}

__declspec(dllexport) WORD WINAPI getCX()
{
	return func.getCX();
}

__declspec(dllexport) DWORD WINAPI getECX()
{
	return func.getECX();
}

__declspec(dllexport) BYTE WINAPI getDL()
{
	return func.getDL();
}

__declspec(dllexport) BYTE WINAPI getDH()
{
	return func.getDH();
}

__declspec(dllexport) WORD WINAPI getDX()
{
	return func.getDX();
}

__declspec(dllexport) DWORD WINAPI getEDX()
{
	return func.getEDX();
}

__declspec(dllexport) WORD WINAPI getSP()
{
	return func.getSP();
}

__declspec(dllexport) DWORD WINAPI getESP()
{
	return func.getESP();
}

__declspec(dllexport) WORD WINAPI getBP()
{
	return func.getBP();
}

__declspec(dllexport) DWORD WINAPI getEBP()
{
	return func.getEBP();
}

__declspec(dllexport) WORD WINAPI getSI()
{
	return func.getSI();
}

__declspec(dllexport) DWORD WINAPI getESI()
{
	return func.getESI();
}

__declspec(dllexport) WORD WINAPI getDI()
{
	return func.getDI();
}

__declspec(dllexport) DWORD WINAPI getEDI()
{
	return func.getEDI();
}

__declspec(dllexport) void WINAPI setAL(BYTE val)
{
	func.setAL(val);
}

__declspec(dllexport) void WINAPI setAH(BYTE val)
{
	func.setAH(val);
}

__declspec(dllexport) void WINAPI setAX(WORD val)
{
	func.setAX(val);
}

__declspec(dllexport) void WINAPI setEAX(DWORD val)
{
	func.setEAX(val);
}

__declspec(dllexport) void WINAPI setBL(BYTE val)
{
	func.setBL(val);
}

__declspec(dllexport) void WINAPI setBH(BYTE val)
{
	func.setBH(val);
}

__declspec(dllexport) void WINAPI setBX(WORD val)
{
	func.setBX(val);
}

__declspec(dllexport) void WINAPI setEBX(DWORD val)
{
	func.setEBX(val);
}

__declspec(dllexport) void WINAPI setCL(BYTE val)
{
	func.setCL(val);
}

__declspec(dllexport) void WINAPI setCH(BYTE val)
{
	func.setCH(val);
}

__declspec(dllexport) void WINAPI setCX(WORD val)
{
	func.setCX(val);
}

__declspec(dllexport) void WINAPI setECX(DWORD val)
{
	func.setECX(val);
}

__declspec(dllexport) void WINAPI setDL(BYTE val)
{
	func.setDL(val);
}

__declspec(dllexport) void WINAPI setDH(BYTE val)
{
	func.setDH(val);
}

__declspec(dllexport) void WINAPI setDX(WORD val)
{
	func.setDX(val);
}

__declspec(dllexport) void WINAPI setEDX(DWORD val)
{
	func.setEDX(val);
}

__declspec(dllexport) void WINAPI setSP(WORD val)
{
	func.setSP(val);
}

__declspec(dllexport) void WINAPI setESP(DWORD val)
{
	func.setESP(val);
}

__declspec(dllexport) void WINAPI setBP(WORD val)
{
	func.setBP(val);
}

__declspec(dllexport) void WINAPI setEBP(DWORD val)
{
	func.setEBP(val);
}

__declspec(dllexport) void WINAPI setSI(WORD val)
{
	func.setSI(val);
}

__declspec(dllexport) void WINAPI setESI(DWORD val)
{
	func.setESI(val);
}

__declspec(dllexport) void WINAPI setDI(WORD val)
{
	func.setDI(val);
}

__declspec(dllexport) void WINAPI setEDI(DWORD val)
{
	func.setEDI(val);
}

__declspec(dllexport) WORD WINAPI getDS()
{
	return func.getDS();
}

__declspec(dllexport) WORD WINAPI getES()
{
	return func.getES();
}

__declspec(dllexport) WORD WINAPI getCS()
{
	return func.getCS();
}

__declspec(dllexport) WORD WINAPI getSS()
{
	return func.getSS();
}

__declspec(dllexport) WORD WINAPI getFS()
{
	return func.getFS();
}

__declspec(dllexport) WORD WINAPI getGS()
{
	return func.getGS();
}

__declspec(dllexport) void WINAPI setDS(WORD val)
{
	func.setDS(val);
}

__declspec(dllexport) void WINAPI setES(WORD val)
{
	func.setES(val);
}

__declspec(dllexport) void WINAPI setCS(WORD val)
{
	func.setCS(val);
}

__declspec(dllexport) void WINAPI setSS(WORD val)
{
	func.setSS(val);
}

__declspec(dllexport) void WINAPI setFS(WORD val)
{
	func.setFS(val);
}

__declspec(dllexport) void WINAPI setGS(WORD val)
{
	func.setGS(val);
}

__declspec(dllexport) WORD WINAPI getIP()
{
	return func.getIP();
}

__declspec(dllexport) DWORD WINAPI getEIP()
{
	return func.getEIP();
}

__declspec(dllexport) void WINAPI setIP(WORD val)
{
	func.setIP(val);
}

__declspec(dllexport) void WINAPI setEIP(DWORD val)
{
	func.setEIP(val);
}

__declspec(dllexport) DWORD WINAPI getCF()
{
	return func.getCF();
}

__declspec(dllexport) DWORD WINAPI getPF()
{
	return func.getPF();
}

__declspec(dllexport) DWORD WINAPI getAF()
{
	return func.getAF();
}

__declspec(dllexport) DWORD WINAPI getZF()
{
	return func.getZF();
}

__declspec(dllexport) DWORD WINAPI getSF()
{
	return func.getSF();
}

__declspec(dllexport) DWORD WINAPI getIF()
{
	return func.getIF();
}

__declspec(dllexport) DWORD WINAPI getDF()
{
	return func.getDF();
}

__declspec(dllexport) DWORD WINAPI getOF()
{
	return func.getOF();
}

__declspec(dllexport) void WINAPI setCF(DWORD val)
{
	func.setCF(val);
}

__declspec(dllexport) void WINAPI setPF(DWORD val)
{
	func.setPF(val);
}

__declspec(dllexport) void WINAPI setAF(DWORD val)
{
	func.setAF(val);
}

__declspec(dllexport) void WINAPI setZF(DWORD val)
{
	func.setZF(val);
}

__declspec(dllexport) void WINAPI setSF(DWORD val)
{
	func.setSF(val);
}

__declspec(dllexport) void WINAPI setIF(DWORD val)
{
	func.setIF(val);
}

__declspec(dllexport) void WINAPI setDF(DWORD val)
{
	func.setDF(val);
}

__declspec(dllexport) void WINAPI setOF(DWORD val)
{
	func.setOF(val);
}

__declspec(dllexport) DWORD WINAPI getEFLAGS()
{
	return func.getEFLAGS();
}

__declspec(dllexport) void WINAPI setEFLAGS(DWORD val)
{
	func.setEFLAGS(val);
}

__declspec(dllexport) WORD WINAPI getMSW()
{
	return func.getMSW();
}

__declspec(dllexport) void WINAPI setMSW(WORD val)
{
	func.setMSW(val);
}

__declspec(dllexport) PVOID WINAPI getIntelRegistersPointer()
{
	return func.getIntelRegistersPointer();
}

// NTVDM CCPU MIPS Compatibility

__declspec(dllexport) BYTE WINAPI c_getAL()
{
	return func.getAL();
}

__declspec(dllexport) BYTE WINAPI c_getAH()
{
	return func.getAH();
}

__declspec(dllexport) WORD WINAPI c_getAX()
{
	return func.getAX();
}

__declspec(dllexport) DWORD WINAPI c_getEAX()
{
	return func.getEAX();
}

__declspec(dllexport) BYTE WINAPI c_getBL()
{
	return func.getBL();
}

__declspec(dllexport) BYTE WINAPI c_getBH()
{
	return func.getBH();
}

__declspec(dllexport) WORD WINAPI c_getBX()
{
	return func.getBX();
}

__declspec(dllexport) DWORD WINAPI c_getEBX()
{
	return func.getEBX();
}

__declspec(dllexport) BYTE WINAPI c_getCL()
{
	return func.getCL();
}

__declspec(dllexport) BYTE WINAPI c_getCH()
{
	return func.getCH();
}

__declspec(dllexport) WORD WINAPI c_getCX()
{
	return func.getCX();
}

__declspec(dllexport) DWORD WINAPI c_getECX()
{
	return func.getECX();
}

__declspec(dllexport) BYTE WINAPI c_getDL()
{
	return func.getDL();
}

__declspec(dllexport) BYTE WINAPI c_getDH()
{
	return func.getDH();
}

__declspec(dllexport) WORD WINAPI c_getDX()
{
	return func.getDX();
}

__declspec(dllexport) DWORD WINAPI c_getEDX()
{
	return func.getEDX();
}

__declspec(dllexport) WORD WINAPI c_getSP()
{
	return func.getSP();
}

__declspec(dllexport) DWORD WINAPI c_getESP()
{
	return func.getESP();
}

__declspec(dllexport) WORD WINAPI c_getBP()
{
	return func.getBP();
}

__declspec(dllexport) DWORD WINAPI c_getEBP()
{
	return func.getEBP();
}

__declspec(dllexport) WORD WINAPI c_getSI()
{
	return func.getSI();
}

__declspec(dllexport) DWORD WINAPI c_getESI()
{
	return func.getESI();
}

__declspec(dllexport) WORD WINAPI c_getDI()
{
	return func.getDI();
}

__declspec(dllexport) DWORD WINAPI c_getEDI()
{
	return func.getEDI();
}

__declspec(dllexport) void WINAPI c_setAL(BYTE val)
{
	func.setAL(val);
}

__declspec(dllexport) void WINAPI c_setAH(BYTE val)
{
	func.setAH(val);
}

__declspec(dllexport) void WINAPI c_setAX(WORD val)
{
	func.setAX(val);
}

__declspec(dllexport) void WINAPI c_setEAX(DWORD val)
{
	func.setEAX(val);
}

__declspec(dllexport) void WINAPI c_setBL(BYTE val)
{
	func.setBL(val);
}

__declspec(dllexport) void WINAPI c_setBH(BYTE val)
{
	func.setBH(val);
}

__declspec(dllexport) void WINAPI c_setBX(WORD val)
{
	func.setBX(val);
}

__declspec(dllexport) void WINAPI c_setEBX(DWORD val)
{
	func.setEBX(val);
}

__declspec(dllexport) void WINAPI c_setCL(BYTE val)
{
	func.setCL(val);
}

__declspec(dllexport) void WINAPI c_setCH(BYTE val)
{
	func.setCH(val);
}

__declspec(dllexport) void WINAPI c_setCX(WORD val)
{
	func.setCX(val);
}

__declspec(dllexport) void WINAPI c_setECX(DWORD val)
{
	func.setECX(val);
}

__declspec(dllexport) void WINAPI c_setDL(BYTE val)
{
	func.setDL(val);
}

__declspec(dllexport) void WINAPI c_setDH(BYTE val)
{
	func.setDH(val);
}

__declspec(dllexport) void WINAPI c_setDX(WORD val)
{
	func.setDX(val);
}

__declspec(dllexport) void WINAPI c_setEDX(DWORD val)
{
	func.setEDX(val);
}

__declspec(dllexport) void WINAPI c_setSP(WORD val)
{
	func.setSP(val);
}

__declspec(dllexport) void WINAPI c_setESP(DWORD val)
{
	func.setESP(val);
}

__declspec(dllexport) void WINAPI c_setBP(WORD val)
{
	func.setBP(val);
}

__declspec(dllexport) void WINAPI c_setEBP(DWORD val)
{
	func.setEBP(val);
}

__declspec(dllexport) void WINAPI c_setSI(WORD val)
{
	func.setSI(val);
}

__declspec(dllexport) void WINAPI c_setESI(DWORD val)
{
	func.setESI(val);
}

__declspec(dllexport) void WINAPI c_setDI(WORD val)
{
	func.setDI(val);
}

__declspec(dllexport) void WINAPI c_setEDI(DWORD val)
{
	func.setEDI(val);
}

__declspec(dllexport) WORD WINAPI c_getDS()
{
	return func.getDS();
}

__declspec(dllexport) WORD WINAPI c_getES()
{
	return func.getES();
}

__declspec(dllexport) WORD WINAPI c_getCS()
{
	return func.getCS();
}

__declspec(dllexport) WORD WINAPI c_getSS()
{
	return func.getSS();
}

__declspec(dllexport) WORD WINAPI c_getFS()
{
	return func.getFS();
}

__declspec(dllexport) WORD WINAPI c_getGS()
{
	return func.getGS();
}

__declspec(dllexport) void WINAPI c_setDS(WORD val)
{
	func.setDS(val);
}

__declspec(dllexport) void WINAPI c_setES(WORD val)
{
	func.setES(val);
}

__declspec(dllexport) void WINAPI c_setCS(WORD val)
{
	func.setCS(val);
}

__declspec(dllexport) void WINAPI c_setSS(WORD val)
{
	func.setSS(val);
}

__declspec(dllexport) void WINAPI c_setFS(WORD val)
{
	func.setFS(val);
}

__declspec(dllexport) void WINAPI c_setGS(WORD val)
{
	func.setGS(val);
}

__declspec(dllexport) WORD WINAPI c_getIP()
{
	return func.getIP();
}

__declspec(dllexport) DWORD WINAPI c_getEIP()
{
	return func.getEIP();
}

__declspec(dllexport) void WINAPI c_setIP(WORD val)
{
	func.setIP(val);
}

__declspec(dllexport) void WINAPI c_setEIP(DWORD val)
{
	func.setEIP(val);
}

__declspec(dllexport) DWORD WINAPI c_getCF()
{
	return func.getCF();
}

__declspec(dllexport) DWORD WINAPI c_getPF()
{
	return func.getPF();
}

__declspec(dllexport) DWORD WINAPI c_getAF()
{
	return func.getAF();
}

__declspec(dllexport) DWORD WINAPI c_getZF()
{
	return func.getZF();
}

__declspec(dllexport) DWORD WINAPI c_getSF()
{
	return func.getSF();
}

__declspec(dllexport) DWORD WINAPI c_getIF()
{
	return func.getIF();
}

__declspec(dllexport) DWORD WINAPI c_getDF()
{
	return func.getDF();
}

__declspec(dllexport) DWORD WINAPI c_getOF()
{
	return func.getOF();
}

__declspec(dllexport) void WINAPI c_setCF(DWORD val)
{
	func.setCF(val);
}

__declspec(dllexport) void WINAPI c_setPF(DWORD val)
{
	func.setPF(val);
}

__declspec(dllexport) void WINAPI c_setAF(DWORD val)
{
	func.setAF(val);
}

__declspec(dllexport) void WINAPI c_setZF(DWORD val)
{
	func.setZF(val);
}

__declspec(dllexport) void WINAPI c_setSF(DWORD val)
{
	func.setSF(val);
}

__declspec(dllexport) void WINAPI c_setIF(DWORD val)
{
	func.setIF(val);
}

__declspec(dllexport) void WINAPI c_setDF(DWORD val)
{
	func.setDF(val);
}

__declspec(dllexport) void WINAPI c_setOF(DWORD val)
{
	func.setOF(val);
}

__declspec(dllexport) WORD WINAPI c_getMSW()
{
	return func.getMSW();
}

__declspec(dllexport) void WINAPI c_setMSW(WORD val)
{
	func.setMSW(val);
}

// NTVDM DOS-32 Emulation (from ReactOS/subsystem/mvdm/dos/dem.c)

__declspec(dllexport) DWORD WINAPI demClientErrorEx(IN HANDLE FileHandle, CHAR Unknown, BOOL Flag)
{
	return GetLastError();
}

__declspec(dllexport) DWORD WINAPI demFileDelete(IN LPCSTR FileName)
{
	if (DeleteFileA(FileName)) SetLastError(ERROR_SUCCESS);

	return GetLastError();
}

typedef struct _DOS_FIND_FILE_BLOCK
{
	CHAR DriveLetter;
	CHAR Pattern[11];
	UCHAR AttribMask;
	DWORD Unused;
	HANDLE SearchHandle;

	/* The following part of the structure is documented */
	UCHAR Attributes;
	WORD FileTime;
	WORD FileDate;
	DWORD FileSize;
	CHAR FileName[13];
} DOS_FIND_FILE_BLOCK, *PDOS_FIND_FILE_BLOCK;

__declspec(dllexport) DWORD WINAPI demFileFindFirst(PVOID lpFindFileData, LPCSTR FileName, WORD AttribMask)
{
	BOOLEAN Success = TRUE;
	WIN32_FIND_DATAA FindData;
	HANDLE SearchHandle;
	PDOS_FIND_FILE_BLOCK FindFileBlock = (PDOS_FIND_FILE_BLOCK)lpFindFileData;

	/* Start a search */
	SearchHandle = FindFirstFileA(FileName, &FindData);
	if (SearchHandle == INVALID_HANDLE_VALUE) return GetLastError();

	do
	{
		/* Check the attributes and retry as long as we haven't found a matching file */
		if (!((FindData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN |
		                                    FILE_ATTRIBUTE_SYSTEM |
		                                    FILE_ATTRIBUTE_DIRECTORY))
		     & ~AttribMask))
		{
			break;
		}
	}
	while ((Success = FindNextFileA(SearchHandle, &FindData)));

	/* If we failed at some point, close the search and return an error */
	if (!Success)
	{
		FindClose(SearchHandle);
		return GetLastError();
	}

	/* Fill the block */
	FindFileBlock->DriveLetter  = 'A' + (_getdrive() - 1);
	FindFileBlock->AttribMask   = (UCHAR)AttribMask;
	FindFileBlock->SearchHandle = SearchHandle;
	FindFileBlock->Attributes   = LOBYTE(FindData.dwFileAttributes);
	FileTimeToDosDateTime(&FindData.ftLastWriteTime,
	                      &FindFileBlock->FileDate,
	                      &FindFileBlock->FileTime);
	FindFileBlock->FileSize = FindData.nFileSizeHigh ? 0xFFFFFFFF
	                                                 : FindData.nFileSizeLow;
	/* Build a short path name */
	if (*FindData.cAlternateFileName)
		strncpy(FindFileBlock->FileName, FindData.cAlternateFileName, sizeof(FindFileBlock->FileName));
	else
		GetShortPathNameA(FindData.cFileName, FindFileBlock->FileName, sizeof(FindFileBlock->FileName));

	return ERROR_SUCCESS;
}

__declspec(dllexport) DWORD WINAPI demFileFindNext(OUT PVOID lpFindFileData)
{
	WIN32_FIND_DATAA FindData;
	PDOS_FIND_FILE_BLOCK FindFileBlock = (PDOS_FIND_FILE_BLOCK)lpFindFileData;

	do
	{
		/* Continue searching as long as we haven't found a matching file */

		/* If we failed at some point, close the search and return an error */
		if (!FindNextFileA(FindFileBlock->SearchHandle, &FindData))
		{
			FindClose(FindFileBlock->SearchHandle);
			return GetLastError();
		}
	}
	while ((FindData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN |
	                                     FILE_ATTRIBUTE_SYSTEM |
	                                     FILE_ATTRIBUTE_DIRECTORY))
	       & ~FindFileBlock->AttribMask);

	/* Update the block */
	FindFileBlock->Attributes = LOBYTE(FindData.dwFileAttributes);
	FileTimeToDosDateTime(&FindData.ftLastWriteTime,
	                      &FindFileBlock->FileDate,
	                      &FindFileBlock->FileTime);
	FindFileBlock->FileSize = FindData.nFileSizeHigh ? 0xFFFFFFFF
	                                                 : FindData.nFileSizeLow;
	/* Build a short path name */
	if (*FindData.cAlternateFileName)
		strncpy(FindFileBlock->FileName, FindData.cAlternateFileName, sizeof(FindFileBlock->FileName));
	else
		GetShortPathNameA(FindData.cFileName, FindFileBlock->FileName, sizeof(FindFileBlock->FileName));

	return ERROR_SUCCESS;
}

__declspec(dllexport) UCHAR WINAPI demGetPhysicalDriveType(IN UCHAR DriveNumber)
{
	if(DriveNumber < 26) {
		char Volume[] = "A:\\";
		Volume[0] = 'A' + DriveNumber;
		return GetDriveTypeA(Volume);
	}
	return 0;
}

#ifdef _MBCS
static __inline  char *my_strchr(char *str, int chr)
{
	return (char *)_mbschr((unsigned char *)(str), (unsigned int)(chr));
}
static __inline  char *my_strrchr(char *str, int chr)
{
	return (char *)_mbsrchr((unsigned char *)(str), (unsigned int)(chr));
}
#else
#define my_strchr(str, chr) strchr((str), (chr))
#define my_strrchr(str, chr) strrchr((str), (chr))
#endif

static BOOL IsInvalidName(char *Name, size_t Length)
{
	if(strcmp(Name, ".") == 0 || strcmp(Name, "..") == 0) {
		return FALSE;
	}
	if(strlen(Name) > Length) {
		return TRUE;
	}
	if(my_strchr(Name, ' ') != NULL ||
	   my_strchr(Name, '"') != NULL ||
	   my_strchr(Name, '<') != NULL ||
	   my_strchr(Name, '>') != NULL ||
	   my_strchr(Name, '|') != NULL ||
	   my_strchr(Name, ':') != NULL ||
	   my_strchr(Name, '*') != NULL ||
	   my_strchr(Name, '?') != NULL ||
	   my_strchr(Name, '.') != NULL) {
		return TRUE;
	}
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI demIsShortPathName(LPCSTR Path, BOOL Unknown)
{
	if(((Path[0] >= 'A' && Path[0] <= 'Z') || (Path[0] >= 'a' && Path[0] <= 'z')) && Path[1] == ':') {
		Path += 2;
	}
	while(Path[0] != '\0') {
		char Name[MAX_PATH], *Sep1, *Sep2, *Ext;

		strncpy(Name, Path, MAX_PATH);
		Name[MAX_PATH - 1] = '\0';
		
		if((Sep1 = my_strchr(Name, '\\')) != NULL) {
			*Sep1 = '\0';
		}
		if((Sep2 = my_strchr(Name, '/')) != NULL) {
			*Sep2 = '\0';
		}
		Path += strlen(Name) + (Sep1 || Sep2 ? 1 : 0);
		
		Ext = my_strrchr(Name, '.');
		if(Ext) {
			if(Sep1 || Sep2) {
				return FALSE;
			}
			*Ext++ = '\0';
			if(IsInvalidName(Ext, 3)) {
				return FALSE;
			}
		}
		if(IsInvalidName(Name, 8)) {
			return FALSE;
		}
	}
	return TRUE;
}

__declspec(dllexport) DWORD WINAPI demSetCurrentDirectoryGetDrive(LPCSTR CurrentDirectory, PUCHAR DriveNumber)
{
	return ERROR_SUCCESS;
}

// NTVDM Miscellaneous

__declspec(dllexport) BYTE *WINAPI MGetVdmPointer(DWORD addr, DWORD size, BOOL protmode)
{
	return func.MGetVdmPointer(addr, size, protmode);
}

__declspec(dllexport) BYTE *WINAPI Sim32pGetVDMPointer(DWORD addr, BOOL protmode)
{
	return func.MGetVdmPointer(addr, 0, protmode);
}

// NT 3.1 Compatibility
__declspec(dllexport) BYTE *WINAPI Sim32GetVDMPointer(DWORD addr, WORD size, BOOL protmode)
{
	return func.MGetVdmPointer(addr, (DWORD)size, protmode);
}

// NT 3.1 Compatibility
__declspec(dllexport) BOOL WINAPI Sim32FlushVDMPointer(DWORD addr, WORD size, PVOID buf, BOOL protmode)
{
	return TRUE;
}

// NT 3.1 Compatibility
__declspec(dllexport) BOOL WINAPI Sim32FreeVDMPointer(DWORD addr, WORD size, PVOID buf, BOOL protmode)
{
	return TRUE;
}

__declspec(dllexport) BYTE *WINAPI VdmMapFlat(WORD seg, DWORD ofs, VDM_MODE mode)
{
	return func.VdmMapFlat(seg, ofs, mode);
}

// VdmUnmapFlat and VdmFlushCache are not supported on x86
#if 0
__declspec(dllexport) BOOL WINAPI VdmUnmapFlat(WORD seg, DWORD ofs, PVOID buf, VDM_MODE mode)
{
	return TRUE;
}

__declspec(dllexport) BOOL WINAPI VdmFlushCache(WORD seg, DWORD ofs, DWORD size, VDM_MODE mode)
{
	return TRUE;
}
#endif

__declspec(dllexport) BOOL WINAPI VDDInstallMemoryHook(HANDLE hVdd, PVOID addr, DWORD size, PVDD_MEMORY_HANDLER handler)
{
	return func.VDDInstallMemoryHook(hVdd, addr, size, handler);
}

__declspec(dllexport) BOOL WINAPI VDDDeInstallMemoryHook(HANDLE hVdd, PVOID addr, DWORD size)
{
	return func.VDDDeInstallMemoryHook(hVdd, addr, size);
}

__declspec(dllexport) BOOL WINAPI VDDAllocMem(HANDLE hVdd, PVOID addr, DWORD size)
{
	return func.VDDAllocMem(hVdd, addr, size);
}

__declspec(dllexport) BOOL WINAPI VDDFreeMem(HANDLE hVdd, PVOID addr, DWORD size)
{
	return func.VDDFreeMem(hVdd, addr, size);
}

__declspec(dllexport) BOOL WINAPI VDDIncludeMem(HANDLE hVdd, PVOID addr, DWORD size)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDExcludeMem(HANDLE hVdd, PVOID addr, DWORD size)
{
	return FALSE;
}

__declspec(dllexport) void WINAPI call_ica_hw_interrupt(int ms, BYTE line, int count)
{
	func.VDDSimulateInterrupt(ms, line, count);
}

__declspec(dllexport) WORD WINAPI VDDReserveIrqLine(HANDLE hvdd, WORD line)
{
	return 0xffff;
}

__declspec(dllexport) BOOL WINAPI VDDReleaseIrqLine(HANDLE hvdd, WORD line)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDInstallIOHook(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange, PVDD_IO_HANDLERS IOhandler)
{
	return func.VDDInstallIOHook(hvdd, cPortRange, pPortRange, IOhandler);
}

__declspec(dllexport) void WINAPI VDDDeInstallIOHook(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange)
{
	func.VDDDeInstallIOHook(hvdd, cPortRange, pPortRange);
}

__declspec(dllexport) DWORD WINAPI VDDRequestDMA(HANDLE hvdd, WORD ch, PVOID buf, DWORD len)
{
	return func.VDDRequestDMA(hvdd, ch, buf, len);
}

__declspec(dllexport) BOOL WINAPI VDDQueryDMA(HANDLE hvdd, WORD ch, PVDD_DMA_INFO info)
{
	return func.VDDQueryDMA(hvdd, ch, info);
}

__declspec(dllexport) BOOL WINAPI VDDSetDMA(HANDLE hvdd, WORD ch, WORD flag, PVDD_DMA_INFO info)
{
	return func.VDDSetDMA(hvdd, ch, flag, info);
}

__declspec(dllexport) void WINAPI VDDSimulate16()
{
	func.VDDSimulate16();
}

__declspec(dllexport) void WINAPI host_simulate()
{
	func.VDDSimulate16();
}

__declspec(dllexport) void WINAPI VDDTerminateVDM(void)
{
	func.VDDTerminateVDM();
}

__declspec(dllexport) BOOL WINAPI VDDInstallUserHook(HANDLE hvdd, PFNVDD_UCREATE ucr_Handler, PFNVDD_UTERMINATE uterm_Handler, PFNVDD_UBLOCK ublock_handler, PFNVDD_URESUME uresume_handler)
{
	return func.VDDInstallUserHook(hvdd, ucr_Handler, uterm_Handler, ublock_handler, uresume_handler);
}

__declspec(dllexport) BOOL WINAPI VDDDeInstallUserHook(HANDLE hvdd)
{
	return func.VDDDeInstallUserHook(hvdd);
}

// Unknown functions defined in NT_VDD.H
__declspec(dllexport) SHORT WINAPI VDDAllocateDosHandle(ULONG pPDB, PVOID* ppSFT, PVOID* ppJFT)
{
	return 0;
}

__declspec(dllexport) BOOL WINAPI VDDReleaseDosHandle(ULONG pPDB, SHORT hFile)
{
	return FALSE;
}

__declspec(dllexport) void WINAPI VDDAssociateNtHandle(PVOID pSFT, HANDLE h32File, WORD wAccess)
{
	
}

__declspec(dllexport) HANDLE WINAPI VDDRetrieveNtHandle(ULONG pPDB, SHORT hFile, PVOID* ppSFT, PVOID* ppJFT)
{
	return NULL;
}

__declspec(dllexport) VOID WINAPI VdmTraceEvent(USHORT Type, USHORT wData, ULONG  lData)
{
	
}

__declspec(dllexport) BOOL WINAPI VdmParametersInfo(VDM_INFO_TYPE infotype, PVOID pBuffer, ULONG cbBufferSize)
{
	return FALSE;
}

__declspec(dllexport) VDM_INFO_TYPE WINAPI VdmGetParametersInfoError(VOID)
{
	return 0;
}

enum btnval
{
	kBtnOk = 1,
	kBtnCancel = 2,
	kBtnYes = 3,
	kBtnNo = 4,
	kBtnRetry = 5,
	kBtnAbort = 6,
	kBtnIgnore = 7,
	kBtnClose = 8,
	kBtnHelp = 9
};

__declspec(dllexport) DWORD WOWSysErrorBox(LPCSTR title, LPCSTR message, ULONG btn1, ULONG btn2, ULONG btn3)
{
	ULONG mbbtn = MB_DEFBUTTON1;
	ULONG btn;
	int ret;
	
	if(btn2 & 0x8000) {
		mbbtn = MB_DEFBUTTON2;
	} else if(btn3 & 0x8000) {
		mbbtn = MB_DEFBUTTON3;
	}
	btn = (1 << (btn1 & 0xf)) | (1 << (btn2 & 0xf)) | (1 << (btn3 & 0xf));  // try to convert buttons into MB_*
	
	switch(btn)
	{
		case (1 << kBtnOk) | (1 << kBtnCancel):
		case (1 << kBtnOk) | (1 << kBtnClose):
			mbbtn |= MB_OKCANCEL;
			break;
		case (1 << kBtnAbort) | (1 << kBtnRetry) | (1 << kBtnIgnore):
			mbbtn |= MB_ABORTRETRYIGNORE;
			break;
		case (1 << kBtnCancel) | (1 << kBtnRetry) | (1 << kBtnClose):
			mbbtn |= MB_CANCELTRYCONTINUE;
			break;
		case (1 << kBtnYes) | (1 << kBtnNo):
			mbbtn |= MB_YESNO;
			break;
		case (1 << kBtnYes) | (1 << kBtnNo) | (1 << kBtnCancel):
		case (1 << kBtnYes) | (1 << kBtnNo) | (1 << kBtnClose):
			mbbtn |= MB_YESNOCANCEL;
			break;
		case (1 << kBtnRetry) | (1 << kBtnCancel):
		case (1 << kBtnRetry) | (1 << kBtnClose):
			mbbtn |= MB_RETRYCANCEL;
			break;
		default:
			mbbtn |= MB_OK;
			break;
	}
	ret = MessageBoxA(NULL, title, message, mbbtn);
	switch(ret) {
		case IDOK:
			ret = kBtnOk;
			break;
		case IDCANCEL:
			ret = kBtnCancel; // kBtnClose
			break;
		case IDABORT:
			ret = kBtnAbort;
			break;
		case IDRETRY:
		case IDTRYAGAIN:
			ret = kBtnRetry;
			break;
		case IDIGNORE:
			ret = kBtnIgnore;
			break;
		case IDYES:
			ret = kBtnYes;
			break;
		case IDNO:
			ret = kBtnNo;
			break;
		case IDCONTINUE:
			ret = kBtnClose;
			break;
	}
	if((btn1 == ret) || ((btn1 == kBtnClose) && (ret == kBtnCancel))) {
		return 1;
	} else if((btn2 == ret) || ((btn2 == kBtnClose) && (ret == kBtnCancel))) {
		return 2;
	} else if((btn3 == ret) || ((btn3 == kBtnClose) && (ret == kBtnCancel))) {
		return 3;
	}
	return 1;
}
