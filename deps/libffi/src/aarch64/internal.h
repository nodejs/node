/* 
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
``Software''), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#define AARCH64_RET_VOID	0
#define AARCH64_RET_INT64	1
#define AARCH64_RET_INT128	2

#define AARCH64_RET_UNUSED3	3
#define AARCH64_RET_UNUSED4	4
#define AARCH64_RET_UNUSED5	5
#define AARCH64_RET_UNUSED6	6
#define AARCH64_RET_UNUSED7	7

/* Note that FFI_TYPE_FLOAT == 2, _DOUBLE == 3, _LONGDOUBLE == 4,
   so _S4 through _Q1 are layed out as (TYPE * 4) + (4 - COUNT).  */
#define AARCH64_RET_S4		8
#define AARCH64_RET_S3		9
#define AARCH64_RET_S2		10
#define AARCH64_RET_S1		11

#define AARCH64_RET_D4		12
#define AARCH64_RET_D3		13
#define AARCH64_RET_D2		14
#define AARCH64_RET_D1		15

#define AARCH64_RET_Q4		16
#define AARCH64_RET_Q3		17
#define AARCH64_RET_Q2		18
#define AARCH64_RET_Q1		19

/* Note that each of the sub-64-bit integers gets two entries.  */
#define AARCH64_RET_UINT8	20
#define AARCH64_RET_UINT16	22
#define AARCH64_RET_UINT32	24

#define AARCH64_RET_SINT8	26
#define AARCH64_RET_SINT16	28
#define AARCH64_RET_SINT32	30

#define AARCH64_RET_MASK	31

#define AARCH64_RET_IN_MEM	(1 << 5)
#define AARCH64_RET_NEED_COPY	(1 << 6)

#define AARCH64_FLAG_ARG_V_BIT	7
#define AARCH64_FLAG_ARG_V	(1 << AARCH64_FLAG_ARG_V_BIT)

#define N_X_ARG_REG		8
#define N_V_ARG_REG		8
#define CALL_CONTEXT_SIZE	(N_V_ARG_REG * 16 + N_X_ARG_REG * 8)
