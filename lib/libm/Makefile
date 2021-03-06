#  $FabBSD$
#  $NetBSD: Makefile,v 1.28 1995/11/20 22:06:19 jtc Exp $
#  $OpenBSD: Makefile,v 1.53 2008/07/30 08:02:34 jmc Exp $
#
#  @(#)Makefile 5.1beta 93/09/24 
# 
#  ====================================================
#  Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
# 
#  Developed at SunPro, a Sun Microsystems, Inc. business.
#  Permission to use, copy, modify, and distribute this
#  software is freely granted, provided that this notice 
#  is preserved.
#  ====================================================
# 
# 
#
# There are two options in making libm at fdlibm compile time:
# 	_IEEE_LIBM 	--- IEEE libm; smaller, and somewhat faster
#	_MULTI_LIBM	--- Support multi-standard at runtime by 
#			    imposing wrapper functions defined in 
#			    fdlibm.h:
#				_IEEE_MODE 	-- IEEE
#				_XOPEN_MODE 	-- X/OPEN
#				_POSIX_MODE 	-- POSIX/ANSI
#				_SVID3_MODE 	-- SVID
#
# Here is how to set up CFLAGS to create the desired libm at 
# compile time:
#
# 	CFLAGS = -D_IEEE_LIBM		... IEEE libm (recommended)
#	CFLAGS = -D_SVID3_MODE	... Multi-standard supported
#					    libm with SVID as the 
#					    default standard
#	CFLAGS = -D_XOPEN_MODE	... Multi-standard supported
#					    libm with XOPEN as the 
#					    default standard
#	CFLAGS = -D_POSIX_MODE	... Multi-standard supported
#					    libm with POSIX as the 
#					    default standard
#	CFLAGS = 			... Multi-standard supported
#					    libm with IEEE as the 
#					    default standard
# 

# For NOPROFILE and NOPIC
.include <bsd.own.mk>

CFLAGS+= -D__LIBM_PRIVATE -D_USE_WRITE

.if (${MACHINE_ARCH} == "sparc")
CFLAGS+= -O0
.endif

.if (${MACHINE_ARCH} == "alpha")
.PATH:	${.CURDIR}/arch/alpha
ARCH_SRCS = s_copysign.S s_copysignf.S
.elif (${MACHINE_ARCH} == "i386")
.PATH:	${.CURDIR}/arch/i387
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_remainderf.S e_scalb.S e_sqrt.S e_sqrtf.S \
	    s_atan.S s_atanf.S s_ceil.S s_ceilf.S s_copysign.S s_copysignf.S \
	    s_cos.S s_cosf.S s_finite.S s_finitef.S s_floor.S s_floorf.S \
	    s_ilogb.S s_ilogbf.S s_log1p.S s_log1pf.S s_logb.S s_logbf.S \
	    s_llrint.S s_llrintf.S s_lrint.S s_lrintf.S s_rint.S s_rintf.S\
	    s_scalbn.S s_scalbnf.S s_significand.S s_significandf.S \
	    s_sin.S s_sinf.S s_tan.S s_tanf.S
.elif (${MACHINE_ARCH} == "amd64")
.PATH:	${.CURDIR}/arch/amd64
CPPFLAGS+=-I${.CURDIR}/arch/amd64
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_remainderf.S e_scalb.S e_sqrt.S e_sqrtf.S \
	    s_atan.S s_atanf.S s_ceil.S s_ceilf.S s_copysign.S s_copysignf.S \
	    s_cos.S s_cosf.S s_finite.S s_finitef.S s_floor.S s_floorf.S \
	    s_ilogb.S s_ilogbf.S s_log1p.S s_log1pf.S s_logb.S s_logbf.S \
	    s_rint.S s_rintf.S s_scalbn.S s_scalbnf.S s_significand.S \
	    s_significandf.S s_sin.S s_sinf.S s_tan.S s_tanf.S
.endif

.PATH:	${.CURDIR}/man
.PATH:	${.CURDIR}/src
.PATH:	${.CURDIR}/noieee_src

.if (${MACHINE_ARCH} != "vax")
CFLAGS+= -D_MULTI_LIBM -D_POSIX_MODE
.endif

LIB=	m
COMMON_SRCS = b_exp__D.c b_log__D.c b_tgamma.c \
	e_acos.c e_acosf.c e_acosh.c e_acoshf.c e_asin.c e_asinf.c \
	e_atan2.c e_atan2f.c e_atanh.c e_atanhf.c e_cosh.c e_coshf.c e_exp.c \
	e_expf.c e_fmod.c e_fmodf.c e_hypot.c e_hypotf.c e_j0.c e_j0f.c \
	e_j1.c e_j1f.c e_jn.c e_jnf.c e_lgamma_r.c e_lgammaf_r.c e_log.c \
	e_log10.c e_log10f.c e_logf.c e_pow.c e_powf.c e_rem_pio2.c \
	e_rem_pio2f.c e_remainder.c e_remainderf.c e_scalb.c e_scalbf.c \
	e_sinh.c e_sinhf.c e_sqrt.c e_sqrtf.c \
	k_cos.c k_cosf.c k_rem_pio2.c k_rem_pio2f.c k_sin.c k_sinf.c \
	k_standard.c k_tan.c k_tanf.c \
	s_lround.c s_lroundf.c s_llround.c s_llroundf.c \
	s_asinh.c s_asinhf.c s_atan.c s_atanf.c s_cbrt.c s_cbrtf.c s_ceil.c \
	s_ceilf.c s_copysign.c s_copysignf.c s_cos.c s_cosf.c s_erf.c s_erff.c \
	s_exp2.c s_exp2f.c s_expm1.c s_expm1f.c s_fabsf.c s_finite.c s_finitef.c \
	s_floor.c s_floorf.c s_frexpf.c s_ilogb.c s_ilogbf.c \
	s_ldexpf.c s_lib_version.c s_log1p.c \
	s_log1pf.c s_logb.c s_logbf.c s_llrint.c s_llrintf.c s_lrint.c \
	s_lrintf.c s_matherr.c s_modff.c s_nan.c \
	s_nextafter.c s_nextafterf.c s_remquo.c s_remquof.c s_rint.c \
	s_rintf.c s_round.c s_roundf.c \
	s_scalbn.c s_scalbnf.c s_signgam.c s_significand.c s_significandf.c \
	s_sin.c s_sinf.c s_tan.c s_tanf.c s_tanh.c s_tanhf.c s_trunc.c s_truncf.c \
	w_acos.c w_acosf.c w_acosh.c w_acoshf.c w_asin.c w_asinf.c w_atan2.c \
	w_atan2f.c w_atanh.c w_atanhf.c w_cabs.c w_cabsf.c w_cosh.c w_coshf.c \
	w_drem.c w_dremf.c w_exp.c w_expf.c w_fmod.c w_fmodf.c w_gamma.c \
	w_gamma_r.c w_gammaf.c w_gammaf_r.c w_hypot.c w_hypotf.c w_j0.c \
	w_j0f.c w_j1.c w_j1f.c w_jn.c w_jnf.c w_lgamma.c w_lgamma_r.c \
	w_lgammaf.c w_lgammaf_r.c w_log.c w_log10.c w_log10f.c w_logf.c \
	w_pow.c w_powf.c w_remainder.c w_remainderf.c w_scalb.c w_scalbf.c \
	w_sinh.c w_sinhf.c w_sqrt.c w_sqrtf.c

# math routines for non-IEEE architectures.
NOIEEE_SRCS = n_asincos.c n_acosh.c n_asinh.c n_atan.c n_atanh.c n_cosh.c \
	n_erf.c n_exp.c n_exp__E.c n_expm1.c n_floor.c n_fmod.c n_tgamma.c \
	n_lgamma.c n_j0.c n_j1.c n_jn.c n_log.c n_log10.c n_log1p.c \
	n_log__L.c n_pow.c n_sinh.c n_tanh.c n_atan2.c n_cabs.c n_cbrt.c \
	n_sqrt.c n_sincos.c n_tan.c n_argred.c n_support.c n_infnan.c \
	n_round.c

# OpenBSD's C library supplies these functions:
#COMMON_SRCS+=	s_fabs.c s_frexp.c s_isinf.c s_isinff.c s_isnan.c s_isnanf.c \
#		s_ldexp.c s_modf.c

.if (${MACHINE_ARCH} == "vax")
SRCS=	${NOIEEE_SRCS} ${NOIEEE_ARCH}
MAN+=	infnan.3
.else
SRCS=	${COMMON_SRCS}
.endif

# Substitute common sources with any arch specific sources 
.for i in ${ARCH_SRCS}
    SRCS:=${SRCS:S/${i:S/.S/.c/}/$i/}
    LSRCS+=${i:S/.S/.c/}
.endfor

.if (${MACHINE_ARCH} == "vax")
.for i in ${NOIEEE_ARCH}
    SRCS:=${SRCS:S/${i:S/.S/.c/}/$i/}
    LSRCS+=${i:S/.S/.c/}
.endfor
.endif

MAN+=	acos.3 acosh.3 asin.3 asinh.3 atan.3 atan2.3 atanh.3 ceil.3 \
	cos.3 cosh.3 erf.3 exp.3 fabs.3 floor.3 fmod.3 hypot.3 ieee.3 \
	logb.3 j0.3 lgamma.3 lrint.3 lround.3 math.3 nan.3 remainder.3 \
	rint.3 round.3 sin.3 sinh.3 sqrt.3 tan.3 tanh.3 trunc.3

MLINKS+=erf.3 erfc.3
MLINKS+=exp.3 exp2.3 exp.3 expm1.3 exp.3 log.3 exp.3 log10.3 exp.3 log1p.3 \
	exp.3 pow.3
MLINKS+=hypot.3 cabs.3
MLINKS+=ieee.3 copysign.3 ieee.3 ilogb.3 ieee.3 nextafter.3 ieee.3 scalbn.3
MLINKS+=logb.3 scalb.3
MLINKS+=logb.3 significand.3
MLINKS+=j0.3 j1.3 j0.3 jn.3 j0.3 y0.3 j0.3 y1.3 j0.3 yn.3
MLINKS+=lgamma.3 gamma.3 lgamma.3 tgamma.3
MLINKS+=lrint.3 llrint.3
MLINKS+=lround.3 llround.3
MLINKS+=remainder.3 drem.3 remainder.3 remquo.3
MLINKS+=sqrt.3 cbrt.3

# float versions
MLINKS+=acos.3 acosf.3
MLINKS+=acosh.3 acoshf.3
MLINKS+=asin.3 asinf.3
MLINKS+=asinh.3 asinhf.3
MLINKS+=atan.3 atanf.3
MLINKS+=atan2.3 atan2f.3
MLINKS+=atanh.3 atanhf.3
MLINKS+=ceil.3 ceilf.3
MLINKS+=cos.3 cosf.3
MLINKS+=cosh.3 coshf.3
MLINKS+=erf.3 erfcf.3
MLINKS+=erf.3 erff.3
MLINKS+=exp.3 expf.3
MLINKS+=exp.3 exp2f.3
MLINKS+=exp.3 expm1f.3
MLINKS+=exp.3 log10f.3
MLINKS+=exp.3 log1pf.3
MLINKS+=exp.3 logf.3
MLINKS+=exp.3 powf.3
MLINKS+=fabs.3 fabsf.3
MLINKS+=floor.3 floorf.3
MLINKS+=fmod.3 fmodf.3
MLINKS+=hypot.3 cabsf.3
MLINKS+=hypot.3 hypotf.3
MLINKS+=ieee.3 copysignf.3
MLINKS+=ieee.3 ieeef.3
MLINKS+=ieee.3 ilogbf.3
MLINKS+=ieee.3 nextafterf.3
MLINKS+=ieee.3 scalbnf.3
MLINKS+=j0.3 j0f.3
MLINKS+=j0.3 j1f.3
MLINKS+=j0.3 jnf.3
MLINKS+=j0.3 y0f.3
MLINKS+=j0.3 y1f.3
MLINKS+=j0.3 ynf.3
MLINKS+=lgamma.3 gammaf.3
MLINKS+=lgamma.3 lgammaf.3
MLINKS+=lgamma.3 tgammaf.3
MLINKS+=logb.3 logbf.3
MLINKS+=logb.3 scalbf.3
MLINKS+=logb.3 significandf.3
MLINKS+=lrint.3 lrintf.3
MLINKS+=lrint.3 llrintf.3
MLINKS+=lround.3 lroundf.3
MLINKS+=lround.3 llroundf.3
MLINKS+=math.3 mathf.3
MLINKS+=nan.3 nanf.3
MLINKS+=remainder.3 dremf.3
MLINKS+=remainder.3 remainderf.3
MLINKS+=remainder.3 remquof.3
MLINKS+=rint.3 rintf.3
MLINKS+=round.3 roundf.3
MLINKS+=sin.3 sinf.3
MLINKS+=sinh.3 sinhf.3
MLINKS+=sqrt.3 cbrtf.3
MLINKS+=sqrt.3 sqrtf.3
MLINKS+=tan.3 tanf.3
MLINKS+=tanh.3 tanhf.3
MLINKS+=trunc.3 truncf.3

#.if (${MACHINE_ARCH} == "i386")
#LINKS=	${LIBDIR}/libm.a ${LIBDIR}/libm_i387.a
#.if !defined(NOPROFILE)
#LINKS+=	${LIBDIR}/libm_p.a ${LIBDIR}/libm_i387_p.a
#.endif
#.if !defined(NOPIC)
#LINKS+= ${LIBDIR}/libm.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
#	${LIBDIR}/libm_i387.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
#LINKS+= ${LIBDIR}/libm_pic.a ${LIBDIR}/libm_i387_pic.a
#.endif
#.endif

#e_acos.o: 
#	cpp  -D__LIBM_PRIVATE -D_USE_WRITE -D_MULTI_LIBM -D_POSIX_MODE \
#	/usr/src/lib/libm/arch/mc68881/e_acos.S |  as -o e_acos.o


.include <bsd.lib.mk>
