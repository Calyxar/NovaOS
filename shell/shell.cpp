#include "shell.h"
#include "../kernel/drivers/keyboard/keyboard.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/drivers/mouse/mouse.h"
extern bool up_pressed;
extern bool down_pressed;
extern bool esc_pressed;
extern bool f2_pressed;
#include "../kernel/fs/ramfs.h"
#include "../kernel/proc/scheduler.h"
#include "../kernel/panic.h"
#include "../kernel/drivers/disk/ata.h"
#include "../kernel/fs/novafs_disk.h"

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
#define SEPARATOR_HEIGHT 1
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
        if (ch == '\n') { content_x = SIDEBAR_W+PADDING; content_y += 18; px = content_x; continue; }
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
    Framebuffer::print_at("[1-8: nav]",420,14,CLR_DIM);
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
#define SIDEBAR_ROW_H 50
#define SIDEBAR_TOP   (TOPBAR_H + 10)

static void draw_sidebar() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t H = fb.height;
    Framebuffer::draw_rect(0,TOPBAR_H,SIDEBAR_W,H-TOPBAR_H,0x08031E);
    Framebuffer::draw_rect(SIDEBAR_W-1,TOPBAR_H,1,H-TOPBAR_H,0x1A0855);

    struct Nav { const char* label; const char* sub; uint32_t col; bool active; };
    Nav items[] = {
        {"Dashboard",    "Home screen",    CLR_CYAN,   true},
        {"Nova Browser", "Browse the web", CLR_CYAN,   false},
        {"Nova Docs",    "Office suite",   CLR_PINK,   false},
        {"Game Mode",    "Launch games",   CLR_PINK,   false},
        {"Terminal",     "Nova Shell",     CLR_VIOLET, true},
        {"Nova Files",   "NovaFS",         CLR_DIM,    false},
        {"Nova Store",   "Apps",           CLR_DIM,    false},
        {"Settings",     "CPU/GPU/RAM",    CLR_DIM,    false},
    };

    for (int i = 0; i < 8; i++) {
        int top = SIDEBAR_TOP + i * SIDEBAR_ROW_H;
        if (items[i].active) {
            Framebuffer::draw_rounded_rect(2, top+2, SIDEBAR_W-6, SIDEBAR_ROW_H-4, 12, 0x1A0844);
            Framebuffer::draw_rect(2, top+2, 2, SIDEBAR_ROW_H-4, CLR_VIOLET);
        }
        if (i == 5) {
            Framebuffer::draw_rect(12, top-6, SIDEBAR_W-24, 1, 0x1A0855);
        }
        Framebuffer::draw_circle(20, top+18, 4, items[i].col);
        Framebuffer::print_at(items[i].label, 32, top+12, CLR_WHITE);
        Framebuffer::print_at(items[i].sub,   32, top+24, CLR_GRAY);
    }
}

// Example logic to match your draw loop exactly
static int sidebar_hit_test(int mx, int my) {
    if (mx < 0 || mx >= SIDEBAR_W) return -1;
    if (my < SIDEBAR_TOP) return -1;

    for (int i = 0; i < 8; i++) {
        // Calculate standard top position
        int top = SIDEBAR_TOP + (i * SIDEBAR_ROW_H);
        
        // If we have passed the separator (index 5), apply the offset
        if (i > 5) {
            top += SEPARATOR_HEIGHT;
        }

        // Check if the click falls within this row's boundaries
        if (my >= top && my < top + SIDEBAR_ROW_H) {
            return i;
        }
    }
    return -1;
}

static void draw_cursor() {} 

// ── Header ───────────────────────────────────────────────────
static void draw_header() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W=fb.width;
    draw_wallpaper();
    draw_topbar();
    draw_sidebar();
    int cx=SIDEBAR_W+PADDING, cw=(int)W-SIDEBAR_W-PADDING*2;
    Framebuffer::draw_rounded_rect(cx,TOPBAR_H+PADDING,cw,50,14,0x0D0530);
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

// ── Nova Files browser state ────────────────────────────────
#define MAX_FILE_ENTRIES 32
static char  file_names[MAX_FILE_ENTRIES][32];
static uint32_t file_sizes[MAX_FILE_ENTRIES];
static int   file_count = 0;
static int   file_selected = 0;
static bool  files_panel_active = false;
static bool  file_viewer_active = false;

static int file_collect_idx = 0;
static void collect_file_cb(const char* name, uint32_t size) {
    if (file_collect_idx >= MAX_FILE_ENTRIES) return;
    int i = 0;
    while (name[i] && i < 31) { file_names[file_collect_idx][i] = name[i]; i++; }
    file_names[file_collect_idx][i] = '\0';
    file_sizes[file_collect_idx] = size;
    file_collect_idx++;
}

static void draw_file_list() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    int cx = SIDEBAR_W + PADDING;
    int cw = (int)fb.width - SIDEBAR_W - PADDING*2;

    Framebuffer::draw_rounded_rect(cx, TOPBAR_H+PADDING, cw, 280, 14, 0x0A0525);
    Framebuffer::print_at("Nova Files", cx+10, TOPBAR_H+PADDING+10, CLR_CYAN);
    Framebuffer::print_at("Arrows: navigate | Enter: open | Esc: back",
                          cx+10, TOPBAR_H+PADDING+26, CLR_DIM);

    int ly = TOPBAR_H+PADDING+50;
    for (int i = 0; i < file_count; i++) {
        bool sel = (i == file_selected);
        if (sel) {
            Framebuffer::draw_rect(cx+8, ly-2, cw-16, 16, 0x1A0855);
        }
        Framebuffer::draw_circle(cx+18, ly+5, 3, sel ? CLR_CYAN : CLR_DIM);
        Framebuffer::print_at(file_names[i], cx+30, ly, sel ? CLR_WHITE : CLR_DIM);

        char sbuf[16]; int si=0;
        uint32_t s2 = file_sizes[i];
        if (!s2) sbuf[si++]='0';
        while (s2) { sbuf[si++]='0'+(s2%10); s2/=10; }
        char sout[16]; for(int j=0;j<si;j++) sout[j]=sbuf[si-1-j];
        sout[si]='b'; sout[si+1]=0;
        Framebuffer::print_at(sout, cx+cw-70, ly, CLR_GRAY);

        ly += 18;
    }

    if (file_count == 0) {
        Framebuffer::print_at("No files in NovaFS.", cx+30, ly, CLR_GRAY);
    }
}

static void draw_file_viewer() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    int cx = SIDEBAR_W + PADDING;
    int cw = (int)fb.width - SIDEBAR_W - PADDING*2;

    Framebuffer::draw_rounded_rect(cx, TOPBAR_H+PADDING, cw, 280, 14, 0x0A0525);

    static char view_buf[2048];
    uint32_t view_size = 0;
    bool found = NovaFSDisk::load_file(file_names[file_selected], view_buf, 2047, &view_size);

    Framebuffer::print_at(file_names[file_selected], cx+10, TOPBAR_H+PADDING+10, CLR_PINK);
    Framebuffer::print_at("Esc: back to file list | F2: edit in Nova Docs", cx+10, TOPBAR_H+PADDING+26, CLR_DIM);
    Framebuffer::draw_rect(cx+8, TOPBAR_H+PADDING+44, cw-16, 1, 0x1A0855);

    if (found) {
        Framebuffer::print_at(view_buf, cx+10, TOPBAR_H+PADDING+56, CLR_WHITE);
    } else {
        Framebuffer::print_at("(file not found)", cx+10, TOPBAR_H+PADDING+56, CLR_RED);
    }
}

static void open_files_panel() {
    file_collect_idx = 0;
    NovaFSDisk::list_files(collect_file_cb);
    file_count = file_collect_idx;
    file_selected = 0;
    files_panel_active = true;
    file_viewer_active = false;
    draw_file_list();
}

// ── Nova Docs editor state ──────────────────────────────────
#define DOC_BUF_SIZE 2048
static char doc_buffer[DOC_BUF_SIZE];
static int  doc_len = 0;
static char doc_filename[32] = "untitled.txt";
static bool docs_panel_active = false;
static int  doc_cursor_line = 0;
static int  doc_cursor_col = 0;

static void draw_docs_editor() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    int cx = SIDEBAR_W + PADDING;
    int cw = (int)fb.width - SIDEBAR_W - PADDING*2;

    Framebuffer::draw_rounded_rect(cx, TOPBAR_H+PADDING, cw, 320, 14, 0x0A0525);
    Framebuffer::print_at("Nova Docs", cx+10, TOPBAR_H+PADDING+10, CLR_PINK);
    Framebuffer::print_at(doc_filename, cx+110, TOPBAR_H+PADDING+10, CLR_DIM);
    Framebuffer::print_at("F2: save | Esc: back to sidebar",
                          cx+10, TOPBAR_H+PADDING+26, CLR_DIM);
    Framebuffer::draw_rect(cx+8, TOPBAR_H+PADDING+42, cw-16, 1, 0x1A0855);

    // Render the text buffer line by line
    int ty = TOPBAR_H+PADDING+54;
    int tx = cx+10;
    int line = 0, col = 0;
    int draw_x = tx, draw_y = ty;
    for (int i = 0; i < doc_len; i++) {
        char ch = doc_buffer[i];
        if (ch == '\n') {
            line++; col = 0;
            draw_x = tx; draw_y = ty + line*10;
            continue;
        }
        Framebuffer::draw_char(ch, draw_x, draw_y, CLR_WHITE);
        draw_x += 8; col++;
    }

    // Blinking cursor at current write position (always end of buffer for simplicity)
    bool cur_on = (pit_ticks / 400) % 2 == 0;
    if (cur_on) {
        Framebuffer::draw_rect(draw_x, draw_y, 6, 9, CLR_PINK);
    }
}

static void load_doc(const char* name) {
    int i = 0;
    while (name[i] && i < 31) { doc_filename[i] = name[i]; i++; }
    doc_filename[i] = '\0';

    uint32_t loaded_size = 0;
    bool ok = NovaFSDisk::load_file(name, doc_buffer, DOC_BUF_SIZE-1, &loaded_size);
    doc_len = ok ? (int)loaded_size : 0;
    doc_buffer[doc_len] = '\0';
}

static void save_doc() {
    NovaFSDisk::save_file(doc_filename, doc_buffer, (uint32_t)doc_len);
}

static void open_docs_panel() {
    // Default to a fresh untitled doc each time Nova Docs is opened from sidebar
    doc_filename[0]='u';doc_filename[1]='n';doc_filename[2]='t';doc_filename[3]='i';
    doc_filename[4]='t';doc_filename[5]='l';doc_filename[6]='e';doc_filename[7]='d';
    doc_filename[8]='.';doc_filename[9]='t';doc_filename[10]='x';doc_filename[11]='t';
    doc_filename[12]='\0';
    doc_len = 0;
    doc_buffer[0] = '\0';
    docs_panel_active = true;
    draw_docs_editor();
}

// ── App panels ───────────────────────────────────────────────
static void open_panel(const char* title, uint32_t tcol, const char* line1, const char* line2) {
    Framebuffer::Info& fb=Framebuffer::get_info();
    int cx=SIDEBAR_W+PADDING, cw=(int)fb.width-SIDEBAR_W-PADDING*2;
    Framebuffer::draw_rounded_rect(cx,TOPBAR_H+PADDING,cw,120,14,0x080320);
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
    else if(startswith(cmd,"dsave ")){
        const char* r=cmd+6; int i=0; while(r[i]&&r[i]!=' ')i++;
        if(r[i]==' '){
            char fn[32]; for(int j=0;j<i&&j<31;j++) fn[j]=r[j]; fn[i]=0;
            const char* cnt=r+i+1; uint32_t len=0; while(cnt[len]) len++;
            bool ok = NovaFSDisk::save_file(fn, cnt, len);
            cp("  Disk save: ",CLR_WHITE); cpln(ok?"OK":"FAIL", ok?CLR_GREEN:CLR_RED);
        } else cpln("  Usage: dsave <file> <text>",CLR_YELLOW);
    }
    else if(startswith(cmd,"dload ")){
        static char buf[2048];
        uint32_t sz=0;
        bool ok = NovaFSDisk::load_file(cmd+6, buf, 2047, &sz);
        if (ok) { cp("  ",CLR_WHITE); cpln(buf,CLR_CYAN); }
        else cpln("  Not found on disk",CLR_RED);
    }
    else if(streq(cmd,"dls")){
        cpln("\n  NovaFS-Disk Files",CLR_CYAN);
        NovaFSDisk::list_files(ls_cb);
        cp("\n",CLR_WHITE);
    }
    else if(streq(cmd,"disktest")){
        cpln("\n  Testing ATA disk driver...",CLR_CYAN);
        static uint8_t wbuf[512];
        static uint8_t rbuf[512];
        for (int i=0;i<512;i++) wbuf[i] = (uint8_t)(i & 0xFF);
        bool wok = ATA::write_sector(100, wbuf);
        cp("  Write sector 100: ",CLR_WHITE); cpln(wok?"OK":"FAIL", wok?CLR_GREEN:CLR_RED);
        bool rok = ATA::read_sector(100, rbuf);
        cp("  Read sector 100:  ",CLR_WHITE); cpln(rok?"OK":"FAIL", rok?CLR_GREEN:CLR_RED);
        bool match = true;
        for (int i=0;i<512;i++) if (rbuf[i]!=wbuf[i]) { match=false; break; }
        cp("  Data match:       ",CLR_WHITE); cpln(match?"OK":"MISMATCH", match?CLR_GREEN:CLR_RED);
        cp("\n",CLR_WHITE);
    }
    else if(cmd[0]=='\0'){}
    else{cp("  Unknown: ",CLR_RED);cpln(cmd,CLR_WHITE);}
}

static void draw_prompt() { cp("> ",CLR_CYAN); }

void Shell::init() {
    input_len=0;
    RamFS::init();
    Scheduler::init();
    draw_header();
    draw_prompt();
}

// ---> EXTRACTED HELPER FUNCTION <---
static void execute_nav(int hit) {
    if (hit != 2) docs_panel_active = false;
    if (hit == 0) {
        files_panel_active = false; file_viewer_active = false;
        draw_header();
    }
    else if (hit == 1) {
        files_panel_active = false; file_viewer_active = false;
        open_panel("Nova Browser",CLR_CYAN,"Coming soon — Phase 3 roadmap.","Full browser with Nova engine.");
    }
    else if (hit == 2) {
        files_panel_active = false; file_viewer_active = false;
        open_docs_panel();
    }
    else if (hit == 3) {
        files_panel_active = false; file_viewer_active = false;
        open_panel("Game Mode",CLR_PINK,"NVIDIA GPU + Vulkan — Phase 4.","DLSS, FSR, Nova Overlay.");
    }
    else if (hit == 4) {
        files_panel_active = false; file_viewer_active = false;
        draw_header();
    }
    else if (hit == 5) {
        open_files_panel();
    }
    else if (hit == 6) {
        files_panel_active = false; file_viewer_active = false;
        open_panel("Nova Store",CLR_VIOLET,"App marketplace — Phase 5.","Native + Proton games.");
    }
    else if (hit == 7) {
        files_panel_active = false; file_viewer_active = false;
        open_panel("Settings",CLR_WHITE,"","");
        cpln("  CPU:  x86 i686 32-bit Protected",CLR_DIM);
        cpln("  GPU:  NVIDIA (roadmap)",CLR_DIM);
        cpln("  RAM:  ~30MB",CLR_DIM);
        cpln("  VESA: 800x600x32bpp",CLR_DIM);
        cpln("  Kernel: C/C++/ASM",CLR_DIM);
    }
}

// ── Run loop (non-blocking) ───────────────────────────────────
void Shell::run() {
    static bool last_left = false;
    
    // Mouse coordinates initialized to center of 800x600 screen
    static int mouse_x = 400; 
    static int mouse_y = 300;

    while (true) {
        // MOUSE POLLING
        Mouse::poll();
        Mouse::State& ms = Mouse::get_state();

        // Update to absolute coordinates
        mouse_x = ms.x; 
        mouse_y = ms.y; 
        
        // Clamping to screen boundaries
        if (mouse_x < 0) mouse_x = 0; if (mouse_x > 799) mouse_x = 799;
        if (mouse_y < 0) mouse_y = 0; if (mouse_y > 599) mouse_y = 599;

        // Mouse click handling — use a small history buffer to reliably
        // catch the press edge even on very fast clicks
        static bool left_hist[3] = {false,false,false};
        left_hist[2] = left_hist[1];
        left_hist[1] = left_hist[0];
        left_hist[0] = ms.left;
        bool mouse_clicked = left_hist[0] && !left_hist[1];
        (void)last_left;

        if (mouse_clicked) {
            int hit = sidebar_hit_test(mouse_x, mouse_y);
            if (hit >= 0) {
                execute_nav(hit);
            }
        }

        // KEYBOARD NAVIGATION (1-8) — disabled while typing in Nova Docs,
        // since digits there are document content, not navigation
        if (kb_head != kb_tail && !docs_panel_active) {
            char peek = kb_buf[kb_tail];
            if (input_len == 0 && peek >= '1' && peek <= '8') {
                kb_tail = (kb_tail + 1) % 256;
                execute_nav(peek - '1');
            }
        }

        // FILES PANEL NAVIGATION
        if (files_panel_active) {
            if (up_pressed) {
                up_pressed = false;
                if (!file_viewer_active && file_selected > 0) {
                    file_selected--;
                    draw_file_list();
                }
            }
            if (down_pressed) {
                down_pressed = false;
                if (!file_viewer_active && file_selected < file_count - 1) {
                    file_selected++;
                    draw_file_list();
                }
            }
            if (esc_pressed) {
                esc_pressed = false;
                if (file_viewer_active) {
                    file_viewer_active = false;
                    draw_file_list();
                } else {
                    files_panel_active = false;
                    draw_header();
                }
            }
            if (f2_pressed && file_viewer_active && file_count > 0) {
                f2_pressed = false;
                load_doc(file_names[file_selected]);
                files_panel_active = false;
                file_viewer_active = false;
                docs_panel_active = true;
                draw_docs_editor();
            }
        }

        // TOPBAR REFRESH
        static uint32_t ltop = 0;
        if (pit_ticks - ltop > 1000) { ltop = pit_ticks; draw_topbar(); }

        // BLINK CURSOR
        bool cur_on = (pit_ticks / 400) % 2 == 0;
        Framebuffer::draw_rect(content_x, content_y, 6, 9, cur_on ? CLR_VIOLET : CLR_BG);

        // KEYBOARD INPUT HANDLING
        if (kb_head == kb_tail) continue;
        Framebuffer::draw_rect(content_x, content_y, 6, 9, CLR_BG);
        char c = kb_buf[kb_tail];
        
        if (c == '\n' && files_panel_active && !file_viewer_active && file_count > 0) {
            kb_tail = (kb_tail + 1) % 256;
            file_viewer_active = true;
            draw_file_viewer();
            continue;
        }

        // Nova Docs editor: capture all typing directly into doc_buffer,
        // bypassing the shell prompt entirely while active
        if (docs_panel_active) {
            kb_tail = (kb_tail + 1) % 256;

            if (f2_pressed) { f2_pressed = false; save_doc(); continue; }
            if (esc_pressed) {
                esc_pressed = false;
                docs_panel_active = false;
                draw_header();
                continue;
            }

            if (c == '\b') {
                if (doc_len > 0) doc_len--;
            } else if (doc_len < DOC_BUF_SIZE-1) {
                doc_buffer[doc_len++] = c;
            }
            doc_buffer[doc_len] = '\0';
            draw_docs_editor();
            continue;
        }

        kb_tail = (kb_tail + 1) % 256;

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