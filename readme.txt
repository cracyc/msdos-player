MS-DOS Player for Win32-x64 console
								4/24/2015

----- What's this

This is MS-DOS emulator running on Win32-x64 command prompt.
16bit MS-DOS compatible commands can be executed on Win32-x64 envrionment.


----- How to use

Start command prompt and run this emulator with target command file
and any options.

For example compile sample.c with LSI C-86 and execute compiled binary:

	> msdos lcc sample.c
	> msdos sample

The emulator can access host's file path and envrionment variables directly.

Some softwares (for example DoDiary Version 1.55) invite that the environment
variable table should be less than 1024 bytes.
On the Windows OS, there are many variables and the variable table size will
be more than 1024 bytes and it causes an error.

In this case, please specify the option '-e' and only the minimum variables
(COMSPEC/INCLUDE/LIB/PATH/PROMPT/TEMP/TMP/TZ) are copied to the table.

	> msdos -e dd.com

If COMMAND.COM file does exist
- in the same directory as the target program file,
- in the same directory as msdos.exe,
- in the current directory,
- in the directory that is in your PATH environment variable,
the COMMAND.COM file path is copied to COMSPEC.

"Get Version Number" API (INT 21H, AH=30H) returns the version number 7.10.
If you want to change the version number, please specify the option '-vX.XX'.

	> msdos -v3.30 command.com

This archive contains 8 executable binaries:

	i86_x86 	Emulates 8086 and supports both 32bit/64bit Windows
	i86_x64 	Emulates 8086 and supports only 64bit Windows
	i286_x86	Emulates 80286 and supports both 32bit/64bit Windows
	i286_x64	Emulates 80286 and supports only 64bit Windows
	i386_x86	Emulates 80386 and supports both 32bit/64bit Windows
	i386_x64	Emulates 80386 and supports only 64bit Windows
	i486_x86	Emulates 80486 and supports both 32bit/64bit Windows
	i486_x64	Emulates 80486 and supports only 64bit Windows

80286 binaries are much faster than 80386/80486.
If you don't need the protected mode or mnemonics added with 80386/80486,
I recommend i286_x86 or i286_x64 binary.

You can change the cpu model from 80486 to Pentium/PRO/MMX/2/3/4 or MediaGX.
Replace "HAS_I486" with for example "HAS_PENTIUM" in source/msdos.vcproj.
See also source/msdos.cpp, line 26-58.


----- Supported hardwares

CPU 8086/80286/80386/80486, RAM 16MB, PIC, PIT, RTC CMOS,
Keyboard Controller (A20 Line Mask, CPU Reset), VGA Status Register


----- Memory map

000000H -	Conventional Memory (736KB)
0B8000H -	VGA Text Video RAM
0C0000H -	Upper Memory Block (224KB)
0F8000H -	V-TEXT Shadow Buffer
0FFFF0H -	CPU Boot Address
100000H -	Upper Memory (15MB)


----- Supported system calls

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
	130*H	Write String
	1310H	Read Characters and Standard Attributes
	1311H	Read Characters and Extended Attributes
	1320H	Write Characters and Standard Attributes
	1321H	Write Characters and Extended Attributes
	1BH	Perform Gray-Scale Summing
	8200H	Get/Set Scroll Mode
	EFH	Get Video Adapter Type and Mode (*1)
	FEH	Get Shadow Buffer
	FFH	Update Screen from Shadow Buffer

INT 11H		PC BIOS - Get Equipment List

INT 12H		PC BIOS - Get Memory Size

INT 15H		PC BIOS

	2300H	Get CMOS Data
	2301H	Set CMOS Data
	2400H	Disable A20 Gate
	2401H	Enable A20 Gate
	2402H	Get A20 Gate Status
	2403H	A20 Support
	49H	Get BIOS Type
	86H	Wait
	87H	Copy Extended Memory
	88H	Get Extended Memory Size
	89H	Switch to Protected Mode
	C9H	Get CPU Type and Mask Revision
	CA00H	Read CMOS Memory
	CA01H	Write CMOS Memory

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
	11H	Search First Entry with FCB
	12H	Search Next Entry with FCB
	13H	Delete File with FCB
	18H	Null Function for CP/M Compatibility
	19H	Current Disk
	1AH	Set Disk Transfer Address
	1BH	Get Allocation Information for Default Drive
	1CH	Get Allocation Information for Specified Drive
	1DH	Null Function for CP/M Compatibility
	1EH	Null Function for CP/M Compatibility
	1FH	Get Drive Parameter Block for Default Drive
	20H	Null Function for CP/M Compatibility
	25H	Set Vector
	26H	Create New Program Segment Prefix
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
	3306H	Get MS-DOS Version (*2)
	3307H	Windows95 - Set/Clear DOS_FLAG
	35H	Get Vector
	36H	Get Disk Free Space
	3700H	Set Switch Character
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
	45H	Duplicate File Handle
	46H	Force Duplicate of Handle
	47H	Get Current Directory
	48H	Allocate Memory
	49H	Free Allocated Memory
	4AH	Modify Allocated Memory Blocks
	4B00H	Load and Execute Program
	4B01H	Load Program
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
	60H	Canonicalize Filename Or Path
	61H	Reserved Fnction
	62H	Get Program Segment Prefix Address
	6300H	Get DBCS Vector
	6507H	Get DBCS Vector
	6520H	Character Capitalization
	6521H	String Capitalization
	6522H	ASCIIZ Capitalization
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
	7303H	Windows95 - FAT32 - Get Extended Free Space on Drive

INT 23H		Ctrl-Break Address

INT 24H		Critical Error Handler

INT 25H		Absolute Disk Read

INT 26H		Absolute Disk Write

INT 27H		Terminate and Stay Resident

INT 28H		DOS Idle

INT 29H		DOS Fast Character I/O

INT 2EH		Pass Command to Command Interpreter for Execution

INT 2FH		Multiplex Interrupt

	1600H	Windows Enhanced Mode Installation Check
	1A00H	ANSI.SYS Installation Check (*3)
	4300H	XMS Installation Check (*4)
	4A01H	Query Free HMA Space (*4)
	4A02H	Allocate HMA Space (*4)
	4F00H	BILING - Get Version
	4F01H	BILING - Get Code Page
	AE00H	
	AE01H	Execute
	B700H	APPEND Installation Check (*4)

(*1) Not a Hercules-compatible video adapter
(*2) MS-DOS Version: 7.10
(*3) ANSI.SYS is installed
(*4) XMS/HMA/APPEND are not installed


----- Thanks

80286 code is based on MAME 0.149.
80386 code is based on MAME 0.152 and fixes in MAME 0.154 to 0.160 are applied.

INT 15H AH=87H (copy extended memory) and AH=89H (switch to protected mode)
are based on DOSBox.


----------------------------------------
TAKEDA, toshiya
t-takeda@m1.interq.or.jp
http://homepage3.nifty.com/takeda-toshiya/
