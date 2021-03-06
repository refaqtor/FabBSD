/*	$OpenBSD: wwiomux.c,v 1.9 2003/07/10 00:06:52 david Exp $	*/
/*	$NetBSD: wwiomux.c,v 1.5 1996/02/08 20:45:09 mycroft Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)wwiomux.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: wwiomux.c,v 1.9 2003/07/10 00:06:52 david Exp $";
#endif
#endif /* not lint */

#include "ww.h"
#include <sys/time.h>
#include <sys/types.h>
#if !defined(OLD_TTY) && !defined(TIOCPKT_DATA)
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/*
 * Multiple window output handler.
 * The idea is to copy window outputs to the terminal, via the
 * display package.  We try to give wwcurwin highest priority.
 * The only return conditions are when there is keyboard input
 * and when a child process dies.
 * When there's nothing to do, we sleep in a select().
 * The history of this routine is interesting.
 */
wwiomux()
{
	struct ww *w;
	fd_set imask;
	int n;
	char *p;
	char c;
	struct timeval tv;
	char noblock = 0;

	for (;;) {
		if (wwinterrupt()) {
			wwclrintr();
			return;
		}

		FD_ZERO(&imask);
		n = -1;
		for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw) {
			if (w->ww_pty < 0)
				continue;
			if (w->ww_obq < w->ww_obe) {
				if (w->ww_pty > n)
					n = w->ww_pty + 1;
				FD_SET(w->ww_pty, &imask);
			}
			if (w->ww_obq > w->ww_obp &&
			    !ISSET(w->ww_pflags, WWP_STOPPED))
				noblock = 1;
		}
		if (wwibq < wwibe) {
			if (0 > n)
				n = 0 + 1;
			FD_SET(0, &imask);
		}

		if (!noblock) {
			if (wwcurwin != 0)
				wwcurtowin(wwcurwin);
			wwupdate();
			wwflush();
			(void) setjmp(wwjmpbuf);
			wwsetjmp = 1;
			if (wwinterrupt()) {
				wwsetjmp = 0;
				wwclrintr();
				return;
			}
			/* XXXX */
			tv.tv_sec = 30;
			tv.tv_usec = 0;
		} else {
			tv.tv_sec = 0;
			tv.tv_usec = 10000;
		}
		wwnselect++;
		n = select(n + 1, &imask, (fd_set *)0, (fd_set *)0, &tv);
		wwsetjmp = 0;
		noblock = 0;

		if (n < 0)
			wwnselecte++;
		else if (n == 0)
			wwnselectz++;
		else {
			if (FD_ISSET(0, &imask))
				wwrint();
			for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw) {
				if (w->ww_pty < 0 ||
				    !FD_ISSET(w->ww_pty, &imask))
					continue;
				wwnwread++;
				p = w->ww_obq;
				if (w->ww_type == WWT_PTY) {
					if (p == w->ww_ob) {
						w->ww_obp++;
						w->ww_obq++;
					} else
						p--;
					c = *p;
				}
				n = read(w->ww_pty, p, w->ww_obe - p);
				if (n < 0) {
					wwnwreade++;
					(void) close(w->ww_pty);
					w->ww_pty = -1;
				} else if (n == 0) {
					wwnwreadz++;
					(void) close(w->ww_pty);
					w->ww_pty = -1;
				} else if (w->ww_type != WWT_PTY) {
					wwnwreadd++;
					wwnwreadc += n;
					w->ww_obq += n;
				} else if (*p == TIOCPKT_DATA) {
					n--;
					wwnwreadd++;
					wwnwreadc += n;
					w->ww_obq += n;
				} else {
					wwnwreadp++;
					if (*p & TIOCPKT_STOP)
						SET(w->ww_pflags, WWP_STOPPED);
					if (*p & TIOCPKT_START)
						CLR(w->ww_pflags, WWP_STOPPED);
					if (*p & TIOCPKT_FLUSHWRITE) {
						CLR(w->ww_pflags, WWP_STOPPED);
						w->ww_obq = w->ww_obp =
							w->ww_ob;
					}
				}
			}
		}
		/*
		 * Try the current window first, if there is output
		 * then process it and go back to the top to try again.
		 * This can lead to starvation of the other windows,
		 * but presumably that what we want.
		 * Update will eventually happen when output from wwcurwin
		 * dies down.
		 */
		if ((w = wwcurwin) != 0 && w->ww_pty >= 0 &&
		    w->ww_obq > w->ww_obp &&
		    !ISSET(w->ww_pflags, WWP_STOPPED)) {
			n = wwwrite(w, w->ww_obp, w->ww_obq - w->ww_obp);
			if ((w->ww_obp += n) == w->ww_obq)
				w->ww_obq = w->ww_obp = w->ww_ob;
			noblock = 1;
			continue;
		}
		for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw)
			if (w->ww_pty >= 0 && w->ww_obq > w->ww_obp &&
			    !ISSET(w->ww_pflags, WWP_STOPPED)) {
				n = wwwrite(w, w->ww_obp,
					w->ww_obq - w->ww_obp);
				if ((w->ww_obp += n) == w->ww_obq)
					w->ww_obq = w->ww_obp = w->ww_ob;
				if (wwinterrupt())
					break;
			}
	}
}
