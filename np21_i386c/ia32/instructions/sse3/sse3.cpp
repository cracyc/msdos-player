/*
 * Copyright (c) 2018 SimK
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//#include "compiler.h"

#include <math.h>
#include <float.h>

#define isnan(x) (_isnan(x))

#include "../../cpu.h"
#include "../../ia32.mcr"

#include "../sse/sse.h"
#include "../sse2/sse2.h"
#include "sse3.h"

#if defined(USE_SSE3) && defined(USE_SSE2) && defined(USE_SSE) && defined(USE_FPU)

#define CPU_SSE3WORKCLOCK	CPU_WORKCLOCK(8)

static INLINE void
SSE3_check_NM_EXCEPTION(){
	// SSE3�Ȃ��Ȃ�UD(�����I�y�R�[�h��O)�𔭐�������
	if(!(i386cpuid.cpu_feature_ecx & CPU_FEATURE_ECX_SSE3)){
		EXCEPTION(UD_EXCEPTION, 0);
	}
	// �G�~�����[�V�����Ȃ�UD(�����I�y�R�[�h��O)�𔭐�������
	if(CPU_CR0 & CPU_CR0_EM){
		EXCEPTION(UD_EXCEPTION, 0);
	}
	// �^�X�N�X�C�b�`����NM(�f�o�C�X�g�p�s��O)�𔭐�������
	if (CPU_CR0 & CPU_CR0_TS) {
		EXCEPTION(NM_EXCEPTION, 0);
	}
}

static INLINE void
SSE3_setTag(void)
{
}

// mmx.c�̂��̂Ɠ���
static INLINE void
MMX_setTag(void)
{
	int i;
	
	if(!FPU_STAT.mmxenable){
		FPU_STAT.mmxenable = 1;
		//FPU_CTRLWORD = 0x27F;
		for (i = 0; i < FPU_REG_NUM; i++) {
			FPU_STAT.tag[i] = TAG_Valid;
#ifdef SUPPORT_FPU_DOSBOX2
			FPU_STAT.int_regvalid[i] = 0;
#endif
			FPU_STAT.reg[i].ul.ext = 0xffff;
		}
	}
	FPU_STAT_TOP = 0;
	FPU_STATUSWORD &= ~0x3800;
	FPU_STATUSWORD |= (FPU_STAT_TOP&7)<<11;
}

/*
 * SSE3 interface
 */

// �R�[�h�������Ȃ�̂ł�⋭���ɋ��ʉ�
// xmm/m128 -> xmm
static INLINE void SSE_PART_GETDATA1DATA2_PD(double **data1, double **data2, double *data2buf){
	UINT32 op;
	UINT idx, sub;
	
	SSE3_check_NM_EXCEPTION();
	SSE3_setTag();
	CPU_SSE3WORKCLOCK;
	GET_PCBYTE((op));
	idx = (op >> 3) & 7;
	sub = (op & 7);
	*data1 = (double*)(&(FPU_STAT.xmm_reg[idx]));
	if ((op) >= 0xc0) {
		*data2 = (double*)(&(FPU_STAT.xmm_reg[sub]));
	} else {
		UINT32 maddr;
		maddr = calc_ea_dst((op));
		*((UINT64*)(data2buf+ 0)) = cpu_vmemoryread_q(CPU_INST_SEGREG_INDEX, maddr+ 0);
		*((UINT64*)(data2buf+ 1)) = cpu_vmemoryread_q(CPU_INST_SEGREG_INDEX, maddr+ 8);
		*data2 = data2buf;
	}
}

void SSE3_ADDSUBPD(void)
{
	double data2buf[2];
	double *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD(&data1, &data2, data2buf);
	data1[0] = data1[0] - data2[0];
	data1[1] = data1[1] + data2[1];
}
void SSE3_ADDSUBPS(void)
{
	float data2buf[4];
	float *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD((double**)(&data1), (double**)(&data2), (double*)data2buf);
	data1[0] = data1[0] - data2[0];
	data1[1] = data1[1] + data2[1];
	data1[2] = data1[2] - data2[2];
	data1[3] = data1[3] + data2[3];
}
void SSE3_HADDPD(void)
{
	double data2buf[2];
	double *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD(&data1, &data2, data2buf);
	data1[0] = data1[0] + data1[1];
	data1[1] = data2[0] + data2[1];
}
void SSE3_HADDPS(void)
{
	float data2buf[4];
	float *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD((double**)(&data1), (double**)(&data2), (double*)data2buf);
	data1[0] = data1[0] + data1[1];
	data1[1] = data1[2] + data1[3];
	data1[2] = data2[0] + data2[1];
	data1[3] = data2[2] + data2[3];
}
void SSE3_HSUBPD(void)
{
	double data2buf[2];
	double *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD(&data1, &data2, data2buf);
	data1[0] = data1[0] - data1[1];
	data1[1] = data2[0] - data2[1];
}
void SSE3_HSUBPS(void)
{
	float data2buf[4];
	float *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD((double**)(&data1), (double**)(&data2), (double*)data2buf);
	data1[0] = data1[0] - data1[1];
	data1[1] = data1[2] - data1[3];
	data1[2] = data2[0] - data2[1];
	data1[3] = data2[2] - data2[3];
}

void SSE3_MONITOR(void)
{
	EXCEPTION(UD_EXCEPTION, 0); // ������
}
void SSE3_MWAIT(void)
{
	EXCEPTION(UD_EXCEPTION, 0); // ������
}

//void SSE3_FISTTP(void)
//{
//	EXCEPTION(UD_EXCEPTION, 0);
//}
void SSE3_LDDQU(void)
{
	SSE2_MOVDQAmem2xmm(); // �����ɈႤ���ǂ������ȁE�E�E
}
void SSE3_MOVDDUP(void)
{
	UINT32 op;
	UINT idx, sub;
	
	SSE3_check_NM_EXCEPTION();
	SSE3_setTag();
	CPU_SSE3WORKCLOCK;
	GET_PCBYTE((op));
	idx = (op >> 3) & 7;
	sub = (op & 7);
	if ((op) >= 0xc0) {
		FPU_STAT.xmm_reg[idx].ul64[0] = FPU_STAT.xmm_reg[sub].ul64[0];
	} else {
		UINT32 madr;
		madr = calc_ea_dst(op);
		FPU_STAT.xmm_reg[idx].ul64[0] = cpu_vmemoryread_q(CPU_INST_SEGREG_INDEX, madr);
	}
	FPU_STAT.xmm_reg[idx].ul64[1] = FPU_STAT.xmm_reg[idx].ul64[0];
}
void SSE3_MOVSHDUP(void)
{
	float data2buf[4];
	float *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD((double**)(&data1), (double**)(&data2), (double*)data2buf);
	data1[0] = data2[1];
	data1[1] = data2[1];
	data1[2] = data2[3];
	data1[3] = data2[3];
}
void SSE3_MOVSLDUP(void)
{
	float data2buf[4];
	float *data1, *data2;
	
	SSE_PART_GETDATA1DATA2_PD((double**)(&data1), (double**)(&data2), (double*)data2buf);
	data1[0] = data2[0];
	data1[1] = data2[0];
	data1[2] = data2[2];
	data1[3] = data2[2];
}

#else

/*
 * SSE3 interface
 */

void SSE3_ADDSUBPD(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_ADDSUBPS(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_HADDPD(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_HADDPS(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_HSUBPD(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_HSUBPS(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}

void SSE3_MONITOR(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_MWAIT(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}

void SSE3_FISTTP(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_LDDQU(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_MOVDDUP(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_MOVSHDUP(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}
void SSE3_MOVSLDUP(void)
{
	EXCEPTION(UD_EXCEPTION, 0);
}

#endif