MS-DOS Player for Win32-x64 console
								2/2/2017

----- What's this

This is MS-DOS emulator running on Win32-x64 command prompt.
16bit MS-DOS compatible commands can be executed on Win32-x64 envrionment.

This emulator inherits the environment variables from the host Windows,
and a target command can access the host's file path directly.

So you can execute 16bit MS-DOS compatible commands on your 64bit Windows
in the same sence as you did on 32bit Windows, and you do not need to copy
any files to/from a virtual machine (VMware, Virtual PC, XP mode, or others).

NOTE: This emulator DOES NOT support Win16 execution files.

This emulator aims to support character user interface utilities, for example
file converters, compilers, assemblers, debuggers, and text editors.
This emulator DOES NOT support graphic/sound hardwares and DOES NOT aim to
support game softwares. I recommend DOSBOx for this purpose.


----- How to use

Start a command prompt and run this emulator with a target command file
and any options.

For example, compile a sample.c with LSI C-86 and execute the compiled binary:

	> msdos lcc sample.c
	> msdos sample.exe


----- Options

Usage: MSDOS [-b] [-c[(new exec file)] [-p[P]]] [-d] [-e] [-i] [-m] [-n[L[,C]]]
             [-s[P1[,P2[,P3[,P4]]]]] [-vX.XX] [-wX.XX] [-x] (command) [options]

	-b	stay busy during keyboard polling
	-c	convert command file to 32bit or 64bit execution file
	-p	record current code page when convert command file
	-d	pretend running under straight DOS, not Windows
	-e	use a reduced environment block
	-i	ignore invalid instructions
	-m	restrict free memory to 0x7FFF paragraphs
	-n	create a new buffer (25 lines, 80 columns by default)
	-s	enable serial I/O and set host's COM port numbers
	-v	set the DOS version
	-w	set the Windows version
	-x	enable XMS and LIM EMS

EDIT.COM does not work correctly when a free memory space is large.
Please specify the option '-m' to restrict free memory to 0x7FFF paragraphs.

	> msdos -m edit.com

"Windows Enhanced Mode Installation Check" API (INT 2FH, AX=1600H) and
"Identify Windows Version and Type" API (INT 2FH, AX=160AH) return the Windows
version number 4.10.
If you want to change the version number, please specify the option '-wX.XX'.

	> msdos -w3.10 command.com

Or if you want to pretend that Windows is not running, specify the option '-d'.

	> msdos -d command.com

"Get Version Number" API (INT 21H, AH=30H) returns the version number 7.10.
If you want to change the version number, please specify the option '-vX.XX'.

	> msdos -v3.30 command.com

NOTE: "Get True Version Number" API (INT 21H, AX=3306H) always returns
the version number 7.10 and '-v' option is not affected.

NOTE: the Windows version 4.10 and the DOS version 7.10 are same as Windows 98,

To enable XMS (i286 or later) and LIM EMS, please specify the option '-x'.
In this time, the memory space 0C0000H-0CFFFFH are used for EMS page frame,
so the size of UMB is decreased from 224KB to 160KB.

To enable the serial I/O, please specify the option '-s[P1[,P2[,P3[,P4]]]]'.
If you specify '-s', the virtual COM1-COM4 are connected to the host's COM
ports found by SetupDiGetClassDevs() API.
You can specify the host's COM port numbers connected to the virtual COM ports
by adding numbers to '-s' option, for example '-s3,4,1,2'.

NOTE: The maximum baud rate is limited to 9600bps.


----- Environment variable table

Basically, the environment variable table on the host Windows is copied to
the table on the virtual machine (hereinafter, referred to as "virtual table"),
and in this time, APPEND/MSDOS_PATH/PATH/TEMP/TMP values are converted to
short path.

Some softwares (for example DoDiary Version 1.55) invite that the environment
variable table should be less than 1024 bytes.
On the Windows OS, there are too many variables and the environment variable
table size will be more than 1024 bytes and it causes an error.

In this case, please specify the option '-e' and only the minimum environment
variables (APPEND/COMSPEC/INCLUDE/LIB/MSDOS_PATH/PATH/PROMPT/TEMP/TMP/TZ) are
copied to the virtual table.

	> msdos -e dd.com

The environment variable COMSPEC is not copied from the host Windows, and its
value on the virtual table is always "C:\COMMAND.COM".

If a program tries to open the "C:\COMMAND.COM" file, the file is redirected
to the path in the environment variable MSDOS_COMSPEC on the host Windows.
If MSDOS_COMSPEC is not defined, it is redirected to the COMMAND.COM file that
does exist:

- in the same directory as the target program file,
- in the same directory as the running msdos.exe,
- in the current directory,
- in the directory that is in MSDOS_PATH and PATH environment variables

You may have a directory that contains 16bit command files, and you may want
not to add this path to PATH on the host Windows but want to add it to PATH
on the virtual table.

In this case, please define MSDOS_PATH on the host Windows, and its value is
copied to the top of PATH on the virtual table.
In other words, the value of PATH on the virtual table is MSDOS_PATH;PATH
on the host Windows, so a program searches files in directories in MSDOS_PATH
before files in directories in PATH.

You can also define MSDOS_APPEND on the host Windows, and its value is copied
to the top of APPEND on the virtual table.

The environment variables TEMP and TMP on the host Windows may be very long,
in usually, it is "C:\User\(Your User Name)\AppData\Local\Temp".
DOSSHELL.EXE tries to create a batch file in the TEMP directry to start
the selected program, and it may not work correctly because of the too long
batch file path.
In this case, please define MSDOS_TEMP on the host Windows, for example
"C:\TEMP", and its value is copied to TEMP and TMP on the virtual table.

NOTE: MSDOS_(APPEND/COMSPEC/TEMP) are not copied to  the virtual table,
but MSDOS_PATH is copied to, because some softwares may refer MSDOS_PATH.


----- Convert command file to 32bit or 64bit execution file

You can convert a 16bit command file to a single 32bit or 64bit execution file
by embeding a command file to the msdos.exe.

For exmaple, you can convert LIST.COM by this command:

	> msdos -cLIST32.EXE LIST.COM

and you can simply run LIST32.EXE without msdos.exe.

NOTE: Please specify a target command file with its extension, and specify
a new execution file name other than "msdos.exe". If a new execution file name
is not specified (only "-c" is specified), it is "new_exec_file.exe".

Other options' value are also stored to a new execution file, for example:

	> msdos -cCOMMAND32.EXE -v6.22 -x COMMAND.COM

When you run COMMAND32.EXE, it starts COMMAND.COM with the version 6.22 and
XMS/EMS option enabled.

The active code page can be also stored with '-p' option, for example:

	> msdos -cSW1US32.EXE -p437 -s SW1US.EXE

	> chcp 437
	> msdos -cSW1US32.EXE -p -s SW1US.EXE

When you run SW1US32.EXE, it starts SW1US.EXE with the code page 437 and
serial I/O option enabled.

At the execution time, an embedded command file will be extracted with
the original command file name in the current directory.
But if the original command file exists, it will be extracted with
the temporary command file name not to overwrite the original command file.
The extracted command file will be removed when the execution is finished.


----- Binaries

This archive contains 8 executable binaries:

	i86_x86 	Emulates 8086  and supports both 32bit/64bit Windows
	i86_x64 	Emulates 8086  and supports only 64bit Windows
	i286_x86	Emulates 80286 and supports both 32bit/64bit Windows
	i286_x64	Emulates 80286 and supports only 64bit Windows
	i386_x86	Emulates 80386 and supports both 32bit/64bit Windows
	i386_x64	Emulates 80386 and supports only 64bit Windows
	i486_x86	Emulates 80486 and supports both 32bit/64bit Windows
	i486_x64	Emulates 80486 and supports only 64bit Windows

8086 binaries are much faster than 80286/80386/80486.
If you don't need the protected mode or new mnemonics of 80286/80386/80486,
I recommend i86_x86 or i86_x64 binary.

The VC++ project file "msdos.vcproj/vcxproj" also contains the configurations
for 80186, NEC V30, Pentium/PRO/MMX/2/3/4, and Cyrix MediaGX.
You can build all binaries for several cpu models by running build8_all.bat
or build12_all.bat.
(You need VC++ 2008 with Service Pack 1 or VC++ 2013 with Update 5.)


----- Supported hardwares

This emulator provides a very simple IBM PC compatible hardware emulation:

CPU 8086/80286/80386/80486, RAM 1MB/16MB/32MB, LIM EMS 32MB (Hardware EMS),
PC BIOS, DMA Controller (dummy), Interrupt Controller, System Timer,
Parallel I/O (dummy), Serial I/O (COM1-COM4), Real Time Clock + CMOS Memory,
VGA Status Register, Keyboard Controller (A20 Line Mask, CPU Reset),
and 3-Button Mouse

NOTE:
- Graphic/Sound hardwares are NOT implemented.
- DMA Controller is implemented, but FDC and HDC are not connected.
- Parallel I/O is implemented but is not connected to the host's LPT ports.
- Serial I/O is implemented and can be connected to the host's COM ports.


----- Memory map

000000H -	Conventional Memory (736KB)
0B8000H -	VGA Text Video RAM (32KB)
----------
0C0000H -	Upper Memory Block (224KB)
--- Or ---
0C0000H -	EMS Page Frame (64K)
0D0000H -	Upper Memory Block (160KB)
----------
0F8000H -	V-TEXT Shadow Buffer (32KB-48B)
0FFFD0H		Dummy BIOS/DOS/Driver Service Routines
0FFFF0H -	CPU Boot Address
100000H -	Upper Memory (15MB/31MB)


----- Supported system calls

INT 08H		PC BIOS - System Timer

INT 10H		PC BIOS - Video

	00H	Set Video Mode
	02H	Set Cursor Position
	03H	Get Cursor Position and Size
	05H	Select Active Display Page
	06H	Scroll Up Window
	07H	Scroll Down Window
	08H	Read Character and Attribute at Cursor Position
	09H	Write Character and Attribute at Cursor Position
	0AH	Write Character Only at Cursor Position
	0EH	Teletype Output
	0FH	Get Current Video Mode
	1101H	Set Video Mode to 80x28 (Load 8x14 Character Generator ROM)
	1102H	Set Video Mode to 80x50 (Load 8x8 Character Generator ROM)
	1104H	Set Video Mode to 80x25 (Load 8x16 Character Generator ROM)
	1111H	Set Video Mode to 80x28 (Load 8x14 Character Generator ROM)
	1112H	Set Video Mode to 80x50 (Load 8x8 Character Generator ROM)
	1114H	Set Video Mode to 80x25 (Load 8x16 Character Generator ROM)
	1118H	Set Video Mode to 80x50 (Set V-TEXT Vertically Long Mode)
	1130H	Get Font Information
	12H	Alternate Function Select (BL=10H)
	130*H	Write String
	1310H	Read Characters and Standard Attributes
	1311H	Read Characters and Extended Attributes
	1320H	Write Characters and Standard Attributes
	1321H	Write Characters and Extended Attributes
	1A00H	Get Display Combination Code
	1BH	Perform Gray-Scale Summing
	8200H	Get/Set Scroll Mode
	8300H	Get Video RAM Address
	90H	Get Physical Workstation Display Mode
	91H	Get Physical Workstation Adapter Type
	EFH	Get Video Adapter Type and Mode (*1)
	FEH	Get Shadow Buffer
	FFH	Update Screen from Shadow Buffer

INT 11H		PC BIOS - Get Equipment List

INT 12H		PC BIOS - Get Memory Size

INT 14H		PC BIOS - Serial I/O

	00H	Initialize Port
	01H	Write Character to Port
	02H	Read Character from Port
	03H	Get Port Status
	04H	Extended Initialize
	0500H	Read Modem Control Register
	0501H	Write Modem Control Register

INT 15H		PC BIOS

	1000H	TopView Pause
	2300H	Get CMOS Data
	2301H	Set CMOS Data
	2400H	Disable A20 Gate
	2401H	Enable A20 Gate
	2402H	Get A20 Gate Status
	2403H	A20 Support
	49H	Get BIOS Type
	5000H	Get Address of "Read Font" Function
	5001H	Get Address of "Write Font" Function
	86H	Wait
	87H	Copy Extended Memory
	88H	Get Extended Memory Size
	89H	Switch to Protected Mode
	8AH	Get Big Memory Size
	C9H	Get CPU Type and Mask Revision
	CA00H	Read CMOS Memory
	CA01H	Write CMOS Memory
	E801H	Get Memory Size for >64M Configurations

INT 16H		PC BIOS - Keyboard

	00H	Get Keystroke
	01H	Check for Keystroke
	02H	Get Shift Flags
	05H	Store Keystroke in Keyboard Buffer
	10H	Get Keystroke
	11H	Check for Keystroke
	12H	Get Extended Shift States

INT 1AH		PC BIOS - Timer

	1A00H	Get System Timer
	1A02H	Get Real Time Clock Time
	1A04H	Get Real Time Clock Date
	1A0AH	Read System-Timer Day Counter

INT 20H		Program Terminate

INT 21H		MS-DOS System Call

	00H	Program Terminate
	01H	Keyboard Input
	02H	Console Output
	03H	(Auxiliary Input)
	04H	(Auxiliary Output)
	05H	(Printer Output)
	06H	Direct Console I/O
	07H	Direct Console Input
	08H	Console Input without Echo
	09H	Print String
	0AH	Buffered Keyboard Input
	0BH	Check Console Status
	0CH	Character Input with Buffer Flush
	0DH	Disk Reset
	0EH	Select Disk
	0FH	Open File with FCB
	10H	Close File with FCB
	11H	Search First Entry with FCB
	12H	Search Next Entry with FCB
	13H	Delete File with FCB
	14H	Sequential Read with FCB
	15H	Sequential Write with FCB
	16H	Create New File with FCB
	17H	Rename File with FCB
	18H	Null Function for CP/M Compatibility
	19H	Current Disk
	1AH	Set Disk Transfer Address
	1BH	Get Allocation Information for Default Drive
	1CH	Get Allocation Information for Specified Drive
	1DH	Null Function for CP/M Compatibility
	1EH	Null Function for CP/M Compatibility
	1FH	Get Drive Parameter Block for Default Drive
	20H	Null Function for CP/M Compatibility
	21H	Random Read with FCB
	22H	Randome Write with FCB
	23H	Get File Size with FCB
	24H	Set Relative Record Field with FCB
	25H	Set Vector
	26H	Create New Program Segment Prefix
	27H	Random Block Read with FCB
	28H	Random Block Write with FCB
	29H	Parse File Name
	2AH	Get Date
	2BH	Set Date
	2CH	Get Time
	2DH	Set Time
	2EH	Set/Reset Verify Switch
	2FH	Get Disk Transfer Address
	30H	Get Version Number (*2)
	31H	Keep Process
	32H	Get DOS Drive Parameter Block
	3300H	Get Ctrl-Break
	3301H	Set Ctrl-Break
	3305H	Get Boot Drive
	3306H	Get True Version Number (*3)
	3307H	Windows95 - Set/Clear DOS_FLAG
	34H	Get Address of InDOS Flag
	35H	Get Vector
	36H	Get Disk Free Space
	3700H	Get Switch Character
	3701H	Set Switch Character
	3702H	Get AvailDev Flag
	3703H	Set AvailDev Flag
	3800H	Get Current Country Specifiy Information
	39H	Create Subdirectory
	3AH	Remove Subdirectory
	3BH	Change Current Directory
	3CH	Create File
	3DH	Open File Handle
	3EH	Close File Handle
	3FH	Read from File or Device
	40H	Write to File or Device
	41H	Erase File from Directory
	42H	Move File Read/Write Pointer
	4300H	Get File Attribute
	4301H	Set File Attribute
	4400H	Get Device Information
	4401H	Set Device Information
	4406H	Get Input Status
	4407H	Get Output Status
	4408H	Device Removable Query
	4409H	Device Local or Remote Query
	440AH	Handle Local or Remote Query
	440CH	Generic Character Device Request
	440DH	Generic Block Device Request
	4410H	Query Generic IOCTRL Capability (Handle)
	4411H	Query Generic IOCTRL Capability (Drive)
	45H	Duplicate File Handle
	46H	Force Duplicate of Handle
	47H	Get Current Directory
	48H	Allocate Memory
	49H	Free Allocated Memory
	4AH	Modify Allocated Memory Blocks
	4B00H	Load and Execute Program
	4B01H	Load Program
	4B03H	Load Overlay
	4CH	Terminate Process
	4DH	Get Subprocess Return Code
	4EH	Find First Matching File
	4FH	Find Next Matching File
	50H	Set Program Segment Prefix Address
	51H	Get Program Segment Prefix Address
	52H	Get DOS Info Table
	54H	Get Verify State
	55H	Create Child Program Segment Prefix
	56H	Rename File
	5700H	Get Last Written Date and Time
	5701H	Set Last Written Date and Time
	5704H	Windows95 - Get Last Access Date and Time
	5705H	Windows95 - Set Last Access Date and Time
	5706H	Windows95 - Get Creation Date and Time
	5707H	Windows95 - Set Creation Date and Time
	5800H	Get Memory Allocation Strategy
	5801H	Set Memory Allocation Strategy
	5802H	Get UMB Link State
	5803H	Set UMB Link State
	59H	Get Extended Error Information
	5AH	Create Unique File
	5BH	Create New File
	5CH	Lock/Unlock File Access
	5D06H	Get Address of DOS Swappable Data Area
	5F02H	Get Redirection List Entry
	60H	Canonicalize Filename Or Path
	61H	Reserved Fnction
	62H	Get Program Segment Prefix Address
	6300H	Get DBCS Vector
	6501H	Get General Internationalization Info
	6502H	Get Upper Case Table
	6503H	Get Lower Case Table
	6504H	Get File Name Upper Case Table
	6505H	Get File Name Lower Case Table
	6506H	Get Collating Sequence Table
	6507H	Get DBCS Vector
	6520H	Character Capitalization
	6521H	String Capitalization
	6522H	ASCIIZ Capitalization
	6523H	Determine If Character Represents Yes/No Response
	65A0H	Character Capitalization
	65A1H	String Capitalization
	65A2H	ASCIIZ Capitalization
	6601H	Get Global Code Page Table
	6602H	Get Global Code Page Table
	67H	Set Handle Count
	68H	Commit File
	6900H	Get Disk Serial Number
	6AH	Commit File
	6BH	Null Function
	6CH	Extended Open/Create
	710DH	Windows95 - Reset Drive
	7139H	Windows95 - LFN - Create Subdirectory
	713AH	Windows95 - LFN - Remove Subdirectory
	713BH	Windows95 - LFN - Change Current Directory
	7141H	Windows95 - LFN - Erase File from Directory
	7143H	Windows95 - LFN - Get/Set File Attribute
	7147H	Windows95 - LFN - Get Current Directory
	714EH	Windows95 - LFN - Find First Matching File
	714FH	Windows95 - LFN - Find Next Matching File
	7156H	Windows95 - LFN - Rename File
	7160H	Windows95 - LFN - Canonicalize Filename Or Path
	716CH	Windows95 - LFN - Extended Open/Create
	71A0H	Windows95 - LFN - Get Volume Information
	71A1H	Windows95 - LFN - Terminate Directory Search
	71A6H	Windows95 - LFN - Get File Information by Handle
	71A7H	Windows95 - LFN - Convert File Time/DOS Time
	71A8H	Windows95 - LFN - Generate Short File Name
	71AAH	Windows95 - LFN - Create/Terminate/Query SUBST
	7300H	Windows95 - FAT32 - Get Drive Locking
	7302H	Windows95 - FAT32 - Get Extended DPB
	7303H	Windows95 - FAT32 - Get Extended Free Space on Drive
	DBH	Novell NetWare - Workstation - Get Number of Local Drives
	DCH	Novell NetWare - Connection Services - Get Connection Number

INT 23H		Ctrl-Break Address

INT 24H		Critical Error Handler

INT 25H		Absolute Disk Read

INT 26H		Absolute Disk Write (*4)

INT 27H		Terminate and Stay Resident

INT 28H		DOS Idle

INT 29H		DOS Fast Character I/O

INT 2EH		Pass Command to Command Interpreter for Execution

INT 2FH		Multiplex Interrupt

	0500H	DOS 3.0+ Critical Error Handler - Installation Check
	0502H	DOS 3.0+ Critical Error Handler - Expand Error into String
	1200H	DOS 3.0+ internal - Installation Check
	1202H	DOS 3.0+ internal - Get Interrupt Address
	1203H	DOS 3.0+ Internal - Get DOS Data Segment
	1204H	DOS 3.0+ internal - Normalize Path Separator
	1205H	DOS 3.0+ internal - Output Character to Standard Output
	120DH	DOS 3.0+ internal - Get Date and Time
	1211H	DOS 3.0+ internal - Normalize ASCIZ Filename
	1212H	DOS 3.0+ internal - Get Length of ASCIZ String
	1213H	DOS 3.0+ internal - Uppercase Character
	1214H	DOS 3.0+ internal - Compare Far Pointers
	1216H	DOS 3.0+ internal - Get System File Table Entry
	121AH	DOS 3.0+ internal - Get File's Drive
	121BH	DOS 3.0+ internal - Set Year/Length of February
	121EH	DOS 3.0+ internal - Compare Filenames
	1220H	DOS 3.0+ internal - Get Job File Table Entry
	1221H	DOS 3.0+ internal - Canonicalize File Name
	1225H	DOS 3.0+ internal - Get Length of ASCIZ String
	1226H	DOS 3.3+ internal - Open File
	1227H	DOS 3.3+ internal - Close File
	1228H	DOS 3.3+ internal - Move File Pointer
	1229H	DOS 3.3+ internal - Read From File
	122BH	DOS 3.3+ internal - IOCTL
	122CH	DOS 3.3+ internal - Get Device Chain
	122DH	DOS 3.3+ internal - Get Extended Error Code
	122EH	DOS 4.0+ internal - Get Error Table Addresses
	122FH	DOS 4.0+ internal - Set DOS Version Number to Return
	1400H	NLSFUNC.COM - Installation Check
	1401H	NLSFUNC.COM - Change Code Page
	1402H	NLSFUNC.COM - Get Extended Country Info
	1403H	NLSFUNC.COM - Set Code Page
	1404H	NLSFUNC.COM - Get Country Info
	1600H	Windows - Windows Enhanced Mode Installation Check (*5)
	1605H	Windows - Windows Enhanced Mode & 286 DOSX Init Broadcast
	160AH	Windows - Identify Windows Version and Type (*5)
	1680H	Windows, DPMI - Release Current Virtual Machine Time-Slice
	1A00H	ANSI.SYS - Installation Check
	4000H	Windows 3+ - Get Virtual Device Driver (VDD) Capabilities
	4300H	XMS - Installation Check
	4310H	XMS - Get Driver Address
	4810H	Read Input Line From Console
	4A01H	DOS 5.0+ - Query Free HMA Space
	4A02H	DOS 5.0+ - Allocate HMA Space
	4A03H	Windows95 - (De)Allocate HMA Memory Block
	4A04H	Windows95 - Get Start of HMA Memory Chain
	AD00H	DISPLAY.SYS - Installation Check
	AD01H	DISPLAY.SYS - Set Active Code Page
	AD02H	DISPLAY.SYS - Get Active Code Page
	AD03H	DISPLAY.SYS - Get Code Page Information
	4F00H	BILING - Get Version
	4F01H	BILING - Get Code Page
	AE00H	Installable Command - Installation Check
	AE01H	Installable Command - Execute

INT 33H		Mouse

	0000H	Reset Driver and Rest Status
	0001H	Show Mouse Cursor
	0002H	Hide Mouse Cursor
	0003H	Return Position and Button Status
	0005H	Return Button Press Data
	0006H	Return Button Release Data
	0007H	Define Horizontal Cursor Range
	0008H	Define Vertical Cursor Range
	000BH	Read Motion Counters
	000CH	Define Interrupt Sub Routine Parameters
	000FH	Define Mickey/Pixel Ratio
	0011H	Get Number of Buttons
	0014H	Exchange Interrupt Sub Routines
	0015H	Return Driver Storage Requirements
	0016H	Save Driver State
	0017H	Restore Driver State
	001AH	Set Mouse Sensitivity
	001BH	Return Mouse Sensitivity
	001DH	Define Display Page Number
	001EH	Return Display Page Number
	0021H	Software Reset
	0022H	Set Language for Messages
	0023H	Get Language for Messages
	0024H	Get Software Version, Moouse Type, and IRQ Number (*6)
	0026H	Get Maximum Virtual Coordinates
	002AH	Get Cursor Host Spot
	0031H	Get Current Minimum/Maximum Virtual Coordinates
	0032H	Get Active Advanced Functions

INT 67H		LIM EMS

	40H	LIM EMS - Get Manager Status
	41H	LIM EMS - Get Page Frame Segment
	42H	LIM EMS - Get Number of Pages
	43H	LIM EMS - Get Handle and Allocate Memory
	44H	LIM EMS - Map/Unmap Memory
	45H	LIM EMS - Release Handle and Memory
	46H	LIM EMS - Get EMM Version (*7)
	47H	LIM EMS - Save Mapping Context
	48H	LIM EMS - Restore Mapping Context
	4BH	LIM EMS - Get Number of EMM Handles
	4CH	LIM EMS - Get Pages Owned By Handle
	4DH	LIM EMS - Get Pages for All Handles
	4E00H	LIM EMS - Get Page Map
	4E01H	LIM EMS - Set Page Map
	4E02H	LIM EMS - Get and Set Page Map
	4E03H	LIM EMS - Get Page Map Array Size
	4F00H	LIM EMS 4.0 - Get Partial Page Map
	4F01H	LIM EMS 4.0 - Set Partial Page Map
	4F02H	LIM EMS 4.0 - Get Partial Page Map Array Size
	50H	LIM EMS 4.0 - Map/Unmap Multiple Handle Pages
	51H	LIM EMS 4.0 - Reallocate Pages
	52H	LIM EMS 4.0 - Get/Set Handle Attributes
	5300H	LIM EMS 4.0 - Get Handle Name
	5301H	LIM EMS 4.0 - Set Handle Name
	5400H	LIM EMS 4.0 - Get Handle Directory
	5401H	LIM EMS 4.0 - Search for Named Handle
	5402H	LIM EMS 4.0 - Get Total Handles
	5700H	LIM EMS 4.0 - Move Memory Region
	5701H	LIM EMS 4.0 - Exchange Memory Region
	5800H	LIM EMS 4.0 - Get Mappable Physical Address Array
	5801H	LIM EMS 4.0 - Get Mappable Physical Address Array Entries
	5A00H	LIM EMS 4.0 - Allocate Standard Pages
	5A01H	LIM EMS 4.0 - Allocate Raw Pages

CALL FAR XMS

	00H	XMS - Get XMS Version Number (*8)
	01H	XMS - Request High Memory Area
	02H	XMS - Release High Memory Area
	03H	XMS - Global Enable A20
	04H	XMS - Global Disable A20
	05H	XMS - Local Enable A20
	06H	XMS - Local Disable A20
	07H	XMS - Query A20 State
	08H	XMS - Query Free Extended Memory
	09H	XMS - Allocate Extended Memory Block
	0AH	XMS - Free Extended Memory Block
	0BH	XMS - Move Extended Memory Block
	0CH	XMS - Lock Extended Memory Block
	0DH	XMS - Unlock Extended Memory Block
	0EH	XMS - Get Handle Information
	0FH	XMS - Reallocate Extended Memory Block
	10H	XMS - Request Upper Memory Block
	11H	XMS - Release Upper Memory Block
	12H	XMS 3.0 - Reallocate Upper Memory Block
	88H	XMS 3.0 - Query Free Extended Memory
	89H	XMS 3.0 - Allocate Any Extended Memory
	8EH	XMS 3.0 - Get Extended EMB Handle Information
	8FH	XMS 3.0 - Reallocate Any Extended Memory Block

(*1) Not a Hercules-compatible video adapter
(*2) MS-DOS Version: 7.10 (default) or specified version with -v option
(*3) MS-DOS Version: 7.10, -v option is not affected
(*4) Support only floppy disk drive
(*5) Windows Version: 4.10 (default) or specified version with -w option
(*6) Mouse Version: 8.05
(*7) EMS Version: 4.0
(*8) XMS Version: 3.95


--- License

The copyright belongs to the author, but you can use the source codes under
the GNU GENERAL PUBLIC LICENSE Version 2.

See also COPYING.txt for more details about the license.


----- Thanks

8086/80186/80286 code is based on MAME 0.149.
NEC V30 instructions code is based on MAME 0.128
80386 code is based on MAME 0.152 and fixes in MAME 0.154 to 0.174 are applied.

INT 15H AH=87H (copy extended memory), AH=89H (switch to protected mode),
and some DOS info block improvements are based on DOSBox.

Patched by Mr.Sagawa, Mr.sava, Mr.Kimura (emk) and Mr.Jason Hood.

----------------------------------------
TAKEDA, toshiya
t-takeda@m1.interq.or.jp
http://takeda-toshiya.my.coocan.jp/
