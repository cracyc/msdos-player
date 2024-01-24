/*
   i386 Disassembler

   Written by Ville Linde
*/

#include "emu.h"

enum
{
	PARAM_REG = 1,		/* 16 or 32-bit register */
	PARAM_REG8,			/* 8-bit register */
	PARAM_REG16,		/* 16-bit register */
	PARAM_REG32,		/* 32-bit register */
	PARAM_REG3264,		/* 32-bit or 64-bit register */
	PARAM_REG2_32,		/* 32-bit register */
	PARAM_MMX,			/* MMX register */
	PARAM_XMM,			/* XMM register */
	PARAM_RM,			/* 16 or 32-bit memory or register */
	PARAM_RM8,			/* 8-bit memory or register */
	PARAM_RM16,			/* 16-bit memory or register */
	PARAM_RM32,			/* 32-bit memory or register */
	PARAM_RMPTR,		/* 16 or 32-bit memory or register */
	PARAM_RMPTR8,		/* 8-bit memory or register */
	PARAM_RMPTR16,		/* 16-bit memory or register */
	PARAM_RMPTR32,		/* 32-bit memory or register */
	PARAM_RMXMM,		/* 32 or 64-bit memory or register */
	PARAM_REGORXMM,		/* 32 or 64-bit register or XMM register */
	PARAM_M64,			/* 64-bit memory */
	PARAM_M64PTR,		/* 64-bit memory */
	PARAM_MMXM,			/* 64-bit memory or MMX register */
	PARAM_XMMM,			/* 128-bit memory or XMM register */
	PARAM_I4,			/* 4-bit signed immediate */
	PARAM_I8,			/* 8-bit signed immediate */
	PARAM_I16,			/* 16-bit signed immediate */
	PARAM_UI8,			/* 8-bit unsigned immediate */
	PARAM_UI16,			/* 16-bit unsigned immediate */
	PARAM_IMM,			/* 16 or 32-bit immediate */
	PARAM_IMM64,		/* 16, 32 or 64-bit immediate */
	PARAM_ADDR,			/* 16:16 or 16:32 address */
	PARAM_REL,			/* 16 or 32-bit PC-relative displacement */
	PARAM_REL8,			/* 8-bit PC-relative displacement */
	PARAM_MEM_OFFS,		/* 16 or 32-bit mem offset */
	PARAM_SREG,			/* segment register */
	PARAM_CREG,			/* control register */
	PARAM_DREG,			/* debug register */
	PARAM_TREG,			/* test register */
	PARAM_1,			/* used by shift/rotate instructions */
	PARAM_AL,
	PARAM_CL,
	PARAM_DL,
	PARAM_BL,
	PARAM_AH,
	PARAM_CH,
	PARAM_DH,
	PARAM_BH,
	PARAM_DX,
	PARAM_EAX,			/* EAX or AX */
	PARAM_ECX,			/* ECX or CX */
	PARAM_EDX,			/* EDX or DX */
	PARAM_EBX,			/* EBX or BX */
	PARAM_ESP,			/* ESP or SP */
	PARAM_EBP,			/* EBP or BP */
	PARAM_ESI,			/* ESI or SI */
	PARAM_EDI,			/* EDI or DI */
};

enum
{
	MODRM = 1,
	GROUP,
	FPU,
	OP_SIZE,
	ADDR_SIZE,
	TWO_BYTE,
	PREFIX,
	SEG_CS,
	SEG_DS,
	SEG_ES,
	SEG_FS,
	SEG_GS,
	SEG_SS,
	ISREX
};

#define FLAGS_MASK			0x0ff
#define VAR_NAME			0x100
#define VAR_NAME4			0x200
#define ALWAYS64			0x400
#define SPECIAL64			0x800
#define SPECIAL64_ENT(x)	(SPECIAL64 | ((x) << 24))

typedef struct {
	const char *mnemonic;
	UINT32 flags;
	UINT32 param1;
	UINT32 param2;
	UINT32 param3;
	offs_t dasm_flags;
} I386_OPCODE;

typedef struct {
	char mnemonic[32];
	const I386_OPCODE *opcode;
} GROUP_OP;

static const UINT8 *opcode_ptr;
static const UINT8 *opcode_ptr_base;

static const I386_OPCODE i386_opcode_table1[256] =
{
	// 0x00
	{"add",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"add",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"add",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"add",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"add",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"add",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push    es",		0,				0,					0,					0				},
	{"pop     es",		0,				0,					0,					0				},
	{"or",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"or",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"or",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"or",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"or",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"or",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push    cs",		0,				0,					0,					0				},
	{"two_byte",		TWO_BYTE,		0,					0,					0				},
	// 0x10
	{"adc",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"adc",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"adc",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"adc",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"adc",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"adc",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push    ss",		0,				0,					0,					0				},
	{"pop     ss",		0,				0,					0,					0				},
	{"sbb",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"sbb",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"sbb",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"sbb",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"sbb",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"sbb",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"push    ds",		0,				0,					0,					0				},
	{"pop     ds",		0,				0,					0,					0				},
	// 0x20
	{"and",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"and",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"and",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"and",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"and",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"and",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_es",			SEG_ES,			0,					0,					0				},
	{"daa",				0,				0,					0,					0				},
	{"sub",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"sub",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"sub",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"sub",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"sub",				0,				PARAM_AL,			PARAM_I8,			0				},
	{"sub",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_cs",			SEG_CS,			0,					0,					0				},
	{"das",				0,				0,					0,					0				},
	// 0x30
	{"xor",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"xor",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"xor",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"xor",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"xor",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"xor",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_ss",			SEG_SS,			0,					0,					0				},
	{"aaa",				0,				0,					0,					0				},
	{"cmp",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"cmp",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"cmp",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"cmp",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmp",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"cmp",				0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"seg_ds",			SEG_DS,			0,					0,					0				},
	{"aas",				0,				0,					0,					0				},
	// 0x40
	{"inc",				ISREX,			PARAM_EAX,			0,					0				},
	{"inc",				ISREX,			PARAM_ECX,			0,					0				},
	{"inc",				ISREX,			PARAM_EDX,			0,					0				},
	{"inc",				ISREX,			PARAM_EBX,			0,					0				},
	{"inc",				ISREX,			PARAM_ESP,			0,					0				},
	{"inc",				ISREX,			PARAM_EBP,			0,					0				},
	{"inc",				ISREX,			PARAM_ESI,			0,					0				},
	{"inc",				ISREX,			PARAM_EDI,			0,					0				},
	{"dec",				ISREX,			PARAM_EAX,			0,					0				},
	{"dec",				ISREX,			PARAM_ECX,			0,					0				},
	{"dec",				ISREX,			PARAM_EDX,			0,					0				},
	{"dec",				ISREX,			PARAM_EBX,			0,					0				},
	{"dec",				ISREX,			PARAM_ESP,			0,					0				},
	{"dec",				ISREX,			PARAM_EBP,			0,					0				},
	{"dec",				ISREX,			PARAM_ESI,			0,					0				},
	{"dec",				ISREX,			PARAM_EDI,			0,					0				},
	// 0x50
	{"push",			ALWAYS64,		PARAM_EAX,			0,					0				},
	{"push",			ALWAYS64,		PARAM_ECX,			0,					0				},
	{"push",			ALWAYS64,		PARAM_EDX,			0,					0				},
	{"push",			ALWAYS64,		PARAM_EBX,			0,					0				},
	{"push",			ALWAYS64,		PARAM_ESP,			0,					0				},
	{"push",			ALWAYS64,		PARAM_EBP,			0,					0				},
	{"push",			ALWAYS64,		PARAM_ESI,			0,					0				},
	{"push",			ALWAYS64,		PARAM_EDI,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_EAX,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_ECX,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_EDX,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_EBX,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_ESP,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_EBP,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_ESI,			0,					0				},
	{"pop",				ALWAYS64,		PARAM_EDI,			0,					0				},
	// 0x60
	{"pusha\0pushad\0<invalid>",VAR_NAME,0,					0,					0				},
	{"popa\0popad\0<invalid>",	VAR_NAME,0,					0,					0				},
	{"bound",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"arpl",			MODRM | SPECIAL64_ENT(0),PARAM_RM,	PARAM_REG16,		0				},
	{"seg_fs",			SEG_FS,			0,					0,					0				},
	{"seg_gs",			SEG_GS,			0,					0,					0				},
	{"op_size",			OP_SIZE,		0,					0,					0				},
	{"addr_size",		ADDR_SIZE,		0,					0,					0				},
	{"push",			0,				PARAM_IMM,			0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			PARAM_IMM		},
	{"push",			0,				PARAM_I8,			0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			PARAM_I8		},
	{"insb",			0,				0,					0,					0				},
	{"insw\0insd\0insd",VAR_NAME,		0,					0,					0				},
	{"outsb",			0,				0,					0,					0				},
	{"outsw\0outsd\0outsd",VAR_NAME,	0,					0,					0				},
	// 0x70
	{"jo",				0,				PARAM_REL8,			0,					0				},
	{"jno",				0,				PARAM_REL8,			0,					0				},
	{"jb",				0,				PARAM_REL8,			0,					0				},
	{"jae",				0,				PARAM_REL8,			0,					0				},
	{"je",				0,				PARAM_REL8,			0,					0				},
	{"jne",				0,				PARAM_REL8,			0,					0				},
	{"jbe",				0,				PARAM_REL8,			0,					0				},
	{"ja",				0,				PARAM_REL8,			0,					0				},
	{"js",				0,				PARAM_REL8,			0,					0				},
	{"jns",				0,				PARAM_REL8,			0,					0				},
	{"jp",				0,				PARAM_REL8,			0,					0				},
	{"jnp",				0,				PARAM_REL8,			0,					0				},
	{"jl",				0,				PARAM_REL8,			0,					0				},
	{"jge",				0,				PARAM_REL8,			0,					0				},
	{"jle",				0,				PARAM_REL8,			0,					0				},
	{"jg",				0,				PARAM_REL8,			0,					0				},
	// 0x80
	{"group80",			GROUP,			0,					0,					0				},
	{"group81",			GROUP,			0,					0,					0				},
	{"group80",			GROUP,			0,					0,					0				},
	{"group83",			GROUP,			0,					0,					0				},
	{"test",			MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"test",			MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"xchg",			MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"xchg",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_RM8,			PARAM_REG8,			0				},
	{"mov",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"mov",				MODRM,			PARAM_REG8,			PARAM_RM8,			0				},
	{"mov",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_RM,			PARAM_SREG,			0				},
	{"lea",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_SREG,			PARAM_RM,			0				},
	{"pop",				MODRM,			PARAM_RM,			0,					0				},
	// 0x90
	{"nop",				0,				0,					0,					0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_ECX,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EDX,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EBX,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_ESP,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EBP,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_ESI,			0				},
	{"xchg",			0,				PARAM_EAX,			PARAM_EDI,			0				},
	{"cbw\0cwde\0cdqe",	VAR_NAME,		0,					0,					0				},
	{"cwd\0cdq\0cqo",	VAR_NAME,		0,					0,					0				},
	{"call",			ALWAYS64,		PARAM_ADDR,			0,					0,				DASMFLAG_STEP_OVER},
	{"wait",			0,				0,					0,					0				},
	{"pushf\0pushfd\0pushfq",VAR_NAME,	0,					0,					0				},
	{"popf\0popfd\0popfq",VAR_NAME,		0,					0,					0				},
	{"sahf",			0,				0,					0,					0				},
	{"lahf",			0,				0,					0,					0				},
	// 0xa0
	{"mov",				0,				PARAM_AL,			PARAM_MEM_OFFS,		0				},
	{"mov",				0,				PARAM_EAX,			PARAM_MEM_OFFS,		0				},
	{"mov",				0,				PARAM_MEM_OFFS,		PARAM_AL,			0				},
	{"mov",				0,				PARAM_MEM_OFFS,		PARAM_EAX,			0				},
	{"movsb",			0,				0,					0,					0				},
	{"movsw\0movsd\0movsq",VAR_NAME,	0,					0,					0				},
	{"cmpsb",			0,				0,					0,					0				},
	{"cmpsw\0cmpsd\0cmpsq",VAR_NAME,	0,					0,					0				},
	{"test",			0,				PARAM_AL,			PARAM_UI8,			0				},
	{"test",			0,				PARAM_EAX,			PARAM_IMM,			0				},
	{"stosb",			0,				0,					0,					0				},
	{"stosw\0stosd\0stosq",VAR_NAME,	0,					0,					0				},
	{"lodsb",			0,				0,					0,					0				},
	{"lodsw\0lodsd\0lodsq",VAR_NAME,	0,					0,					0				},
	{"scasb",			0,				0,					0,					0				},
	{"scasw\0scasd\0scasq",VAR_NAME,	0,					0,					0				},
	// 0xb0
	{"mov",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_CL,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_DL,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_BL,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_AH,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_CH,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_DH,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_BH,			PARAM_UI8,			0				},
	{"mov",				0,				PARAM_EAX,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_ECX,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_EDX,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_EBX,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_ESP,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_EBP,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_ESI,			PARAM_IMM64,		0				},
	{"mov",				0,				PARAM_EDI,			PARAM_IMM64,		0				},
	// 0xc0
	{"groupC0",			GROUP,			0,					0,					0				},
	{"groupC1",			GROUP,			0,					0,					0				},
	{"ret",				0,				PARAM_I16,			0,					0,				DASMFLAG_STEP_OUT},
	{"ret",				0,				0,					0,					0,				DASMFLAG_STEP_OUT},
	{"les",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lds",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"mov",				MODRM,			PARAM_RMPTR8,		PARAM_UI8,			0				},
	{"mov",				MODRM,			PARAM_RMPTR,		PARAM_IMM,			0				},
	{"enter",			0,				PARAM_I16,			PARAM_UI8,			0				},
	{"leave",			0,				0,					0,					0				},
	{"retf",			0,				PARAM_I16,			0,					0,				DASMFLAG_STEP_OUT},
	{"retf",			0,				0,					0,					0,				DASMFLAG_STEP_OUT},
	{"int 3",			0,				0,					0,					0,				DASMFLAG_STEP_OVER},
	{"int",				0,				PARAM_UI8,			0,					0,				DASMFLAG_STEP_OVER},
	{"into",			0,				0,					0,					0				},
	{"iret",			0,				0,					0,					0,				DASMFLAG_STEP_OUT},
	// 0xd0
	{"groupD0",			GROUP,			0,					0,					0				},
	{"groupD1",			GROUP,			0,					0,					0				},
	{"groupD2",			GROUP,			0,					0,					0				},
	{"groupD3",			GROUP,			0,					0,					0				},
	{"aam",				0,				PARAM_I8,			0,					0				},
	{"aad",				0,				PARAM_I8,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"xlat",			0,				0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	{"escape",			FPU,			0,					0,					0				},
	// 0xe0
	{"loopne",			0,				PARAM_REL8,			0,					0,				DASMFLAG_STEP_OVER},
	{"loopz",			0,				PARAM_REL8,			0,					0,				DASMFLAG_STEP_OVER},
	{"loop",			0,				PARAM_REL8,			0,					0,				DASMFLAG_STEP_OVER},
	{"jcxz\0jecxz\0jrcxz",VAR_NAME,		PARAM_REL8,			0,					0				},
	{"in",				0,				PARAM_AL,			PARAM_UI8,			0				},
	{"in",				0,				PARAM_EAX,			PARAM_UI8,			0				},
	{"out",				0,				PARAM_UI8,			PARAM_AL,			0				},
	{"out",				0,				PARAM_UI8,			PARAM_EAX,			0				},
	{"call",			0,				PARAM_REL,			0,					0,				DASMFLAG_STEP_OVER},
	{"jmp",				0,				PARAM_REL,			0,					0				},
	{"jmp",				0,				PARAM_ADDR,			0,					0				},
	{"jmp",				0,				PARAM_REL8,			0,					0				},
	{"in",				0,				PARAM_AL,			PARAM_DX,			0				},
	{"in",				0,				PARAM_EAX,			PARAM_DX,			0				},
	{"out",				0,				PARAM_DX,			PARAM_AL,			0				},
	{"out",				0,				PARAM_DX,			PARAM_EAX,			0				},
	// 0xf0
	{"lock",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"repne",			PREFIX,			0,					0,					0				},
	{"rep",				PREFIX,			0,					0,					0				},
	{"hlt",				0,				0,					0,					0				},
	{"cmc",				0,				0,					0,					0				},
	{"groupF6",			GROUP,			0,					0,					0				},
	{"groupF7",			GROUP,			0,					0,					0				},
	{"clc",				0,				0,					0,					0				},
	{"stc",				0,				0,					0,					0				},
	{"cli",				0,				0,					0,					0				},
	{"sti",				0,				0,					0,					0				},
	{"cld",				0,				0,					0,					0				},
	{"std",				0,				0,					0,					0				},
	{"groupFE",			GROUP,			0,					0,					0				},
	{"groupFF",			GROUP,			0,					0,					0				}
};

static const I386_OPCODE x64_opcode_alt[] =
{
	{"movsxd",			MODRM | ALWAYS64,PARAM_REG,			PARAM_RMPTR32,		0				},
};

static const I386_OPCODE i386_opcode_table2[256] =
{
	// 0x00
	{"group0F00",		GROUP,			0,					0,					0				},
	{"group0F01",		GROUP,			0,					0,					0				},
	{"lar",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lsl",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"???",				0,				0,					0,					0				},
	{"syscall",			0,				0,					0,					0				},
	{"clts",			0,				0,					0,					0				},
	{"sysret",			0,				0,					0,					0				},
	{"invd",			0,				0,					0,					0				},
	{"wbinvd",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"ud2",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x10
	{"movups\0"
	 "movupd\0"
	 "movsd\0"
	 "movss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"movups\0"
	 "movupd\0"
	 "movsd\0"
	 "movss",			MODRM|VAR_NAME4,PARAM_XMMM,			PARAM_XMM,			0				},
	{"movlps\0"
	 "movlpd\0"
	 "movddup\0"
	 "movsldup",		MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"movlps\0"
	 "movlpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMMM,			PARAM_XMM,			0				},
	{"unpcklps\0"
	 "unpcklpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"unpckhps\0"
	 "unpckhpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"movhps\0"
	 "movhpd\0"
	 "???\0"
	 "movshdup",		MODRM|VAR_NAME4,PARAM_XMMM,			PARAM_XMM,			0				},
	{"movhps\0"
	 "movhpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"group0F18",		GROUP,			0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x20
	{"mov",				MODRM,			PARAM_REG2_32,		PARAM_CREG,			0				},
	{"mov",				MODRM,			PARAM_REG2_32,		PARAM_DREG,			0				},
	{"mov",				MODRM,			PARAM_CREG,			PARAM_REG2_32,		0				},
	{"mov",				MODRM,			PARAM_DREG,			PARAM_REG2_32,		0				},
	{"mov",				MODRM,			PARAM_REG2_32,		PARAM_TREG,			0				},
	{"???",				0,				0,					0,					0				},
	{"mov",				MODRM,			PARAM_TREG,			PARAM_REG2_32,		0				},
	{"???",				0,				0,					0,					0				},
	{"movaps\0"
	 "movapd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"movaps\0"
	 "movapd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMMM,			PARAM_XMM,			0				},
	{"cvtpi2ps\0"
	 "cvtpi2pd\0"
	 "cvtsi2sd\0"
	 "cvtsi2ss",		MODRM|VAR_NAME4,PARAM_XMM,			PARAM_RMXMM,		0				},
	{"movntps\0"
	 "movntpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMMM,			PARAM_XMM,			0				},
	{"cvttps2pi\0"
	 "cvttpd2pi\0"
	 "cvttsd2si\0"
	 "cvttss2si",		MODRM|VAR_NAME4,PARAM_REGORXMM,		PARAM_XMMM,			0				},
	{"cvtps2pi\0"
	 "cvtpd2pi\0"
	 "cvtsd2si\0"
	 "cvtss2si",		MODRM|VAR_NAME4,PARAM_REGORXMM,		PARAM_XMMM,			0				},
	{"ucomiss\0"
	 "ucomisd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"comiss\0"
	 "comisd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	// 0x30
	{"wrmsr",			0,				0,					0,					0				},
	{"rdtsc",			0,				0,					0,					0				},
	{"rdmsr",			0,				0,					0,					0				},
	{"rdpmc",			0,				0,					0,					0				},
	{"sysenter",		0,				0,					0,					0				},
	{"sysexit",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	// 0x40
	{"cmovo",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovno",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovb",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovae",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmove",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovne",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovbe",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmova",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovs",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovns",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovpe",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovpo",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovl",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovge",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovle",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"cmovg",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	// 0x50
	{"movmskps\0"
	 "movmskpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_REG3264,		PARAM_XMMM,			0				},
	{"sqrtps\0"
	 "sqrtpd\0"
	 "sqrtsd\0"
	 "sqrtss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"rsqrtps\0"
	 "???\0"
	 "???\0"
	 "rsqrtss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"rcpps\0"
	 "???\0"
	 "???\0"
	 "rcpss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"andps\0"
	 "andpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"andnps\0"
	 "andnpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"orps\0"
	 "orpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"xorps\0"
	 "xorpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"addps\0"
	 "addpd\0"
	 "addsd\0"
	 "addss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"mulps\0"
	 "mulpd\0"
	 "mulsd\0"
	 "mulss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"cvtps2pd\0"
	 "cvtpd2ps\0"
	 "cvtsd2ss\0"
	 "cvtss2sd",		MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"cvtdq2ps\0"
	 "cvtps2dq\0"
	 "???\0"
	 "cvttps2dq",		MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"subps\0"
	 "subpd\0"
	 "subsd\0"
	 "subss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"minps\0"
	 "minpd\0"
	 "minsd\0"
	 "minss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"divps\0"
	 "divpd\0"
	 "divsd\0"
	 "divss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"maxps\0"
	 "maxpd\0"
	 "maxsd\0"
	 "maxss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	// 0x60
	{"punpcklbw",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"punpcklwd",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"punpckldq",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"packsswb",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pcmpgtb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pcmpgtw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pcmpgtd",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"packuswb",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"punpckhbw",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"punpckhwd",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"punpckhdq",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"packssdw",		MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"punpcklqdq",		MODRM,			PARAM_XMM,			PARAM_XMMM,			0				},
	{"punpckhqdq",		MODRM,			PARAM_XMM,			PARAM_XMMM,			0				},
	{"movd",			MODRM,			PARAM_MMX,			PARAM_RM,			0				},
	{"movq\0"
	 "movdqa\0"
	 "???\0"
	 "movdqu",			MODRM|VAR_NAME4,PARAM_MMX,			PARAM_MMXM,			0				},
	// 0x70
	{"pshufw\0"
	 "pshufd\0"
	 "pshuflw\0"
	 "pshufhw",			MODRM|VAR_NAME4,PARAM_MMX,			PARAM_MMXM,			PARAM_I8		},
	{"group0F71",		GROUP,			0,					0,					0				},
	{"group0F72",		GROUP,			0,					0,					0				},
	{"group0F73",		GROUP,			0,					0,					0				},
	{"pcmpeqb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pcmpeqw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pcmpeqd",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"emms",			0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???\0"
	 "haddpd\0"
	 "haddps\0"
	 "???",				MODRM|VAR_NAME4,PARAM_MMX,			PARAM_MMXM,			0				},
	{"???\0"
	 "hsubpd\0"
	 "hsubps\0"
	 "???",				MODRM|VAR_NAME4,PARAM_MMX,			PARAM_MMXM,			0				},
	{"movd\0"
	 "movd\0"
	 "???\0"
	 "movq",			MODRM|VAR_NAME4,PARAM_RM,			PARAM_MMX,			0				},
	{"movq\0"
	 "movdqa\0"
	 "???\0"
	 "movdqu",			MODRM|VAR_NAME4,PARAM_MMXM,			PARAM_MMX,			0				},
	// 0x80
	{"jo",				0,				PARAM_REL,			0,					0				},
	{"jno",				0,				PARAM_REL,			0,					0				},
	{"jb",				0,				PARAM_REL,			0,					0				},
	{"jae",				0,				PARAM_REL,			0,					0				},
	{"je",				0,				PARAM_REL,			0,					0				},
	{"jne",				0,				PARAM_REL,			0,					0				},
	{"jbe",				0,				PARAM_REL,			0,					0				},
	{"ja",				0,				PARAM_REL,			0,					0				},
	{"js",				0,				PARAM_REL,			0,					0				},
	{"jns",				0,				PARAM_REL,			0,					0				},
	{"jp",				0,				PARAM_REL,			0,					0				},
	{"jnp",				0,				PARAM_REL,			0,					0				},
	{"jl",				0,				PARAM_REL,			0,					0				},
	{"jge",				0,				PARAM_REL,			0,					0				},
	{"jle",				0,				PARAM_REL,			0,					0				},
	{"jg",				0,				PARAM_REL,			0,					0				},
	// 0x90
	{"seto",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setno",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setb",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setae",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"sete",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setne",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setbe",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"seta",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"sets",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setns",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setp",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setnp",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setl",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setge",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setle",			MODRM,			PARAM_RMPTR8,		0,					0				},
	{"setg",			MODRM,			PARAM_RMPTR8,		0,					0				},
	// 0xa0
	{"push    fs",		0,				0,					0,					0				},
	{"pop     fs",		0,				0,					0,					0				},
	{"cpuid",			0,				0,					0,					0				},
	{"bt",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"shld",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_I8		},
	{"shld",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_CL		},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"push    gs",		0,				0,					0,					0				},
	{"pop     gs",		0,				0,					0,					0				},
	{"rsm",				0,				0,					0,					0				},
	{"bts",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"shrd",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_I8		},
	{"shrd",			MODRM,			PARAM_RM,			PARAM_REG,			PARAM_CL		},
	{"group0FAE",		GROUP,			0,					0,					0				},
	{"imul",			MODRM,			PARAM_REG,			PARAM_RM,			0				},
	// 0xb0
	{"cmpxchg",			MODRM,			PARAM_RM8,			PARAM_REG,			0				},
	{"cmpxchg",			MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"lss",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"btr",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"lfs",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"lgs",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"movzx",			MODRM,			PARAM_REG,			PARAM_RMPTR8,		0				},
	{"movzx",			MODRM,			PARAM_REG,			PARAM_RMPTR16,		0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"group0FBA",		GROUP,			0,					0,					0				},
	{"btc",				MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"bsf",				MODRM,			PARAM_REG,			PARAM_RM,			0				},
	{"bsr",				MODRM,			PARAM_REG,			PARAM_RM,			0,				DASMFLAG_STEP_OVER},
	{"movsx",			MODRM,			PARAM_REG,			PARAM_RMPTR8,		0				},
	{"movsx",			MODRM,			PARAM_REG,			PARAM_RMPTR16,		0				},
	// 0xc0
	{"xadd",			MODRM,			PARAM_RM8,			PARAM_REG,			0				},
	{"xadd",			MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"cmpps\0"
	 "cmppd\0"
	 "cmpsd\0"
	 "cmpss",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"movnti",			MODRM,			PARAM_RM,			PARAM_REG,			0				},
	{"pinsrw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			PARAM_I8		},
	{"pextrw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			PARAM_I8		},
	{"shufps\0"
	 "shufpd\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			PARAM_I8		},
	{"cmpxchg8b",		MODRM,			PARAM_M64PTR,		0,					0				},
	{"bswap",			0,				PARAM_EAX,			0,					0				},
	{"bswap",			0,				PARAM_ECX,			0,					0				},
	{"bswap",			0,				PARAM_EDX,			0,					0				},
	{"bswap",			0,				PARAM_EBX,			0,					0				},
	{"bswap",			0,				PARAM_ESP,			0,					0				},
	{"bswap",			0,				PARAM_EBP,			0,					0				},
	{"bswap",			0,				PARAM_ESI,			0,					0				},
	{"bswap",			0,				PARAM_EDI,			0,					0				},
	// 0xd0
	{"addsubpd\0"
	 "???\0"
	 "addsubps\0"
	 "???\0",			MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"psrlw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psrld",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psrlq",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddq",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmullw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"movq\0"
	 "???\0"
	 "movdq2q\0"
	 "movq2dq",			MODRM|VAR_NAME4,PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmovmskb",		MODRM,			PARAM_REG3264,		PARAM_MMXM,			0				},
	{"psubusb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psubusw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pminub",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pand",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddub",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"padduw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmaxub",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pandn",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	// 0xe0
	{"pavgb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psraw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psrad",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pavgw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmulhuw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmulhw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"???\0"
	 "cvttpd2dq\0"
	 "cvtpd2dq\0"
	 "cvtdq2pd",		MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"movntq",			MODRM,			PARAM_M64,			PARAM_MMX,			0				},
	{"psubsb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psubsw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pminsw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"por",				MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddsb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddsw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmaxsw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pxor",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	// 0xf0
	{"???\0"
	 "???\0"
	 "lddqu\0"
	 "???",				MODRM|VAR_NAME4,PARAM_XMM,			PARAM_XMMM,			0				},
	{"psllw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pslld",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psllq",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmuludq",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"pmaddwd",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psadbw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"maskmovq\0"
	 "maskmovdqu\0"
	 "???\0"
	 "???",				MODRM|VAR_NAME4,PARAM_MMX,			PARAM_MMXM,			0				},
	{"psubb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psubw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psubd",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"psubq",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddb",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddw",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"paddd",			MODRM,			PARAM_MMX,			PARAM_MMXM,			0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE group80_table[8] =
{
	{"add",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"or",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"adc",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"sbb",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"and",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"sub",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"xor",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"cmp",				0,				PARAM_RMPTR8,		PARAM_I8,			0				}
};

static const I386_OPCODE group81_table[8] =
{
	{"add",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"or",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"adc",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"sbb",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"and",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"sub",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"xor",				0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"cmp",				0,				PARAM_RMPTR,		PARAM_IMM,			0				}
};

static const I386_OPCODE group83_table[8] =
{
	{"add",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"or",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"adc",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"sbb",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"and",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"sub",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"xor",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"cmp",				0,				PARAM_RMPTR,		PARAM_I8,			0				}
};

static const I386_OPCODE groupC0_table[8] =
{
	{"rol",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"ror",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"rcl",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"rcr",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"shl",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"shr",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"sal",				0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"sar",				0,				PARAM_RMPTR8,		PARAM_I8,			0				}
};

static const I386_OPCODE groupC1_table[8] =
{
	{"rol",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"ror",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"rcl",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"rcr",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"shl",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"shr",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"sal",				0,				PARAM_RMPTR,		PARAM_I8,			0				},
	{"sar",				0,				PARAM_RMPTR,		PARAM_I8,			0				}
};

static const I386_OPCODE groupD0_table[8] =
{
	{"rol",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"ror",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"rcl",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"rcr",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"shl",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"shr",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"sal",				0,				PARAM_RMPTR8,		PARAM_1,			0				},
	{"sar",				0,				PARAM_RMPTR8,		PARAM_1,			0				}
};

static const I386_OPCODE groupD1_table[8] =
{
	{"rol",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"ror",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"rcl",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"rcr",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"shl",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"shr",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"sal",				0,				PARAM_RMPTR,		PARAM_1,			0				},
	{"sar",				0,				PARAM_RMPTR,		PARAM_1,			0				}
};

static const I386_OPCODE groupD2_table[8] =
{
	{"rol",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"ror",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"rcl",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"rcr",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"shl",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"shr",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"sal",				0,				PARAM_RMPTR8,		PARAM_CL,			0				},
	{"sar",				0,				PARAM_RMPTR8,		PARAM_CL,			0				}
};

static const I386_OPCODE groupD3_table[8] =
{
	{"rol",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"ror",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"rcl",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"rcr",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"shl",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"shr",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"sal",				0,				PARAM_RMPTR,		PARAM_CL,			0				},
	{"sar",				0,				PARAM_RMPTR,		PARAM_CL,			0				}
};

static const I386_OPCODE groupF6_table[8] =
{
	{"test",			0,				PARAM_RMPTR8,		PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"not",				0,				PARAM_RMPTR8,		0,					0				},
	{"neg",				0,				PARAM_RMPTR8,		0,					0				},
	{"mul",				0,				PARAM_RMPTR8,		0,					0				},
	{"imul",			0,				PARAM_RMPTR8,		0,					0				},
	{"div",				0,				PARAM_RMPTR8,		0,					0				},
	{"idiv",			0,				PARAM_RMPTR8,		0,					0				}
};

static const I386_OPCODE groupF7_table[8] =
{
	{"test",			0,				PARAM_RMPTR,		PARAM_IMM,			0				},
	{"???",				0,				0,					0,					0				},
	{"not",				0,				PARAM_RMPTR,		0,					0				},
	{"neg",				0,				PARAM_RMPTR,		0,					0				},
	{"mul",				0,				PARAM_RMPTR,		0,					0				},
	{"imul",			0,				PARAM_RMPTR,		0,					0				},
	{"div",				0,				PARAM_RMPTR,		0,					0				},
	{"idiv",			0,				PARAM_RMPTR,		0,					0				}
};

static const I386_OPCODE groupFE_table[8] =
{
	{"inc",				0,				PARAM_RMPTR8,		0,					0				},
	{"dec",				0,				PARAM_RMPTR8,		0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE groupFF_table[8] =
{
	{"inc",				0,				PARAM_RMPTR,		0,					0				},
	{"dec",				0,				PARAM_RMPTR,		0,					0				},
	{"call",			ALWAYS64,		PARAM_RMPTR,		0,					0,				DASMFLAG_STEP_OVER},
	{"call    far ptr ",0,				PARAM_RM,			0,					0,				DASMFLAG_STEP_OVER},
	{"jmp",				ALWAYS64,		PARAM_RMPTR,		0,					0				},
	{"jmp     far ptr ",0,				PARAM_RM,			0,					0				},
	{"push",			0,				PARAM_RMPTR,		0,					0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE group0F00_table[8] =
{
	{"sldt",			0,				PARAM_RM,			0,					0				},
	{"str",				0,				PARAM_RM,			0,					0				},
	{"lldt",			0,				PARAM_RM,			0,					0				},
	{"ltr",				0,				PARAM_RM,			0,					0				},
	{"verr",			0,				PARAM_RM,			0,					0				},
	{"verw",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE group0F01_table[8] =
{
	{"sgdt",			0,				PARAM_RM,			0,					0				},
	{"sidt",			0,				PARAM_RM,			0,					0				},
	{"lgdt",			0,				PARAM_RM,			0,					0				},
	{"lidt",			0,				PARAM_RM,			0,					0				},
	{"smsw",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"lmsw",			0,				PARAM_RM,			0,					0				},
	{"invlpg",			0,				PARAM_RM,			0,					0				}
};

static const I386_OPCODE group0F18_table[8] =
{
	{"prefetchnta",		0,				PARAM_RM8,			0,					0				},
	{"prefetch0",		0,				PARAM_RM8,			0,					0				},
	{"prefetch1",		0,				PARAM_RM8,			0,					0				},
	{"prefetch2",		0,				PARAM_RM8,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE group0F71_table[8] =
{
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"psrlw",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"psraw",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"psllw",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE group0F72_table[8] =
{
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"psrld",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"psrldq",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"pslld",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"pslldq",			0,				PARAM_MMX,			PARAM_I8,			0				},
};

static const I386_OPCODE group0F73_table[8] =
{
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"psrlq",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"psraq",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				},
	{"psllq",			0,				PARAM_MMX,			PARAM_I8,			0				},
	{"???",				0,				0,					0,					0				}
};

static const I386_OPCODE group0FAE_table[8] =
{
	{"fxsave",			0,				PARAM_RM,			0,					0				},
	{"fxrstor",			0,				PARAM_RM,			0,					0				},
	{"ldmxcsr",			0,				PARAM_RM,			0,					0				},
	{"stmxscr",			0,				PARAM_RM,			0,					0				},
	{"???",				0,				0,					0,					0				},
	{"lfence",			0,				0,					0,					0				},
	{"mfence",			0,				0,					0,					0				},
	{"sfence",			0,				0,					0,					0				}
};


static const I386_OPCODE group0FBA_table[8] =
{
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"???",				0,				0,					0,					0				},
	{"bt",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"bts",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"btr",				0,				PARAM_RM,			PARAM_I8,			0				},
	{"btc",				0,				PARAM_RM,			PARAM_I8,			0				}
};

static const GROUP_OP group_op_table[] =
{
	{ "group80",			group80_table			},
	{ "group81",			group81_table			},
	{ "group83",			group83_table			},
	{ "groupC0",			groupC0_table			},
	{ "groupC1",			groupC1_table			},
	{ "groupD0",			groupD0_table			},
	{ "groupD1",			groupD1_table			},
	{ "groupD2",			groupD2_table			},
	{ "groupD3",			groupD3_table			},
	{ "groupF6",			groupF6_table			},
	{ "groupF7",			groupF7_table			},
	{ "groupFE",			groupFE_table			},
	{ "groupFF",			groupFF_table			},
	{ "group0F00",			group0F00_table			},
	{ "group0F01",			group0F01_table			},
	{ "group0F18",			group0F18_table			},
	{ "group0F71",			group0F71_table			},
	{ "group0F72",			group0F72_table			},
	{ "group0F73",			group0F73_table			},
	{ "group0FAE",			group0FAE_table			},
	{ "group0FBA",			group0FBA_table			}
};



static const char *const i386_reg[3][16] =
{
	{"ax",  "cx",  "dx",  "bx",  "sp",  "bp",  "si",  "di",  "r8w", "r9w", "r10w","r11w","r12w","r13w","r14w","r15w"},
	{"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d","r11d","r12d","r13d","r14d","r15d"},
	{"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"}
};

static const char *const i386_reg8[8] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
static const char *const i386_reg8rex[16] = {"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil", "r8l", "r9l", "r10l", "r11l", "r12l", "r13l", "r14l", "r15l"};
static const char *const i386_sreg[8] = {"es", "cs", "ss", "ds", "fs", "gs", "???", "???"};

static int address_size;
static int operand_size;
static int address_prefix;
static int operand_prefix;
static int max_length;
static UINT64 pc;
static UINT8 modrm;
static UINT32 segment;
static offs_t dasm_flags;
static char modrm_string[256];
static UINT8 rex, regex, sibex, rmex;
static UINT8 pre0f;
static UINT8 curmode;

#define MODRM_REG1	((modrm >> 3) & 0x7)
#define MODRM_REG2	(modrm & 0x7)

INLINE UINT8 FETCH(void)
{
	if ((opcode_ptr - opcode_ptr_base) + 1 > max_length)
		return 0xff;
	pc++;
	return *opcode_ptr++;
}

INLINE UINT16 FETCH16(void)
{
	UINT16 d;
	if ((opcode_ptr - opcode_ptr_base) + 2 > max_length)
		return 0xffff;
	d = opcode_ptr[0] | (opcode_ptr[1] << 8);
	opcode_ptr += 2;
	pc += 2;
	return d;
}

INLINE UINT32 FETCH32(void)
{
	UINT32 d;
	if ((opcode_ptr - opcode_ptr_base) + 4 > max_length)
		return 0xffffffff;
	d = opcode_ptr[0] | (opcode_ptr[1] << 8) | (opcode_ptr[2] << 16) | (opcode_ptr[3] << 24);
	opcode_ptr += 4;
	pc += 4;
	return d;
}

INLINE UINT8 FETCHD(void)
{
	if ((opcode_ptr - opcode_ptr_base) + 1 > max_length)
		return 0xff;
	pc++;
	return *opcode_ptr++;
}

INLINE UINT16 FETCHD16(void)
{
	UINT16 d;
	if ((opcode_ptr - opcode_ptr_base) + 2 > max_length)
		return 0xffff;
	d = opcode_ptr[0] | (opcode_ptr[1] << 8);
	opcode_ptr += 2;
	pc += 2;
	return d;
}

INLINE UINT32 FETCHD32(void)
{
	UINT32 d;
	if ((opcode_ptr - opcode_ptr_base) + 4 > max_length)
		return 0xffffffff;
	d = opcode_ptr[0] | (opcode_ptr[1] << 8) | (opcode_ptr[2] << 16) | (opcode_ptr[3] << 24);
	opcode_ptr += 4;
	pc += 4;
	return d;
}

static char *hexstring(UINT32 value, int digits)
{
	static char buffer[20];
	buffer[0] = '0';
	if (digits)
		sprintf(&buffer[1], "%0*Xh", digits, value);
	else
		sprintf(&buffer[1], "%Xh", value);
	return (buffer[1] >= '0' && buffer[1] <= '9') ? &buffer[1] : &buffer[0];
}

static char *hexstring64(UINT32 lo, UINT32 hi)
{
	static char buffer[40];
	buffer[0] = '0';
	if (hi != 0)
		sprintf(&buffer[1], "%X%08Xh", hi, lo);
	else
		sprintf(&buffer[1], "%Xh", lo);
	return (buffer[1] >= '0' && buffer[1] <= '9') ? &buffer[1] : &buffer[0];
}

static char *hexstringpc(UINT64 pc)
{
	if (curmode == 64)
		return hexstring64((UINT32)pc, (UINT32)(pc >> 32));
	else
		return hexstring((UINT32)pc, 0);
}

static char *shexstring(UINT32 value, int digits, int always)
{
	static char buffer[20];
	if (value >= 0x80000000)
		sprintf(buffer, "-%s", hexstring(-value, digits));
	else if (always)
		sprintf(buffer, "+%s", hexstring(value, digits));
	else
		return hexstring(value, digits);
	return buffer;
}

static char* handle_sib_byte( char* s, UINT8 mod )
{
	UINT32 i32;
	UINT8 scale, i, base;
	UINT8 sib = FETCHD();

	scale = (sib >> 6) & 0x3;
	i = ((sib >> 3) & 0x7) | sibex;
	base = (sib & 0x7) | rmex;

	if (base == 5 && mod == 0) {
		i32 = FETCH32();
		s += sprintf( s, "%s", hexstring(i32, 0) );
	} else if (base != 5 || mod != 3)
		s += sprintf( s, "%s", i386_reg[address_size][base] );

	if ( i != 4 ) {
		s += sprintf( s, "+%s", i386_reg[address_size][i] );
		if (scale)
			s += sprintf( s, "*%d", 1 << scale );
	}
	return s;
}

static void handle_modrm(char* s)
{
	INT8 disp8;
	INT16 disp16;
	INT32 disp32;
	UINT8 mod, rm;

	modrm = FETCHD();
	mod = (modrm >> 6) & 0x3;
	rm = (modrm & 0x7) | rmex;

	if( modrm >= 0xc0 )
		return;

	switch(segment)
	{
		case SEG_CS: s += sprintf( s, "cs:" ); break;
		case SEG_DS: s += sprintf( s, "ds:" ); break;
		case SEG_ES: s += sprintf( s, "es:" ); break;
		case SEG_FS: s += sprintf( s, "fs:" ); break;
		case SEG_GS: s += sprintf( s, "gs:" ); break;
		case SEG_SS: s += sprintf( s, "ss:" ); break;
	}

	s += sprintf( s, "[" );
	if( address_size == 2 ) {
		if ((rm & 7) == 4)
			s = handle_sib_byte( s, mod );
		else if ((rm & 7) == 5 && mod == 0) {
			disp32 = FETCHD32();
			s += sprintf( s, "rip%s", shexstring(disp32, 0, TRUE) );
		} else
			s += sprintf( s, "%s", i386_reg[2][rm]);
		if( mod == 1 ) {
			disp8 = FETCHD();
			if (disp8 != 0)
				s += sprintf( s, "%s", shexstring((INT32)disp8, 0, TRUE) );
		} else if( mod == 2 ) {
			disp32 = FETCHD32();
			if (disp32 != 0)
				s += sprintf( s, "%s", shexstring(disp32, 0, TRUE) );
		}
	} else if (address_size == 1) {
		if ((rm & 7) == 4)
			s = handle_sib_byte( s, mod );
		else if ((rm & 7) == 5 && mod == 0) {
			disp32 = FETCHD32();
			if (curmode == 64)
				s += sprintf( s, "eip%s", shexstring(disp32, 0, TRUE) );
			else
				s += sprintf( s, "%s", hexstring(disp32, 0) );
		} else
			s += sprintf( s, "%s", i386_reg[1][rm]);
		if( mod == 1 ) {
			disp8 = FETCHD();
			if (disp8 != 0)
				s += sprintf( s, "%s", shexstring((INT32)disp8, 0, TRUE) );
		} else if( mod == 2 ) {
			disp32 = FETCHD32();
			if (disp32 != 0)
				s += sprintf( s, "%s", shexstring(disp32, 0, TRUE) );
		}
	} else {
		switch( rm )
		{
			case 0: s += sprintf( s, "bx+si" ); break;
			case 1: s += sprintf( s, "bx+di" ); break;
			case 2: s += sprintf( s, "bp+si" ); break;
			case 3: s += sprintf( s, "bp+di" ); break;
			case 4: s += sprintf( s, "si" ); break;
			case 5: s += sprintf( s, "di" ); break;
			case 6:
				if( mod == 0 ) {
					disp16 = FETCHD16();
					s += sprintf( s, "%s", hexstring((unsigned) (UINT16) disp16, 0) );
				} else {
					s += sprintf( s, "bp" );
				}
				break;
			case 7: s += sprintf( s, "bx" ); break;
		}
		if( mod == 1 ) {
			disp8 = FETCHD();
			if (disp8 != 0)
				s += sprintf( s, "%s", shexstring((INT32)disp8, 0, TRUE) );
		} else if( mod == 2 ) {
			disp16 = FETCHD16();
			if (disp16 != 0)
				s += sprintf( s, "%s", shexstring((INT32)disp16, 0, TRUE) );
		}
	}
	s += sprintf( s, "]" );
}

static char* handle_param(char* s, UINT32 param)
{
	UINT8 i8;
	UINT16 i16;
	UINT32 i32;
	UINT16 ptr;
	UINT32 addr;
	INT8 d8;
	INT16 d16;
	INT32 d32;

	switch(param)
	{
		case PARAM_REG:
			s += sprintf( s, "%s", i386_reg[operand_size][MODRM_REG1 | regex] );
			break;

		case PARAM_REG8:
			s += sprintf( s, "%s", (rex ? i386_reg8rex : i386_reg8)[MODRM_REG1 | regex] );
			break;

		case PARAM_REG16:
			s += sprintf( s, "%s", i386_reg[0][MODRM_REG1 | regex] );
			break;

		case PARAM_REG32:
			s += sprintf( s, "%s", i386_reg[1][MODRM_REG1 | regex] );
			break;

		case PARAM_REG3264:
			s += sprintf( s, "%s", i386_reg[(operand_size == 2) ? 2 : 1][MODRM_REG1 | regex] );
			break;

		case PARAM_MMX:
			if (pre0f == 0x66 || pre0f == 0xf2 || pre0f == 0xf3)
				s += sprintf( s, "xmm%d", MODRM_REG1 | regex );
			else
				s += sprintf( s, "mm%d", MODRM_REG1 | regex );
			break;

		case PARAM_XMM:
			s += sprintf( s, "xmm%d", MODRM_REG1 | regex );
			break;

		case PARAM_REGORXMM:
			if (pre0f != 0xf2 && pre0f != 0xf3)
				s += sprintf( s, "xmm%d", MODRM_REG1 | regex );
			else
				s += sprintf( s, "%s", i386_reg[(operand_size == 2) ? 2 : 1][MODRM_REG1 | regex] );
			break;

		case PARAM_REG2_32:
			s += sprintf( s, "%s", i386_reg[1][MODRM_REG2 | rmex] );
			break;

		case PARAM_RM:
		case PARAM_RMPTR:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", i386_reg[operand_size][MODRM_REG2 | rmex] );
			} else {
				if (param == PARAM_RMPTR)
				{
					if( operand_size == 2 )
						s += sprintf( s, "qword ptr " );
					else if (operand_size == 1)
						s += sprintf( s, "dword ptr " );
					else
						s += sprintf( s, "word ptr " );
				}
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_RM8:
		case PARAM_RMPTR8:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", (rex ? i386_reg8rex : i386_reg8)[MODRM_REG2 | rmex] );
			} else {
				if (param == PARAM_RMPTR8)
					s += sprintf( s, "byte ptr " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_RM16:
		case PARAM_RMPTR16:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", i386_reg[0][MODRM_REG2 | rmex] );
			} else {
				if (param == PARAM_RMPTR16)
					s += sprintf( s, "word ptr " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_RM32:
		case PARAM_RMPTR32:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "%s", i386_reg[1][MODRM_REG2 | rmex] );
			} else {
				if (param == PARAM_RMPTR32)
					s += sprintf( s, "dword ptr " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_RMXMM:
			if( modrm >= 0xc0 ) {
				if (pre0f != 0xf2 && pre0f != 0xf3)
					s += sprintf( s, "xmm%d", MODRM_REG2 | rmex );
				else
					s += sprintf( s, "%s", i386_reg[(operand_size == 2) ? 2 : 1][MODRM_REG2 | rmex] );
			} else {
				if (param == PARAM_RMPTR32)
					s += sprintf( s, "dword ptr " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_M64:
		case PARAM_M64PTR:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "???" );
			} else {
				if (param == PARAM_M64PTR)
					s += sprintf( s, "qword ptr " );
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_MMXM:
			if( modrm >= 0xc0 ) {
				if (pre0f == 0x66 || pre0f == 0xf2 || pre0f == 0xf3)
					s += sprintf( s, "xmm%d", MODRM_REG2 | rmex );
				else
					s += sprintf( s, "mm%d", MODRM_REG2 | rmex );
			} else {
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_XMMM:
			if( modrm >= 0xc0 ) {
				s += sprintf( s, "xmm%d", MODRM_REG2 | rmex );
			} else {
				s += sprintf( s, "%s", modrm_string );
			}
			break;

		case PARAM_I4:
			i8 = FETCHD();
			s += sprintf( s, "%d", i8 & 0x0f );
			break;

		case PARAM_I8:
			i8 = FETCHD();
			s += sprintf( s, "%s", shexstring((INT8)i8, 0, FALSE) );
			break;

		case PARAM_I16:
			i16 = FETCHD16();
			s += sprintf( s, "%s", shexstring((INT16)i16, 0, FALSE) );
			break;

		case PARAM_UI8:
			i8 = FETCHD();
			s += sprintf( s, "%s", shexstring((UINT8)i8, 0, FALSE) );
			break;

		case PARAM_UI16:
			i16 = FETCHD16();
			s += sprintf( s, "%s", shexstring((UINT16)i16, 0, FALSE) );
			break;

		case PARAM_IMM64:
			if (operand_size == 2) {
				UINT32 lo32 = FETCHD32();
				i32 = FETCHD32();
				s += sprintf( s, "%s", hexstring64(lo32, i32) );
			} else if( operand_size ) {
				i32 = FETCHD32();
				s += sprintf( s, "%s", hexstring(i32, 0) );
			} else {
				i16 = FETCHD16();
				s += sprintf( s, "%s", hexstring(i16, 0) );
			}
			break;

		case PARAM_IMM:
			if( operand_size ) {
				i32 = FETCHD32();
				s += sprintf( s, "%s", hexstring(i32, 0) );
			} else {
				i16 = FETCHD16();
				s += sprintf( s, "%s", hexstring(i16, 0) );
			}
			break;

		case PARAM_ADDR:
			if( operand_size ) {
				addr = FETCHD32();
				ptr = FETCHD16();
				s += sprintf( s, "%s:", hexstring(ptr, 4) );
				s += sprintf( s, "%s", hexstring(addr, 0) );
			} else {
				addr = FETCHD16();
				ptr = FETCHD16();
				s += sprintf( s, "%s:", hexstring(ptr, 4) );
				s += sprintf( s, "%s", hexstring(addr, 0) );
			}
			break;

		case PARAM_REL:
			if( operand_size ) {
				d32 = FETCHD32();
				s += sprintf( s, "%s", hexstringpc(pc + d32) );
			} else {
				/* make sure to keep the relative offset within the segment */
				d16 = FETCHD16();
				s += sprintf( s, "%s", hexstringpc((pc & 0xFFFF0000) | ((pc + d16) & 0x0000FFFF)) );
			}
			break;

		case PARAM_REL8:
			d8 = FETCHD();
			s += sprintf( s, "%s", hexstringpc(pc + d8) );
			break;

		case PARAM_MEM_OFFS:
			switch(segment)
			{
				case SEG_CS: s += sprintf( s, "cs:" ); break;
				case SEG_DS: s += sprintf( s, "ds:" ); break;
				case SEG_ES: s += sprintf( s, "es:" ); break;
				case SEG_FS: s += sprintf( s, "fs:" ); break;
				case SEG_GS: s += sprintf( s, "gs:" ); break;
				case SEG_SS: s += sprintf( s, "ss:" ); break;
			}

			if( address_size ) {
				i32 = FETCHD32();
				s += sprintf( s, "[%s]", hexstring(i32, 0) );
			} else {
				i16 = FETCHD16();
				s += sprintf( s, "[%s]", hexstring(i16, 0) );
			}
			break;

		case PARAM_SREG:
			s += sprintf( s, "%s", i386_sreg[MODRM_REG1] );
			break;

		case PARAM_CREG:
			s += sprintf( s, "cr%d", MODRM_REG1 | regex );
			break;

		case PARAM_TREG:
			s += sprintf( s, "tr%d", MODRM_REG1 | regex );
			break;

		case PARAM_DREG:
			s += sprintf( s, "dr%d", MODRM_REG1 | regex );
			break;

		case PARAM_1:
			s += sprintf( s, "1" );
			break;

		case PARAM_DX:
			s += sprintf( s, "dx" );
			break;

		case PARAM_AL: s += sprintf( s, "al" ); break;
		case PARAM_CL: s += sprintf( s, "cl" ); break;
		case PARAM_DL: s += sprintf( s, "dl" ); break;
		case PARAM_BL: s += sprintf( s, "bl" ); break;
		case PARAM_AH: s += sprintf( s, "ah" ); break;
		case PARAM_CH: s += sprintf( s, "ch" ); break;
		case PARAM_DH: s += sprintf( s, "dh" ); break;
		case PARAM_BH: s += sprintf( s, "bh" ); break;

		case PARAM_EAX: s += sprintf( s, "%s", i386_reg[operand_size][0 | rmex] ); break;
		case PARAM_ECX: s += sprintf( s, "%s", i386_reg[operand_size][1 | rmex] ); break;
		case PARAM_EDX: s += sprintf( s, "%s", i386_reg[operand_size][2 | rmex] ); break;
		case PARAM_EBX: s += sprintf( s, "%s", i386_reg[operand_size][3 | rmex] ); break;
		case PARAM_ESP: s += sprintf( s, "%s", i386_reg[operand_size][4 | rmex] ); break;
		case PARAM_EBP: s += sprintf( s, "%s", i386_reg[operand_size][5 | rmex] ); break;
		case PARAM_ESI: s += sprintf( s, "%s", i386_reg[operand_size][6 | rmex] ); break;
		case PARAM_EDI: s += sprintf( s, "%s", i386_reg[operand_size][7 | rmex] ); break;
	}
	return s;
}

static void handle_fpu(char *s, UINT8 op1, UINT8 op2)
{
	switch (op1 & 0x7)
	{
		case 0:		// Group D8
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fadd    dword ptr %s", modrm_string); break;
					case 1: sprintf(s, "fmul    dword ptr %s", modrm_string); break;
					case 2: sprintf(s, "fcom    dword ptr %s", modrm_string); break;
					case 3: sprintf(s, "fcomp   dword ptr %s", modrm_string); break;
					case 4: sprintf(s, "fsub    dword ptr %s", modrm_string); break;
					case 5: sprintf(s, "fsubr   dword ptr %s", modrm_string); break;
					case 6: sprintf(s, "fdiv    dword ptr %s", modrm_string); break;
					case 7: sprintf(s, "fdivr   dword ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fadd    st(0),st(%d)", op2 & 0x7); break;
					case 1: sprintf(s, "fmul    st(0),st(%d)", op2 & 0x7); break;
					case 2: sprintf(s, "fcom    st(0),st(%d)", op2 & 0x7); break;
					case 3: sprintf(s, "fcomp   st(0),st(%d)", op2 & 0x7); break;
					case 4: sprintf(s, "fsub    st(0),st(%d)", op2 & 0x7); break;
					case 5: sprintf(s, "fsubr   st(0),st(%d)", op2 & 0x7); break;
					case 6: sprintf(s, "fdiv    st(0),st(%d)", op2 & 0x7); break;
					case 7: sprintf(s, "fdivr   st(0),st(%d)", op2 & 0x7); break;
				}
			}
			break;
		}

		case 1:		// Group D9
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fld     dword ptr %s", modrm_string); break;
					case 1: sprintf(s, "??? (FPU)"); break;
					case 2: sprintf(s, "fst     dword ptr %s", modrm_string); break;
					case 3: sprintf(s, "fstp    dword ptr %s", modrm_string); break;
					case 4: sprintf(s, "fldenv  word ptr %s", modrm_string); break;
					case 5: sprintf(s, "fldcw   word ptr %s", modrm_string); break;
					case 6: sprintf(s, "fstenv  word ptr %s", modrm_string); break;
					case 7: sprintf(s, "fstcw   word ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
						sprintf(s, "fld     st(0),st(%d)", op2 & 0x7); break;

					case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
						sprintf(s, "fxch    st(0),st(%d)", op2 & 0x7); break;

					case 0x10: sprintf(s, "fnop"); break;
					case 0x20: sprintf(s, "fchs"); break;
					case 0x21: sprintf(s, "fabs"); break;
					case 0x24: sprintf(s, "ftst"); break;
					case 0x25: sprintf(s, "fxam"); break;
					case 0x28: sprintf(s, "fld1"); break;
					case 0x29: sprintf(s, "fldl2t"); break;
					case 0x2a: sprintf(s, "fldl2e"); break;
					case 0x2b: sprintf(s, "fldpi"); break;
					case 0x2c: sprintf(s, "fldlg2"); break;
					case 0x2d: sprintf(s, "fldln2"); break;
					case 0x2e: sprintf(s, "fldz"); break;
					case 0x30: sprintf(s, "f2xm1"); break;
					case 0x31: sprintf(s, "fyl2x"); break;
					case 0x32: sprintf(s, "fptan"); break;
					case 0x33: sprintf(s, "fpatan"); break;
					case 0x34: sprintf(s, "fxtract"); break;
					case 0x35: sprintf(s, "fprem1"); break;
					case 0x36: sprintf(s, "fdecstp"); break;
					case 0x37: sprintf(s, "fincstp"); break;
					case 0x38: sprintf(s, "fprem"); break;
					case 0x39: sprintf(s, "fyl2xp1"); break;
					case 0x3a: sprintf(s, "fsqrt"); break;
					case 0x3b: sprintf(s, "fsincos"); break;
					case 0x3c: sprintf(s, "frndint"); break;
					case 0x3d: sprintf(s, "fscale"); break;
					case 0x3e: sprintf(s, "fsin"); break;
					case 0x3f: sprintf(s, "fcos"); break;

					default: sprintf(s, "??? (FPU)"); break;
				}
			}
			break;
		}

		case 2:		// Group DA
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fiadd   dword ptr %s", modrm_string); break;
					case 1: sprintf(s, "fimul   dword ptr %s", modrm_string); break;
					case 2: sprintf(s, "ficom   dword ptr %s", modrm_string); break;
					case 3: sprintf(s, "ficomp  dword ptr %s", modrm_string); break;
					case 4: sprintf(s, "fisub   dword ptr %s", modrm_string); break;
					case 5: sprintf(s, "fisubr  dword ptr %s", modrm_string); break;
					case 6: sprintf(s, "fidiv   dword ptr %s", modrm_string); break;
					case 7: sprintf(s, "fidivr  dword ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
						sprintf(s, "fcmovb  st(0),st(%d)", op2 & 0x7); break;

					case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
						sprintf(s, "fcmove  st(0),st(%d)", op2 & 0x7); break;

					case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
						sprintf(s, "fcmovbe st(0),st(%d)", op2 & 0x7); break;

					case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
						sprintf(s, "fcmovu  st(0),st(%d)", op2 & 0x7); break;
					case 0x29:
						sprintf(s, "fucompp"); break;

					default: sprintf(s, "??? (FPU)"); break;

				}
			}
			break;
		}

		case 3:		// Group DB
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fild    dword ptr %s", modrm_string); break;
					case 1: sprintf(s, "fisttp  dword ptr %s", modrm_string); break;
					case 2: sprintf(s, "fist    dword ptr %s", modrm_string); break;
					case 3: sprintf(s, "fistp   dword ptr %s", modrm_string); break;
					case 4: sprintf(s, "??? (FPU)"); break;
					case 5: sprintf(s, "fld     tword ptr %s", modrm_string); break;
					case 6: sprintf(s, "??? (FPU)"); break;
					case 7: sprintf(s, "fstp    tword ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
						sprintf(s, "fcmovnb st(0),st(%d)", op2 & 0x7); break;

					case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
						sprintf(s, "fcmovne st(0),st(%d)", op2 & 0x7); break;

					case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
						sprintf(s, "fcmovnbe st(0),st(%d)", op2 & 0x7); break;

					case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
						sprintf(s, "fcmovnu st(0),st(%d)", op2 & 0x7); break;

					case 0x22: sprintf(s, "fclex"); break;
					case 0x23: sprintf(s, "finit"); break;

					case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
						sprintf(s, "fucomi  st(0),st(%d)", op2 & 0x7); break;

					case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
						sprintf(s, "fcomi   st(0),st(%d)", op2 & 0x7); break;

					default: sprintf(s, "??? (FPU)"); break;
				}
			}
			break;
		}

		case 4:		// Group DC
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fadd    qword ptr %s", modrm_string); break;
					case 1: sprintf(s, "fmul    qword ptr %s", modrm_string); break;
					case 2: sprintf(s, "fcom    qword ptr %s", modrm_string); break;
					case 3: sprintf(s, "fcomp   qword ptr %s", modrm_string); break;
					case 4: sprintf(s, "fsub    qword ptr %s", modrm_string); break;
					case 5: sprintf(s, "fsubr   qword ptr %s", modrm_string); break;
					case 6: sprintf(s, "fdiv    qword ptr %s", modrm_string); break;
					case 7: sprintf(s, "fdivr   qword ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
						sprintf(s, "fadd    st(%d),st(0)", op2 & 0x7); break;

					case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
						sprintf(s, "fmul    st(%d),st(0)", op2 & 0x7); break;

					case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
						sprintf(s, "fsubr   st(%d),st(0)", op2 & 0x7); break;

					case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
						sprintf(s, "fsub    st(%d),st(0)", op2 & 0x7); break;

					case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
						sprintf(s, "fdivr   st(%d),st(0)", op2 & 0x7); break;

					case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
						sprintf(s, "fdiv    st(%d),st(0)", op2 & 0x7); break;

					default: sprintf(s, "??? (FPU)"); break;
				}
			}
			break;
		}

		case 5:		// Group DD
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fld     qword ptr %s", modrm_string); break;
					case 1: sprintf(s, "fisttp  qword ptr %s", modrm_string); break;
					case 2: sprintf(s, "fst     qword ptr %s", modrm_string); break;
					case 3: sprintf(s, "fstp    qword ptr %s", modrm_string); break;
					case 4: sprintf(s, "frstor  %s", modrm_string); break;
					case 5: sprintf(s, "??? (FPU)"); break;
					case 6: sprintf(s, "fsave   %s", modrm_string); break;
					case 7: sprintf(s, "fstsw   word ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
						sprintf(s, "ffree   st(%d)", op2 & 0x7); break;

					case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
						sprintf(s, "fst     st(%d)", op2 & 0x7); break;

					case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
						sprintf(s, "fstp    st(%d)", op2 & 0x7); break;

					case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
						sprintf(s, "fucom   st(%d), st(0)", op2 & 0x7); break;

					case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
						sprintf(s, "fucomp  st(%d)", op2 & 0x7); break;

					default: sprintf(s, "??? (FPU)"); break;
				}
			}
			break;
		}

		case 6:		// Group DE
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fiadd   word ptr %s", modrm_string); break;
					case 1: sprintf(s, "fimul   word ptr %s", modrm_string); break;
					case 2: sprintf(s, "ficom   word ptr %s", modrm_string); break;
					case 3: sprintf(s, "ficomp  word ptr %s", modrm_string); break;
					case 4: sprintf(s, "fisub   word ptr %s", modrm_string); break;
					case 5: sprintf(s, "fisubr  word ptr %s", modrm_string); break;
					case 6: sprintf(s, "fidiv   word ptr %s", modrm_string); break;
					case 7: sprintf(s, "fidivr  word ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
						sprintf(s, "faddp   st(%d)", op2 & 0x7); break;

					case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
						sprintf(s, "fmulp   st(%d)", op2 & 0x7); break;

					case 0x19: sprintf(s, "fcompp"); break;

					case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
						sprintf(s, "fsubrp  st(%d)", op2 & 0x7); break;

					case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
						sprintf(s, "fsubp   st(%d)", op2 & 0x7); break;

					case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
						sprintf(s, "fdivrp  st(%d), st(0)", op2 & 0x7); break;

					case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
						sprintf(s, "fdivp   st(%d)", op2 & 0x7); break;

					default: sprintf(s, "??? (FPU)"); break;
				}
			}
			break;
		}

		case 7:		// Group DF
		{
			if (op2 < 0xc0)
			{
				pc--;		// adjust fetch pointer, so modrm byte read again
				opcode_ptr--;
				handle_modrm( modrm_string );
				switch ((op2 >> 3) & 0x7)
				{
					case 0: sprintf(s, "fild    word ptr %s", modrm_string); break;
					case 1: sprintf(s, "fisttp  word ptr %s", modrm_string); break;
					case 2: sprintf(s, "fist    word ptr %s", modrm_string); break;
					case 3: sprintf(s, "fistp   word ptr %s", modrm_string); break;
					case 4: sprintf(s, "fbld    %s", modrm_string); break;
					case 5: sprintf(s, "fild    qword ptr %s", modrm_string); break;
					case 6: sprintf(s, "fbstp   %s", modrm_string); break;
					case 7: sprintf(s, "fistp   qword ptr %s", modrm_string); break;
				}
			}
			else
			{
				switch (op2 & 0x3f)
				{
					case 0x20: sprintf(s, "fstsw   ax"); break;

					case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
						sprintf(s, "fucomip st(%d)", op2 & 0x7); break;

					case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
						sprintf(s, "fcomip  st(%d),st(0)", op2 & 0x7); break;

					default: sprintf(s, "??? (FPU)"); break;
				}
			}
			break;
		}
	}
}

static void decode_opcode(char *s, const I386_OPCODE *op, UINT8 op1)
{
	int i;
	UINT8 op2;

	if ((op->flags & SPECIAL64) && (address_size == 2))
		op = &x64_opcode_alt[op->flags >> 24];

	switch( op->flags & FLAGS_MASK )
	{
		case ISREX:
			if (curmode == 64)
			{
				rex = op1;
				operand_size = (op1 & 8) ? 2 : 1;
				regex = (op1 << 1) & 8;
				sibex = (op1 << 2) & 8;
				rmex = (op1 << 3) & 8;
				op2 = FETCH();
				decode_opcode( s, &i386_opcode_table1[op2], op1 );
				return;
			}
			break;

		case OP_SIZE:
			rex = regex = sibex = rmex = 0;
			if (operand_size < 2 && operand_prefix == 0)
			{
				operand_size ^= 1;
				operand_prefix = 1;
			}
			op2 = FETCH();
			decode_opcode( s, &i386_opcode_table1[op2], op2 );
			return;

		case ADDR_SIZE:
			rex = regex = sibex = rmex = 0;
			if(address_prefix == 0)
			{
				if (curmode != 64)
					address_size ^= 1;
				else
					address_size ^= 3;
				address_prefix = 1;
			}
			op2 = FETCH();
			decode_opcode( s, &i386_opcode_table1[op2], op2 );
			return;

		case TWO_BYTE:
			if (&opcode_ptr[-2] >= opcode_ptr_base)
				pre0f = opcode_ptr[-2];
			op2 = FETCHD();
			decode_opcode( s, &i386_opcode_table2[op2], op1 );
			return;

		case SEG_CS:
		case SEG_DS:
		case SEG_ES:
		case SEG_FS:
		case SEG_GS:
		case SEG_SS:
			rex = regex = sibex = rmex = 0;
			segment = op->flags;
			op2 = FETCH();
			decode_opcode( s, &i386_opcode_table1[op2], op2 );
			return;

		case PREFIX:
			op2 = FETCH();
			if (op2 != 0x0f)
				s += sprintf( s, "%-7s ", op->mnemonic );
			decode_opcode( s, &i386_opcode_table1[op2], op2 );
			return;

		case GROUP:
			handle_modrm( modrm_string );
			for( i=0; i < ARRAY_LENGTH(group_op_table); i++ ) {
				if( strcmp(op->mnemonic, group_op_table[i].mnemonic) == 0 ) {
					decode_opcode( s, &group_op_table[i].opcode[MODRM_REG1], op1 );
					return;
				}
			}
			goto handle_unknown;

		case FPU:
			op2 = FETCHD();
			handle_fpu( s, op1, op2);
			return;

		case MODRM:
			handle_modrm( modrm_string );
			break;
	}

	if ((op->flags & ALWAYS64) && curmode == 64)
		operand_size = 2;

	if ((op->flags & VAR_NAME) && operand_size > 0)
	{
		const char *mnemonic = op->mnemonic + strlen(op->mnemonic) + 1;
		if (operand_size == 2)
			mnemonic += strlen(mnemonic) + 1;
		s += sprintf( s, "%-7s ", mnemonic );
	}
	else if (op->flags & VAR_NAME4)
	{
		const char *mnemonic = op->mnemonic;
		int which = (pre0f == 0xf3) ? 3 : (pre0f == 0xf2) ? 2 : (pre0f == 0x66) ? 1 : 0;
		while (which--)
			mnemonic += strlen(mnemonic) + 1;
		s += sprintf( s, "%-7s ", mnemonic );
	}
	else
		s += sprintf( s, "%-7s ", op->mnemonic );
	dasm_flags = op->dasm_flags;

	if( op->param1 != 0 ) {
		s = handle_param( s, op->param1 );
	}

	if( op->param2 != 0 ) {
		s += sprintf( s, "," );
		s = handle_param( s, op->param2 );
	}

	if( op->param3 != 0 ) {
		s += sprintf( s, "," );
		s = handle_param( s, op->param3 );
	}
	return;

handle_unknown:
	sprintf(s, "???");
}

int i386_dasm_one_ex(char *buffer, UINT64 eip, const UINT8 *oprom, int mode)
{
	UINT8 op;

	opcode_ptr = opcode_ptr_base = oprom;
	switch(mode)
	{
		case 1: /* 8086/8088/80186/80188 */
			address_size = 0;
			operand_size = 0;
			max_length = 8;	/* maximum without redundant prefixes - not enforced by chip */
			break;
		case 2: /* 80286 */
			address_size = 0;
			operand_size = 0;
			max_length = 10;
			break;
		case 16: /* 80386+ 16-bit code segment */
			address_size = 0;
			operand_size = 0;
			max_length = 15;
			break;
		case 32: /* 80386+ 32-bit code segment */
			address_size = 1;
			operand_size = 1;
			max_length = 15;
			break;
		case 64: /* x86_64 */
			address_size = 2;
			operand_size = 1;
			max_length = 15;
			break;
	}
	pc = eip;
	dasm_flags = 0;
	segment = 0;
	curmode = mode;
	pre0f = 0;
	rex = regex = sibex = rmex = 0;
	address_prefix = 0;
	operand_prefix = 0;

	op = FETCH();

	decode_opcode( buffer, &i386_opcode_table1[op], op );
	return (pc-eip) | dasm_flags | DASMFLAG_SUPPORTED;
}

int i386_dasm_one(char *buffer, offs_t eip, const UINT8 *oprom, int mode)
{
	return i386_dasm_one_ex(buffer, eip, oprom, mode);
}

CPU_DISASSEMBLE( x86_16 )
{
	return i386_dasm_one_ex(buffer, pc, oprom, 16);
}

CPU_DISASSEMBLE( x86_32 )
{
	return i386_dasm_one_ex(buffer, pc, oprom, 32);
}

CPU_DISASSEMBLE( x86_64 )
{
	return i386_dasm_one_ex(buffer, pc, oprom, 64);
}
