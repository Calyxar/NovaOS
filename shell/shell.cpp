#include "shell.h"
#include "../kernel/drivers/keyboard/keyboard.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/drivers/mouse/mouse.h"
#include "../kernel/fs/ramfs.h"
#include "../kernel/proc/scheduler.h"
#include "../kernel/panic.h"

static char input_buf[256];
static int  input_len = 0;

#define CLR_BG      0x050520
#define CLR_WHITE   0xFFFFFF
#define CLR_CYAN    0x00E5FF
#define CLR_VIOLET  0x7B2FF7
#define CLR_PINK    0xFF0080
#define CLR_DIM     0x6666AA
#define CLR_GRAY    0x444466
#define CLR_GREEN   0x00C2A8
#define CLR_YELLOW  0xFFD700
#define CLR_RED     0xFF4444

#define SIDEBAR_W   180
#define TOPBAR_H    36
#define PADDING     10

extern uint32_t pit_ticks;
extern char kb_buf[256];
extern int  kb_head;
extern int  kb_tail;

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}
static bool startswith(const char* s, const char* p) {
    while (*p) { if (*s++ != *p++) return false; }
    return true;
}

void Shell::print(const char* s)   { Framebuffer::print(s, CLR_WHITE); }
void Shell::println(const char* s) { Framebuffer::print(s, CLR_WHITE); Framebuffer::print("\n", CLR_WHITE); }

static void p(const char* s, uint32_t c) { Framebuffer::print(s, c); }

static void print_num(uint32_t n) {
    if (!n) { p("0", CLR_WHITE); return; }
    char buf[12]; int i = 0;
    while (n) { buf[i++] = '0'+(n%10); n/=10; }
    char out[12]; for(int j=0;j<i;j++) out[j]=buf[i-1-j]; out[i]=0;
    p(out, CLR_WHITE);
}

static int content_x = SIDEBAR_W + PADDING;
static int content_y = TOPBAR_H + PADDING;

static void cp(const char* s, uint32_t c) {
    Framebuffer::Info& fb = Framebuffer::get_info();
    int px = content_x;
    for (int i = 0; s[i]; i++) {
        char ch = s[i];
        if (ch == '\n') { content_x = SIDEBAR_W+PADDING; content_y += 10; px = content_x; continue; }
        if (ch == '\b') { if (px > SIDEBAR_W+PADDING) { px -= 8; Framebuffer::draw_rect(px,content_y,8,10,CLR_BG); } content_x=px; continue; }
        Framebuffer::draw_char(ch, px, content_y, c);
        px += 8;
        if (px+8 > (int)fb.width-PADDING) { px=SIDEBAR_W+PADDING; content_y+=10; }
    }
    content_x = px;
}
static void cpln(const char* s, uint32_t c) { cp(s,c); cp("\n",c); }

// ── Wallpaper ────────────────────────────────────────────────
static uint32_t rng = 12345;
static uint32_t rnext() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }

static void draw_wallpaper() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W = fb.width, H = fb.height;
    Framebuffer::clear(0x010610);

    // Nebula glows
    struct NB { int x,y,r; uint32_t rc,gc,bc; uint32_t a; };
    NB nbs[] = {
        {(int)(W*7/10),(int)(H*2/10),(int)(H*6/10),80,0,220,18},
        {(int)(W*1/10),(int)(H*7/10),(int)(H*5/10),0,180,255,14},
        {(int)(W*9/10),(int)(H*8/10),(int)(H*5/10),200,0,120,12},
        {(int)(W*3/10),(int)(H*1/10),(int)(H*4/10),0,200,240,10},
    };
    for (int n=0;n<4;n++) {
        NB& nb=nbs[n]; int r2=nb.r*nb.r;
        for (int dy=-nb.r;dy<=nb.r;dy+=2) for (int dx=-nb.r;dx<=nb.r;dx+=2) {
            if (dx*dx+dy*dy>r2) continue;
            int px=nb.x+dx,py=nb.y+dy;
            if (px<0||py<0||px>=(int)W||py>=(int)H) continue;
            uint32_t isq=nb.r;
            uint32_t dd=(uint32_t)(dx*dx+dy*dy);
            for(uint32_t s=1;s*s<=dd;s++) isq=s;
            uint32_t fade=(nb.r-isq)*nb.a/nb.r;
            if (!fade) continue;
            uint32_t nr=nb.rc*fade/255, ng=nb.gc*fade/255, nbl=nb.bc*fade/255;
            if(nr>255)nr=255; if(ng>255)ng=255; if(nbl>255)nbl=255;
            uint32_t col=(nr<<16)|(ng<<8)|nbl;
            Framebuffer::put_pixel(px,py,col); Framebuffer::put_pixel(px+1,py,col);
            Framebuffer::put_pixel(px,py+1,col); Framebuffer::put_pixel(px+1,py+1,col);
        }
    }
    // Stars
    rng=42;
    for(int i=0;i<200;i++) { int x=rnext()%W,y=rnext()%H; uint32_t b=20+rnext()%40; Framebuffer::put_pixel(x,y,(b<<16)|(b<<8)|b); }
    for(int i=0;i<80;i++) {
        int x=rnext()%W,y=rnext()%H; uint32_t b=60+rnext()%80,t=rnext()%3;
        uint32_t col=t==0?(b<<16)|(b<<8)|b:t==1?((b/2)<<16)|((b/2)<<8)|b:(b<<16)|((b/2)<<8)|(b/2);
        Framebuffer::put_pixel(x,y,col); if(b>100) Framebuffer::put_pixel(x+1,y,col);
    }
    // Bright stars
    for(int i=0;i<12;i++) {
        int x=20+rnext()%(W-40),y=20+rnext()%(H-40);
        uint32_t gc2=rnext()%3;
        for(int r=6;r>=1;r--) {
            uint32_t b2=255/(r+1);
            uint32_t gc3=gc2==0?(0|(0<<8)|b2):gc2==1?(b2|(0<<8)|(b2/2)):(0|(b2/2<<8)|b2);
            for(int dy2=-r;dy2<=r;dy2++) for(int dx2=-r;dx2<=r;dx2++) if(dx2*dx2+dy2*dy2<=r*r) Framebuffer::put_pixel(x+dx2,y+dy2,gc3);
        }
        Framebuffer::put_pixel(x,y,0xFFFFFF);
    }
    // Orbs
    struct Orb { int x,y,r; uint32_t rc,gc,bc; };
    Orb orbs[]={ {(int)(W*7/100),(int)(H*16/100),22,0,229,255}, {(int)(W*93/100),(int)(H*32/100),14,244,114,182}, {(int)(W*12/100),(int)(H*84/100),18,123,47,247}, {(int)(W*85/100),(int)(H*88/100),10,0,229,255} };
    for(int o=0;o<4;o++) {
        Orb& orb=orbs[o]; int maxR=orb.r*4;
        for(int dy2=-maxR;dy2<=maxR;dy2++) for(int dx2=-maxR;dx2<=maxR;dx2++) {
            int d2=dx2*dx2+dy2*dy2; if(d2>maxR*maxR) continue;
            int px2=orb.x+dx2,py2=orb.y+dy2;
            if(px2<0||py2<0||px2>=(int)W||py2>=(int)H) continue;
            uint32_t dist2=1; for(uint32_t s=1;s*s<=(uint32_t)d2;s++) dist2=s;
            uint32_t fade2=(dist2<(uint32_t)maxR)?((uint32_t)maxR-dist2)*80/(uint32_t)maxR:0;
            if(!fade2) continue;
            uint32_t nr2=orb.rc*fade2/255,ng2=orb.gc*fade2/255,nb2=orb.bc*fade2/255;
            if(nr2>255)nr2=255; if(ng2>255)ng2=255; if(nb2>255)nb2=255;
            Framebuffer::put_pixel(px2,py2,((nr2&0xFF)<<16)|((ng2&0xFF)<<8)|(nb2&0xFF));
        }
        for(int dy2=-orb.r/2;dy2<=orb.r/2;dy2++) for(int dx2=-orb.r/2;dx2<=orb.r/2;dx2++) if(dx2*dx2+dy2*dy2<=(orb.r/2)*(orb.r/2)) {
            int px2=orb.x+dx2,py2=orb.y+dy2;
            if(px2<0||py2<0||px2>=(int)W||py2>=(int)H) continue;
            uint32_t nr2=orb.rc*180/255,ng2=orb.gc*180/255,nb2=orb.bc*180/255;
            Framebuffer::put_pixel(px2,py2,((nr2&0xFF)<<16)|((ng2&0xFF)<<8)|(nb2&0xFF));
        }
    }
}

// ── Topbar ───────────────────────────────────────────────────
static void draw_topbar() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W = fb.width;
    Framebuffer::draw_rect(0,0,W,TOPBAR_H,0x0A0820);
    for(uint32_t x=0;x<W;x++) {
        uint32_t t=x*255/W;
        uint32_t r=(0*(255-t)+123*t)/255, g=(229*(255-t)+47*t)/255, b=(255*(255-t)+247*t)/255;
        Framebuffer::put_pixel(x,TOPBAR_H-1,(r<<16)|(g<<8)|b);
    }
    Framebuffer::draw_circle(16,18,5,CLR_VIOLET);
    Framebuffer::draw_circle(16,18,3,CLR_CYAN);
    Framebuffer::draw_circle(16,18,1,CLR_WHITE);
    Framebuffer::print_at("NovaOS",28,14,CLR_CYAN);
    Framebuffer::print_at("Home",110,14,CLR_WHITE);
    Framebuffer::print_at("Files",155,14,CLR_DIM);
    Framebuffer::print_at("Apps",205,14,CLR_DIM);
    // Uptime
    uint32_t secs=pit_ticks/1000u;
    char ut[24]="uptime: "; int ui=8;
    char ns[12]; int ni=0; uint32_t tmp=secs;
    if(!tmp){ns[ni++]='0';} while(tmp){ns[ni++]='0'+(tmp%10);tmp/=10;}
    for(int i=0;i<ni;i++) ut[ui++]=ns[ni-1-i];
    ut[ui++]='s'; ut[ui]=0;
    Framebuffer::print_at(ut,(int)W-120,14,CLR_DIM);
}

// ── Sidebar ──────────────────────────────────────────────────
static void draw_sidebar() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t H = fb.height;
    Framebuffer::draw_rect(0,TOPBAR_H,SIDEBAR_W,H-TOPBAR_H,0x08031E);
    Framebuffer::draw_rect(SIDEBAR_W-1,TOPBAR_H,1,H-TOPBAR_H,0x1A0855);
    int y=TOPBAR_H+16;
    Framebuffer::print_at("PINNED",12,y,CLR_GRAY); y+=16;
    struct Nav { const char* label; const char* sub; uint32_t col; bool active; };
    Nav items[]={
        {"Dashboard","Home screen",CLR_CYAN,false},
        {"Nova Browser","Browse the web",CLR_CYAN,false},
        {"Nova Docs","Office suite",CLR_PINK,false},
        {"Game Mode","Launch games",CLR_PINK,false},
        {"Terminal","Nova Shell",CLR_VIOLET,true},
    };
    for(int i=0;i<5;i++){
        if(items[i].active){Framebuffer::draw_rect(4,y-2,SIDEBAR_W-8,22,0x1A0844);Framebuffer::draw_rect(4,y-2,2,22,CLR_VIOLET);}
        Framebuffer::draw_circle(18,y+7,4,items[i].col);
        Framebuffer::print_at(items[i].label,30,y,CLR_WHITE);
        Framebuffer::print_at(items[i].sub,30,y+10,CLR_GRAY);
        y+=28;
    }
    Framebuffer::draw_rect(12,y,SIDEBAR_W-24,1,0x1A0855); y+=10;
    Framebuffer::print_at("SYSTEM",12,y,CLR_GRAY); y+=16;
    Nav sys[]={ {"Nova Files","NovaFS",CLR_DIM,false}, {"Nova Store","Apps",CLR_DIM,false}, {"Settings","CPU/GPU/RAM",CLR_DIM,false} };
    for(int i=0;i<3;i++){
        Framebuffer::draw_circle(18,y+7,4,sys[i].col);
        Framebuffer::print_at(sys[i].label,30,y,CLR_DIM);
        Framebuffer::print_at(sys[i].sub,30,y+10,CLR_GRAY);
        y+=28;
    }
}

// ── Cursor ───────────────────────────────────────────────────
static void draw_cursor() {} // host cursor used instead

// ── Header ───────────────────────────────────────────────────
static void draw_header() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W=fb.width;
    draw_wallpaper();
    draw_topbar();
    draw_sidebar();
    int cx=SIDEBAR_W+PADDING, cw=(int)W-SIDEBAR_W-PADDING*2;
    Framebuffer::draw_rect(cx,TOPBAR_H+PADDING,cw,50,0x0D0530);
    for(int x=cx;x<cx+cw;x++){
        uint32_t t=(x-cx)*255/cw,r=(0*(255-t)+255*t)/255,g=(229*(255-t)+0*t)/255,b=(255*(255-t)+128*t)/255;
        Framebuffer::put_pixel(x,TOPBAR_H+PADDING,(r<<16)|(g<<8)|b);
    }
    Framebuffer::print_at("Welcome to NovaOS",cx+10,TOPBAR_H+PADDING+10,CLR_WHITE);
    Framebuffer::print_at("Productivity  |  Gaming  |  Open Source",cx+10,TOPBAR_H+PADDING+26,CLR_DIM);
    Framebuffer::draw_rect(cx,TOPBAR_H+PADDING+52,cw,1,0x1A0855);
    content_x=cx; content_y=TOPBAR_H+PADDING+62;
    cpln("  Nova Shell v0.2.0 - pixel mode",CLR_CYAN);
    cpln("  Type 'help' for commands.",CLR_DIM);
    cp("\n",CLR_WHITE);
}

// ── App panels ───────────────────────────────────────────────
static void open_panel(const char* title, uint32_t tcol, const char* line1, const char* line2) {
    Framebuffer::Info& fb=Framebuffer::get_info();
    int cx=SIDEBAR_W+PADDING, cw=(int)fb.width-SIDEBAR_W-PADDING*2;
    Framebuffer::draw_rect(cx,TOPBAR_H+PADDING,cw,120,0x080320);
    Framebuffer::draw_rect(cx,TOPBAR_H+PADDING,cw,1,tcol);
    Framebuffer::print_at(title,cx+10,TOPBAR_H+PADDING+10,tcol);
    Framebuffer::print_at(line1,cx+10,TOPBAR_H+PADDING+32,CLR_DIM);
    Framebuffer::print_at(line2,cx+10,TOPBAR_H+PADDING+48,CLR_GRAY);
    content_x=cx; content_y=TOPBAR_H+PADDING+72;
}

// ── ls callback ──────────────────────────────────────────────
static void ls_cb(const char* name, uint32_t size) {
    cp("  ",CLR_WHITE); cp(name,CLR_CYAN);
    cp("  (",CLR_GRAY); print_num(size); cpln(" bytes)",CLR_GRAY);
}

// ── Processes ────────────────────────────────────────────────
static void proc_hello()   { cpln("[hello] Hello from NovaOS!",CLR_GREEN); Scheduler::exit(0); }
static void proc_counter() { cp("[counter] ",CLR_YELLOW); for(int i=1;i<=10;i++){char s[4];s[0]='0'+i;s[1]=' ';s[2]=0;if(i==10){s[0]='1';s[1]='0';s[2]=' ';s[3]=0;}cp(s,CLR_YELLOW);} cpln("",CLR_WHITE); Scheduler::exit(0); }
static void proc_sysinfo() { cpln("[sysinfo] NovaOS v0.2.0",CLR_CYAN); cpln("[sysinfo] VESA 800x600x32",CLR_CYAN); Scheduler::exit(0); }

// ── Commands ─────────────────────────────────────────────────
static void handle_command(const char* cmd) {
    if(streq(cmd,"help")){
        cpln("\n  Commands:",CLR_CYAN);
        const char* cs[]={"help","version","about","clear","cpu","mem","uptime","echo","ls","cat","touch","write","rm","ps","run","panic","color"};
        for(int i=0;i<17;i++){cp("  ",CLR_WHITE);cpln(cs[i],CLR_GREEN);}
        cp("\n",CLR_WHITE);
    }
    else if(streq(cmd,"version")){cpln("\n  NovaOS v0.2.0 - VESA pixel GUI\n",CLR_CYAN);}
    else if(streq(cmd,"about")){cpln("\n  NovaOS - Next-Gen OS",CLR_CYAN);cpln("  Kernel: C/C++/ASM",CLR_WHITE);cpln("  Display: VESA 800x600",CLR_WHITE);cpln("  GPU: NVIDIA (roadmap)\n",CLR_GREEN);}
    else if(streq(cmd,"clear")){draw_header();}
    else if(streq(cmd,"mem")){cp("\n  Kernel: ",CLR_CYAN);cpln("0x100000",CLR_YELLOW);cp("  FB:     ",CLR_CYAN);cpln("0xFD000000",CLR_YELLOW);cp("  RAM:    ",CLR_CYAN);cpln("~30MB\n",CLR_YELLOW);}
    else if(streq(cmd,"cpu")){cp("\n  CPU: ",CLR_CYAN);cpln("x86 i686 32-bit",CLR_WHITE);cp("  Next: ",CLR_CYAN);cpln("x86-64 + NVIDIA\n",CLR_GREEN);}
    else if(streq(cmd,"uptime")){cp("\n  Uptime: ",CLR_CYAN);print_num(pit_ticks/1000u);cpln("s\n",CLR_WHITE);}
    else if(startswith(cmd,"echo ")){cp("\n  ",CLR_WHITE);cpln(cmd+5,CLR_CYAN);cp("\n",CLR_WHITE);}
    else if(streq(cmd,"ls")){cpln("\n  NovaFS Files",CLR_CYAN);RamFS::list(ls_cb);cp("\n",CLR_WHITE);}
    else if(startswith(cmd,"cat ")){RamFile* f=RamFS::find(cmd+4);if(f){cp("\n  ",CLR_WHITE);cpln(f->data,CLR_WHITE);cp("\n",CLR_WHITE);}else{cp("  Not found: ",CLR_RED);cpln(cmd+4,CLR_WHITE);}}
    else if(startswith(cmd,"touch ")){if(RamFS::create(cmd+6)){cp("  Created: ",CLR_GREEN);cpln(cmd+6,CLR_WHITE);}else cpln("  Error",CLR_RED);}
    else if(startswith(cmd,"write ")){
        const char* r=cmd+6; int i=0; while(r[i]&&r[i]!=' ')i++;
        if(r[i]==' '){char fn[32];for(int j=0;j<i&&j<31;j++)fn[j]=r[j];fn[i]=0;const char* cnt=r+i+1;uint32_t len=0;while(cnt[len])len++;
        if(RamFS::write(fn,cnt,len)){cp("  Written: ",CLR_GREEN);cpln(fn,CLR_WHITE);}else cpln("  File not found",CLR_RED);}
        else cpln("  Usage: write <file> <text>",CLR_YELLOW);
    }
    else if(startswith(cmd,"rm ")){if(RamFS::remove(cmd+3)){cp("  Deleted: ",CLR_PINK);cpln(cmd+3,CLR_WHITE);}else cpln("  Not found",CLR_RED);}
    else if(streq(cmd,"ps")){cpln("\n  PID 1  nova-shell  RUNNING\n",CLR_GREEN);}
    else if(startswith(cmd,"run ")){
        const char* prog=cmd+4;
        if(streq(prog,"hello")){Scheduler::spawn("hello",proc_hello);proc_hello();}
        else if(streq(prog,"counter")){Scheduler::spawn("counter",proc_counter);proc_counter();}
        else if(streq(prog,"sysinfo")){Scheduler::spawn("sysinfo",proc_sysinfo);proc_sysinfo();}
        else{cp("  Unknown: ",CLR_RED);cpln(prog,CLR_WHITE);}
    }
    else if(streq(cmd,"color")){cpln("  CYAN",CLR_CYAN);cpln("  VIOLET",CLR_VIOLET);cpln("  PINK",CLR_PINK);}
    else if(streq(cmd,"panic")){PANIC("Manual panic from shell");}
    else if(cmd[0]=='\0'){}
    else{cp("  Unknown: ",CLR_RED);cpln(cmd,CLR_WHITE);}
}

// ── prompt ───────────────────────────────────────────────────
static void draw_prompt() { cp("> ",CLR_CYAN); }

// ── Init ─────────────────────────────────────────────────────
void Shell::init() {
    input_len=0;
    RamFS::init();
    Scheduler::init();
    draw_header();
    draw_prompt();
}

// ── Run loop (non-blocking) ───────────────────────────────────
void Shell::run() {
    static bool last_left = false;

    while (true) {
        // Mouse
        Mouse::poll();
        Mouse::State& ms = Mouse::get_state();

        bool clicked = ms.left && !last_left;
        last_left = ms.left;
        if (f1_pressed) { f1_pressed = false; clicked = true; }


        // Show Y coordinate always in top-left
        {
            char dbg[16]; int di=0;
            int yv=ms.y; char ys[8]; int yi=0;
            if(!yv){ys[yi++]='0';}
            while(yv){ys[yi++]='0'+(yv%10);yv/=10;}
            dbg[di++]='Y'; dbg[di++]=':';
            for(int i=yi-1;i>=0;i--) dbg[di++]=ys[i];
            dbg[di++]=' ';
            int xv=ms.x; char xs[8]; int xi=0;
            if(!xv){xs[xi++]='0';}
            while(xv){xs[xi++]='0'+(xv%10);xv/=10;}
            dbg[di++]='X'; dbg[di++]=':';
            for(int i=xi-1;i>=0;i--) dbg[di++]=xs[i];
            dbg[di]=0;
            Framebuffer::draw_rect(0,0,140,14,0x000000);
            Framebuffer::print_at(dbg,2,3,0x00FF00);
        }



        // Sidebar click
        if (clicked) {
            if (ms.x >= 0 && ms.x < SIDEBAR_W) {
                // Sidebar hit test
                // Items start at y = TOPBAR_H+16+16 = TOPBAR_H+32
                // Each item is 28px tall
                // Divide sidebar into 8 equal click zones below topbar
                int hit = -1;
                int sy = ms.y - TOPBAR_H;
                if (sy >= 0) hit = sy / 36; // 36px per zone
                if (hit > 7) hit = -1;
                Framebuffer::Info& fb2 = Framebuffer::get_info();
                int cw = (int)fb2.width - SIDEBAR_W - PADDING*2;
                (void)cw;
                if (hit == 0) draw_header();
                else if (hit == 1) open_panel("Nova Browser",CLR_CYAN,"Coming soon — Phase 3 roadmap.","Full browser with Nova engine.");
                else if (hit == 2) open_panel("Nova Docs",CLR_PINK,"Office suite — word, sheets, slides.","Coming in Phase 3.");
                else if (hit == 3) open_panel("Game Mode",CLR_PINK,"NVIDIA GPU + Vulkan — Phase 4.","DLSS, FSR, Nova Overlay.");
                else if (hit == 4) draw_header();
                else if (hit == 5) {
                    open_panel("Nova Files",CLR_CYAN,"NovaFS — in-memory filesystem.","");
                    RamFS::list(ls_cb);
                }
                else if (hit == 6) open_panel("Nova Store",CLR_VIOLET,"App marketplace — Phase 5.","Native + Proton games.");
                else if (hit == 7) {
                    open_panel("Settings",CLR_WHITE,"","");
                    cpln("  CPU:  x86 i686 32-bit Protected",CLR_DIM);
                    cpln("  GPU:  NVIDIA (roadmap)",CLR_DIM);
                    cpln("  RAM:  ~30MB",CLR_DIM);
                    cpln("  VESA: 800x600x32bpp",CLR_DIM);
                    cpln("  Kernel: C/C++/ASM",CLR_DIM);
                }
            }
        }

        // Cursor

        // Topbar refresh every second
        static uint32_t ltop = 0;
        if (pit_ticks - ltop > 1000) { ltop = pit_ticks; draw_topbar(); }

        // Blink text cursor
        bool cur_on = (pit_ticks / 400) % 2 == 0;
        Framebuffer::draw_rect(content_x, content_y, 6, 9,
            cur_on ? CLR_VIOLET : CLR_BG);

        // Non-blocking keyboard
        if (kb_head == kb_tail) continue;
        Framebuffer::draw_rect(content_x, content_y, 6, 9, CLR_BG);
        char c = kb_buf[kb_tail];
        kb_tail = (kb_tail+1) % 256;

        if (c == '\n') {
            input_buf[input_len] = '\0';
            cp("\n", CLR_WHITE);
            handle_command(input_buf);
            input_len = 0;
            draw_prompt();
        } else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                content_x -= 8;
                Framebuffer::draw_rect(content_x, content_y, 8, 10, CLR_BG);
            }
        } else if (input_len < 255) {
            input_buf[input_len++] = c;
            Framebuffer::draw_char(c, content_x, content_y, CLR_WHITE);
            content_x += 8;
        }
    }
}
