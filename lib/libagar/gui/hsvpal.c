/*
 * Copyright (c) 2005-2007 Hypertriton, Inc. <http://hypertriton.com/>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <core/core.h>
#include <core/config.h>

#include "view.h"

#include <rg/prim.h>					/* XXX depends on rg */

#include "hsvpal.h"
#include "primitive.h"
#include "numerical.h"
#include "gui_math.h"

#include <string.h>

static float cH = 0.0, cS = 0.0, cV = 0.0, cA = 0.0;	/* Copy buffer */

#ifdef AG_THREADS
static AG_Mutex CopyLock = AG_MUTEX_INITIALIZER;
#endif

static void RenderPalette(AG_HSVPal *);

AG_HSVPal *
AG_HSVPalNew(void *parent, Uint flags)
{
	AG_HSVPal *pal;

	pal = Malloc(sizeof(AG_HSVPal));
	AG_ObjectInit(pal, &agHSVPalClass);
	pal->flags |= flags;

	if (flags & AG_HSVPAL_HFILL) { AG_ExpandHoriz(pal); }
	if (flags & AG_HSVPAL_VFILL) { AG_ExpandVert(pal); }

	AG_ObjectAttach(parent, pal);
	return (pal);
}

/* Return the 8-bit representation of the current alpha value. */
static __inline__ Uint8
GetAlpha8(AG_HSVPal *pal)
{
	AG_Variable *bAlpha;
	Uint8 a = 255;
	void *p;

	bAlpha = AG_GetVariable(pal, "alpha", &p);
	switch (AG_VARIABLE_TYPE(bAlpha)) {
	case AG_VARIABLE_FLOAT:
		a = (Uint8)((*(float *)p)*255.0);
		break;
	case AG_VARIABLE_DOUBLE:
		a = (Uint8)((*(double *)p)*255.0);
		break;
	case AG_VARIABLE_INT:
		a = (int)((*(int *)p));
		break;
	case AG_VARIABLE_UINT8:
		a = (int)((*(Uint8 *)p));
		break;
	default:
		break;
	}
	AG_UnlockVariable(bAlpha);
	return (a);
}
	
static __inline__ void
SetAlpha8(AG_HSVPal *pal, Uint8 a)
{
	AG_Variable *bAlpha;
	void *pAlpha;

	bAlpha = AG_GetVariable(pal, "alpha", &pAlpha);
	switch (AG_VARIABLE_TYPE(bAlpha)) {
	case AG_VARIABLE_FLOAT:
		*(float *)pAlpha = (float)(((float)a)/255.0);
		break;
	case AG_VARIABLE_DOUBLE:
		*(double *)pAlpha = (double)(((double)a)/255.0);
		break;
	case AG_VARIABLE_INT:
		*(int *)pAlpha = (int)a;
		break;
	case AG_VARIABLE_UINT8:
		*(Uint8 *)pAlpha = (Uint8)a;
		break;
	default:
		break;
	}
	AG_UnlockVariable(bAlpha);
}

static __inline__ void
UpdatePixelFromHSVA(AG_HSVPal *pal)
{
	Uint8 r, g, b, a;
	AG_Variable *bFormat, *bv;
	AG_PixelFormat **pFormat;
	void *v;

	AG_HSV2RGB(AG_GetFloat(pal, "hue"),
	           AG_GetFloat(pal, "saturation"),
		   AG_GetFloat(pal, "value"), &r, &g, &b);
	a = GetAlpha8(pal);
	
	if ((bv = AG_GetVariable(pal, "RGBv", &v)) != NULL) {
		switch (AG_VARIABLE_TYPE(bv)) {
		case AG_VARIABLE_FLOAT:
			((float *)v)[0] = (float)r/255.0;
			((float *)v)[1] = (float)g/255.0;
			((float *)v)[2] = (float)b/255.0;
			break;
		case AG_VARIABLE_DOUBLE:
			((double *)v)[0] = (double)r/255.0;
			((double *)v)[1] = (double)g/255.0;
			((double *)v)[2] = (double)b/255.0;
			break;
		case AG_VARIABLE_INT:
			((int *)v)[0] = (int)r;
			((int *)v)[1] = (int)g;
			((int *)v)[2] = (int)b;
			break;
		case AG_VARIABLE_UINT8:
			((Uint8 *)v)[0] = r;
			((Uint8 *)v)[1] = g;
			((Uint8 *)v)[2] = b;
			break;
		default:
			break;
		}
		AG_UnlockVariable(bv);
	}
	if ((bv = AG_GetVariable(pal, "RGBAv", &v)) != NULL) {
		switch (AG_VARIABLE_TYPE(bv)) {
		case AG_VARIABLE_FLOAT:
			((float *)v)[0] = (float)r/255.0;
			((float *)v)[1] = (float)g/255.0;
			((float *)v)[2] = (float)b/255.0;
			((float *)v)[3] = (float)a/255.0;
			break;
		case AG_VARIABLE_DOUBLE:
			((double *)v)[0] = (double)r/255.0;
			((double *)v)[1] = (double)g/255.0;
			((double *)v)[2] = (double)b/255.0;
			((double *)v)[3] = (double)a/255.0;
			break;
		case AG_VARIABLE_INT:
			((int *)v)[0] = (int)r;
			((int *)v)[1] = (int)g;
			((int *)v)[2] = (int)b;
			((int *)v)[3] = (int)a;
			break;
		case AG_VARIABLE_UINT8:
			((Uint8 *)v)[0] = r;
			((Uint8 *)v)[1] = g;
			((Uint8 *)v)[2] = b;
			((Uint8 *)v)[3] = a;
			break;
		default:
			break;
		}
		AG_UnlockVariable(bv);
	}
	
	bFormat = AG_GetVariable(pal, "pixel-format", &pFormat);
	AG_SetUint32(pal, "pixel", AG_MapRGBA(*pFormat, r,g,b,a));
	AG_UnlockVariable(bFormat);
}

static void
UpdateHSVFromPixel(AG_HSVPal *hsv, Uint32 pixel)
{
	Uint8 r, g, b, a;
	float h, s, v;
	AG_Variable *bFormat;
	AG_PixelFormat **pFormat;
	
	bFormat = AG_GetVariable(hsv, "pixel-format", &pFormat);
	AG_GetRGBA(pixel, *pFormat, &r,&g,&b,&a);
	AG_RGB2HSV(r, g, b, &h,&s,&v);
	AG_SetFloat(hsv, "hue", h);
	AG_SetFloat(hsv, "saturation", s);
	AG_SetFloat(hsv, "value", v);
	SetAlpha8(hsv, a);
	AG_UnlockVariable(bFormat);
}

static void
UpdateHSVFromRGBAv(AG_HSVPal *hsv)
{
	AG_Variable *bRGBAv;
	void *RGBAv;
	Uint8 r, g, b, a;
	float h, s, v;

	if ((bRGBAv = AG_GetVariable(hsv, "RGBAv", &RGBAv)) == NULL) {
		return;
	}
	switch (AG_VARIABLE_TYPE(bRGBAv)) {
	case AG_VARIABLE_FLOAT:
		r = (Uint8)(((float *)RGBAv)[0] * 255.0);
		g = (Uint8)(((float *)RGBAv)[1] * 255.0);
		b = (Uint8)(((float *)RGBAv)[2] * 255.0);
		a = (Uint8)(((float *)RGBAv)[3] * 255.0);
		break;
	case AG_VARIABLE_DOUBLE:
		r = (Uint8)(((double *)RGBAv)[0] * 255.0);
		g = (Uint8)(((double *)RGBAv)[1] * 255.0);
		b = (Uint8)(((double *)RGBAv)[2] * 255.0);
		a = (Uint8)(((double *)RGBAv)[3] * 255.0);
		break;
	case AG_VARIABLE_INT:
		r = (Uint8)(((int *)RGBAv)[0]);
		g = (Uint8)(((int *)RGBAv)[1]);
		b = (Uint8)(((int *)RGBAv)[2]);
		a = (Uint8)(((int *)RGBAv)[3]);
		break;
	case AG_VARIABLE_UINT8:
		r = ((Uint8 *)RGBAv)[0];
		g = ((Uint8 *)RGBAv)[1];
		b = ((Uint8 *)RGBAv)[2];
		a = ((Uint8 *)RGBAv)[3];
		break;
	default:
		r = 0;
		g = 0;
		b = 0;
		a = 0;
		break;
	}

	AG_RGB2HSV(r, g, b, &h, &s, &v);
	AG_SetFloat(hsv, "hue", h);
	AG_SetFloat(hsv, "saturation", s);
	AG_SetFloat(hsv, "value", v);
	SetAlpha8(hsv, a);
	AG_UnlockVariable(bRGBAv);
	hsv->flags |= AG_HSVPAL_DIRTY;
}

static void
UpdateHSVFromRGBv(AG_HSVPal *hsv)
{
	AG_Variable *bRGBv;
	void *RGBv;
	Uint8 r, g, b;
	float h, s, v;

	if ((bRGBv = AG_GetVariable(hsv, "RGBv", &RGBv)) == NULL) {
		return;
	}
	switch (AG_VARIABLE_TYPE(bRGBv)) {
	case AG_VARIABLE_FLOAT:
		r = (Uint8)(((float *)RGBv)[0] * 255.0);
		g = (Uint8)(((float *)RGBv)[1] * 255.0);
		b = (Uint8)(((float *)RGBv)[2] * 255.0);
		break;
	case AG_VARIABLE_DOUBLE:
		r = (Uint8)(((double *)RGBv)[0] * 255.0);
		g = (Uint8)(((double *)RGBv)[1] * 255.0);
		b = (Uint8)(((double *)RGBv)[2] * 255.0);
		break;
	case AG_VARIABLE_INT:
		r = (Uint8)(((int *)RGBv)[0]);
		g = (Uint8)(((int *)RGBv)[1]);
		b = (Uint8)(((int *)RGBv)[2]);
		break;
	case AG_VARIABLE_UINT8:
		r = ((Uint8 *)RGBv)[0];
		g = ((Uint8 *)RGBv)[1];
		b = ((Uint8 *)RGBv)[2];
		break;
	default:
		r = 0;
		g = 0;
		b = 0;
		break;
	}

	AG_RGB2HSV(r, g, b, &h, &s, &v);
	AG_SetFloat(hsv, "hue", h);
	AG_SetFloat(hsv, "saturation", s);
	AG_SetFloat(hsv, "value", v);
	AG_UnlockVariable(bRGBv);
	hsv->flags |= AG_HSVPAL_DIRTY;
}

static void
UpdateHue(AG_HSVPal *pal, int x, int y)
{
	float h;

	h = Atan2((float)y, (float)x);
	if (h < 0) {
		h += (float)(2*AG_PI);
	}
	AG_SetFloat(pal, "hue", h/(2*AG_PI)*360.0);

	UpdatePixelFromHSVA(pal);
	AG_PostEvent(NULL, pal, "h-changed", NULL);
	pal->flags |= AG_HSVPAL_DIRTY;
}

static void
UpdateSV(AG_HSVPal *pal, int ax, int ay)
{
	float s, v;
	int x = ax - pal->triangle.x;
	int y = ay - pal->triangle.y;

	if (x < -y/2) { x = -y/2; }
	if (x > y/2) { x = y/2; }
	if (y > pal->triangle.h-1) { y = pal->triangle.h-1; }

	s = 1.0 - (float)y/(float)pal->triangle.h;
	v = 1.0 - (float)(x + y/2)/(float)pal->triangle.h;

	if (s < 0.0F) { s = 0.00001F; }
	else if (s > 1.0F) { s = 1.0F; }
	if (v < 0.0F) { v = 0.0001F; }
	else if (v > 1.0F) { v = 1.0F; }

	AG_SetFloat(pal, "saturation", s);
	AG_SetFloat(pal, "value", v);

	UpdatePixelFromHSVA(pal);
	AG_PostEvent(NULL, pal, "sv-changed", NULL);
	pal->flags |= AG_HSVPAL_DIRTY;
}

static void
UpdateAlpha(AG_HSVPal *pal, int x)
{
	AG_Variable *bAlpha;
	void *pAlpha;

	bAlpha = AG_GetVariable(pal, "alpha", &pAlpha);
	switch (AG_VARIABLE_TYPE(bAlpha)) {
	case AG_VARIABLE_FLOAT:
		*(float *)pAlpha = ((float)x)/((float)pal->rAlpha.w);
		if (*(float *)pAlpha > 1.0) {
			*(float *)pAlpha = 1.0;
		} else if (*(float *)pAlpha < 0.0) {
			*(float *)pAlpha = 0.0;
		}
		break;
	case AG_VARIABLE_DOUBLE:
		*(double *)pAlpha = ((double)x)/((double)pal->rAlpha.w);
		if (*(double *)pAlpha > 1.0) {
			*(double *)pAlpha = 1.0;
		} else if (*(double *)pAlpha < 0.0) {
			*(double *)pAlpha = 0.0;
		}
		break;
	case AG_VARIABLE_INT:
		*(int *)pAlpha = x/pal->rAlpha.w;
		if (*(int *)pAlpha > 255) {
			*(int *)pAlpha = 255;
		} else if (*(int *)pAlpha < 0) {
			*(int *)pAlpha = 0;
		}
		break;
	case AG_VARIABLE_UINT8:
		*(Uint8 *)pAlpha = (Uint8)(x/pal->rAlpha.w);
		break;
	default:
		break;
	}
	AG_UnlockVariable(bAlpha);

	UpdatePixelFromHSVA(pal);
	AG_PostEvent(NULL, pal, "a-changed", NULL);
}

static void
CloseMenu(AG_HSVPal *pal)
{
	AG_MenuCollapse(pal->menu, pal->menu_item);
	AG_ObjectDestroy(pal->menu);

	pal->menu = NULL;
	pal->menu_item = NULL;
	pal->menu_win = NULL;
}

static void
ShowRGBValue(AG_Event *event)
{
	AG_HSVPal *pal = AG_PTR(1);
	Uint8 r, g, b;
	float h, s, v;

	AG_ObjectLock(pal);
	h = AG_GetFloat(pal, "hue");
	s = AG_GetFloat(pal, "saturation");
	v = AG_GetFloat(pal, "value");
	AG_ObjectUnlock(pal);

	AG_HSV2RGB(h, s, v, &r, &g, &b);
	AG_TextMsg(AG_MSG_INFO, "%.2f,%.2f,%.2f -> %u,%u,%u", h, s, v, r, g, b);
}

#if 0
static void
EditNumValues(AG_Event *event)
{
	AG_HSVPal *pal = AG_PTR(1);
	AG_Window *pwin;
	AG_Window *win;
	AG_Numerical *num;
	AG_Variable *b1, *b2;
	float v;

	if ((pwin = AG_WidgetParentWindow(pal)) == NULL)
		return;

	if ((win = AG_WindowNewNamed(AG_WINDOW_NOMAXIMIZE, "hsvpal-%p-numedit",
	    pal)) == NULL) {
		return;
	}
	AG_WindowSetCaption(win, _("Color values"));
	AG_WindowSetPosition(win, AG_WINDOW_LOWER_LEFT, 0);
	AG_WindowSetCloseAction(win, AG_WINDOW_DETACH);
	{
		AG_Variable *bAlpha;
		void *pAlpha;

		num = AG_NumericalNew(win, 0, NULL, _("Hue: "));
		AG_NumericalSizeHint(num, "000");
		AG_WidgetCopyBinding(num, "value", pal, "hue");
		AG_NumericalSetRange(num, 0.0, 359.0);
		AG_NumericalSetIncrement(num, 1);
		AG_NumericalSetPrecision(num, "f", 0);
		
		num = AG_NumericalNew(win, 0, NULL, _("Saturation: "));
		AG_NumericalSizeHint(num, "00.00");
		AG_WidgetCopyBinding(num, "value", pal, "saturation");
		AG_NumericalSetRange(num, 0.0, 1.0);
		AG_NumericalSetIncrement(num, 0.01);
		AG_NumericalSetPrecision(num, "f", 2);

		num = AG_NumericalNew(win, 0, NULL, _("Value: "));
		AG_NumericalSizeHint(num, "00.00");
		AG_WidgetCopyBinding(num, "value", pal, "value");
		AG_NumericalSetRange(num, 0.0, 1.0);
		AG_NumericalSetIncrement(num, 0.01);
		AG_NumericalSetPrecision(num, "f", 2);

		num = AG_NumericalNew(win, 0, NULL, _("Alpha: "));
		AG_NumericalSizeHint(num, "0.000");
		AG_WidgetCopyBinding(num, "value", pal, "alpha");
		bAlpha = AG_GetVariable(pal, "alpha", &pAlpha);
		switch (AG_VARIABLE_TYPE(bAlpha)) {
		case AG_VARIABLE_FLOAT:
		case AG_VARIABLE_DOUBLE:
			AG_NumericalSetRange(num, 0.0, 1.0);
			AG_NumericalSetIncrement(num, 0.005);
			AG_NumericalSetPrecision(num, "f", 3);
			break;
		case AG_VARIABLE_INT:
		case AG_VARIABLE_UINT:
		case AG_VARIABLE_UINT8:
			AG_NumericalSetRange(num, 0.0, 255.0);
			AG_NumericalSetIncrement(num, 1.0);
			AG_NumericalSetPrecision(num, "f", 0);
			break;
		}
		AG_UnlockVariable(bAlpha);
	}

	AG_WindowAttach(pwin, win);
	AG_WindowShow(win);
}
#endif

static void
SetComplementaryColor(AG_Event *event)
{
	AG_HSVPal *pal = AG_PTR(1);
	float hue;

	AG_ObjectLock(pal);
	hue = AG_GetFloat(pal, "hue");
	AG_SetFloat(pal, "hue", ((int)hue+180) % 359);
	UpdatePixelFromHSVA(pal);
	pal->flags |= AG_HSVPAL_DIRTY;
	AG_PostEvent(NULL, pal, "h-changed", NULL);
	AG_PostEvent(NULL, pal, "sv-changed", NULL);
	AG_ObjectUnlock(pal);
}

static void
CopyColor(AG_Event *event)
{
	AG_HSVPal *pal = AG_PTR(1);
	
	AG_ObjectLock(pal);
	AG_MutexLock(&CopyLock);
	cH = AG_GetFloat(pal, "hue");
	cS = AG_GetFloat(pal, "saturation");
	cV = AG_GetFloat(pal, "value");
	cA = AG_GetFloat(pal, "alpha");
	AG_MutexUnlock(&CopyLock);
	AG_ObjectUnlock(pal);
}

static void
PasteColor(AG_Event *event)
{
	AG_HSVPal *pal = AG_PTR(1);

	AG_ObjectLock(pal);

	AG_MutexLock(&CopyLock);
	AG_SetFloat(pal, "hue", cH);
	AG_SetFloat(pal, "saturation", cS);
	AG_SetFloat(pal, "value", cV);
	AG_SetFloat(pal, "alpha", cA);
	AG_MutexUnlock(&CopyLock);
	
	UpdatePixelFromHSVA(pal);
	pal->flags |= AG_HSVPAL_DIRTY;
	AG_PostEvent(NULL, pal, "h-changed", NULL);
	AG_PostEvent(NULL, pal, "sv-changed", NULL);

	AG_ObjectUnlock(pal);
}

static void
OpenMenu(AG_HSVPal *pal)
{
	int x, y;

	if (pal->menu != NULL)
		CloseMenu(pal);

	pal->menu = AG_MenuNew(NULL, 0);
	pal->menu_item = AG_MenuAddItem(pal->menu, NULL);
	{
#if 0
		AG_MenuAction(pal->menu_item, _("Edit numerically"), NULL,
		    EditNumValues, "%p", pal);
#endif
		AG_MenuAction(pal->menu_item, _("Copy"), NULL,
		    CopyColor, "%p", pal);
		AG_MenuAction(pal->menu_item, _("Paste"), NULL,
		    PasteColor, "%p", pal);
		AG_MenuSeparator(pal->menu_item);
		AG_MenuAction(pal->menu_item, _("Show RGB value"), NULL,
		    ShowRGBValue, "%p", pal);
		AG_MenuAction(pal->menu_item, _("Complementary color"), NULL,
		    SetComplementaryColor, "%p", pal);
	}
	pal->menu->itemSel = pal->menu_item;
	
	SDL_GetMouseState(&x, &y);
	pal->menu_win = AG_MenuExpand(pal->menu, pal->menu_item, x, y);
}

static void
MouseButtonDown(AG_Event *event)
{
	AG_HSVPal *pal = AG_SELF();
	int btn = AG_INT(1);
	int x = AG_INT(2);
	int y = AG_INT(3);
	float r;

	switch (btn) {
	case SDL_BUTTON_LEFT:
		if (y > pal->rAlpha.y) {
			UpdateAlpha(pal, x);
			pal->state = AG_HSVPAL_SEL_A;
		} else {
			x -= pal->circle.x;
			y -= pal->circle.y;
			r = Hypot((float)x, (float)y);

			if (r > (float)pal->circle.rin) {
				UpdateHue(pal, x, y);
				pal->state = AG_HSVPAL_SEL_H;
			} else {
				UpdateSV(pal, AG_INT(2), AG_INT(3));
				pal->state = AG_HSVPAL_SEL_SV;
			}
		}
		AG_WidgetFocus(pal);
		break;
	case SDL_BUTTON_MIDDLE:
	case SDL_BUTTON_RIGHT:
		OpenMenu(pal);
		break;
	}
}

static void
MouseButtonUp(AG_Event *event)
{
	AG_HSVPal *pal = AG_SELF();

	pal->state = AG_HSVPAL_SEL_NONE;
}

static void
MouseMotion(AG_Event *event)
{
	AG_HSVPal *pal = AG_SELF();
	int x = AG_INT(1);
	int y = AG_INT(2);

	switch (pal->state) {
	case AG_HSVPAL_SEL_NONE:
		break;
	case AG_HSVPAL_SEL_H:
		UpdateHue(pal,
		    x - pal->circle.x,
		    y - pal->circle.y);
		break;
	case AG_HSVPAL_SEL_SV:
		UpdateSV(pal, x, y);
		break;
	case AG_HSVPAL_SEL_A:
		UpdateAlpha(pal, x);
		break;
	}
}

static void
Bound(AG_Event *event)
{
	AG_HSVPal *hsv = AG_SELF();
	AG_Variable *V = AG_PTR(1);

	if (AG_VARIABLE_TYPE(V) == AG_VARIABLE_UINT32 &&
	    strcmp(V->name, "pixel") == 0) {
#if 0
		hsv->flags |= AG_HSVPAL_PIXEL;
#endif
		UpdateHSVFromPixel(hsv, *(Uint32 *)V->data.p);
	} else if (strcmp(V->name, "RGBAv") == 0) {
		UpdateHSVFromRGBAv(hsv);
	} else if (strcmp(V->name, "RGBv") == 0) {
		UpdateHSVFromRGBv(hsv);
	}
}

static void
Init(void *obj)
{
	AG_HSVPal *pal = obj;

	WIDGET(pal)->flags |= AG_WIDGET_FOCUSABLE;

	AG_BindFloat(pal, "hue", &pal->h);
	AG_BindFloat(pal, "saturation", &pal->s);
	AG_BindFloat(pal, "value", &pal->v);
	AG_BindFloat(pal, "alpha", &pal->a);
	AG_BindUint32(pal, "pixel", &pal->pixel);
	AG_BindPointer(pal, "pixel-format", (void *)&agVideoFmt);
/*	AG_BindFloat(pal, "red", &pal->r); */
/*	AG_BindFloat(pal, "green", &pal->g); */
/*	AG_BindFloat(pal, "blue", &pal->b); */
/*	AG_BindFloat(pal, "RGBAv", &pal->rgbav); */

	pal->flags = 0;
	pal->h = 0.0;
	pal->s = 0.0;
	pal->v = 0.0;
	pal->a = 1.0;
	pal->pixel = AG_MapRGBA(agVideoFmt, 0,0,0,255);
	pal->circle.spacing = 10;
	pal->circle.width = 20;
	pal->state = AG_HSVPAL_SEL_NONE;
	pal->surface = NULL;
	pal->menu = NULL;
	pal->menu_item = NULL;
	pal->menu_win = NULL;
	AG_WidgetMapSurface(pal, NULL);

	AG_SetEvent(pal, "window-mousebuttonup", MouseButtonUp, NULL);
	AG_SetEvent(pal, "window-mousebuttondown", MouseButtonDown, NULL);
	AG_SetEvent(pal, "window-mousemotion", MouseMotion, NULL);
	AG_SetEvent(pal, "bound", Bound, NULL);
}

static void
RenderPalette(AG_HSVPal *pal)
{
	float h, cur_h, cur_s, cur_v;
	Uint32 pc;
	Uint8 r, g, b, a, da;
	int x, y, i;
	AG_Rect rd;

	cur_h = (AG_GetFloat(pal, "hue")/360) * 2*AG_PI;
	cur_s = AG_GetFloat(pal, "saturation");
	cur_v = AG_GetFloat(pal, "value");

	AG_SurfaceLock(pal->surface);

	/* Render the circle of hues. */
	for (h = 0.0; h < 2*AG_PI; h += pal->circle.dh) {
		AG_HSV2RGB((h/(2*AG_PI)*360.0), 1.0, 1.0, &r, &g, &b);
		pc = AG_MapRGB(agVideoFmt, r,g,b);

		for (i = 0; i < pal->circle.width; i++) {
			x = (pal->circle.rout - i)*Cos(h);
			y = (pal->circle.rout - i)*Sin(h);

			AG_PUT_PIXEL2(pal->surface,
			    pal->circle.x+x,
			    pal->circle.y+y,
			    pc);
		}
	}

	/* Render the triangle of saturation and value. */
	for (y = 0; y < pal->triangle.h; y += 2) {
		float sat = (float)(pal->triangle.h - y) /
		            (float)(pal->triangle.h);

		for (x = 0; x < y; x++) {
			AG_HSV2RGB((cur_h/(2*AG_PI))*360.0, sat,
			    1.0 - ((float)x/(float)pal->triangle.h),
			    &r, &g, &b);
			pc = AG_MapRGB(agVideoFmt, r,g,b);
			AG_PUT_PIXEL2(pal->surface,
			    pal->triangle.x + x - y/2,
			    pal->triangle.y + y,
			    pc);
			AG_PUT_PIXEL2(pal->surface,
			    pal->triangle.x + x - y/2,
			    pal->triangle.y + y + 1,
			    pc);
		}
	}

	/* Render the alpha selector. */
	/* XXX overblending */
	for (y = 8; y < pal->rAlpha.h+16; y+=8) {
		for (x = 0; x < pal->rAlpha.w; x+=16) {
			rd.w = 8;
			rd.h = 8;
			rd.x = pal->rAlpha.x+x;
			rd.y = pal->rAlpha.y+y;
			AG_FillRect(pal->surface, &rd, pal->cTile);
		}
		y += 8;
		for (x = 8; x < pal->rAlpha.w; x+=16) {
			rd.w = 8;
			rd.h = 8;
			rd.x = pal->rAlpha.x+x;
			rd.y = pal->rAlpha.y+y;
			AG_FillRect(pal->surface, &rd, pal->cTile);
		}
	}
	AG_HSV2RGB((cur_h/(2*AG_PI))*360.0, cur_s, cur_v, &r, &g, &b);
	da = MIN(1, pal->surface->w/255);
	for (y = pal->rAlpha.y+8; y < pal->surface->h; y++) {
		for (x = 0, a = 0; x < pal->surface->w; x++) {
			AG_BLEND_RGBA2_CLIPPED(pal->surface, x, y,
			    r, g, b, a, AG_ALPHA_SRC);
			a = x*255/pal->surface->w;
		}
	}
	AG_SurfaceUnlock(pal->surface);
}

static void
SizeRequest(void *obj, AG_SizeReq *r)
{
	r->w = 128;
	r->h = 128;
}

static int
SizeAllocate(void *obj, const AG_SizeAlloc *a)
{
	AG_HSVPal *pal = obj;

	if (a->w < 32 || a->h < 32)
		return (-1);

	pal->rAlpha.x = 0;
	pal->rAlpha.h = 32;
	pal->rAlpha.y = a->h - 32;
	pal->rAlpha.w = a->w;
	
	pal->circle.rout = MIN(a->w, (a->h - pal->rAlpha.h))/2;
	pal->circle.rin = pal->circle.rout - pal->circle.width;
	pal->circle.dh = (float)(1.0/(pal->circle.rout*AG_PI));
	pal->circle.x = a->w/2;
	pal->circle.y = (a->h - pal->rAlpha.h)/2;

	pal->triangle.x = a->w/2;
	pal->triangle.y = pal->circle.y+pal->circle.width-pal->circle.rout;
	pal->triangle.h = pal->circle.rin*Sin((37.0/360.0)*(2*AG_PI)) -
			  pal->circle.rin*Sin((270.0/360.0)*(2*AG_PI));
	
	pal->selcircle_r = pal->circle.width/2 - 4;

	pal->flags |= AG_HSVPAL_DIRTY;
	return (0);
}

static void
Draw(void *obj)
{
	AG_HSVPal *pal = obj;
	float cur_h, cur_s, cur_v;
	Uint8 r, g, b, a;
	int x, y;

	if (WIDGET(pal)->w < 16 || WIDGET(pal)->h < 16)
		return;

	if (pal->flags & AG_HSVPAL_DIRTY) {
		pal->flags &= ~(AG_HSVPAL_DIRTY);
		pal->surface = AG_SurfaceVideoRGB(WIDTH(pal), HEIGHT(pal));
		if (pal->surface == NULL) {
			AG_FatalError(NULL);
		}
		pal->cTile = AG_MapRGB(pal->surface->format, 140,140,140);
		RenderPalette(pal);
		AG_WidgetReplaceSurface(pal, 0, pal->surface);
	}

	cur_h = (AG_GetFloat(pal, "hue") / 360.0) * 2*AG_PI;
	cur_s = AG_GetFloat(pal, "saturation");
	cur_v = AG_GetFloat(pal, "value");
	a = (Uint8)(AG_GetFloat(pal, "alpha")*255);

	AG_WidgetBlitFrom(pal, pal, 0, NULL, 0, 0);

	/* Indicate the current selection. */
	AG_DrawCircle(pal,
	    pal->circle.x + (pal->circle.rin + pal->circle.width/2)*Cos(cur_h),
	    pal->circle.y + (pal->circle.rin + pal->circle.width/2)*Sin(cur_h),
	    pal->selcircle_r,
	    AG_COLOR(HSVPAL_CIRCLE_COLOR));
	
	/* The rendering routine uses (v = 1 - x/h), so (x = -v*h + h). */
	y = (int)((1.0 - cur_s) * (float)pal->triangle.h);
	x = (int)(-(cur_v*(float)pal->triangle.h - (float)pal->triangle.h));
	if (x < 0) { x = 0; }
	if (x > y) { x = y; }
	AG_DrawCircle(pal,
	    pal->triangle.x + x - y/2,
	    pal->triangle.y + y,
	    pal->selcircle_r,
	    AG_COLOR(HSVPAL_CIRCLE_COLOR));

	x = a*pal->rAlpha.w/255;
	if (x > pal->rAlpha.w-3) { x = pal->rAlpha.w-3; }

	/* Draw the color preview. */
	AG_HSV2RGB((cur_h*360.0)/(2*AG_PI), cur_s, cur_v, &r, &g, &b);
	AG_DrawRectFilled(pal,
	    AG_RECT(pal->rAlpha.x, pal->rAlpha.y, pal->rAlpha.w, 8),
	    AG_MapRGB(agVideoFmt, r,g,b));

	/* Draw the alpha bar. */
	AG_DrawLineV(pal,
	    pal->rAlpha.x + x,
	    pal->rAlpha.y + 1,
	    pal->rAlpha.y + pal->rAlpha.h,
	    AG_COLOR(HSVPAL_BAR1_COLOR));
	AG_DrawLineV(pal,
	    pal->rAlpha.x + x + 1,
	    pal->rAlpha.y + 1,
	    pal->rAlpha.y + pal->rAlpha.h,
	    AG_COLOR(HSVPAL_BAR2_COLOR));
	AG_DrawLineV(pal,
	    pal->rAlpha.x + x + 2,
	    pal->rAlpha.y + 1,
	    pal->rAlpha.y + pal->rAlpha.h,
	    AG_COLOR(HSVPAL_BAR1_COLOR));
}

AG_WidgetClass agHSVPalClass = {
	{
		"Agar(Widget:HSVPal)",
		sizeof(AG_HSVPal),
		{ 0,0 },
		Init,
		NULL,		/* free */
		NULL,		/* destroy */
		NULL,		/* load */
		NULL,		/* save */
		NULL		/* edit */
	},
	Draw,
	SizeRequest,
	SizeAllocate
};
