#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>

enum
{
	Threshold = 5,
};

enum
{
	Emouse,
	Eresize,
	Ekeyboard,
};

Mousectl *mctl;
Keyboardctl *kctl;
Image *bg;
Image *p;
Image *n;
Point pos, oldpos;
enum { Mcrop, Mundo, Msave, Mexit };
char  *menustr[] = { "crop", "undo", "save", "exit", 0 };
Menu   menu = { menustr };

void
redraw(void)
{
	draw(screen, screen->r, bg, nil, ZP);
	draw(screen, rectaddpt(n->r, addpt(pos, screen->r.min)), n, nil, n->r.min);
	flushimage(display, 1);
}

void
translate(Point d)
{
	Rectangle r, nr;

	if(d.x==0 && d.y==0)
		return;
	r = rectaddpt(n->r, addpt(pos, screen->r.min));
	pos = addpt(pos, d);
	nr = rectaddpt(r, d);
	draw(screen, screen->r, bg, nil, ZP);
	draw(screen, nr, n, nil, n->r.min);
	flushimage(display, 1);
}

void
crop(void)
{
	Rectangle r;
	Point o;
	Image *i;

	r = getrect(1, mctl);
	if(eqrect(r, ZR) || badrect(r) || (Dx(r)<Threshold && Dy(r)<Threshold))
		return;
	o = subpt(r.min, screen->r.min);
	o = addpt(n->r.min, o);
	o = subpt(o, addpt(n->r.min, pos));
	/* clamp rect to image bounds */
	if(o.x < n->r.min.x) o.x = n->r.min.x;
	if(o.y < n->r.min.y) o.y = n->r.min.y;
	if((o.x + Dx(r)) > n->r.max.x) r.max.x = r.min.x + n->r.max.x - o.x;
	if((o.y + Dy(r)) > n->r.max.y) r.max.y = r.min.y + n->r.max.y - o.y;
	i = allocimage(display, r, n->chan, 0, DTransparent);
	if(i==nil)
		sysfatal("allocimage: %r");
	draw(i, i->r, n, nil, o);
	if(p)
		freeimage(p);
	p = n;
	n = i;
	oldpos = pos;
	pos = subpt(ZP, n->r.min);
	redraw();
}

void
save(void)
{
	char buf[4096] = {0};
	int i, fd;

	i = enter("Save as:", buf, sizeof buf, mctl, kctl, nil);
	if(i<=0)
		return;
	fd = create(buf, OWRITE, 0644);
	if(fd<0)
		sysfatal("create: %r");
	i = writeimage(fd, n, 0);
	if(i<0)
		sysfatal("writeimage: %r");
	close(fd);
}

void
undo(void)
{
	if(p==nil)
		return;
	freeimage(n);
	n = p;
	p = nil;
	pos = oldpos;
	redraw();
}

void
menu3hit(void)
{
	int i;

	i = menuhit(3, mctl, &menu, nil);
	switch(i){
	case Mcrop:
		crop();
		break;
	case Mundo:
		undo();
		break;
	case Msave:
		save();
		break;
	case Mexit:
		threadexitsall(nil);
	}
}

void
usage(char *n)
{
	fprint(2, "usage: %s <[image]>\n", n);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mouse m;
	Rune k;
	Point o;
	int fd;
	Alt a[] = {
		{ nil, &m,  CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k,  CHANRCV },
		{ nil, nil, CHANEND },
	};

	if(argc > 2)
		usage(argv[0]);
	fd = 0;
	if(argc==2){
		fd = open(argv[1], OREAD);
		if(fd<0)
			sysfatal("open: %r");
	}
	if(initdraw(nil, nil, "vcrop")<0)
		sysfatal("initdraw: %r");
	display->locking = 0;
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kctl = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	a[Emouse].c = mctl->c;
	a[Eresize].c = mctl->resizec;
	a[Ekeyboard].c = kctl->c;
	bg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xCCCCCCFF);
	n = readimage(display, fd, 0);
	if(n==nil)
		sysfatal("readimage: %r");
	close(fd);
	p = nil;
	pos = subpt(ZP, n->r.min);
	oldpos = pos;
	redraw();
	for(;;){
		switch(alt(a)){
		case Emouse:
			if(m.buttons==1){
				for(;;) {
					o = m.xy;
					if(!readmouse(mctl))
						break;
					if((mctl->buttons & 1) == 0)
						break;
					translate(subpt(mctl->xy, o));
				}
			}else if(m.buttons==2){
				mctl->buttons = 1;
				crop();
			}else if(m.buttons==4)
				menu3hit();
			break;
		case Eresize:
			if(getwindow(display, Refnone)<0)
				sysfatal("cannot reattach: %r");
			redraw();
			break;
		case Ekeyboard:
			if(k==Kdel)
				threadexitsall(nil);
			break;
		}
	}
}
