/*$Id: baijfact12.c,v 1.17 2001/08/31 16:22:11 bsmith Exp $*/
/*
    Factorization code for BAIJ format. 
*/
#include "src/mat/impls/baij/seq/baij.h"
#include "src/vec/vecimpl.h"
#include "src/inline/ilu.h"

#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering"
int MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering(Mat A,Mat *B)
{
/*
    Default Version for when blocks are 4 by 4 Using natural ordering
*/
  Mat         C = *B;
  Mat_SeqBAIJ *a = (Mat_SeqBAIJ*)A->data,*b = (Mat_SeqBAIJ*)C->data;
  int         ierr,i,j,n = a->mbs,*bi = b->i,*bj = b->j;
  int         *ajtmpold,*ajtmp,nz,row;
  int         *diag_offset = b->diag,*ai=a->i,*aj=a->j,*pj;
  MatScalar   *pv,*v,*rtmp,*pc,*w,*x;
  MatScalar   p1,p2,p3,p4,m1,m2,m3,m4,m5,m6,m7,m8,m9,x1,x2,x3,x4;
  MatScalar   p5,p6,p7,p8,p9,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15,x16;
  MatScalar   p10,p11,p12,p13,p14,p15,p16,m10,m11,m12;
  MatScalar   m13,m14,m15,m16;
  MatScalar   *ba = b->a,*aa = a->a;
  PetscTruth  pivotinblocks = b->pivotinblocks;

  PetscFunctionBegin;
  ierr = PetscMalloc(16*(n+1)*sizeof(MatScalar),&rtmp);CHKERRQ(ierr);

  for (i=0; i<n; i++) {
    nz    = bi[i+1] - bi[i];
    ajtmp = bj + bi[i];
    for  (j=0; j<nz; j++) {
      x = rtmp+16*ajtmp[j]; 
      x[0]  = x[1]  = x[2]  = x[3]  = x[4]  = x[5]  = x[6] = x[7] = x[8] = x[9] = 0.0;
      x[10] = x[11] = x[12] = x[13] = x[14] = x[15] = 0.0;
    }
    /* load in initial (unfactored row) */
    nz       = ai[i+1] - ai[i];
    ajtmpold = aj + ai[i];
    v        = aa + 16*ai[i];
    for (j=0; j<nz; j++) {
      x    = rtmp+16*ajtmpold[j];
      x[0]  = v[0];  x[1]  = v[1];  x[2]  = v[2];  x[3]  = v[3];
      x[4]  = v[4];  x[5]  = v[5];  x[6]  = v[6];  x[7]  = v[7];  x[8]  = v[8];
      x[9]  = v[9];  x[10] = v[10]; x[11] = v[11]; x[12] = v[12]; x[13] = v[13];
      x[14] = v[14]; x[15] = v[15]; 
      v    += 16;
    }
    row = *ajtmp++;
    while (row < i) {
      pc  = rtmp + 16*row;
      p1  = pc[0];  p2  = pc[1];  p3  = pc[2];  p4  = pc[3];
      p5  = pc[4];  p6  = pc[5];  p7  = pc[6];  p8  = pc[7];  p9  = pc[8];
      p10 = pc[9];  p11 = pc[10]; p12 = pc[11]; p13 = pc[12]; p14 = pc[13];
      p15 = pc[14]; p16 = pc[15]; 
      if (p1 != 0.0 || p2 != 0.0 || p3 != 0.0 || p4 != 0.0 || p5 != 0.0 ||
          p6 != 0.0 || p7 != 0.0 || p8 != 0.0 || p9 != 0.0 || p10 != 0.0 ||
          p11 != 0.0 || p12 != 0.0 || p13 != 0.0 || p14 != 0.0 || p15 != 0.0
          || p16 != 0.0) {
        pv = ba + 16*diag_offset[row];
        pj = bj + diag_offset[row] + 1;
        x1  = pv[0];  x2  = pv[1];  x3  = pv[2];  x4  = pv[3];
        x5  = pv[4];  x6  = pv[5];  x7  = pv[6];  x8  = pv[7];  x9  = pv[8];
        x10 = pv[9];  x11 = pv[10]; x12 = pv[11]; x13 = pv[12]; x14 = pv[13];
        x15 = pv[14]; x16 = pv[15]; 
        pc[0] = m1 = p1*x1 + p5*x2  + p9*x3  + p13*x4;
        pc[1] = m2 = p2*x1 + p6*x2  + p10*x3 + p14*x4;
        pc[2] = m3 = p3*x1 + p7*x2  + p11*x3 + p15*x4;
        pc[3] = m4 = p4*x1 + p8*x2  + p12*x3 + p16*x4;

        pc[4] = m5 = p1*x5 + p5*x6  + p9*x7  + p13*x8;
        pc[5] = m6 = p2*x5 + p6*x6  + p10*x7 + p14*x8;
        pc[6] = m7 = p3*x5 + p7*x6  + p11*x7 + p15*x8;
        pc[7] = m8 = p4*x5 + p8*x6  + p12*x7 + p16*x8;

        pc[8]  = m9  = p1*x9 + p5*x10  + p9*x11  + p13*x12;
        pc[9]  = m10 = p2*x9 + p6*x10  + p10*x11 + p14*x12;
        pc[10] = m11 = p3*x9 + p7*x10  + p11*x11 + p15*x12;
        pc[11] = m12 = p4*x9 + p8*x10  + p12*x11 + p16*x12;

        pc[12] = m13 = p1*x13 + p5*x14  + p9*x15  + p13*x16;
        pc[13] = m14 = p2*x13 + p6*x14  + p10*x15 + p14*x16;
        pc[14] = m15 = p3*x13 + p7*x14  + p11*x15 + p15*x16;
        pc[15] = m16 = p4*x13 + p8*x14  + p12*x15 + p16*x16;
        nz = bi[row+1] - diag_offset[row] - 1;
        pv += 16;
        for (j=0; j<nz; j++) {
          x1   = pv[0];  x2  = pv[1];   x3 = pv[2];  x4  = pv[3];
          x5   = pv[4];  x6  = pv[5];   x7 = pv[6];  x8  = pv[7]; x9 = pv[8];
          x10  = pv[9];  x11 = pv[10]; x12 = pv[11]; x13 = pv[12];
          x14  = pv[13]; x15 = pv[14]; x16 = pv[15];
          x    = rtmp + 16*pj[j];
          x[0] -= m1*x1 + m5*x2  + m9*x3  + m13*x4;
          x[1] -= m2*x1 + m6*x2  + m10*x3 + m14*x4;
          x[2] -= m3*x1 + m7*x2  + m11*x3 + m15*x4;
          x[3] -= m4*x1 + m8*x2  + m12*x3 + m16*x4;

          x[4] -= m1*x5 + m5*x6  + m9*x7  + m13*x8;
          x[5] -= m2*x5 + m6*x6  + m10*x7 + m14*x8;
          x[6] -= m3*x5 + m7*x6  + m11*x7 + m15*x8;
          x[7] -= m4*x5 + m8*x6  + m12*x7 + m16*x8;

          x[8]  -= m1*x9 + m5*x10 + m9*x11  + m13*x12;
          x[9]  -= m2*x9 + m6*x10 + m10*x11 + m14*x12;
          x[10] -= m3*x9 + m7*x10 + m11*x11 + m15*x12;
          x[11] -= m4*x9 + m8*x10 + m12*x11 + m16*x12;

          x[12] -= m1*x13 + m5*x14  + m9*x15  + m13*x16;
          x[13] -= m2*x13 + m6*x14  + m10*x15 + m14*x16;
          x[14] -= m3*x13 + m7*x14  + m11*x15 + m15*x16;
          x[15] -= m4*x13 + m8*x14  + m12*x15 + m16*x16;

          pv   += 16;
        }
        PetscLogFlops(128*nz+112);
      } 
      row = *ajtmp++;
    }
    /* finished row so stick it into b->a */
    pv = ba + 16*bi[i];
    pj = bj + bi[i];
    nz = bi[i+1] - bi[i];
    for (j=0; j<nz; j++) {
      x      = rtmp+16*pj[j];
      pv[0]  = x[0];  pv[1]  = x[1];  pv[2]  = x[2];  pv[3]  = x[3];
      pv[4]  = x[4];  pv[5]  = x[5];  pv[6]  = x[6];  pv[7]  = x[7]; pv[8] = x[8];
      pv[9]  = x[9];  pv[10] = x[10]; pv[11] = x[11]; pv[12] = x[12];
      pv[13] = x[13]; pv[14] = x[14]; pv[15] = x[15];
      pv   += 16;
    }
    /* invert diagonal block */
    w = ba + 16*diag_offset[i];
    if (pivotinblocks) {
      ierr = Kernel_A_gets_inverse_A_4(w);CHKERRQ(ierr);
    } else {
      ierr = Kernel_A_gets_inverse_A_4_nopivot(w);CHKERRQ(ierr);
    }
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  C->factor    = FACTOR_LU;
  C->assembled = PETSC_TRUE;
  PetscLogFlops(1.3333*64*b->mbs); /* from inverting diagonal blocks */
  PetscFunctionReturn(0);
}


#if defined(PETSC_HAVE_SSE)

#include PETSC_HAVE_SSE

/* SSE Version for when blocks are 4 by 4 Using natural ordering */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering_SSE"
int MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering_SSE(Mat A,Mat *B)
{
  Mat         C = *B;
  Mat_SeqBAIJ *a = (Mat_SeqBAIJ*)A->data,*b = (Mat_SeqBAIJ*)C->data;
  int         ierr,i,j,n = a->mbs,*bi = b->i,*bj = b->j;
  int         *ajtmpold,*ajtmp,nz,row;
  int         *diag_offset = b->diag,*ai=a->i,*aj=a->j,*pj;
  MatScalar   *pv,*v,*rtmp,*pc,*w,*x;
  MatScalar   *ba = b->a,*aa = a->a;
  int nonzero=0;

  PetscFunctionBegin;
  SSE_SCOPE_BEGIN;

  ierr = PetscMalloc(16*(n+1)*sizeof(MatScalar),&rtmp);CHKERRQ(ierr);
  if (((int) (rtmp)) % 16) SETERRQ(1,"rtmp is misaligned in MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering_SSE");
  if (((int) (ba)) % 16) SETERRQ(1,"ba is misaligned in MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering_SSE");
  if (((int) (aa)) % 16) SETERRQ(1,"aa is misaligned in MatLUFactorNumeric_SeqBAIJ_4_NaturalOrdering_SSE");
  for (i=0; i<n; i++) {
    nz    = bi[i+1] - bi[i];
    ajtmp = bj + bi[i];
    /* zero out the 4x4 block accumulators */
    /* zero out one register */ 
    XOR_PS(XMM7,XMM7);
    for  (j=0; j<nz; j++) {
      x = rtmp+16*ajtmp[j];
      SSE_INLINE_BEGIN_1(x)
        /* Copy zero register to memory locations */
        /* Note: on future SSE architectures, STORE might be more efficient than STOREL/H */
        SSE_STOREL_PS(SSE_ARG_1,FLOAT_0,XMM7)
        SSE_STOREH_PS(SSE_ARG_1,FLOAT_2,XMM7)
        SSE_STOREL_PS(SSE_ARG_1,FLOAT_4,XMM7)
        SSE_STOREH_PS(SSE_ARG_1,FLOAT_6,XMM7)
        SSE_STOREL_PS(SSE_ARG_1,FLOAT_8,XMM7)
        SSE_STOREH_PS(SSE_ARG_1,FLOAT_10,XMM7)
        SSE_STOREL_PS(SSE_ARG_1,FLOAT_12,XMM7)
        SSE_STOREH_PS(SSE_ARG_1,FLOAT_14,XMM7)
      SSE_INLINE_END_1;
    }
    /* load in initial (unfactored row) */
    nz       = ai[i+1] - ai[i];
    ajtmpold = aj + ai[i];
    v        = aa + 16*ai[i];
    for (j=0; j<nz; j++) {
      x = rtmp+16*ajtmpold[j];
      /* Copy v block into x block */ 
      SSE_INLINE_BEGIN_2(v,x)
        /* Note: on future SSE architectures, STORE might be more efficient than STOREL/H */
        SSE_LOADL_PS(SSE_ARG_1,FLOAT_0,XMM0)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_0,XMM0)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_2,XMM1)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_2,XMM1)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_4,XMM2)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_4,XMM2)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_6,XMM3)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_6,XMM3)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_8,XMM4)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_8,XMM4)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_10,XMM5)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_10,XMM5)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_12,XMM6)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_12,XMM6)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_14,XMM0)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_14,XMM0)
      SSE_INLINE_END_2;

      v += 16;
    }
    row = *ajtmp++;
    while (row < i) {
      pc  = rtmp + 16*row;
      SSE_INLINE_BEGIN_1(pc)
        /* Load block from lower triangle */ 
        /* Note: on future SSE architectures, STORE might be more efficient than STOREL/H */
        SSE_LOADL_PS(SSE_ARG_1,FLOAT_0,XMM0)
        SSE_LOADH_PS(SSE_ARG_1,FLOAT_2,XMM0)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_4,XMM1)
        SSE_LOADH_PS(SSE_ARG_1,FLOAT_6,XMM1)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_8,XMM2)
        SSE_LOADH_PS(SSE_ARG_1,FLOAT_10,XMM2)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_12,XMM3)
        SSE_LOADH_PS(SSE_ARG_1,FLOAT_14,XMM3)

        /* Compare block to zero block */ 

        SSE_COPY_PS(XMM4,XMM7)
        SSE_CMPNEQ_PS(XMM4,XMM0)

        SSE_COPY_PS(XMM5,XMM7)
        SSE_CMPNEQ_PS(XMM5,XMM1)

        SSE_COPY_PS(XMM6,XMM7)
        SSE_CMPNEQ_PS(XMM6,XMM2)

        SSE_CMPNEQ_PS(XMM7,XMM3)

        /* Reduce the comparisons to one SSE register */
        SSE_OR_PS(XMM6,XMM7)
        SSE_OR_PS(XMM5,XMM4)
        SSE_OR_PS(XMM5,XMM6)
      SSE_INLINE_END_1;

      /* Reduce the one SSE register to an integer register for branching */
      /* Note: Since nonzero is an int, there is no INLINE block version of this call */
      MOVEMASK(nonzero,XMM5);

      /* If block is nonzero ... */
      if (nonzero) {
        pv = ba + 16*diag_offset[row];
        PREFETCH_L1(&pv[16]);
        pj = bj + diag_offset[row] + 1;

        /* Form Multiplier, one column at a time (Matrix-Matrix Product) */
        /* L_ij^(k+1) = L_ij^(k)*inv(L_jj^(k)) */ 
        /* but the diagonal was inverted already */
        /* and, L_ij^(k) is already loaded into registers XMM0-XMM3 columnwise */

        SSE_INLINE_BEGIN_2(pv,pc)
          /* Column 0, product is accumulated in XMM4 */ 
          SSE_LOAD_SS(SSE_ARG_1,FLOAT_0,XMM4)
          SSE_SHUFFLE(XMM4,XMM4,0x00)
          SSE_MULT_PS(XMM4,XMM0)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_1,XMM5)
          SSE_SHUFFLE(XMM5,XMM5,0x00)
          SSE_MULT_PS(XMM5,XMM1)
          SSE_ADD_PS(XMM4,XMM5)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_2,XMM6)
          SSE_SHUFFLE(XMM6,XMM6,0x00)
          SSE_MULT_PS(XMM6,XMM2)
          SSE_ADD_PS(XMM4,XMM6)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_3,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM7,XMM3)
          SSE_ADD_PS(XMM4,XMM7)

          SSE_STOREL_PS(SSE_ARG_2,FLOAT_0,XMM4)
          SSE_STOREH_PS(SSE_ARG_2,FLOAT_2,XMM4)

          /* Column 1, product is accumulated in XMM5 */
          SSE_LOAD_SS(SSE_ARG_1,FLOAT_4,XMM5)
          SSE_SHUFFLE(XMM5,XMM5,0x00)
          SSE_MULT_PS(XMM5,XMM0)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_5,XMM6)
          SSE_SHUFFLE(XMM6,XMM6,0x00)
          SSE_MULT_PS(XMM6,XMM1)
          SSE_ADD_PS(XMM5,XMM6)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_6,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM7,XMM2)
          SSE_ADD_PS(XMM5,XMM7)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_7,XMM6)
          SSE_SHUFFLE(XMM6,XMM6,0x00)
          SSE_MULT_PS(XMM6,XMM3)
          SSE_ADD_PS(XMM5,XMM6)

          SSE_STOREL_PS(SSE_ARG_2,FLOAT_4,XMM5)
          SSE_STOREH_PS(SSE_ARG_2,FLOAT_6,XMM5)

          SSE_PREFETCH_L1(SSE_ARG_1,FLOAT_24)

          /* Column 2, product is accumulated in XMM6 */
          SSE_LOAD_SS(SSE_ARG_1,FLOAT_8,XMM6)
          SSE_SHUFFLE(XMM6,XMM6,0x00)
          SSE_MULT_PS(XMM6,XMM0)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_9,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM7,XMM1)
          SSE_ADD_PS(XMM6,XMM7)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_10,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM7,XMM2)
          SSE_ADD_PS(XMM6,XMM7)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_11,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM7,XMM3)
          SSE_ADD_PS(XMM6,XMM7)
  
          SSE_STOREL_PS(SSE_ARG_2,FLOAT_8,XMM6)
          SSE_STOREH_PS(SSE_ARG_2,FLOAT_10,XMM6)

          /* Note: For the last column, we no longer need to preserve XMM0->XMM3 */
          /* Column 3, product is accumulated in XMM0 */
          SSE_LOAD_SS(SSE_ARG_1,FLOAT_12,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM0,XMM7)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_13,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM1,XMM7)
          SSE_ADD_PS(XMM0,XMM1)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_14,XMM1)
          SSE_SHUFFLE(XMM1,XMM1,0x00)
          SSE_MULT_PS(XMM1,XMM2)
          SSE_ADD_PS(XMM0,XMM1)

          SSE_LOAD_SS(SSE_ARG_1,FLOAT_15,XMM7)
          SSE_SHUFFLE(XMM7,XMM7,0x00)
          SSE_MULT_PS(XMM3,XMM7)
          SSE_ADD_PS(XMM0,XMM3)

          SSE_STOREL_PS(SSE_ARG_2,FLOAT_12,XMM0)
          SSE_STOREH_PS(SSE_ARG_2,FLOAT_14,XMM0)

          /* Simplify Bookkeeping -- Completely Unnecessary Instructions */
          /* This is code to be maintained and read by humans afterall. */
          /* Copy Multiplier Col 3 into XMM3 */
          SSE_COPY_PS(XMM3,XMM0) 
          /* Copy Multiplier Col 2 into XMM2 */
          SSE_COPY_PS(XMM2,XMM6)
          /* Copy Multiplier Col 1 into XMM1 */
          SSE_COPY_PS(XMM1,XMM5)
          /* Copy Multiplier Col 0 into XMM0 */
          SSE_COPY_PS(XMM0,XMM4)
        SSE_INLINE_END_2;

        /* Update the row: */
        nz = bi[row+1] - diag_offset[row] - 1;
        pv += 16;
        for (j=0; j<nz; j++) {
          PREFETCH_L1(&pv[16]);
          x = rtmp + 16*pj[j];

          /* X:=X-M*PV, One column at a time */
          /* Note: M is already loaded columnwise into registers XMM0-XMM3 */
          SSE_INLINE_BEGIN_2(x,pv)
            /* Load First Column of X*/ 
            SSE_LOADL_PS(SSE_ARG_1,FLOAT_0,XMM4)
            SSE_LOADH_PS(SSE_ARG_1,FLOAT_2,XMM4)

            /* Matrix-Vector Product: */
            SSE_LOAD_SS(SSE_ARG_2,FLOAT_0,XMM5)
            SSE_SHUFFLE(XMM5,XMM5,0x00)
            SSE_MULT_PS(XMM5,XMM0)
            SSE_SUB_PS(XMM4,XMM5)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_1,XMM6)
            SSE_SHUFFLE(XMM6,XMM6,0x00)
            SSE_MULT_PS(XMM6,XMM1)
            SSE_SUB_PS(XMM4,XMM6)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_2,XMM7)
            SSE_SHUFFLE(XMM7,XMM7,0x00)
            SSE_MULT_PS(XMM7,XMM2)
            SSE_SUB_PS(XMM4,XMM7)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_3,XMM5)
            SSE_SHUFFLE(XMM5,XMM5,0x00)
            SSE_MULT_PS(XMM5,XMM3)
            SSE_SUB_PS(XMM4,XMM5)

            SSE_STOREL_PS(SSE_ARG_1,FLOAT_0,XMM4)
            SSE_STOREH_PS(SSE_ARG_1,FLOAT_2,XMM4)

            /* Second Column */
            SSE_LOADL_PS(SSE_ARG_1,FLOAT_4,XMM5)
            SSE_LOADH_PS(SSE_ARG_1,FLOAT_6,XMM5)          

            /* Matrix-Vector Product: */
            SSE_LOAD_SS(SSE_ARG_2,FLOAT_4,XMM6)
            SSE_SHUFFLE(XMM6,XMM6,0x00)
            SSE_MULT_PS(XMM6,XMM0)
            SSE_SUB_PS(XMM5,XMM6)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_5,XMM7)
            SSE_SHUFFLE(XMM7,XMM7,0x00)
            SSE_MULT_PS(XMM7,XMM1)
            SSE_SUB_PS(XMM5,XMM7)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_6,XMM4)
            SSE_SHUFFLE(XMM4,XMM4,0x00)
            SSE_MULT_PS(XMM4,XMM2)
            SSE_SUB_PS(XMM5,XMM4)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_7,XMM6)
            SSE_SHUFFLE(XMM6,XMM6,0x00)
            SSE_MULT_PS(XMM6,XMM3)
            SSE_SUB_PS(XMM5,XMM6)
          
            SSE_STOREL_PS(SSE_ARG_1,FLOAT_4,XMM5)
            SSE_STOREH_PS(SSE_ARG_1,FLOAT_6,XMM5)

            SSE_PREFETCH_L1(SSE_ARG_2,FLOAT_24)

            /* Third Column */
            SSE_LOADL_PS(SSE_ARG_1,FLOAT_8,XMM6)
            SSE_LOADH_PS(SSE_ARG_1,FLOAT_10,XMM6)

            /* Matrix-Vector Product: */
            SSE_LOAD_SS(SSE_ARG_2,FLOAT_8,XMM7)
            SSE_SHUFFLE(XMM7,XMM7,0x00)
            SSE_MULT_PS(XMM7,XMM0)
            SSE_SUB_PS(XMM6,XMM7)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_9,XMM4)
            SSE_SHUFFLE(XMM4,XMM4,0x00)
            SSE_MULT_PS(XMM4,XMM1)
            SSE_SUB_PS(XMM6,XMM4)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_10,XMM5)
            SSE_SHUFFLE(XMM5,XMM5,0x00)
            SSE_MULT_PS(XMM5,XMM2)
            SSE_SUB_PS(XMM6,XMM5)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_11,XMM7)
            SSE_SHUFFLE(XMM7,XMM7,0x00)
            SSE_MULT_PS(XMM7,XMM3)
            SSE_SUB_PS(XMM6,XMM7)
          
            SSE_STOREL_PS(SSE_ARG_1,FLOAT_8,XMM6)
            SSE_STOREH_PS(SSE_ARG_1,FLOAT_10,XMM6)
          
            /* Fourth Column */
            SSE_LOADL_PS(SSE_ARG_1,FLOAT_12,XMM4)
            SSE_LOADH_PS(SSE_ARG_1,FLOAT_14,XMM4)

            /* Matrix-Vector Product: */
            SSE_LOAD_SS(SSE_ARG_2,FLOAT_12,XMM5)
            SSE_SHUFFLE(XMM5,XMM5,0x00)
            SSE_MULT_PS(XMM5,XMM0)
            SSE_SUB_PS(XMM4,XMM5)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_13,XMM6)
            SSE_SHUFFLE(XMM6,XMM6,0x00)
            SSE_MULT_PS(XMM6,XMM1)
            SSE_SUB_PS(XMM4,XMM6)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_14,XMM7)
            SSE_SHUFFLE(XMM7,XMM7,0x00)
            SSE_MULT_PS(XMM7,XMM2)
            SSE_SUB_PS(XMM4,XMM7)

            SSE_LOAD_SS(SSE_ARG_2,FLOAT_15,XMM5)
            SSE_SHUFFLE(XMM5,XMM5,0x00)
            SSE_MULT_PS(XMM5,XMM3)
            SSE_SUB_PS(XMM4,XMM5)
          
            SSE_STOREL_PS(SSE_ARG_1,FLOAT_12,XMM4)
            SSE_STOREH_PS(SSE_ARG_1,FLOAT_14,XMM4)
          SSE_INLINE_END_2;
          pv   += 16;
        }
        PetscLogFlops(128*nz+112);
      } 
      row = *ajtmp++;
    }
    /* finished row so stick it into b->a */
    pv = ba + 16*bi[i];
    pj = bj + bi[i];
    nz = bi[i+1] - bi[i];

    /* Copy x block back into pv block */
    for (j=0; j<nz; j++) {
      x  = rtmp+16*pj[j];

      SSE_INLINE_BEGIN_2(x,pv)
        /* Note: on future SSE architectures, STORE might be more efficient than STOREL/H */
        SSE_LOADL_PS(SSE_ARG_1,FLOAT_0,XMM1)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_0,XMM1)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_2,XMM2)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_2,XMM2)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_4,XMM3)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_4,XMM3)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_6,XMM4)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_6,XMM4)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_8,XMM5)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_8,XMM5)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_10,XMM6)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_10,XMM6)

        SSE_LOADL_PS(SSE_ARG_1,FLOAT_12,XMM7)
        SSE_STOREL_PS(SSE_ARG_2,FLOAT_12,XMM7)

        SSE_LOADH_PS(SSE_ARG_1,FLOAT_14,XMM0)
        SSE_STOREH_PS(SSE_ARG_2,FLOAT_14,XMM0)
      SSE_INLINE_END_2;
      pv += 16;
    }
    /* invert diagonal block */
    w = ba + 16*diag_offset[i];
    ierr = Kernel_A_gets_inverse_A_4_SSE(w);CHKERRQ(ierr);
    /* Note: Using Kramer's rule, flop count below might be infairly high or low? */ 
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  C->factor    = FACTOR_LU;
  C->assembled = PETSC_TRUE;
  PetscLogFlops(1.3333*64*b->mbs);
  /* Flop Count from inverting diagonal blocks */ 
  SSE_SCOPE_END;
  PetscFunctionReturn(0);
}

#endif
