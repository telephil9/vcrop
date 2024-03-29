#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

enum
{
	Threshold = 5,
};

Image *bg;
Image *p;
Image *n;
Point pos, oldpos;
enum { Mcrop, Mundo, Msave, Mexit };
char  *menustr[] = { "crop", "undo", "save", "exit", 0 };
Menu   menu = { menustr };

void
eresized(int new)
{
	if(new && getwindow(display, Refnone)<0)
		sysfatal("cannot reattach: %r");
	draw(screen, screen->r, bg, nil, ZP);
	draw(screen, rectaddpt(n->r, addpt(pos, screen->r.min)), n, nil, n->r.min);
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
}

void
crop(Mouse *m)
{
	Rectangle r;
	Point o;
	Image *i;

	r = egetrect(1, m);
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
	eresized(0);
}

void
save(Mouse *m)
{
	char buf[255];
	int i, fd;

	i = eenter("Save as:", buf, sizeof buf, m);
	if(i<0)
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
	eresized(0);
}

void
menu3hit(Mouse *m)
{
	int i;

	i = emenuhit(3, m, &menu);
	switch(i){
	case Mcrop:
		m->buttons = 1;
		crop(m);
		break;
	case Mundo:
		undo();
		break;
	case Msave:
		save(m);
		break;
	case Mexit:
		exits(nil);
	}
}

void
usage(char *n)
{
	fprint(2, "usage: %s [image]\n", n);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Event ev;
	Mouse m;
	Point o;
	int e, fd;

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
	einit(Emouse|Ekeyboard);
	bg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xCCCCCCFF);
	n = readimage(display, fd, 0);
	if(n==nil)
		sysfatal("readimage: %r");
	close(fd);
	p = nil;
	pos = subpt(ZP, n->r.min);
	oldpos = pos;
	eresized(0);
	for(;;){
		e = event(&ev);
		switch(e){
		case Emouse:
			if(ev.mouse.buttons==1){
				m = ev.mouse;
				for(;;) {
					o = m.xy;
					m = emouse();
					if((m.buttons & 1) == 0)
						break;
					translate(subpt(m.xy, o));
				}
			}else if(ev.mouse.buttons==2){
				ev.mouse.buttons = 1;
				crop(&ev.mouse);
			}else if(ev.mouse.buttons==4)
				menu3hit(&ev.mouse);
			break;
		case Ekeyboard:
			if(ev.kbdc==Kdel)
				exits(nil);
			break;
		}
	}
}
