/*	$OpenBSD: vbusvar.h,v 1.1 2008/03/09 18:10:34 kettenis Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SPARC64_DEV_VBUSVAR_H_
#define _SPARC64_DEV_VBUSVAR_H_

struct vbus_attach_args {
	char		*va_name;
	int		va_node;

	u_int32_t	*va_reg;
	u_int32_t	*va_intr;

	int		va_nreg;
	int		va_nintr;
};

#endif
