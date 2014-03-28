--- ./dwm.c.orig	2014-03-24 14:44:44.073841600 +0100
+++ ./dwm.c	2014-03-24 14:43:00.564814335 +0100
@@ -49,6 +49,7 @@
 #define LENGTH(X)               (sizeof X / sizeof X[0])
 #define MAX(A, B)               ((A) > (B) ? (A) : (B))
 #define MIN(A, B)               ((A) < (B) ? (A) : (B))
+#define MAXCOLORS               21
 #define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
 #define WIDTH(X)                ((X)->w + 2 * (X)->bw)
 #define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
@@ -86,11 +87,12 @@
 	char name[256];
 	float mina, maxa;
 	int x, y, w, h;
+	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
 	int oldx, oldy, oldw, oldh;
 	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
 	int bw, oldbw;
 	unsigned int tags;
-	Bool isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
+	Bool isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, needresize, iscentred;
 	Client *next;
 	Client *snext;
 	Monitor *mon;
@@ -99,9 +101,8 @@
 
 typedef struct {
 	int x, y, w, h;
-	unsigned long norm[ColLast];
-	unsigned long sel[ColLast];
-	Drawable drawable;
+	unsigned long colors[MAXCOLORS][ColLast];
+ 	Drawable drawable;
 	GC gc;
 	struct {
 		int ascent;
@@ -126,8 +127,6 @@
 
 struct Monitor {
 	char ltsymbol[16];
-	float mfact;
-	int nmaster;
 	int num;
 	int by;               /* bar geometry */
 	int mx, my, mw, mh;   /* screen size */
@@ -143,14 +142,27 @@
 	Monitor *next;
 	Window barwin;
 	const Layout *lt[2];
+	int curtag;
+	int prevtag;
+	const Layout **lts;
+	double *mfacts;
+	int *nmasters;
 };
 
 typedef struct {
+	const char *name;
+	const Layout *layout;
+	float mfact;
+	int nmaster;
+} Tag;
+
+typedef struct {
 	const char *class;
 	const char *instance;
 	const char *title;
 	unsigned int tags;
 	Bool isfloating;
+	Bool iscentred;
 	int monitor;
 } Rule;
 
@@ -178,8 +190,9 @@
 static Monitor *dirtomon(int dir);
 static void drawbar(Monitor *m);
 static void drawbars(void);
-static void drawsquare(Bool filled, Bool empty, Bool invert, unsigned long col[ColLast]);
-static void drawtext(const char *text, unsigned long col[ColLast], Bool invert);
+static void drawcoloredtext(char *text);
+static void drawsquare(Bool filled, Bool empty, unsigned long col[ColLast]);
+static void drawtext(const char *text, unsigned long col[ColLast], Bool pad);
 static void enternotify(XEvent *e);
 static void expose(XEvent *e);
 static void focus(Client *c);
@@ -230,6 +243,7 @@
 static void tile(Monitor *);
 static void togglebar(const Arg *arg);
 static void togglefloating(const Arg *arg);
+static void togglescratch(const Arg *arg);
 static void toggletag(const Arg *arg);
 static void toggleview(const Arg *arg);
 static void unfocus(Client *c, Bool setfocus);
@@ -251,6 +265,7 @@
 static int xerrordummy(Display *dpy, XErrorEvent *ee);
 static int xerrorstart(Display *dpy, XErrorEvent *ee);
 static void zoom(const Arg *arg);
+static void bstack(Monitor *m);
 
 /* variables */
 static const char broken[] = "broken";
@@ -287,6 +302,8 @@
 /* configuration, allows nested code to access above variables */
 #include "config.h"
 
+static unsigned int scratchpadtag = 1 << LENGTH(tags); /* This tag specially for you, Scratchpad. */
+
 /* compile-time check if all tags fit into an unsigned int bit array. */
 struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };
 
@@ -301,6 +318,7 @@
 
 	/* rule matching */
 	c->isfloating = c->tags = 0;
+	c->iscentred = 1;
 	XGetClassHint(dpy, c->win, &ch);
 	class    = ch.res_class ? ch.res_class : broken;
 	instance = ch.res_name  ? ch.res_name  : broken;
@@ -312,6 +330,7 @@
 		&& (!r->instance || strstr(instance, r->instance)))
 		{
 			c->isfloating = r->isfloating;
+			c->iscentred = r->iscentred;
 			c->tags |= r->tags;
 			for(m = mons; m && m->num != r->monitor; m = m->next);
 			if(m)
@@ -441,7 +460,7 @@
 	if(ev->window == selmon->barwin) {
 		i = x = 0;
 		do
-			x += TEXTW(tags[i]);
+			x += TEXTW(tags[i].name);
 		while(ev->x >= x && ++i < LENGTH(tags));
 		if(i < LENGTH(tags)) {
 			click = ClkTagBar;
@@ -513,6 +532,9 @@
 	}
 	XUnmapWindow(dpy, mon->barwin);
 	XDestroyWindow(dpy, mon->barwin);
+	free(mon->mfacts);
+	free(mon->nmasters);
+	free(mon->lts);
 	free(mon);
 }
 
@@ -646,17 +668,31 @@
 Monitor *
 createmon(void) {
 	Monitor *m;
+	int i, numtags = LENGTH(tags) + 1;
 
 	if(!(m = (Monitor *)calloc(1, sizeof(Monitor))))
 		die("fatal: could not malloc() %u bytes\n", sizeof(Monitor));
+	if(!(m->mfacts = calloc(numtags, sizeof(double))))
+		die("fatal: could not malloc() %u bytes\n", sizeof(double) * numtags);
+	if(!(m->nmasters = calloc(numtags, sizeof(int))))
+		die("fatal: could not malloc() %u bytes\n", sizeof(int) * numtags);
+	if(!(m->lts = calloc(numtags, sizeof(Layout *))))
+		die("fatal: could not malloc() %u bytes\n", sizeof(Layout *) * numtags);
 	m->tagset[0] = m->tagset[1] = 1;
-	m->mfact = mfact;
-	m->nmaster = nmaster;
+	m->mfacts[0] = mfact;
+	m->nmasters[0] = nmaster;
+	m->lts[0] = &layouts[0];
 	m->showbar = showbar;
 	m->topbar = topbar;
-	m->lt[0] = &layouts[0];
+	m->curtag = m->prevtag = 1;
+	for(i = 1; i < numtags; i++) {
+		m->mfacts[i] = tags[i - 1].mfact < 0 ? mfact : tags[i - 1].mfact;
+		m->nmasters[i] = tags[i - 1].nmaster < 0 ? nmaster : tags[i - 1].nmaster;
+		m->lts[i] = tags[i - 1].layout;
+	}
+	m->lt[0] = m->lts[m->curtag];
 	m->lt[1] = &layouts[1 % LENGTH(layouts)];
-	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
+	strncpy(m->ltsymbol, m->lt[0]->symbol, sizeof m->ltsymbol);
 	return m;
 }
 
@@ -729,15 +765,14 @@
 	}
 	dc.x = 0;
 	for(i = 0; i < LENGTH(tags); i++) {
-		dc.w = TEXTW(tags[i]);
-		col = m->tagset[m->seltags] & 1 << i ? dc.sel : dc.norm;
-		drawtext(tags[i], col, urg & 1 << i);
-		drawsquare(m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
-		           occ & 1 << i, urg & 1 << i, col);
+		dc.w = TEXTW(tags[i].name);
+		col = dc.colors[ (m->tagset[m->seltags] & 1 << i ? 1:(urg & 1 << i ? 2:0))];
+		drawtext(tags[i].name, col, True);
+		drawsquare(m == selmon && selmon->sel && selmon->sel->tags & 1 << i, occ & 1 << i, col);
 		dc.x += dc.w;
 	}
 	dc.w = blw = TEXTW(m->ltsymbol);
-	drawtext(m->ltsymbol, dc.norm, False);
+	drawtext(m->ltsymbol, dc.colors[0], True);
 	dc.x += dc.w;
 	x = dc.x;
 	if(m == selmon) { /* status is only drawn on selected monitor */
@@ -747,19 +782,19 @@
 			dc.x = x;
 			dc.w = m->ww - x;
 		}
-		drawtext(stext, dc.norm, False);
+		drawcoloredtext(stext);
 	}
 	else
 		dc.x = m->ww;
 	if((dc.w = dc.x - x) > bh) {
 		dc.x = x;
 		if(m->sel) {
-			col = m == selmon ? dc.sel : dc.norm;
-			drawtext(m->sel->name, col, False);
-			drawsquare(m->sel->isfixed, m->sel->isfloating, False, col);
+			col = m == selmon ? dc.colors[1] : dc.colors[0];
+			drawtext(m->sel->name, col, True);
+			drawsquare(m->sel->isfixed, m->sel->isfloating, col);
 		}
 		else
-			drawtext(NULL, dc.norm, False);
+			drawtext(NULL, dc.colors[0], False);
 	}
 	XCopyArea(dpy, dc.drawable, m->barwin, dc.gc, 0, 0, m->ww, bh, 0, 0);
 	XSync(dpy, False);
@@ -774,10 +809,39 @@
 }
 
 void
-drawsquare(Bool filled, Bool empty, Bool invert, unsigned long col[ColLast]) {
-	int x;
+drawcoloredtext(char *text) {
+	Bool first=True;
+	char *buf = text, *ptr = buf, c = 1;
+	unsigned long *col = dc.colors[0];
+	int i, ox = dc.x;
 
-	XSetForeground(dpy, dc.gc, col[invert ? ColBG : ColFG]);
+	while( *ptr ) {
+		for( i = 0; *ptr < 0 || *ptr > NUMCOLORS; i++, ptr++);
+		if( !*ptr ) break;
+		c=*ptr;
+		*ptr=0;
+		if( i ) {
+			dc.w = selmon->ww - dc.x;
+			drawtext(buf, col, first);
+			dc.x += textnw(buf, i) + textnw(&c,1);
+			if( first ) dc.x += ( dc.font.ascent + dc.font.descent ) / 2;
+			first = False;
+		} else if( first ) {
+			ox = dc.x += textnw(&c,1);
+		}
+		*ptr = c;
+		col = dc.colors[ c-1 ];
+		buf = ++ptr;
+	}
+	if( !first ) dc.x-=(dc.font.ascent+dc.font.descent)/2;
+	drawtext(buf, col, True);
+	dc.x = ox;
+}
+
+void
+drawsquare(Bool filled, Bool empty, unsigned long col[ColLast]) {
+	int x;
+	XSetForeground(dpy, dc.gc, col[ ColFG ]);
 	x = (dc.font.ascent + dc.font.descent + 2) / 4;
 	if(filled)
 		XFillRectangle(dpy, dc.drawable, dc.gc, dc.x+1, dc.y+1, x+1, x+1);
@@ -786,17 +850,17 @@
 }
 
 void
-drawtext(const char *text, unsigned long col[ColLast], Bool invert) {
+drawtext(const char *text, unsigned long col[ColLast], Bool pad) {
 	char buf[256];
 	int i, x, y, h, len, olen;
 
-	XSetForeground(dpy, dc.gc, col[invert ? ColFG : ColBG]);
+	XSetForeground(dpy, dc.gc, col[ ColBG ]);
 	XFillRectangle(dpy, dc.drawable, dc.gc, dc.x, dc.y, dc.w, dc.h);
 	if(!text)
 		return;
 	olen = strlen(text);
-	h = dc.font.ascent + dc.font.descent;
-	y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
+	h = pad ? (dc.font.ascent + dc.font.descent) : 0;
+	y = dc.y + ((dc.h + dc.font.ascent - dc.font.descent) / 2);
 	x = dc.x + (h / 2);
 	/* shorten text if necessary */
 	for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);
@@ -805,7 +869,7 @@
 	memcpy(buf, text, len);
 	if(len < olen)
 		for(i = len; i && i > len - 3; buf[--i] = '.');
-	XSetForeground(dpy, dc.gc, col[invert ? ColBG : ColFG]);
+	XSetForeground(dpy, dc.gc, col[ ColFG ]);
 	if(dc.font.set)
 		XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
 	else
@@ -855,7 +919,7 @@
 		detachstack(c);
 		attachstack(c);
 		grabbuttons(c, True);
-		XSetWindowBorder(dpy, c->win, dc.sel[ColBorder]);
+		XSetWindowBorder(dpy, c->win, dc.colors[1][ColBorder]);
 		setfocus(c);
 	}
 	else
@@ -1028,7 +1092,7 @@
 
 void
 incnmaster(const Arg *arg) {
-	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
+	selmon->nmasters[selmon->curtag] = MAX(selmon->nmasters[selmon->curtag] + arg->i, 0);
 	arrange(selmon);
 }
 
@@ -1126,8 +1190,14 @@
 		applyrules(c);
 	}
 	/* geometry */
-	c->x = c->oldx = wa->x;
-	c->y = c->oldy = wa->y;
+	if((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && c->iscentred) {
+		c->x = c->oldx = c->mon->wx + (c->mon->ww / 2 - wa->width / 2);
+		c->y = c->oldy = c->mon->wy + (c->mon->wh / 2 - wa->height / 2);
+	}
+	else {
+		c->x = c->oldx = wa->x;
+		c->y = c->oldy = wa->y;
+	}
 	c->w = c->oldw = wa->width;
 	c->h = c->oldh = wa->height;
 	c->oldbw = wa->border_width;
@@ -1142,13 +1212,27 @@
 	           && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
 	c->bw = borderpx;
 
+	if(strcmp(c->name, scratchpadname) == 0) {
+		c->tags = scratchpadtag;
+		c->isfloating = True;
+		c->x = (c->mon->mw - c->w) - 10;
+		c->y = 20;
+		c->mon->tagset[c->mon->seltags] |= c->tags;
+	} else { /* make sure non-scratchpads stay out of scratchpadtag */
+		c->tags &= TAGMASK;
+	}
+
 	wc.border_width = c->bw;
 	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
-	XSetWindowBorder(dpy, w, dc.norm[ColBorder]);
+	XSetWindowBorder(dpy, w, dc.colors[0][ColBorder]);
 	configure(c); /* propagates border_width, if size doesn't change */
 	updatewindowtype(c);
 	updatesizehints(c);
 	updatewmhints(c);
+	c->sfx = c->x;
+	c->sfy = c->y;
+	c->sfw = c->w;
+	c->sfh = c->h;
 	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
 	grabbuttons(c, False);
 	if(!c->isfloating)
@@ -1191,7 +1275,7 @@
 
 void
 monocle(Monitor *m) {
-	unsigned int n = 0;
+	unsigned int n = 0, r = 0;
 	Client *c;
 
 	for(c = m->clients; c; c = c->next)
@@ -1199,8 +1283,17 @@
 			n++;
 	if(n > 0) /* override layout symbol */
 		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
-	for(c = nexttiled(m->clients); c; c = nexttiled(c->next))
-		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, False);
+	for(c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
+		/* remove border when in monocle layout */
+		if(c->bw) {
+			c->oldbw = c->bw;
+			c->bw = 0;
+			r = 1;
+		}
+		resize(c, m->wx, m->wy, m->ww - (2 * c->bw), m->wh - (2 * c->bw), False);
+		if(r)
+			resizeclient(c, m->wx, m->wy, m->ww - (2 * c->bw), m->wh - (2 * c->bw));
+	}
 }
 
 void
@@ -1558,7 +1651,7 @@
 	if(!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
 		selmon->sellt ^= 1;
 	if(arg && arg->v)
-		selmon->lt[selmon->sellt] = (Layout *)arg->v;
+		selmon->lt[selmon->sellt] = selmon->lts[selmon->curtag] = (Layout *)arg->v;
 	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
 	if(selmon->sel)
 		arrange(selmon);
@@ -1573,10 +1666,10 @@
 
 	if(!arg || !selmon->lt[selmon->sellt]->arrange)
 		return;
-	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
+	f = arg->f < 1.0 ? arg->f + selmon->mfacts[selmon->curtag] : arg->f - 1.0;
 	if(f < 0.1 || f > 0.9)
 		return;
-	selmon->mfact = f;
+	selmon->mfacts[selmon->curtag] = f;
 	arrange(selmon);
 }
 
@@ -1612,12 +1705,11 @@
 	cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
 	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);
 	/* init appearance */
-	dc.norm[ColBorder] = getcolor(normbordercolor);
-	dc.norm[ColBG] = getcolor(normbgcolor);
-	dc.norm[ColFG] = getcolor(normfgcolor);
-	dc.sel[ColBorder] = getcolor(selbordercolor);
-	dc.sel[ColBG] = getcolor(selbgcolor);
-	dc.sel[ColFG] = getcolor(selfgcolor);
+	for(int i=0; i<NUMCOLORS; i++) {
+		dc.colors[i][ColBorder] = getcolor( colors[i][ColBorder] );
+		dc.colors[i][ColFG] = getcolor( colors[i][ColFG] );
+		dc.colors[i][ColBG] = getcolor( colors[i][ColBG] );
+	}
 	dc.drawable = XCreatePixmap(dpy, root, DisplayWidth(dpy, screen), bh, DefaultDepth(dpy, screen));
 	dc.gc = XCreateGC(dpy, root, 0, NULL);
 	XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);
@@ -1703,28 +1795,47 @@
 
 void
 tile(Monitor *m) {
-	unsigned int i, n, h, mw, my, ty;
+	unsigned int i, n, h, mw, my, ty, r;
 	Client *c;
 
 	for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
 	if(n == 0)
 		return;
 
-	if(n > m->nmaster)
-		mw = m->nmaster ? m->ww * m->mfact : 0;
+	if(n > m->nmasters[m->curtag])
+		mw = m->nmasters[m->curtag] ? m->ww * m->mfacts[m->curtag] : 0;
 	else
 		mw = m->ww;
-	for(i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
-		if(i < m->nmaster) {
-			h = (m->wh - my) / (MIN(n, m->nmaster) - i);
+	for(i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++, r = 0) {
+		if(n == 1) {
+			if (c->bw) {
+				/* remove border when only one window is on the current tag */
+				c->oldbw = c->bw;
+				c->bw = 0;
+				r = 1;
+			}
+		}
+		else if(!c->bw && c->oldbw) {
+			/* restore border when more than one window is displayed */
+			c->bw = c->oldbw;
+			c->oldbw = 0;
+			r = 1;
+		}
+		if(i < m->nmasters[m->curtag]) {
+			h = (m->wh - my) / (MIN(n, m->nmasters[m->curtag]) - i);
 			resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
+			if(r)
+				resizeclient(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw));
 			my += HEIGHT(c);
 		}
 		else {
 			h = (m->wh - ty) / (n - i);
 			resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), False);
+			if(r)
+				resizeclient(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw));
 			ty += HEIGHT(c);
 		}
+	}
 }
 
 void
@@ -1741,20 +1852,64 @@
 		return;
 	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
 	if(selmon->sel->isfloating)
-		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
-		       selmon->sel->w, selmon->sel->h, False);
+		/* restore border when moving window into floating mode */
+		if(!selmon->sel->bw && selmon->sel->oldbw) {
+			selmon->sel->bw = selmon->sel->oldbw;
+			selmon->sel->oldbw = 0;
+		}
+	if(selmon->sel->isfloating)
+		/*restore last known float dimensions*/
+		resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
+		       selmon->sel->sfw, selmon->sel->sfh, False);
+	else {
+		/*save last known float dimensions*/
+		selmon->sel->sfx = selmon->sel->x;
+		selmon->sel->sfy = selmon->sel->y;
+		selmon->sel->sfw = selmon->sel->w;
+		selmon->sel->sfh = selmon->sel->h;
+	}
 	arrange(selmon);
 }
 
 void
+togglescratch(const Arg *arg) {
+  Client *c = NULL;
+  unsigned int found = 0;
+  /* check if a scratchpad is already there in scratchpadtag */
+  for(c = selmon->clients; c && !(found = c->tags & scratchpadtag); c = c->next);
+  if(!found) { /* not found: launch it and put it in its tag (see manage()) */
+    spawn(arg);
+    return;
+  }
+  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ scratchpadtag;
+  if(newtagset) {
+    selmon->tagset[selmon->seltags] = newtagset;
+    arrange(selmon);
+  }
+  focus(c);
+}
+
+
+void
 toggletag(const Arg *arg) {
-	unsigned int newtags;
+	unsigned int i, newtags;
 
 	if(!selmon->sel)
 		return;
 	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
 	if(newtags) {
 		selmon->sel->tags = newtags;
+		if(newtags == ~0) {
+			selmon->prevtag = selmon->curtag;
+			selmon->curtag = 0;
+		}
+		if(!(newtags & 1 << (selmon->curtag - 1))) {
+			selmon->prevtag = selmon->curtag;
+			for (i=0; !(newtags & 1 << i); i++);
+			selmon->curtag = i + 1;
+		}
+		selmon->sel->tags = newtags;
+		selmon->lt[selmon->sellt] = selmon->lts[selmon->curtag];
 		focus(NULL);
 		arrange(selmon);
 	}
@@ -1776,7 +1931,7 @@
 	if(!c)
 		return;
 	grabbuttons(c, False);
-	XSetWindowBorder(dpy, c->win, dc.norm[ColBorder]);
+	XSetWindowBorder(dpy, c->win, dc.colors[0][ColBorder]);
 	if(setfocus)
 		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
 }
@@ -2043,11 +2198,28 @@
 
 void
 view(const Arg *arg) {
-	if((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
+	unsigned int i;
+  unsigned int stag = selmon->tagset[selmon->seltags] & scratchpadtag;
+
+	if((arg->ui & TAGMASK) == (selmon->tagset[selmon->seltags] & TAGMASK))
 		return;
 	selmon->seltags ^= 1; /* toggle sel tagset */
-	if(arg->ui & TAGMASK)
+	if(arg->ui & TAGMASK) {
 		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
+		selmon->prevtag = selmon->curtag;
+		if(arg->ui == ~0)
+			selmon->curtag = 0;
+		else {
+			for (i=0; !(arg->ui & 1 << i); i++);
+			selmon->curtag = i + 1;
+		}
+	} else {
+		selmon->prevtag= selmon->curtag ^ selmon->prevtag;
+		selmon->curtag^= selmon->prevtag;
+		selmon->prevtag= selmon->curtag ^ selmon->prevtag;
+	}
+	selmon->tagset[selmon->seltags] |= stag;
+	selmon->lt[selmon->sellt]= selmon->lts[selmon->curtag];
 	focus(NULL);
 	arrange(selmon);
 }
@@ -2126,6 +2298,32 @@
 	pop(c);
 }
 
+void
+bstack(Monitor *m) {
+	unsigned int i, n, w, mh, mx, tx;
+	Client *c;
+
+	for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
+	if(n == 0)
+		return;
+
+	if(n > m->nmasters[m->curtag])
+		mh = m->nmasters[m->curtag] ? m->wh * m->mfacts[m->curtag] : 0;
+	else
+		mh = m->wh;
+	for(i = mx = tx = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
+		if(i < m->nmasters[m->curtag]) {
+			w = (m->ww - mx) / (MIN(n, m->nmasters[m->curtag]) - i);
+			resize(c, m->wx + mx, m->wy, w - (2*c->bw), mh - (2*c->bw), False);
+			mx += WIDTH(c);
+		}
+		else {
+			w = (m->ww - tx) / (n - i);
+			resize(c, m->wx + tx, m->wy + mh, w - (2*c->bw), m->wh - mh - (2*c->bw), False);
+			tx += WIDTH(c);
+		}
+}
+
 int
 main(int argc, char *argv[]) {
 	if(argc == 2 && !strcmp("-v", argv[1]))
