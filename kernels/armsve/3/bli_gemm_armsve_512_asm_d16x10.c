    /* 

   BLIS
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2014, The University of Texas at Austin

   SVE enablement was done by Guodong Xu, www.linaro.org
   Copyright (C) 2020, Linaro Ltd.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name(s) of the copyright holder(s) nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


*/

#include "blis.h"

/*
   o 16x12 Double precision micro-kernel
   o Runnable on ARMv8a with SVE 512 (vector length) feature, compiled with
   o   aarch64 GCC v10.0.
   o Tested on qemu-aarch64 and armie for SVE.

   Preconditions:
    - to use this kernel, SVE with vector length of 512 bits is a must.

   May 2020.
*/
void bli_dgemm_armsve512_asm_16x12
     (
       dim_t               k0,
       double*    restrict alpha,
       double*    restrict a,
       double*    restrict b,
       double*    restrict beta,
       double*    restrict c, inc_t rs_c0, inc_t cs_c0,
       auxinfo_t* restrict data,
       cntx_t*    restrict cntx
     )
{
	void* a_next = bli_auxinfo_next_a( data );
	void* b_next = bli_auxinfo_next_b( data );

	// Typecast local copies of integers in case dim_t and inc_t are a
	// different size than is expected by load instructions.
	uint64_t k_iter = k0 / 4;
	uint64_t k_left = k0 % 4;
	uint64_t rs_c   = rs_c0;
	uint64_t cs_c   = cs_c0;

__asm__ volatile
(
"                                            \n\t" 
" ldr x0,%[aaddr]                            \n\t" // Load address of A 
" ldr x1,%[baddr]                            \n\t" // Load address of B
" ldr x2,%[caddr]                            \n\t" // Load address of C
"                                            \n\t"
" ldr x3,%[a_next]                           \n\t" // Move pointer
" ldr x4,%[b_next]                           \n\t" // Move pointer
"                                            \n\t"
" ldr x5,%[k_iter]                           \n\t" // Init guard (k_iter)
" ldr x6,%[k_left]                           \n\t" // Init guard (k_iter)
"                                            \n\t" 
" ldr x7,%[alpha]                            \n\t" // Alpha address      
" ldr x8,%[beta]                             \n\t" // Beta address      
"                                            \n\t" 
" ldr x9,%[cs_c]                             \n\t" // Load cs_c
" lsl x10,x9,#3                              \n\t" // cs_c * sizeof(double)
"                                            \n\t"
" ldr x13,%[rs_c]                            \n\t" // Load rs_c.
" lsl x14,x13,#3                             \n\t" // rs_c * sizeof(double). 
"                                            \n\t"
"                                            \n\t" //x2 : Load address Column 0 of C
" add x18,x2, x10                            \n\t" //x18: Load address Column 1 of C
" add x19,x18,x10                            \n\t" //x19: Load address Column 2 of C
" add x20,x19,x10                            \n\t" //x20: Load address Column 3 of C
" add x21,x20,x10                            \n\t" //x21: Load address Column 4 of C
" add x22,x21,x10                            \n\t" //x22: Load address Column 5 of C
" add x23,x22,x10                            \n\t" //x23: Load address Column 6 of C
" add x24,x23,x10                            \n\t" //x24: Load address Column 7 of C
" add x25,x24,x10                            \n\t" //x25: Load address Column 8 of C
" add x26,x25,x10                            \n\t" //x26: Load address Column 9 of C
"                                            \n\t"
"                                            \n\t" // TODO: should be prefetched to L2 cache?
"                                            \n\t" //   OpenBLAS did that.
"                                            \n\t" //   'prfm pldl2keep, [..]'?
" prfm pldl1keep,[x2]                        \n\t" // Prefetch c.
" prfm pldl1keep,[x18]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x19]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x20]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x21]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x22]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x23]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x24]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x25]                       \n\t" // Prefetch c.
" prfm pldl1keep,[x26]                       \n\t" // Prefetch c.
"                                            \n\t"
" ptrue   p0.d, all                          \n\t"
"                                            \n\t"
"                                            \n\t" // PRFM, the following prefetch on [x1] and [x0]
"                                            \n\t" //   is for b rows 4..7 and a columns 4..7.
"                                            \n\t" //   both of them will be used in next iteration
"                                            \n\t" //   of k_iter (unrolled per 4 loops)
"                                            \n\t"
"                                            \n\t" // Init c (z12 ~ z31) to zero
" dup  z12.d, #0                             \n\t" // Vector for accummulating column 0
" dup  z13.d, #0                             \n\t" // Vector for accummulating column 0
" dup  z14.d, #0                             \n\t" // Vector for accummulating column 1
" dup  z15.d, #0                             \n\t" // Vector for accummulating column 1
" dup  z16.d, #0                             \n\t" // Vector for accummulating column 2
"                                            \n\t"
"                                            \n\t" // Each b row has 80 bytes. 10*sizeof(double)
" prfm    PLDL1KEEP, [x1, #320]              \n\t" // prefetch b row no.4.  80*4 = 320
" dup  z17.d, #0                             \n\t" // Vector for accummulating column 2
"                                            \n\t"
" prfm    PLDL1KEEP, [x1, #400]              \n\t" // prefetch b row no.5
" dup  z18.d, #0                             \n\t" // Vector for accummulating column 3
"                                            \n\t"
" prfm    PLDL1KEEP, [x1, #480]              \n\t" // prefetch b row no.6
" dup  z19.d, #0                             \n\t" // Vector for accummulating column 3
"                                            \n\t"
" prfm    PLDL1KEEP, [x1, #560]              \n\t" // preftech b row no.7
" dup  z20.d, #0                             \n\t" // Vector for accummulating column 4 
"                                            \n\t"
" dup  z21.d, #0                             \n\t" // Vector for accummulating column 4
"                                            \n\t"
" dup  z22.d, #0                             \n\t" // Vector for accummulating column 5
"                                            \n\t"
"                                            \n\t" // Each a column has 128 bytes. 16*sizeof(double)
" prfm    PLDL1KEEP, [x0, #512]              \n\t" // prefetch a col. no.4. 128*4 = 512
"                                            \n\t"
" dup  z23.d, #0                             \n\t" // Vector for accummulating column 5
" prfm    PLDL1KEEP, [x0, #640]              \n\t" // prefetch a col. no.5
"                                            \n\t"
" dup  z24.d, #0                             \n\t" // Vector for accummulating column 6
" prfm    PLDL1KEEP, [x0, #768]              \n\t" // prefetch a col. no.6
"                                            \n\t"
" dup  z25.d, #0                             \n\t" // Vector for accummulating column 6
" prfm    PLDL1KEEP, [x0, #896]              \n\t" // prefetch a col. no.7
"                                            \n\t"
" dup  z26.d, #0                             \n\t" // Vector for accummulating column 7 
" dup  z27.d, #0                             \n\t" // Vector for accummulating column 7
" dup  z28.d, #0                             \n\t" // Vector for accummulating column 8
" dup  z29.d, #0                             \n\t" // Vector for accummulating column 8
" dup  z30.d, #0                             \n\t" // Vector for accummulating column 9
" dup  z31.d, #0                             \n\t" // Vector for accummulating column 9
"                                            \n\t"
"                                            \n\t"
" cmp x5,#0                                  \n\t" // If k_iter == 0, jump to k_left.
" beq .DCONSIDERKLEFT                        \n\t"
"                                            \n\t"
"                                            \n\t" // Load a
" ldr  z0, [x0]                              \n\t" // z0 = a( 0:7 ,l )
" ldr  z1, [x0, #1, MUL VL]                  \n\t" // z1 = a( 8:15,l )
" add x0, x0, #128                           \n\t" //update address of A
"                                            \n\t"
"                                            \n\t" // Load b
" ld1rqd  {z2.d}, p0/z, [x1]                 \n\t" // z2 = b( l,0:1 )
" ld1rqd  {z3.d}, p0/z, [x1, #16]            \n\t" // z3 = b( l,2:3 )
" ld1rqd  {z4.d}, p0/z, [x1, #32]            \n\t" // z4 = b( l,4:5 )
" ld1rqd  {z5.d}, p0/z, [x1, #48]            \n\t" // z5 = b( l,6:7 )
" ld1rqd  {z6.d}, p0/z, [x1, #64]            \n\t" // z6 = b( l,8:9 )
" add x1, x1, #80                            \n\t" //update address of B
"                                            \n\t"
" cmp x5,1                                   \n\t" // If there is just one k_iter, jump to that one. 
" beq .DLASTITER                             \n\t" // (as loop is do-while-like).
"                                            \n\t"
" DLOOP:                                     \n\t" // Body
"                                            \n\t"
" fmla z12.d, z0.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" fmla z13.d, z1.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" ldr  z7, [x0]                              \n\t" // Load a( 0:7, l+1 )
"                                            \n\t"
" fmla z14.d, z0.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z1.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" ldr  z8, [x0, #1, MUL VL]                  \n\t" // Load a( 8:15,l+1 )
"                                            \n\t"
" fmla z16.d, z0.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z1.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" prfm    PLDL1KEEP, [x1, #560]              \n\t" // preftech b row no.8, 80*8-80 = 560
"                                            \n\t"
" fmla z18.d, z0.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z1.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" ld1rqd  {z2.d}, p0/z, [x1]                 \n\t" // z2 = b( l+1,0:1 )
"                                            \n\t"
" fmla z20.d, z0.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z1.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" ld1rqd  {z3.d}, p0/z, [x1, #16]            \n\t" // z3 = b( l+1,2:3 )
"                                            \n\t"
" fmla z22.d, z0.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z1.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" prfm    PLDL1KEEP, [x1, #640]              \n\t" // preftech b row no.9
"                                            \n\t"
" fmla z24.d, z0.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" fmla z25.d, z1.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" ld1rqd  {z4.d}, p0/z, [x1, #32]            \n\t" // z4 = b( l+1,4:5 )
"                                            \n\t"
" fmla z26.d, z0.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z1.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
" prfm    PLDL1KEEP, [x1, #720]              \n\t" // preftech b row no.10
"                                            \n\t"
" fmla z28.d, z0.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" fmla z29.d, z1.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" ld1rqd  {z5.d}, p0/z, [x1, #48]            \n\t" // z5 = b( l+1,6:7 )
"                                            \n\t"
" fmla z30.d, z0.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z1.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
" prfm    PLDL1KEEP, [x1, #800]              \n\t" // preftech b row no.11
"                                            \n\t"
"                                            \n\t"                  // End it 1
"                                            \n\t"
"                                            \n\t"
" fmla z12.d, z7.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" fmla z13.d, z8.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" ld1rqd  {z6.d}, p0/z, [x1, #64]            \n\t" // z6 = b( l+1,8:9 )
"                                            \n\t"
" fmla z14.d, z7.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z8.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" ldr  z0, [x0, #2, MUL VL]                  \n\t" // Load a( 0:7, l+2 )
"                                            \n\t"
" fmla z16.d, z7.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z8.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ldr  z1, [x0, #3, MUL VL]                  \n\t" // Load a( 8:15,l+2 )
"                                            \n\t"
" fmla z18.d, z7.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z8.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" ld1rqd  {z2.d}, p0/z, [x1, #80]            \n\t" // z2 = b( l+2,0:1 )
"                                            \n\t"
" fmla z20.d, z7.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z8.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" ld1rqd  {z3.d}, p0/z, [x1, #96]            \n\t" // z3 = b( l+2,2:3 )
"                                            \n\t"
" fmla z22.d, z7.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z8.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" prfm    PLDL1KEEP, [x0, #896]              \n\t" // prefetch a col. no.8, 128*8-128 = 896
"                                            \n\t"
" fmla z24.d, z7.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" fmla z25.d, z8.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" ld1rqd  {z4.d}, p0/z, [x1, #112]           \n\t" // z4 = b( l+2,4:5 )
"                                            \n\t"
" fmla z26.d, z7.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z8.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
" prfm    PLDL1KEEP, [x0, #1024]             \n\t" // prefetch a col. no.9
"                                            \n\t"
" fmla z28.d, z7.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" fmla z29.d, z8.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" ld1rqd  {z5.d}, p0/z, [x1, #128]           \n\t" // z5 = b( l+2,6:7 )
"                                            \n\t"
" fmla z30.d, z7.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z8.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
" prfm    PLDL1KEEP, [x0, #1152]             \n\t" // prefetch a col. no.10
"                                            \n\t"
"                                            \n\t"                  // End it 2
"                                            \n\t"
" fmla z12.d, z0.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" fmla z13.d, z1.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" ld1rqd  {z6.d}, p0/z, [x1, #144]           \n\t" // z6 = b( l+2,8:9 )
"                                            \n\t"
" fmla z14.d, z0.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z1.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" prfm    PLDL1KEEP, [x0, #1280]             \n\t" // prefetch a col. no.11
"                                            \n\t"
" fmla z16.d, z0.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z1.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ldr  z7, [x0, #4, MUL VL]                  \n\t" // Load a( 0:7, l+3 )
"                                            \n\t"
" fmla z18.d, z0.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z1.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" ldr  z8, [x0, #5, MUL VL]                  \n\t" // Load a( 8:15,l+3 )
"                                            \n\t"
" fmla z20.d, z0.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z1.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" ld1rqd  {z2.d}, p0/z, [x1, #160]           \n\t" // z2 = b( l+3,0:1 )
"                                            \n\t"
" fmla z22.d, z0.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z1.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" ld1rqd  {z3.d}, p0/z, [x1, #176]           \n\t" // z3 = b( l+3,2:3 )
"                                            \n\t"
" fmla z24.d, z0.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" fmla z25.d, z1.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" ld1rqd  {z4.d}, p0/z, [x1, #192]           \n\t" // z4 = b( l+3,4:5 )
"                                            \n\t"
" fmla z26.d, z0.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z1.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
"                                            \n\t"
" fmla z28.d, z0.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" fmla z29.d, z1.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" ld1rqd  {z5.d}, p0/z, [x1, #208]           \n\t" // z5 = b( l+3,6:7 )
"                                            \n\t"
" fmla z30.d, z0.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z1.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
"                                            \n\t"
"                                            \n\t"
"                                            \n\t"                  // End it 3
"                                            \n\t"
" fmla z12.d, z7.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" fmla z13.d, z8.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" ld1rqd  {z6.d}, p0/z, [x1, #224]           \n\t" // z6 = b( l+3,8:9 )
"                                            \n\t"
" fmla z14.d, z7.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z8.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" ldr  z0, [x0, #6, MUL VL]                  \n\t" // Load a( 0:7, l+4 )
"                                            \n\t"
" fmla z16.d, z7.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z8.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ldr  z1, [x0, #7, MUL VL]                  \n\t" // Load a( 8:15,l+4 )
"                                            \n\t"
" fmla z18.d, z7.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z8.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" ld1rqd  {z2.d}, p0/z, [x1, #240]           \n\t" // z2 = b( l+4,0:1 )
"                                            \n\t"
" fmla z20.d, z7.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z8.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" ld1rqd  {z3.d}, p0/z, [x1, #256]           \n\t" // z3 = b( l+4,2:3 )
"                                            \n\t"
" fmla z22.d, z7.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z8.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
"                                            \n\t"
" fmla z24.d, z7.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" fmla z25.d, z8.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" ld1rqd  {z4.d}, p0/z, [x1, #272]           \n\t" // z4 = b( l+4,4:5 )
"                                            \n\t"
" fmla z26.d, z7.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z8.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
" ld1rqd  {z5.d}, p0/z, [x1, #288]           \n\t" // z5 = b( l+4,6:7 )
"                                            \n\t"
" fmla z28.d, z7.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" fmla z29.d, z8.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
"                                            \n\t"
" fmla z30.d, z7.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z8.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
" ld1rqd  {z6.d}, p0/z, [x1, #304]           \n\t" // z6 = b( l+4,8:9 )
"                                            \n\t"
"                                            \n\t"                  // End it 4
"                                            \n\t"
" add x0, x0, #512                           \n\t"
" add x1, x1, #320                           \n\t"
"                                            \n\t"
" sub x5,x5,1                                \n\t" // i-=1
" cmp x5,1                                   \n\t" // Iterate again if we are not in k_iter == 1.
" bne DLOOP                                  \n\t"
"                                            \n\t"
".DLASTITER:                                 \n\t"
"                                            \n\t"
" fmla z12.d, z0.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" fmla z13.d, z1.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" ldr  z7, [x0]                              \n\t" // Load a( 0:7, l+1 )
"                                            \n\t"
" fmla z14.d, z0.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z1.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" ldr  z8, [x0, #1, MUL VL]                  \n\t" // Load a( 8:15,l+1 )
"                                            \n\t"
" fmla z16.d, z0.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z1.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ld1rqd  {z2.d}, p0/z, [x1]                 \n\t" // z2 = b( l+1,0:1 )
"                                            \n\t"
" fmla z18.d, z0.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z1.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" fmla z20.d, z0.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
"                                            \n\t"
" ld1rqd  {z3.d}, p0/z, [x1, #16]            \n\t" // z3 = b( l+1,2:3 )
" fmla z21.d, z1.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" fmla z22.d, z0.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z1.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" fmla z24.d, z0.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
"                                            \n\t"
" ld1rqd  {z4.d}, p0/z, [x1, #32]            \n\t" // z4 = b( l+1,4:5 )
" fmla z25.d, z1.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" fmla z26.d, z0.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z1.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
" fmla z28.d, z0.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" ld1rqd  {z5.d}, p0/z, [x1, #48]            \n\t" // z5 = b( l+1,6:7 )
"                                            \n\t"
" fmla z29.d, z1.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" fmla z30.d, z0.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z1.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
"                                            \n\t"
"                                            \n\t"                  // End it 1
"                                            \n\t"
" fmla z12.d, z7.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" ld1rqd  {z6.d}, p0/z, [x1, #64]            \n\t" // z6 = b( l+1,8:9 )
"                                            \n\t"
" fmla z13.d, z8.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" fmla z14.d, z7.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z8.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" ldr  z0, [x0, #2, MUL VL]                  \n\t" // Load a( 0:7, l+2 )
"                                            \n\t"
" fmla z16.d, z7.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z8.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ldr  z1, [x0, #3, MUL VL]                  \n\t" // Load a( 8:15,l+2 )
"                                            \n\t"
" fmla z18.d, z7.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z8.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" ld1rqd  {z2.d}, p0/z, [x1, #80]            \n\t" // z2 = b( l+2,0:1 )
"                                            \n\t"
" fmla z20.d, z7.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z8.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" ld1rqd  {z3.d}, p0/z, [x1, #96]            \n\t" // z3 = b( l+2,2:3 )
"                                            \n\t"
" fmla z22.d, z7.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z8.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" fmla z24.d, z7.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" ld1rqd  {z4.d}, p0/z, [x1, #112]           \n\t" // z4 = b( l+2,4:5 )
"                                            \n\t"
" fmla z25.d, z8.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" fmla z26.d, z7.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z8.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
" fmla z28.d, z7.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" ld1rqd  {z5.d}, p0/z, [x1, #128]           \n\t" // z5 = b( l+2,6:7 )
"                                            \n\t"
" fmla z29.d, z8.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" fmla z30.d, z7.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z8.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
"                                            \n\t"
"                                            \n\t"                  // End it 2
"                                            \n\t"
" fmla z12.d, z0.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" ld1rqd  {z6.d}, p0/z, [x1, #144]           \n\t" // z6 = b( l+2,8:9 )
"                                            \n\t"
" fmla z13.d, z1.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" fmla z14.d, z0.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z1.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" ldr  z7, [x0, #4, MUL VL]                  \n\t" // Load a( 0:7, l+3 )
"                                            \n\t"
" fmla z16.d, z0.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z1.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ldr  z8, [x0, #5, MUL VL]                  \n\t" // Load a( 8:15,l+3 )
"                                            \n\t"
" fmla z18.d, z0.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z1.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" ld1rqd  {z2.d}, p0/z, [x1, #160]           \n\t" // z2 = b( l+3,0:1 )
"                                            \n\t"
" fmla z20.d, z0.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z1.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" ld1rqd  {z3.d}, p0/z, [x1, #176]           \n\t" // z3 = b( l+3,2:3 )
"                                            \n\t"
" fmla z22.d, z0.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z1.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" fmla z24.d, z0.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" ld1rqd  {z4.d}, p0/z, [x1, #192]           \n\t" // z4 = b( l+3,4:5 )
"                                            \n\t"
" fmla z25.d, z1.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" fmla z26.d, z0.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z1.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
" fmla z28.d, z0.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" ld1rqd  {z5.d}, p0/z, [x1, #208]           \n\t" // z5 = b( l+3,6:7 )
"                                            \n\t"
" fmla z29.d, z1.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" fmla z30.d, z0.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z1.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
"                                            \n\t"
"                                            \n\t"                  // End it 3
"                                            \n\t"
" fmla z12.d, z7.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" ld1rqd  {z6.d}, p0/z, [x1, #224]           \n\t" // z6 = b( l+3,8:9 )

" fmla z13.d, z8.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" fmla z14.d, z7.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" fmla z15.d, z8.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" add x0, x0, #384                           \n\t" // 384 = 6 * sizeof(Vector Length 512 bits)
"                                            \n\t"
" fmla z16.d, z7.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z8.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" fmla z18.d, z7.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z8.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" add x1, x1, #240                           \n\t"
"                                            \n\t"
" fmla z20.d, z7.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" fmla z21.d, z8.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
"                                            \n\t"
" fmla z22.d, z7.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z8.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
"                                            \n\t"
" fmla z24.d, z7.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" fmla z25.d, z8.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
"                                            \n\t"
" fmla z26.d, z7.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z8.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
"                                            \n\t"
" fmla z28.d, z7.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" fmla z29.d, z8.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
"                                            \n\t"
" fmla z30.d, z7.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z8.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
"                                            \n\t"
"                                            \n\t"                  // End it 4
"                                            \n\t"
".DCONSIDERKLEFT:                            \n\t" 
" cmp x6,0                                   \n\t" // If k_left == 0, we are done.
" beq .DPOSTACCUM                            \n\t" // else, we enter the k_left loop.
"                                            \n\t"
".DLOOPKLEFT:                                \n\t"
"                                            \n\t"
" ldr  z0, [x0]                              \n\t" // Load a( 0:7, l+1 )
" ldr  z1, [x0, #1, MUL VL]                  \n\t" // Load a( 8:15,l+1 )
" ld1rqd  {z2.d}, p0/z, [x1]                 \n\t" // z2 = b( l+1,0:1 )
"                                            \n\t"
" fmla z12.d, z0.d, z2.d[0]                  \n\t" // Accummulate  c(0:7, 0)+=a(0:7, l)*b(l,0)
" fmla z13.d, z1.d, z2.d[0]                  \n\t" // Accummulate  c(8:15,0)+=a(8:15,l)*b(l,0)
" fmla z14.d, z0.d, z2.d[1]                  \n\t" // Accummulate  c(0:7, 1)+=a(0:7, l)*b(l,1)
" ld1rqd  {z3.d}, p0/z, [x1, #16]            \n\t" // z3 = b( l+1,2:3 )
"                                            \n\t"
" fmla z15.d, z1.d, z2.d[1]                  \n\t" // Accummulate  c(8:15,1)+=a(8:15,l)*b(l,1)
" fmla z16.d, z0.d, z3.d[0]                  \n\t" // Accummulate  c(0:7, 2)+=a(0:7, l)*b(l,2)
" fmla z17.d, z1.d, z3.d[0]                  \n\t" // Accummulate  c(8:15,2)+=a(8:15,l)*b(l,2)
" ld1rqd  {z4.d}, p0/z, [x1, #32]            \n\t" // z4 = b( l+1,4:5 )
" add x0, x0, #128                           \n\t" // 128 = 2 * sizeof(Vector Length 512 bits)
"                                            \n\t"
" fmla z18.d, z0.d, z3.d[1]                  \n\t" // Accummulate  c(0:7, 3)+=a(0:7, l)*b(l,3)
" fmla z19.d, z1.d, z3.d[1]                  \n\t" // Accummulate  c(8:15,3)+=a(8:15,l)*b(l,3)
" fmla z20.d, z0.d, z4.d[0]                  \n\t" // Accummulate  c(0:7, 4)+=a(0:7, l)*b(l,4)
" ld1rqd  {z5.d}, p0/z, [x1, #48]            \n\t" // z5 = b( l+1,6:7 )
"                                            \n\t"
" fmla z21.d, z1.d, z4.d[0]                  \n\t" // Accummulate  c(8:15,4)+=a(8:15,l)*b(l,4)
" fmla z22.d, z0.d, z4.d[1]                  \n\t" // Accummulate  c(0:7, 5)+=a(0:7, l)*b(l,5)
" fmla z23.d, z1.d, z4.d[1]                  \n\t" // Accummulate  c(8:15,5)+=a(8:15,l)*b(l,5)
" ld1rqd  {z6.d}, p0/z, [x1, #64]            \n\t" // z6 = b( l+1,8:9 )
" add x1, x1, #80                            \n\t"
"                                            \n\t"
" fmla z24.d, z0.d, z5.d[0]                  \n\t" // Accummulate  c(0:7, 6)+=a(0:7, l)*b(l,6)
" fmla z25.d, z1.d, z5.d[0]                  \n\t" // Accummulate  c(8:15,6)+=a(8:15,l)*b(l,6)
" fmla z26.d, z0.d, z5.d[1]                  \n\t" // Accummulate  c(0:7, 7)+=a(0:7, l)*b(l,7)
" fmla z27.d, z1.d, z5.d[1]                  \n\t" // Accummulate  c(8:15,7)+=a(8:15,l)*b(l,7)
"                                            \n\t"
" sub x6,x6,1                                \n\t"
"                                            \n\t"
" fmla z28.d, z0.d, z6.d[0]                  \n\t" // Accummulate  c(0:7, 8)+=a(0:7, l)*b(l,8)
" fmla z29.d, z1.d, z6.d[0]                  \n\t" // Accummulate  c(8:15,8)+=a(8:15,l)*b(l,8)
" fmla z30.d, z0.d, z6.d[1]                  \n\t" // Accummulate  c(0:7, 9)+=a(0:7, l)*b(l,9)
" fmla z31.d, z1.d, z6.d[1]                  \n\t" // Accummulate  c(8:15,9)+=a(8:15,l)*b(l,9)
"                                            \n\t"
" cmp x6,0                                   \n\t" // Iterate again.
" bne .DLOOPKLEFT                            \n\t" // if i!=0.
"                                            \n\t"
" .DPOSTACCUM:                               \n\t"
"                                            \n\t"
" ld1rd {z10.d}, p0/z, [x7]                  \n\t" // Load alpha.
" ld1rd {z11.d}, p0/z, [x8]                  \n\t" // Load beta
"                                            \n\t"
" cmp x13,#1                                 \n\t" // If rs_c != 1 (column-major)
" bne .DGENSTORED                            \n\t"
"                                            \n\t"
" .DCOLSTORED:                               \n\t" // C is column-major.
"                                            \n\t"
"                                            \n\t" // store C column 0 ~ 4
" dup  z0.d, #0                              \n\t" // TODO: optimization storing phase
" dup  z1.d, #0                              \n\t"
" dup  z2.d, #0                              \n\t"
" dup  z3.d, #0                              \n\t"
" dup  z4.d, #0                              \n\t"
" dup  z5.d, #0                              \n\t"
" dup  z6.d, #0                              \n\t"
" dup  z7.d, #0                              \n\t"
" dup  z8.d, #0                              \n\t"
" dup  z9.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROCOLSTOREDS1                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ldr z0, [x2]                               \n\t" //Load column 0 of C
" ldr z1, [x2, #1, MUL VL]                   \n\t"
"                                            \n\t"
" ldr z2, [x18]                              \n\t" //Load column 1 of C
" ldr z3, [x18, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z4, [x19]                              \n\t" //Load column 2 of C
" ldr z5, [x19, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z6, [x20]                              \n\t" //Load column 3 of C
" ldr z7, [x20, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z8, [x21]                              \n\t" //Load column 4 of C
" ldr z9, [x21, #1, MUL VL]                  \n\t"
"                                            \n\t"
" fmul z0.d, z0.d, z11.d                     \n\t" // Scale by beta
" fmul z1.d, z1.d, z11.d                     \n\t" // Scale by beta
" fmul z2.d, z2.d, z11.d                     \n\t" // Scale by beta
" fmul z3.d, z3.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" fmul z4.d, z4.d, z11.d                     \n\t" // Scale by beta
" fmul z5.d, z5.d, z11.d                     \n\t" // Scale by beta
" fmul z6.d, z6.d, z11.d                     \n\t" // Scale by beta
" fmul z7.d, z7.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" fmul z8.d, z8.d, z11.d                     \n\t" // Scale by beta
" fmul z9.d, z9.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROCOLSTOREDS1:                     \n\t"
"                                            \n\t"
" fmla z0.d, z12.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z1.d, z13.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z2.d, z14.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z3.d, z15.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" fmla z4.d, z16.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z5.d, z17.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z6.d, z18.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z7.d, z19.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" fmla z8.d, z20.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z9.d, z21.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" str z0, [x2]                               \n\t" //Store column 0 of C
" str z1, [x2, #1, MUL VL]                   \n\t"
"                                            \n\t"
" str z2, [x18]                              \n\t" //Store column 1 of C
" str z3, [x18, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z4, [x19]                              \n\t" //Store column 2 of C
" str z5, [x19, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z6, [x20]                              \n\t" //Store column 3 of C
" str z7, [x20, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z8, [x21]                              \n\t" //Store column 4 of C
" str z9, [x21, #1, MUL VL]                  \n\t"
"                                            \n\t"
"                                            \n\t" // store C column 5 ~ 9
" dup  z0.d, #0                              \n\t" // TODO: optimization storing phase
" dup  z1.d, #0                              \n\t"
" dup  z2.d, #0                              \n\t"
" dup  z3.d, #0                              \n\t"
" dup  z4.d, #0                              \n\t"
" dup  z5.d, #0                              \n\t"
" dup  z6.d, #0                              \n\t"
" dup  z7.d, #0                              \n\t"
" dup  z8.d, #0                              \n\t"
" dup  z9.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROCOLSTOREDS2                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ldr z0, [x22]                              \n\t" //Load column 5 of C
" ldr z1, [x22, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z2, [x23]                              \n\t" //Load column 6 of C
" ldr z3, [x23, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z4, [x24]                              \n\t" //Load column 7 of C
" ldr z5, [x24, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z6, [x25]                              \n\t" //Load column 8 of C
" ldr z7, [x25, #1, MUL VL]                  \n\t"
"                                            \n\t"
" ldr z8, [x26]                              \n\t" //Load column 9 of C
" ldr z9, [x26, #1, MUL VL]                  \n\t"
"                                            \n\t"
" fmul z0.d, z0.d, z11.d                     \n\t" // Scale by beta
" fmul z1.d, z1.d, z11.d                     \n\t" // Scale by beta
" fmul z2.d, z2.d, z11.d                     \n\t" // Scale by beta
" fmul z3.d, z3.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" fmul z4.d, z4.d, z11.d                     \n\t" // Scale by beta
" fmul z5.d, z5.d, z11.d                     \n\t" // Scale by beta
" fmul z6.d, z6.d, z11.d                     \n\t" // Scale by beta
" fmul z7.d, z7.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" fmul z8.d, z8.d, z11.d                     \n\t" // Scale by beta
" fmul z9.d, z9.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROCOLSTOREDS2:                     \n\t"
"                                            \n\t"
" fmla z0.d, z22.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z1.d, z23.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z2.d, z24.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z3.d, z25.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" fmla z4.d, z26.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z5.d, z27.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z6.d, z28.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z7.d, z29.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" fmla z8.d, z30.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z9.d, z31.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" str z0, [x22]                              \n\t" //Store column 5 of C
" str z1, [x22, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z2, [x23]                              \n\t" //Store column 6 of C
" str z3, [x23, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z4, [x24]                              \n\t" //Store column 7 of C
" str z5, [x24, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z6, [x25]                              \n\t" //Store column 8 of C
" str z7, [x25, #1, MUL VL]                  \n\t"
"                                            \n\t"
" str z8, [x26]                              \n\t" //Store column 9 of C
" str z9, [x26, #1, MUL VL]                  \n\t"
"                                            \n\t"
" b .DEND                                    \n\t"
"                                            \n\t"
" .DGENSTORED:                               \n\t" // C is general-stride stored.
"                                            \n\t"
"                                            \n\t" // x14 is row-stride in number of bytes.
" lsl x15,x14,#3                             \n\t" // x15 is 8-row-stride, which is the address offset 
"                                            \n\t" //     btw c(8,*) and c(0,*)
" index z4.d, xzr, x14                       \n\t" // z4  is address offsets between each of 8 elements
"                                            \n\t" //     in the same column. such as c( 0:7,* ).
"                                            \n\t" // z4  is used as vector index for gather/scatter
"                                            \n\t" //     loading/storing from column of *c
"                                            \n\t"
"                                            \n\t" // C's each column's address:
"                                            \n\t" // c(0, 0:9):
"                                            \n\t" //     x2, x18, x19, x20, x21, x22, x23, x24, x25, x26
"                                            \n]t" // c(8,0:9)
"                                            \n\t" //     x5, x6,  x7,  x8,  x9,  x10, x11, x12, x13, x14
" add  x5,  x15, x2                          \n\t" // x5  is address of c(8,0)
" add  x6,  x15, x18                         \n\t" // x6  is address of c(8,1)
" add  x7,  x15, x19                         \n\t" // x7  is address of c(8,2)
" add  x8,  x15, x20                         \n\t" // x8  is address of c(8,3)
" add  x9,  x15, x21                         \n\t" // x9  is address of c(8,4)
" add  x10, x15, x22                         \n\t" // x10 is address of c(8,5)
" add  x11, x15, x23                         \n\t" // x11 is address of c(8,6)
" add  x12, x15, x24                         \n\t" // x12 is address of c(8,7)
" add  x13, x15, x25                         \n\t" // x13 is address of c(8,8)
" add  x14, x15, x26                         \n\t" // x14 is address of c(8,9)
"                                            \n\t"
"                                            \n\t" // TODO: optimize storing phase
"                                            \n\t" //      loop unrolling, and instruction interleavings
"                                            \n\t"
" dup  z0.d, #0                              \n\t" // C column 0, 1
" dup  z1.d, #0                              \n\t"
" dup  z2.d, #0                              \n\t"
" dup  z3.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROGENSTOREDS1                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ld1d {z0.d}, p0/z, [x2,  z4.d]             \n\t" // Load c( 0:7, 0 ) into z0
" ld1d {z1.d}, p0/z, [x5,  z4.d]             \n\t" // Load c( 8:15,0 ) into z1
" ld1d {z2.d}, p0/z, [x18, z4.d]             \n\t" // Load c( 0:7, 1 ) into z2
" ld1d {z3.d}, p0/z, [x6,  z4.d]             \n\t" // Load c( 8:15,1 ) into z3
"                                            \n\t"
" fmul z0.d, z0.d, z11.d                     \n\t" // Scale by beta
" fmul z1.d, z1.d, z11.d                     \n\t" // Scale by beta
" fmul z2.d, z2.d, z11.d                     \n\t" // Scale by beta
" fmul z3.d, z3.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROGENSTOREDS1:                     \n\t"
"                                            \n\t"
" fmla z0.d, z12.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z1.d, z13.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z2.d, z14.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z3.d, z15.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" st1d {z0.d}, p0, [x2 , z4.d]               \n\t" // Store c( 0:7, 0 ) <- z0
" st1d {z1.d}, p0, [x5 , z4.d]               \n\t" // Store c( 8:15,0 ) <- z1
" st1d {z2.d}, p0, [x18, z4.d]               \n\t" // Store c( 0:7, 1 ) <- z2
" st1d {z3.d}, p0, [x6 , z4.d]               \n\t" // Store c( 8:15,1 ) <- z3
"                                            \n\t"
"                                            \n\t"
" dup  z5.d, #0                              \n\t" // C column 2, 3
" dup  z6.d, #0                              \n\t"
" dup  z7.d, #0                              \n\t"
" dup  z8.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROGENSTOREDS2                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ld1d {z5.d}, p0/z, [x19, z4.d]             \n\t"
" ld1d {z6.d}, p0/z, [x7 , z4.d]             \n\t"
" ld1d {z7.d}, p0/z, [x20, z4.d]             \n\t"
" ld1d {z8.d}, p0/z, [x8 , z4.d]             \n\t"
"                                            \n\t"
" fmul z5.d, z5.d, z11.d                     \n\t" // Scale by beta
" fmul z6.d, z6.d, z11.d                     \n\t" // Scale by beta
" fmul z7.d, z7.d, z11.d                     \n\t" // Scale by beta
" fmul z8.d, z8.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROGENSTOREDS2:                     \n\t"
"                                            \n\t"
" fmla z5.d, z16.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z6.d, z17.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z7.d, z18.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z8.d, z19.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" st1d {z5.d}, p0, [x19, z4.d]               \n\t"
" st1d {z6.d}, p0, [x7 , z4.d]               \n\t"
" st1d {z7.d}, p0, [x20, z4.d]               \n\t"
" st1d {z8.d}, p0, [x8 , z4.d]               \n\t"
"                                            \n\t"
" dup  z0.d, #0                              \n\t" // C column 4, 5
" dup  z1.d, #0                              \n\t"
" dup  z2.d, #0                              \n\t"
" dup  z3.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROGENSTOREDS3                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ld1d {z0.d}, p0/z, [x21, z4.d]             \n\t"
" ld1d {z1.d}, p0/z, [x9,  z4.d]             \n\t"
" ld1d {z2.d}, p0/z, [x22, z4.d]             \n\t"
" ld1d {z3.d}, p0/z, [x10, z4.d]             \n\t"
"                                            \n\t"
" fmul z0.d, z0.d, z11.d                     \n\t" // Scale by beta
" fmul z1.d, z1.d, z11.d                     \n\t" // Scale by beta
" fmul z2.d, z2.d, z11.d                     \n\t" // Scale by beta
" fmul z3.d, z3.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROGENSTOREDS3:                     \n\t"
"                                            \n\t"
" fmla z0.d, z20.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z1.d, z21.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z2.d, z22.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z3.d, z23.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" st1d {z0.d}, p0, [x21, z4.d]               \n\t"
" st1d {z1.d}, p0, [x9,  z4.d]               \n\t"
" st1d {z2.d}, p0, [x22, z4.d]               \n\t"
" st1d {z3.d}, p0, [x10, z4.d]               \n\t"
"                                            \n\t"
" dup  z5.d, #0                              \n\t" // C column 6, 7
" dup  z6.d, #0                              \n\t"
" dup  z7.d, #0                              \n\t"
" dup  z8.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROGENSTOREDS4                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ld1d {z5.d}, p0/z, [x23, z4.d]             \n\t"
" ld1d {z6.d}, p0/z, [x11, z4.d]             \n\t"
" ld1d {z7.d}, p0/z, [x24, z4.d]             \n\t"
" ld1d {z8.d}, p0/z, [x12, z4.d]             \n\t"
"                                            \n\t"
" fmul z5.d, z5.d, z11.d                     \n\t" // Scale by beta
" fmul z6.d, z6.d, z11.d                     \n\t" // Scale by beta
" fmul z7.d, z7.d, z11.d                     \n\t" // Scale by beta
" fmul z8.d, z8.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROGENSTOREDS4:                     \n\t"
"                                            \n\t"
" fmla z5.d, z24.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z6.d, z25.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z7.d, z26.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z8.d, z27.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" st1d {z5.d}, p0, [x23, z4.d]               \n\t"
" st1d {z6.d}, p0, [x11, z4.d]               \n\t"
" st1d {z7.d}, p0, [x24, z4.d]               \n\t"
" st1d {z8.d}, p0, [x12, z4.d]               \n\t"
"                                            \n\t"
" dup  z0.d, #0                              \n\t" // C column 8, 9
" dup  z1.d, #0                              \n\t"
" dup  z2.d, #0                              \n\t"
" dup  z3.d, #0                              \n\t"
"                                            \n\t"
" fcmp d7,#0.0                               \n\t"
" beq .DBETAZEROGENSTOREDS5                  \n\t" // Taking care of the beta==0 case.
"                                            \n\t"
" ld1d {z0.d}, p0/z, [x25, z4.d]             \n\t"
" ld1d {z1.d}, p0/z, [x13, z4.d]             \n\t"
" ld1d {z2.d}, p0/z, [x26, z4.d]             \n\t"
" ld1d {z3.d}, p0/z, [x14, z4.d]             \n\t"
"                                            \n\t"
" fmul z0.d, z0.d, z11.d                     \n\t" // Scale by beta
" fmul z1.d, z1.d, z11.d                     \n\t" // Scale by beta
" fmul z2.d, z2.d, z11.d                     \n\t" // Scale by beta
" fmul z3.d, z3.d, z11.d                     \n\t" // Scale by beta
"                                            \n\t"
" .DBETAZEROGENSTOREDS5:                     \n\t"
"                                            \n\t"
" fmla z0.d, z28.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z1.d, z29.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z2.d, z30.d, z10.d[0]                 \n\t" // Scale by alpha
" fmla z3.d, z31.d, z10.d[0]                 \n\t" // Scale by alpha
"                                            \n\t"
" st1d {z0.d}, p0, [x25, z4.d]               \n\t"
" st1d {z1.d}, p0, [x13, z4.d]               \n\t"
" st1d {z2.d}, p0, [x26, z4.d]               \n\t"
" st1d {z3.d}, p0, [x14, z4.d]               \n\t"
"                                            \n\t"
" .DEND:                                     \n\t" // Done!
"                                            \n\t"
:// output operands (none)
:// input operands
 [aaddr]  "m" (a),      // 0
 [baddr]  "m" (b),      // 1
 [caddr]  "m" (c),      // 2
 [k_iter] "m" (k_iter), // 3
 [k_left] "m" (k_left), // 4
 [alpha]  "m" (alpha),  // 5
 [beta]   "m" (beta),   // 6
 [rs_c]   "m" (rs_c),   // 6
 [cs_c]   "m" (cs_c),   // 7
 [a_next] "m" (a_next), // 8
 [b_next] "m" (b_next)  // 9
:// Register clobber list
 "x0","x1","x2","x3",
 "x4","x5","x6",
 "x7","x8","x9",
 "x10","x11","x12","x13","x14","x15","x16","x17","x18","x19",
 "x20","x21","x22","x23","x24","x25","x26",
 "x27",       
 "v0","v1","v2",
 "v3","v4","v5",
 "v6","v7","v8",
 "v9","v10","v11",
 "v12","v13","v14",
 "v15","v16","v17","v18","v19",
 "v20","v21","v22","v23",
 "v24","v25","v26","v27",
 "v28","v29","v30","v31"
);

}
