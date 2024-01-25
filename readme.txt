MS-DOS Player for Win32-x64 console
								7/1/2022

----- What's This

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

NOTE: This emulator DOES NOT support graphic/sound hardwares and DOES NOT
aim to support game softwares. I recommend DOSBOx for this purpose.


----- How To Use

Start a command prompt and run this emulator with a target command file
and any options.

For example, compile a sample.c with LSI C-86 and execute the compiled binary:

	> msdos lcc sample.c
	> msdos sample.exe


----- Options

Usage:

MSDOS [-b] [-c[(new exec file)] [-p[P]]] [-d] [-e] [-i] [-m] [-n[L[,C]]]
      [-s[P1[,P2[,P3[,P4]]]]] [-sd] [-sc] [-vX.XX] [-wX.XX] [-x] [-a] [-l]
      (command) [options]

	-b	stay busy during keyboard polling
	-c	convert command file to 32bit or 64bit execution file
	-p	record current code page when convert command file
	-d	pretend running under straight DOS, not Windows
	-e	use a reduced environment block
	-i	ignore invalid instructions
	-m	restrict free memory to 0x7FFF paragraphs
	-n	create a new buffer (25 lines, 80 columns by default)
	-s	enable serial I/O and set host's COM port numbers
	-sd	enable DTR/DSR flow control
	-sc	enable RTS/CTS flow control
	-v	set the DOS version
	-w	set the Windows version
	-x	enable XMS and LIM EMS
	-a	disable ANSI.SYS
	-l	draw box lines with ank characters

ISH.COM contains any invalid instructions and it cause an error.
Please specify the option '-i' to ignore the invalid instructions.

	> msdos -i ish.com

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

If you specify '-sd', DTR/DSR flow control is enabled.
In this time, MS-DOS Player sets/gets DTR and DSR pins of the host's COM port.
Otherwise, virtual DTR pin is internally connected to virtual DSR pin, and
DTR pin of the host's COM port is always active.

If you specify '-sc', RTS/CTS flow control is enabled.
In this time, MS-DOS Player sets/gets RTS and CTS pins of the host's COM port.
Otherwise, virtual RTS pin is internally connected to virtual CTS pin, and
RTS pin of the host's COM port is always active.

NOTE: The maximum baud rate is limited to 9600bps.


----- Environment Variable Table

Basically, the environment variable table on the host Windows is copied to
the table on the virtual machine (hereinafter, referred to as "virtual table"),
and in this time, APPEND/MSDOS_PATH/PATH/TEMP/TMP values are converted to
short path.

Some softwares (for example DoDiary Version 1.55) invite that the environment
variable table should be less than 1024 bytes.
On the Windows OS, there are too many variables and the environment variable
table size will be more than 1024 bytes and it causes an error.

In this case, please specify the option '-e' and only the minimum environment
variables (APPEND/COMSPEC/LASTDRIVE/MSDOS_PATH/PATH/PROMPT/TEMP/TMP/TZ) are
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

If the environment variable MSDOS_LASTDRIVE is defined on the host Windows,
its value is copied to LASTDRIVE on the virtual table.
If both MSDOS_LASTDRIVE and LASTDRIVE are not defined on the host Windows,
LASTDRIVE=Z is automatically defined on the virtual table.

If the environment variable PROMPT is not defined on the host Windows,
PROMPT=$P$G is automatically defined on the virtual table.

If the environment variable MSDOS_TZ is defined on the host Windows,
its value is copied to TZ on the virtual table.
If both MSDOS_TZ and TZ are not defined on the host Windows,
and if your timezone is in the following table, TZ is automatically defined
on the virtual table, for example TZ=JST-9 in Japan.

NOTE: This table is from:
https://science.ksc.nasa.gov/software/winvn/userguide/3_1_4.htm

------- Greenwich Mean Time ---------------------------------------------------
+00:00  GMT Standard Time                   GMT     BST     GB London
+00:00  GMT Standard Time                   GMT     IST     IE Dublin
+00:00  GMT Standard Time                   WET     WES     PT Lisbon
+00:00  Greenwich Standard Time             GMT     GST     IS Reykjavik
------- Fernando De Noronha Std -----------------------------------------------
-02:00  Mid-Atlantic Standard Time          FST     FDT     BR Noronha
-02:00  UTC-02                              FST     FDT     BR Noronha
------- Brazil Standard Time --------------------------------------------------
-03:00  Bahia Standard Time                 BST     BDT     BR Bahia
-03:00  SA Eastern Standard Time            BST     BDT     BR Fortaleza
-03:00  Tocantins Standard Time             BST     BDT     BR Palmas
------- Eastern Standard (Brazil) ---------------------------------------------
-03:00  E. South America Standard Time      EST     EDT     BR Sao Paulo
------- Greenland Standard Time -----------------------------------------------
-03:00  Greenland Standard Time             GST     GDT     GL Godthab
------- Newfoundland Standard Time --------------------------------------------
-03:30  Newfoundland Standard Time          NST     NDT     CA St.Johns
------- Atlantic Standard Time ------------------------------------------------
-04:00  Atlantic Standard Time              AST     ADT     CA Halifax
------- Western Standard (Brazil) ---------------------------------------------
-04:00  Central Brazilian Standard Time     WST     WDT     BR Cuiaba
-04:00  SA Western Standard Time            WST     WDT     BR Manaus
------- Chile Standard Time ---------------------------------------------------
-04:00  Pacific SA Standard Time            CST     CDT     CL Santiago
------- Eastern Standard Time -------------------------------------------------
-05:00  Eastern Standard Time               EST     EDT     US New York
-05:00  Eastern Standard Time (Mexico)      EST     EDT     MX Cancun
-05:00  US Eastern Standard Time            EST     EDT     US Indianapolis
------- Acre Standard Time ----------------------------------------------------
-05:00  SA Pacific Standard Time            AST     ADT     BR Rio Branco
------- Cuba Standard Time ----------------------------------------------------
-05:00  Cuba Standard Time                  CST     CDT     CU Havana
------- Central Standard Time -------------------------------------------------
-06:00  Canada Central Standard Time        CST     CDT     CA Regina
-06:00  Central Standard Time               CST     CDT     US Chicago
-06:00  Central Standard Time (Mexico)      CST     CDT     MX Mexico City
------- Easter Island Standard ------------------------------------------------
-06:00  Easter Island Standard Time         EST     EDT     CL Easter
------- Mountain Standard Time ------------------------------------------------
-07:00  Mountain Standard Time              MST     MDT     US Denver
-07:00  Mountain Standard Time (Mexico)     MST     MDT     MX Chihuahua
-07:00  US Mountain Standard Time           MST     MDT     US Phoenix
------- Pacific Standard Time -------------------------------------------------
-08:00  Pacific Standard Time               PST     PDT     US Los Angeles
-08:00  Pacific Standard Time (Mexico)      PST     PDT     MX Tijuana
------- Alaska Standard Time --------------------------------------------------
-09:00  Alaskan Standard Time               AKS     AKD     US Anchorage
------- Hawaii Standard Time --------------------------------------------------
-10:00  Aleutian Standard Time              HST     HDT     US Aleutian
-10:00  Hawaiian Standard Time              HST     HDT     US Honolulu
------- Samoa Standard Time ---------------------------------------------------
+13:00  Samoa Standard Time                 SST     SDT     US Samoa
------- New Zealand Standard Time ---------------------------------------------
+12:00  New Zealand Standard Time           NZS     NZD     NZ Auckland
------- Guam Standard Time ----------------------------------------------------
+10:00  West Pacific Standard Time          GST     GDT     GU Guam
------- Eastern Australian Standard -------------------------------------------
+10:00  AUS Eastern Standard Time           EAS     EAD     AU Sydney
+10:00  E. Australia Standard Time          EAS     EAD     AU Brisbane
+10:00  Tasmania Standard Time              EAS     EAD     AU Hobart
------- Central Australian Standard -------------------------------------------
+09:30  AUS Central Standard Time           CAS     CAD     AU Darwin
+09:30  Cen. Australia Standard Time        CAS     CAD     AU Adelaide
------- Japan Standard Time ---------------------------------------------------
+09:00  Tokyo Standard Time                 JST     JDT     JP Tokyo
------- Korean Standard Time --------------------------------------------------
+09:00  Korea Standard Time                 KST     KDT     KR Seoul
+09:00  North Korea Standard Time           KST     KDT     KP Pyongyang
------- China Coast Time ------------------------------------------------------
+08:00  China Standard Time                 CCT     CDT     CN Shanghai
+08:00  Taipei Standard Time                CCT     CDT     TW Taipei
------- Hong Kong Time --------------------------------------------------------
+08:00  China Standard Time                 HKT     HKS     HK Hong Kong
------- Singapore Standard Time -----------------------------------------------
+08:00  Singapore Standard Time             SST     SDT     SG Singapore
------- Western Australian Standard -------------------------------------------
+08:45  Aus Central W. Standard Time        WAS     WAD     AU Eucla
+08:00  W. Australia Standard Time          WAS     WAD     AU Perth
------- North Sumatra Time ----------------------------------------------------
+07:00  SE Asia Standard Time               NST     NDT     ID Jakarta
------- Indian Standard Time --------------------------------------------------
+05:30  India Standard Time                 IST     IDT     IN Calcutta
------- Iran Standard Time ----------------------------------------------------
+03:30  Iran Standard Time                  IST     IDT     IR Tehran
------- Moscow Winter Time ----------------------------------------------------
+03:00  Belarus Standard Time               MSK     MSD     BY Minsk
+03:00  Russian Standard Time               MSK     MSD     RU Moscow
------- Eastern Europe Time ---------------------------------------------------
+02:00  E. Europe Standard Time             EET     EES     MD Chisinau
+02:00  FLE Standard Time                   EET     EES     UA Kiev
+02:00  GTB Standard Time                   EET     EES     RO Bucharest
+02:00  Kaliningrad Standard Time           EET     EES     RU Kaliningrad
------- Israel Standard Time --------------------------------------------------
+02:00  Israel Standard Time                IST     IDT     IL Jerusalem
------- Central European Time -------------------------------------------------
+01:00  Central Europe Standard Time        CET     CES     HU Budapest
+01:00  Central European Standard Time      CET     CES     PL Warsaw
+01:00  Romance Standard Time               CET     CES     FR Paris
+01:00  W. Europe Standard Time             CET     CES     DE Berlin
------- West African Time -----------------------------------------------------
+01:00  Namibia Standard Time               WAT     WAS     NA Windhoek
+01:00  W. Central Africa Standard Time     WAT     WAS     NG Lagos

NOTE: MSDOS_(APPEND/COMSPEC/LASTDRIVE/TEMP/TZ) are not copied to the virtual
table, but MSDOS_PATH is copied to, because some softwares may refer it.


----- Recommended Configurations

Create C:\DOS and C:\TEMP folders.
Copy COMMAND.COM (I recommend the Windows 98's COMMAND.COM Version 7.10) and
other DOS command files (ex. MORE.COM) into C:\DOS folder.

Set the environment variables listed below on the host Windows:

MSDOS_COMSPEC = C:\DOS\COMMAND.COM
MSDOS_PATH = C:\DOS
MSDOS_TEMP = C:\TEMP


----- Convert Command File To 32bit Or 64bit Execution File

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

This archive contains 12 executable binaries:

	i86_x86 	Emulates 8086 and supports both 32bit/64bit Windows
	i86_x64 	Emulates 8086 and supports only 64bit Windows
	i286_x86	Emulates 80286 and supports both 32bit/64bit Windows
	i286_x64	Emulates 80286 and supports only 64bit Windows
	i386_x86	Emulates 80386 and supports both 32bit/64bit Windows
	i386_x64	Emulates 80386 and supports only 64bit Windows
	i486_x86	Emulates 80486 and supports both 32bit/64bit Windows
	i486_x64	Emulates 80486 and supports only 64bit Window
	v30_x86 	Emulates NEC V30 and supports both 32bit/64bit Windows
	v30_x64 	Emulates NEC V30 and supports only 64bit Windows
	ia32_x86	Emulates IA32 and supports both 32bit/64bit Windows
	ia32_x64	Emulates IA32 and supports only 64bit Windows

8086 binaries are much faster than 80286/80386/80486/IA32 binaries.
If you don't need the protected mode or new mnemonics of 80286/80386/80486,
I recommend i86_x86 or i86_x64 binary.

NEC V30 binaries support 8080 emulation mode.
If you try CP/M-80 emulator using 8080 emulation mode, use these binaries.

IA32 binaries include Neko Project 21/W i386c core and support FPU/MMX/SSE/2/3.
These binaries will emulate the protected mode more correctly.

The VC++ project file "msdos.vcproj/vcxproj" also contains the configurations
for 80186, Pentium/PRO/MMX/2/3/4, and Cyrix MediaGX.
You can build all binaries for several cpu models by running build9_all.bat
or build12_all.bat.
(You need VC++ 2008 with Service Pack 1 or VC++ 2013 with Update 5.)


----- Internal Debugger

MS-DOS Player contains the internal debugger for developers, but it is
disabled on the binaries in the archive.
To enable the internal debugger, please remove the comment out of definition
"//#define USE_DEBUGGER" in common.h and build the binary.

MS-DOS Player opens the telnet port.
The port number is 23 in default, but if it is already used, 9000 or later.
When you start your telnet client and connect to MS-DOS Player,
the virtual cpu is suspended and you can start the debugger.

If you see the messsage "waiting until cpu is suspended...",
the internal DOS service is running, for example waiting your key inputs.
The virtual cpu will be suspended when exit the internal DOS service,
and you can start the debugger.

If Tera Term PRO, PuTTY, or Windows telnet client (telnet.exe) is installed,
it is automatically started and connected to MS-DOS Player.
When you use Tera Term PRO or PuTTY, I recommend to disable "Line at Time"
and enable "Character at Time".

NOTE: When you run 32bit version of MS-DOS Player on Windows x64,
MS-DOS Player will try to start 32bit version of telnet.exe in SysWOW64,
but it is not usually installed.

Please enter ? command to show the debugger commands.
They are very similar to DEBUG command, and some break point functions
(break at memory access (*), I/O port access, and interrupt) are added.

(*) break does not occur when DOS service/BIOS emulation codes access.


----- Supported Hardwares

This emulator provides a very simple IBM PC-like hardware emulation:

CPU 8086/80286/80386/80486, RAM 1MB/16MB/32MB, LIM EMS 32MB (Hardware EMS),
PC BIOS, DMA Controller (dummy), Interrupt Controller, System Timer,
Parallel I/O (LPT1-3), Serial I/O (COM1-4), Real Time Clock + CMOS Memory,
VGA Status Register, Keyboard Controller (A20 Line Mask, CPU Reset),
and 2-Button Mouse

NOTE:
- Graphic/Sound hardwares are NOT implemented.
- DMA Controller is implemented, but FDC and HDC are not connected.
- Parallel I/O is implemented and the output data is written to the file
  "Year-Month-Day_Hour-Minute-Second.PRN" created in the TEMP directory.
- Serial I/O is implemented and can be connected to the host's COM ports.


----- Memory Map

000000H -	Conventional Memory (736KB)
0B8000H -	VGA Text Video RAM (32KB)
----------
0C0000H -	Upper Memory Block (224KB)
--- Or ---
0C0000H -	EMS Page Frame (64K)
0D0000H -	Upper Memory Block (160KB)
----------
0F8000H -	V-TEXT Shadow Buffer (32KB-64B)
0FFFC0H -	Dummy BIOS/DOS/Driver Service Routines
0FFFF0H -	CPU Boot Address
100000H -	Upper Memory (15MB/31MB)


----- Supported System Calls

INT 08H		PC BIOS - System Timer

INT 10H		PC BIOS - Video

	00H	Set Video Mode
	01H	Set Text-Mode Cursor Shape
	02H	Set Cursor Position
	03H	Get Cursor Position And Size
	05H	Select Active Display Page
	06H	Scroll Up Window
	07H	Scroll Down Window
	08H	Read Character And Attribute At Cursor Position
	09H	Write Character And Attribute At Cursor Position
	0AH	Write Character Only At Cursor Position
	0CH	Write Graphics Pixel
	0DH	Read Graphics Pixel
	0EH	Teletype Output
	0FH	Get Current Video Mode
	1100H	Load User-Specified Patterns
	1101H	Load ROM 8x14 Monochrome Patterns
	1102H	Load ROM 8x8 Dbl-Dot Patterns
	1104H	Load ROM 8x16 Character Set
	1110H	Load User-Specified Patterns
	1111H	Load ROM 8x14 Monochrome Patterns
	1112H	Load ROM 8x8 Dbl-Dot Patterns
	1114H	Load ROM 8x16 Character Set
	1118H	Set V-TEXT Vertically Long Mode
	1130H	Get Font Information
	12H	Alternate Function Select (BL=10H)
	130*H	Write String
	1310H	Read Characters And Standard Attributes
	1311H	Read Characters And Extended Attributes
	1312H	Write Characters And Standard Attributes (???)
	1313H	Write Characters And Extended Attributes (???)
	1320H	Write Characters And Standard Attributes
	1321H	Write Characters And Extended Attributes
	1A00H	Get Display Combination Code
	1BH	Perform Gray-Scale Summing
	8200H	Get/Set Scroll Mode
	8300H	Get Video RAM Address
	90H	Get Physical Workstation Display Mode
	91H	Get Physical Workstation Adapter Type
	EFH	Get Video Adapter Type And Mode (*1)
	FEH	Get Shadow Buffer
	FFH	Update Screen From Shadow Buffer

INT 11H		PC BIOS - Get Equipment List

INT 12H		PC BIOS - Get Memory Size

INT 13H		PC BIOS - Disk I/O

	00H	Reset Disk System
	01H	Get Status Of Last Operation
	02H	Read Sectors Into Memory
	03H	Write Disk Sectors (*2)
	04H	Verify Disk Sectors
	08H	Get Drive Parameters
	0AH	Read Long Sectors
	0BH	Write Long Sectors
	0DH	Reset Hard Disks
	10H	Check If Drive Ready
	15H	Get Disk Type

INT 14H		PC BIOS - Serial I/O

	00H	Initialize Port
	01H	Write Character To Port
	02H	Read Character From Port
	03H	Get Port Status
	04H	Extended Initialize
	0500H	Read Modem Control Register
	0501H	Write Modem Control Register

INT 15H		PC BIOS

	2300H	Get CMOS Data
	2301H	Set CMOS Data
	2400H	Disable A20 Gate
	2401H	Enable A20 Gate
	2402H	Get A20 Gate Status
	2403H	A20 Support
	49H	Get BIOS Type
	5000H	Get Address Of "Read Font" Function
	5001H	Get Address Of "Write Font" Function
	84H	Joystick Support (*3)
	86H	Wait
	87H	Copy Extended Memory
	88H	Get Extended Memory Size
	89H	Switch To Protected Mode
	8AH	Get Big Memory Size
	C0H	Get Configuration
	C200H	Pointing Device BIOS Interface (PS) - Enable/Disable
	C201H	Pointing Device BIOS Interface (PS) - Reset
	C202H	Pointing Device BIOS Interface (PS) - Set Sampling Rate
	C203H	Pointing Device BIOS Interface (PS) - Set Resolution
	C204H	Pointing Device BIOS Interface (PS) - Get Type
	C205H	Pointing Device BIOS Interface (PS) - Initialize
	C206H	Pointing Device BIOS Interface (PS) - Extended Commands
	C207H	Pointing Device BIOS Interface (PS) - Set Device Handler Addr
	C208H	Pointing Device BIOS Interface (PS) - Write To Pointer Port
	C209H	Pointing Device BIOS Interface (PS) - Read From Pointer Port
	C9H	Get CPU Type And Mask Revision
	CA00H	Read CMOS Memory
	CA01H	Write CMOS Memory
	E801H	Get Memory Size For >64M Configurations
	E820H	Get System Memory MAP
	E881H	Get Memory Size For >64M Configurations

INT 16H		PC BIOS - Keyboard

	00H	Get Keystroke
	01H	Check For Keystroke
	02H	Get Shift Flags
	05H	Store Keystroke In Keyboard Buffer
	09H	Get Keyboard Functionality
	0AH	Get Keyboard ID
	10H	Get Enhanced Keystroke
	11H	Check For Enhanced Keystroke
	12H	Get Extended Shift States
	51H	AX (Japanese AT) Keyboard - Read Shift Key Status

INT 17H		PC BIOS - Printer

	00H	Write Character
	01H	Initialize Port
	02H	Get Status
	03H	AX (Japanese AT) Printer - Output String Without Conversion
	5000H	AX (Japanese AT) Printer - Set Printer Country Code
	5001H	AX (Japanese AT) Printer - Get Printer Country Code
	51H	AX (Japanese AT) Printer - JIS To Shift-JIS Conversion
	52H	AX (Japanese AT) Printer - Shift-JIS To JIS Conversion
	84H	AX (Japanese AT) Printer - Output Character Without Conversion
	85H	AX (Japanese AT) Printer - Enable/Disable Character Conversion

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
	08H	Console Input Without Echo
	09H	Print String
	0AH	Buffered Keyboard Input
	0BH	Check Console Status
	0CH	Character Input With Buffer Flush
	0DH	Disk Reset
	0EH	Select Disk
	0FH	Open File With FCB
	10H	Close File With FCB
	11H	Search First Entry With FCB
	12H	Search Next Entry With FCB
	13H	Delete File With FCB
	14H	Sequential Read With FCB
	15H	Sequential Write With FCB
	16H	Create New File With FCB
	17H	Rename File With FCB
	18H	Null Function For CP/M Compatibility
	19H	Current Disk
	1AH	Set Disk Transfer Address
	1BH	Get Allocation Information For Default Drive
	1CH	Get Allocation Information For Specified Drive
	1DH	Null Function For CP/M Compatibility
	1EH	Null Function For CP/M Compatibility
	1FH	Get Drive Parameter Block For Default Drive
	20H	Null Function For CP/M Compatibility
	21H	Random Read With FCB
	22H	Randome Write With FCB
	23H	Get File Size With FCB
	24H	Set Relative Record Field With FCB
	25H	Set Vector
	26H	Create New Program Segment Prefix
	27H	Random Block Read With FCB
	28H	Random Block Write With FCB
	29H	Parse File Name
	2AH	Get Date
	2BH	Set Date
	2CH	Get Time
	2DH	Set Time
	2EH	Set/Reset Verify Switch
	2FH	Get Disk Transfer Address
	30H	Get Version Number (*4)
	31H	Keep Process
	32H	Get DOS Drive Parameter Block
	3300H	Get Ctrl-Break Checking State
	3301H	Set Ctrl-Break Checking State
	3302H	Get/Set Ctrl-Break Checking State
	3305H	Get Boot Drive
	3306H	Get True Version Number (*5)
	3307H	Windows95 - Set/Clear DOS_FLAG
	34H	Get Address Of InDOS Flag
	35H	Get Vector
	36H	Get Disk Free Space
	3700H	Get Switch Character
	3701H	Set Switch Character
	3702H	Get AvailDev Flag
	3703H	Set AvailDev Flag
	38H	Get Country Specific Information
	39H	Create Subdirectory
	3AH	Remove Subdirectory
	3BH	Change Current Directory
	3CH	Create File
	3DH	Open File Handle
	3EH	Close File Handle
	3FH	Read From File Or Device
	40H	Write To File Or Device
	41H	Erase File From Directory
	42H	Move File Read/Write Pointer
	4300H	Get File Attribute
	4301H	Set File Attribute
	4302H	MS-DOS 7 - Get Compressed File Size
	43FFH	Windows98 - Extended-Length File Name Operations
	4400H	Get Device Information
	4401H	Set Device Information
	4402H	Read From Character Device Control Channel
	4406H	Get Input Status
	4407H	Get Output Status
	4408H	Device Removable Query
	4409H	Device Local Or Remote Query
	440AH	Handle Local Or Remote Query
	440CH	Generic Character Device Request
	440DH	Generic Block Device Request
	4410H	Query Generic IOCTRL Capability (Handle)
	4411H	Query Generic IOCTRL Capability (Drive)
	45H	Duplicate File Handle
	46H	Force Duplicate Of Handle
	47H	Get Current Directory
	48H	Allocate Memory
	49H	Free Allocated Memory
	4AH	Modify Allocated Memory Blocks
	4B00H	Load And Execute Program
	4B01H	Load Program
	4B03H	Load Overlay
	4CH	Terminate Process
	4DH	Get Subprocess Return Code
	4EH	Find First Matching File
	4FH	Find Next Matching File
	50H	Set Program Segment Prefix Address
	51H	Get Program Segment Prefix Address
	52H	Get DOS Info Table
	53H	Translate BIOS Parameter Block To Drive Param Bock
	54H	Get Verify State
	55H	Create Child Program Segment Prefix
	56H	Rename File
	5700H	Get Last Written Date And Time
	5701H	Set Last Written Date And Time
	5704H	Windows95 - Get Last Access Date And Time
	5705H	Windows95 - Set Last Access Date And Time
	5706H	Windows95 - Get Creation Date And Time
	5707H	Windows95 - Set Creation Date And Time
	5800H	Get Memory Allocation Strategy
	5801H	Set Memory Allocation Strategy
	5802H	Get UMB Link State
	5803H	Set UMB Link State
	59H	Get Extended Error Information
	5AH	Create Unique File
	5BH	Create New File
	5CH	Lock/Unlock File Access
	5D00H	Server Function Call
	5D06H	Get Address Of DOS Swappable Data Area
	5D0AH	Set Extended Error Information
	5E00H	Get Machine Name
	5F02H	Get Redirection List Entry
	5F05H	Get Extended Redirection List Entry
	5F07H	Enable Drive
	5F08H	Disable Drive
	60H	Canonicalize Filename Or Path
	61H	Reserved Fnction
	62H	Get Program Segment Prefix Address
	6300H	Get DBCS Vector
	6500H	Windows95 OSR2 - Set General Internationalization Info
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
	6602H	Set Global Code Page Table
	67H	Set Handle Count
	68H	Commit File
	6900H	Get Disk Serial Number
	6AH	Commit File
	6BH	Null Function
	6CH	Extended Open/Create
	7002H	Windows95 - Set General Internationalization Info
	710DH	Windows95 - Reset Drive
	7139H	Windows95 - LFN - Create Subdirectory
	713AH	Windows95 - LFN - Remove Subdirectory
	713BH	Windows95 - LFN - Change Current Directory
	7141H	Windows95 - LFN - Erase File From Directory
	7143H	Windows95 - LFN - Get/Set File Attribute
	7147H	Windows95 - LFN - Get Current Directory
	714EH	Windows95 - LFN - Find First Matching File
	714FH	Windows95 - LFN - Find Next Matching File
	7156H	Windows95 - LFN - Rename File
	7160H	Windows95 - LFN - Canonicalize Filename Or Path
	716CH	Windows95 - LFN - Extended Open/Create
	71A0H	Windows95 - LFN - Get Volume Information
	71A1H	Windows95 - LFN - Terminate Directory Search
	71A6H	Windows95 - LFN - Get File Information By Handle
	71A7H	Windows95 - LFN - Convert File Time/DOS Time
	71A8H	Windows95 - LFN - Generate Short File Name
	71A9H	Windows95 - LFN - Extended Server Open/Create
	71AAH	Windows95 - LFN - Create/Terminate/Query SUBST
	7300H	Windows95 - FAT32 - Get Drive Locking
	7302H	Windows95 - FAT32 - Get Extended DPB
	7303H	Windows95 - FAT32 - Get Extended Free Space On Drive
	DBH	Novell NetWare - Workstation - Get Number Of Local Drives
	DCH	Novell NetWare - Connection Services - Get Connection Number

INT 23H		Ctrl-Break Address

INT 24H		Critical Error Handler

INT 25H		Absolute Disk Read

INT 26H		Absolute Disk Write (*2)

INT 27H		Terminate And Stay Resident

INT 28H		DOS Idle

INT 29H		DOS Fast Character I/O

INT 2EH		Pass Command To Command Interpreter For Execution

INT 2FH		Multiplex Interrupt

	0500H	DOS 3.0+ Critical Error Handler - Installation Check
	0502H	DOS 3.0+ Critical Error Handler - Expand Error Into String
	1200H	DOS 3.0+ internal - Installation Check
	1202H	DOS 3.0+ internal - Get Interrupt Address
	1203H	DOS 3.0+ Internal - Get DOS Data Segment
	1204H	DOS 3.0+ internal - Normalize Path Separator
	1205H	DOS 3.0+ internal - Output Character To Standard Output
	120DH	DOS 3.0+ internal - Get Date And Time
	1211H	DOS 3.0+ internal - Normalize ASCIZ Filename
	1212H	DOS 3.0+ internal - Get Length Of ASCIZ String
	1213H	DOS 3.0+ internal - Uppercase Character
	1214H	DOS 3.0+ internal - Compare Far Pointers
	1216H	DOS 3.0+ internal - Get System File Table Entry
	1217H	DOS 3.0+ internal - Get Current Directory Structure For Drive
	121AH	DOS 3.0+ internal - Get File's Drive
	121BH	DOS 3.0+ internal - Set Year/Length Of February
	121EH	DOS 3.0+ internal - Compare Filenames
	121FH	DOS 3.0+ internal - Build Current Directory Structure
	1220H	DOS 3.0+ internal - Get Job File Table Entry
	1221H	DOS 3.0+ internal - Canonicalize File Name
	1222H	DOS 3.0+ internal - Set Extended Error Info
	1225H	DOS 3.0+ internal - Get Length Of ASCIZ String
	1226H	DOS 3.3+ internal - Open File
	1227H	DOS 3.3+ internal - Close File
	1228H	DOS 3.3+ internal - Move File Pointer
	1229H	DOS 3.3+ internal - Read From File
	122BH	DOS 3.3+ internal - IOCTL
	122CH	DOS 3.3+ internal - Get Device Chain
	122DH	DOS 3.3+ internal - Get Extended Error Code
	122EH	DOS 4.0+ internal - Get Error Table Addresses
	122FH	DOS 4.0+ internal - Set DOS Version Number To Return
	1400H	NLSFUNC.COM - Installation Check
	1401H	NLSFUNC.COM - Change Code Page
	1402H	NLSFUNC.COM - Get Extended Country Info
	1403H	NLSFUNC.COM - Set Code Page
	1404H	NLSFUNC.COM - Get Country Info
	150BH	CD-ROM v2.00+ - Drive Check
	150DH	CD-ROM v2.00+ - Get CD-ROM Drive Letters
	1600H	Windows - Windows Enhanced Mode Installation Check (*6)
	1605H	Windows - Windows Enhanced Mode & 286 DOSX Init Broadcast
	160AH	Windows - Identify Windows Version And Type (*6)
	1680H	Windows, DPMI - Release Current Virtual Machine Time-Slice
	1683H	Windows 3+ - Get Current Virtual Machine ID
	1A00H	ANSI.SYS - Installation Check
	1A01H	ANSI.SYS - Set/Get Display Information
	4000H	Windows 3+ - Get Virtual Device Driver (VDD) Capabilities
	4300H	XMS - Installation Check
	4308H	HIMEM.SYS - Get A20 Handler Number
	4310H	XMS - Get Driver Address
	4810H	Read Input Line From Console
	4A01H	DOS 5.0+ - Query Free HMA Space
	4A02H	DOS 5.0+ - Allocate HMA Space
	4A03H	Windows95 - (De)Allocate HMA Memory Block
	4A04H	Windows95 - Get Start Of HMA Memory Chain
	AD00H	DISPLAY.SYS - Installation Check
	AD01H	DISPLAY.SYS - Set Active Code Page
	AD02H	DISPLAY.SYS - Get Active Code Page
	AD03H	DISPLAY.SYS - Get Code Page Information
	4F00H	BILING - Get Version
	4F01H	BILING - Get Code Page
	AE00H	Installable Command - Installation Check
	AE01H	Installable Command - Execute

INT 33H		Mouse

	0000H	Reset Driver And Rest Status
	0001H	Show Mouse Cursor
	0002H	Hide Mouse Cursor
	0003H	Return Position And Button Status
	0004H	Position Mouse Cursor
	0005H	Return Button Press Data
	0006H	Return Button Release Data
	0007H	Define Horizontal Cursor Range
	0008H	Define Vertical Cursor Range
	0009H	Define Graphics Cursor
	000AH	Define Text Cursor
	000BH	Read Motion Counters
	000CH	Define Interrupt Sub Routine Parameters
	000FH	Define Mickey/Pixel Ratio
	0011H	Get Number Of Buttons
	0014H	Exchange Interrupt Sub Routines
	0015H	Return Driver Storage Requirements
	0016H	Save Driver State
	0017H	Restore Driver State
	0018H	Set Alternate Mouse User Handler
	0019H	Return User Alternate Interrupt Vector
	001AH	Set Mouse Sensitivity
	001BH	Return Mouse Sensitivity
	001DH	Define Display Page Number
	001EH	Return Display Page Number
	001FH	Disable Mouse Driver
	0020H	Enable Mouse Driver
	0021H	Software Reset
	0022H	Set Language For Messages
	0023H	Get Language For Messages
	0024H	Get Software Version, Moouse Type, And IRQ Number (*7)
	0025H	Get General Driver Information
	0026H	Get Maximum Virtual Coordinates
	0027H	Get Screen/Cursor Masks And Mickey Counts
	0028H	Set Video Mode
	0029H	Enumerate Video Modes
	002AH	Get Cursor Host Spot
	0031H	Get Current Minimum/Maximum Virtual Coordinates
	0032H	Get Active Advanced Functions
	004DH	Return Pointer To Copyright String
	006DH	Get Version String

INT 65H-66H	(Used For Dummy LIM EMS/XMS Service Routines)

INT 67H		LIM EMS

	40H	LIM EMS - Get Manager Status
	41H	LIM EMS - Get Page Frame Segment
	42H	LIM EMS - Get Number Of Pages
	43H	LIM EMS - Get Handle And Allocate Memory
	44H	LIM EMS - Map/Unmap Memory
	45H	LIM EMS - Release Handle And Memory
	46H	LIM EMS - Get EMM Version (*8)
	47H	LIM EMS - Save Mapping Context
	48H	LIM EMS - Restore Mapping Context
	4BH	LIM EMS - Get Number Of EMM Handles
	4CH	LIM EMS - Get Pages Owned By Handle
	4DH	LIM EMS - Get Pages For All Handles
	4E00H	LIM EMS - Get Page Map
	4E01H	LIM EMS - Set Page Map
	4E02H	LIM EMS - Get And Set Page Map
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
	5401H	LIM EMS 4.0 - Search For Named Handle
	5402H	LIM EMS 4.0 - Get Total Handles
	55H	LIM EMS 4.0 - Alter Page Map And Jump
	56H	LIM EMS 4.0 - Alter Page Map And Call
	5602H	LIM EMS 4.0 - Get Page Map Stack Space Required
	5700H	LIM EMS 4.0 - Move Memory Region
	5701H	LIM EMS 4.0 - Exchange Memory Region
	5800H	LIM EMS 4.0 - Get Mappable Physical Address Array
	5801H	LIM EMS 4.0 - Get Mappable Physical Address Array Entries
	5900H	LIM EMS 4.0 - Get Hardware Configuration Array
	5901H	LIM EMS 4.0 - Get Unallocated Raw Page Count
	5A00H	LIM EMS 4.0 - Allocate Standard Pages
	5A01H	LIM EMS 4.0 - Allocate Raw Pages
	5B00H	LIM EMS 4.0 - Get Alternate Map Register Set
	5B01H	LIM EMS 4.0 - Set Alternate Map Register Set
	5B02H	LIM EMS 4.0 - Get Alternate Map Save Array Size
	7000H	EMS - Get Page Frame Status (NEC PC-9801 Only ???)
	7001H	EMS - Enable/Disable Page Frame (NEC PC-9801 Only ???)
	DE00H	VCPI - Installation Check
	DE01H	VCPI - Get Protected Mode Interface
	DE02H	VCPI - Get Max Physical Memory Address
	DE03H	VCPI - Get Number Of Free 4KB Pages
	DE04H	VCPI - Allocate 4KB Page
	DE05H	VCPI - Free 4KB Page
	DE06H	VCPI - Get Physical Address Of Page In First MB
	DE07H	VCPI - Read CR0
	DE08H	VCPI - Read Debug Registers
	DE09H	VCPI - Write Debug Registers
	DE0AH	VCPI - Get 8259 Interrupt Vector Mapping
	DE0BH	VCPI - Set 8259 Interrupt Vector Mapping
	DE0CH	VCPI - Switch To Protected Mode


INT 68H-6FH	(Used For Dummy BIOS/DOS/Driver Service Routines)

CALL FAR XMS

	00H	XMS - Get XMS Version Number (*9)
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
(*2) Support only floppy disk drive
(*3) Joysticks are not connected
(*4) MS-DOS Version: 7.10 (default) or specified version with -v option
(*5) MS-DOS Version: 7.10, -v option is not affected
(*6) Windows Version: 4.10 (default) or specified version with -w option
(*7) Mouse Version: 8.05
(*8) EMS Version: 4.0
(*9) XMS Version: 3.95


----- INT 29H Limitations

When INT 21H/2FH service outputs a character to console, INT 29H service
(DOS Fast Character I/O) is called.

If INT 21H/2FH service that outputs a character to console is called again
in INT 29H handler, INT 29H is not called again to prevent an infinite loop.

In usual, the virtual CPU is running while waiting inputs from console in
INT 21H, AH=01H/0AH/3FH service and a timer interrupt can be raised.
If INT 29H handler is hooked by your software, the virtual CPU is suspended
while waiting inputs from console, and a timer interrupt never be raised.


----- Win32/64 Child Process

When we run a Win32/64 program on usual MS-DOS environment, we will see
an error message "This program cannot be run in DOS mode."

When a Win32/64 program file is called by INT 21H, AX=4B00H, INT 2EH, or
INT 2FH, AX=AE01H services, for example the case we run it on COMMAND.COM,
it is executed as a Win32/64 child process by Win32 system() API and
waits until the child process is terminated.
In this time, any characters output to the console by this Win32/64 child
process is not hooked by INT 29H service.

When you run a Win32/64 program with pipe, it may not work as you invite.
For example, you may run Win32/64 version of more.com with pipe as follows:

> msdos command.com
> type foo.txt | more (more.com is Win32/64 program in C:\Windows\System32)

MS-DOS Player starts more.com as a Win32/64 child process and will wait until
the child process is terminated.
While waiting until the child process is terminated, the virtual CPU is
suspended and outputs of "type foo.txt" never be sent to the child process.


--- License

The copyright belongs to the author, but you can use the source codes under
the GNU GENERAL PUBLIC LICENSE Version 2.

See also COPYING.txt for more details about the license.


----- Thanks

8086/80286 code is based on MAME 0.149 and applied fixes in MAME 0.150-0.185.
NEC V30 instructions code is based on MAME 0.128.
8038/80486 code is based on MAME 0.152 and applied fixes in MAME 0.154-0.185.
IA32 code is based on Neko Project 21/W.

INT 15H AH=84H (Joystick Support),
INT 15H AH=87H (Copy Extended Memory),
INT 15H AH=89H (Switch To Protected Mode),
INT 15H AX=C209H (Read From Pointer Port)
INT 21H AX=4402H (Read From Character Device Control Channel),
INT 33H AX=001FH (Disable Mouse Driver),
INT 33H AX=0020H (Enable Mouse Driver),
INT 67H AX=DE01H (Get Protected Mode Interface),
INT 67H AX=DE0CH (Switch To Protected Mode),
and some DOS info block improvements are based on DOSBox.

Imported many fixes from Mr.cracyc's fork hosted at:
https://github.com/cracyc/msdos-player

Patched by Mr.Sagawa, Mr.sava (lukewarm), Mr.Kimura (emk), Mr.Jason Hood,
Mr.N2583ZYOB6, and Mr.Fujita.

----------------------------------------
TAKEDA, toshiya
t-takeda@m1.interq.or.jp
http://takeda-toshiya.my.coocan.jp/
