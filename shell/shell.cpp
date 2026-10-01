#include "shell.h"

#include "../kernel/drivers/keyboard/keyboard.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/drivers/mouse/mouse.h"

#include "../kernel/fs/ramfs.h"
#include "../kernel/proc/scheduler.h"
#include "../kernel/panic.h"
#include "../kernel/drivers/disk/ata.h"
#include "../kernel/fs/novafs_disk.h"


extern bool up_pressed;
extern bool down_pressed;
extern bool esc_pressed;
extern bool f2_pressed;

extern uint32_t pit_ticks;

extern char kb_buf[256];
extern int kb_head;
extern int kb_tail;


// =============================================================
// Shell input
// =============================================================

static char input_buf[256];

static int input_len = 0;


// =============================================================
// Colors
// =============================================================

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


// =============================================================
// Layout
// =============================================================

#define SIDEBAR_W         180
#define SEPARATOR_HEIGHT  1
#define TOPBAR_H          36
#define PADDING           10

#define SIDEBAR_ROW_H     50
#define SIDEBAR_TOP       (TOPBAR_H + 10)


// =============================================================
// String helpers
// =============================================================

static bool streq(
    const char* a,
    const char* b
) {
    while (
        *a &&
        *b
    ) {
        if (
            *a != *b
        ) {
            return false;
        }

        ++a;
        ++b;
    }

    return
        *a == *b;
}


static bool startswith(
    const char* s,
    const char* p
) {
    while (*p) {
        if (
            *s++ != *p++
        ) {
            return false;
        }
    }

    return true;
}


// =============================================================
// Basic shell output
// =============================================================

void Shell::print(
    const char* s
) {
    Framebuffer::print(
        s,
        CLR_WHITE
    );
}


void Shell::println(
    const char* s
) {
    Framebuffer::print(
        s,
        CLR_WHITE
    );

    Framebuffer::print(
        "\n",
        CLR_WHITE
    );
}


static void p(
    const char* s,
    uint32_t color
) {
    Framebuffer::print(
        s,
        color
    );
}


static void print_num(
    uint32_t n
) {
    if (!n) {
        p(
            "0",
            CLR_WHITE
        );

        return;
    }


    char buf[12];

    int i =
        0;


    while (n) {
        buf[i++] =
            '0' +
            (n % 10);

        n /=
            10;
    }


    char out[12];


    for (
        int j = 0;
        j < i;
        ++j
    ) {
        out[j] =
            buf[
                i - 1 - j
            ];
    }


    out[i] =
        '\0';


    p(
        out,
        CLR_WHITE
    );
}


// =============================================================
// Content drawing cursor
// =============================================================

static int content_x =
    SIDEBAR_W +
    PADDING;

static int content_y =
    TOPBAR_H +
    PADDING;


// =============================================================
// Content printing
// =============================================================

static void cp(
    const char* s,
    uint32_t color
) {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    int px =
        content_x;


    for (
        int i = 0;
        s[i];
        ++i
    ) {
        char ch =
            s[i];


        if (
            ch == '\n'
        ) {
            content_x =
                SIDEBAR_W +
                PADDING;

            content_y +=
                18;

            px =
                content_x;

            continue;
        }


        if (
            ch == '\b'
        ) {
            if (
                px >
                SIDEBAR_W +
                PADDING
            ) {
                px -=
                    8;


                Framebuffer::draw_rect(
                    px,
                    content_y,
                    8,
                    10,
                    CLR_BG
                );
            }


            content_x =
                px;

            continue;
        }


        Framebuffer::draw_char(
            ch,
            px,
            content_y,
            color
        );


        px +=
            8;


        if (
            px + 8 >
            (int)fb.width -
            PADDING
        ) {
            px =
                SIDEBAR_W +
                PADDING;

            content_y +=
                10;
        }
    }


    content_x =
        px;
}


static void cpln(
    const char* s,
    uint32_t color
) {
    cp(
        s,
        color
    );

    cp(
        "\n",
        color
    );
}


// =============================================================
// Random generator
// =============================================================

static uint32_t rng =
    12345;


static uint32_t rnext() {
    rng ^=
        rng << 13;

    rng ^=
        rng >> 17;

    rng ^=
        rng << 5;

    return rng;
}


// =============================================================
// Fast integer square root
// =============================================================
//
// Much faster than repeatedly testing s*s inside every wallpaper
// pixel calculation.
// =============================================================

static uint32_t fast_isqrt(
    uint32_t value
) {
    uint32_t result =
        0;


    uint32_t bit =
        1u << 30;


    while (
        bit > value
    ) {
        bit >>=
            2;
    }


    while (
        bit != 0
    ) {
        if (
            value >=
            result + bit
        ) {
            value -=
                result + bit;


            result =
                (result >> 1) +
                bit;
        }

        else {
            result >>=
                1;
        }


        bit >>=
            2;
    }


    return result;
}


// =============================================================
// Wallpaper
// =============================================================

static void draw_wallpaper() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    uint32_t W =
        fb.width;

    uint32_t H =
        fb.height;


    Framebuffer::clear(
        0x010610
    );


    // ---------------------------------------------------------
    // Nebula glows
    // ---------------------------------------------------------

    struct NB {
        int x;
        int y;
        int r;

        uint32_t rc;
        uint32_t gc;
        uint32_t bc;

        uint32_t a;
    };


    NB nbs[] = {
        {
            (int)(W * 7 / 10),
            (int)(H * 2 / 10),
            (int)(H * 6 / 10),
            80,
            0,
            220,
            18
        },

        {
            (int)(W * 1 / 10),
            (int)(H * 7 / 10),
            (int)(H * 5 / 10),
            0,
            180,
            255,
            14
        },

        {
            (int)(W * 9 / 10),
            (int)(H * 8 / 10),
            (int)(H * 5 / 10),
            200,
            0,
            120,
            12
        },

        {
            (int)(W * 3 / 10),
            (int)(H * 1 / 10),
            (int)(H * 4 / 10),
            0,
            200,
            240,
            10
        }
    };


    for (
        int n = 0;
        n < 4;
        ++n
    ) {
        NB& nb =
            nbs[n];


        int r2 =
            nb.r *
            nb.r;


        for (
            int dy = -nb.r;
            dy <= nb.r;
            dy += 2
        ) {
            for (
                int dx = -nb.r;
                dx <= nb.r;
                dx += 2
            ) {
                int distanceSquared =
                    dx * dx +
                    dy * dy;


                if (
                    distanceSquared >
                    r2
                ) {
                    continue;
                }


                int px =
                    nb.x +
                    dx;


                int py =
                    nb.y +
                    dy;


                if (
                    px < 0 ||
                    py < 0 ||
                    px >= (int)W ||
                    py >= (int)H
                ) {
                    continue;
                }


                uint32_t dd =
                    (uint32_t)
                    distanceSquared;


                uint32_t distance =
                    fast_isqrt(
                        dd
                    );


                uint32_t fade =
                    (
                        nb.r -
                        distance
                    ) *
                    nb.a /
                    nb.r;


                if (!fade)
                    continue;


                uint32_t nr =
                    nb.rc *
                    fade /
                    255;


                uint32_t ng =
                    nb.gc *
                    fade /
                    255;


                uint32_t nbl =
                    nb.bc *
                    fade /
                    255;


                if (
                    nr > 255
                ) {
                    nr =
                        255;
                }


                if (
                    ng > 255
                ) {
                    ng =
                        255;
                }


                if (
                    nbl > 255
                ) {
                    nbl =
                        255;
                }


                uint32_t col =
                    (nr << 16) |
                    (ng << 8) |
                    nbl;


                Framebuffer::put_pixel(
                    px,
                    py,
                    col
                );


                if (
                    px + 1 <
                    (int)W
                ) {
                    Framebuffer::put_pixel(
                        px + 1,
                        py,
                        col
                    );
                }


                if (
                    py + 1 <
                    (int)H
                ) {
                    Framebuffer::put_pixel(
                        px,
                        py + 1,
                        col
                    );
                }


                if (
                    px + 1 < (int)W &&
                    py + 1 < (int)H
                ) {
                    Framebuffer::put_pixel(
                        px + 1,
                        py + 1,
                        col
                    );
                }
            }
        }
    }


    // ---------------------------------------------------------
    // Stars
    // ---------------------------------------------------------

    rng =
        42;


    for (
        int i = 0;
        i < 200;
        ++i
    ) {
        int x =
            rnext() %
            W;


        int y =
            rnext() %
            H;


        uint32_t b =
            20 +
            rnext() %
            40;


        Framebuffer::put_pixel(
            x,
            y,
            (b << 16) |
            (b << 8) |
            b
        );
    }


    for (
        int i = 0;
        i < 80;
        ++i
    ) {
        int x =
            rnext() %
            W;


        int y =
            rnext() %
            H;


        uint32_t b =
            60 +
            rnext() %
            80;


        uint32_t t =
            rnext() %
            3;


        uint32_t col;


        if (
            t == 0
        ) {
            col =
                (b << 16) |
                (b << 8) |
                b;
        }

        else if (
            t == 1
        ) {
            col =
                ((b / 2) << 16) |
                ((b / 2) << 8) |
                b;
        }

        else {
            col =
                (b << 16) |
                ((b / 2) << 8) |
                (b / 2);
        }


        Framebuffer::put_pixel(
            x,
            y,
            col
        );


        if (
            b > 100 &&
            x + 1 < (int)W
        ) {
            Framebuffer::put_pixel(
                x + 1,
                y,
                col
            );
        }
    }


    // ---------------------------------------------------------
    // Bright stars
    // ---------------------------------------------------------

    if (
        W > 40 &&
        H > 40
    ) {
        for (
            int i = 0;
            i < 12;
            ++i
        ) {
            int x =
                20 +
                rnext() %
                (W - 40);


            int y =
                20 +
                rnext() %
                (H - 40);


            uint32_t gc2 =
                rnext() %
                3;


            for (
                int r = 6;
                r >= 1;
                --r
            ) {
                uint32_t b2 =
                    255 /
                    (r + 1);


                uint32_t gc3;


                if (
                    gc2 == 0
                ) {
                    gc3 =
                        b2;
                }

                else if (
                    gc2 == 1
                ) {
                    gc3 =
                        (b2 << 16) |
                        (b2 / 2);
                }

                else {
                    gc3 =
                        ((b2 / 2) << 8) |
                        b2;
                }


                for (
                    int dy2 = -r;
                    dy2 <= r;
                    ++dy2
                ) {
                    for (
                        int dx2 = -r;
                        dx2 <= r;
                        ++dx2
                    ) {
                        if (
                            dx2 * dx2 +
                            dy2 * dy2 <=
                            r * r
                        ) {
                            Framebuffer::put_pixel(
                                x + dx2,
                                y + dy2,
                                gc3
                            );
                        }
                    }
                }
            }


            Framebuffer::put_pixel(
                x,
                y,
                CLR_WHITE
            );
        }
    }


    // ---------------------------------------------------------
    // Orbs
    // ---------------------------------------------------------

    struct Orb {
        int x;
        int y;
        int r;

        uint32_t rc;
        uint32_t gc;
        uint32_t bc;
    };


    Orb orbs[] = {
        {
            (int)(W * 7 / 100),
            (int)(H * 16 / 100),
            22,
            0,
            229,
            255
        },

        {
            (int)(W * 93 / 100),
            (int)(H * 32 / 100),
            14,
            244,
            114,
            182
        },

        {
            (int)(W * 12 / 100),
            (int)(H * 84 / 100),
            18,
            123,
            47,
            247
        },

        {
            (int)(W * 85 / 100),
            (int)(H * 88 / 100),
            10,
            0,
            229,
            255
        }
    };


    for (
        int o = 0;
        o < 4;
        ++o
    ) {
        Orb& orb =
            orbs[o];


        int maxR =
            orb.r *
            4;


        int maxRSquared =
            maxR *
            maxR;


        for (
            int dy2 = -maxR;
            dy2 <= maxR;
            ++dy2
        ) {
            for (
                int dx2 = -maxR;
                dx2 <= maxR;
                ++dx2
            ) {
                int d2 =
                    dx2 * dx2 +
                    dy2 * dy2;


                if (
                    d2 >
                    maxRSquared
                ) {
                    continue;
                }


                int px2 =
                    orb.x +
                    dx2;


                int py2 =
                    orb.y +
                    dy2;


                if (
                    px2 < 0 ||
                    py2 < 0 ||
                    px2 >= (int)W ||
                    py2 >= (int)H
                ) {
                    continue;
                }


                uint32_t distance =
                    fast_isqrt(
                        (uint32_t)d2
                    );


                uint32_t fade2 =
                    (
                        distance <
                        (uint32_t)maxR
                    )
                        ? (
                            (
                                (uint32_t)maxR -
                                distance
                            ) *
                            80 /
                            (uint32_t)maxR
                        )
                        : 0;


                if (!fade2)
                    continue;


                uint32_t nr2 =
                    orb.rc *
                    fade2 /
                    255;


                uint32_t ng2 =
                    orb.gc *
                    fade2 /
                    255;


                uint32_t nb2 =
                    orb.bc *
                    fade2 /
                    255;


                if (
                    nr2 > 255
                ) {
                    nr2 =
                        255;
                }


                if (
                    ng2 > 255
                ) {
                    ng2 =
                        255;
                }


                if (
                    nb2 > 255
                ) {
                    nb2 =
                        255;
                }


                Framebuffer::put_pixel(
                    px2,
                    py2,
                    (
                        (nr2 & 0xFF) << 16
                    ) |
                    (
                        (ng2 & 0xFF) << 8
                    ) |
                    (
                        nb2 & 0xFF
                    )
                );
            }
        }


        int coreRadius =
            orb.r /
            2;


        for (
            int dy2 = -coreRadius;
            dy2 <= coreRadius;
            ++dy2
        ) {
            for (
                int dx2 = -coreRadius;
                dx2 <= coreRadius;
                ++dx2
            ) {
                if (
                    dx2 * dx2 +
                    dy2 * dy2 >
                    coreRadius *
                    coreRadius
                ) {
                    continue;
                }


                int px2 =
                    orb.x +
                    dx2;


                int py2 =
                    orb.y +
                    dy2;


                if (
                    px2 < 0 ||
                    py2 < 0 ||
                    px2 >= (int)W ||
                    py2 >= (int)H
                ) {
                    continue;
                }


                uint32_t nr2 =
                    orb.rc *
                    180 /
                    255;


                uint32_t ng2 =
                    orb.gc *
                    180 /
                    255;


                uint32_t nb2 =
                    orb.bc *
                    180 /
                    255;


                Framebuffer::put_pixel(
                    px2,
                    py2,
                    (
                        (nr2 & 0xFF) << 16
                    ) |
                    (
                        (ng2 & 0xFF) << 8
                    ) |
                    (
                        nb2 & 0xFF
                    )
                );
            }
        }
    }
}


// =============================================================
// Top bar
// =============================================================

static void draw_topbar() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    uint32_t W =
        fb.width;


    Framebuffer::draw_rect(
        0,
        0,
        W,
        TOPBAR_H,
        0x0A0820
    );


    for (
        uint32_t x = 0;
        x < W;
        ++x
    ) {
        uint32_t t =
            x *
            255 /
            W;


        uint32_t r =
            (
                123 *
                t
            ) /
            255;


        uint32_t g =
            (
                229 *
                (255 - t) +
                47 *
                t
            ) /
            255;


        uint32_t b =
            (
                255 *
                (255 - t) +
                247 *
                t
            ) /
            255;


        Framebuffer::put_pixel(
            x,
            TOPBAR_H - 1,
            (r << 16) |
            (g << 8) |
            b
        );
    }


    Framebuffer::draw_circle_aa(
        16,
        18,
        5,
        CLR_VIOLET
    );


    Framebuffer::draw_circle_aa(
        16,
        18,
        3,
        CLR_CYAN
    );


    Framebuffer::draw_circle_aa(
        16,
        18,
        1,
        CLR_WHITE
    );


    Framebuffer::print_at(
        "NovaOS",
        28,
        14,
        CLR_CYAN
    );


    Framebuffer::print_at(
        "[1-8: nav]",
        420,
        14,
        CLR_DIM
    );


    Framebuffer::print_at(
        "Home",
        110,
        14,
        CLR_WHITE
    );


    Framebuffer::print_at(
        "Files",
        155,
        14,
        CLR_DIM
    );


    Framebuffer::print_at(
        "Apps",
        205,
        14,
        CLR_DIM
    );


    // ---------------------------------------------------------
    // Uptime
    // ---------------------------------------------------------

    uint32_t secs =
        pit_ticks /
        1000u;


    char ut[24] =
        "uptime: ";


    int ui =
        8;


    char ns[12];

    int ni =
        0;


    uint32_t tmp =
        secs;


    if (!tmp) {
        ns[ni++] =
            '0';
    }


    while (tmp) {
        ns[ni++] =
            '0' +
            (tmp % 10);

        tmp /=
            10;
    }


    for (
        int i = 0;
        i < ni;
        ++i
    ) {
        ut[ui++] =
            ns[
                ni - 1 - i
            ];
    }


    ut[ui++] =
        's';

    ut[ui] =
        '\0';


    Framebuffer::print_at(
        ut,
        (int)W - 120,
        14,
        CLR_DIM
    );
}


// =============================================================
// Sidebar
// =============================================================

static int current_nav =
    4;


static void draw_sidebar() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    uint32_t H =
        fb.height;


    Framebuffer::draw_rect_round_br(
        0,
        TOPBAR_H,
        SIDEBAR_W,
        (int)(
            H -
            TOPBAR_H
        ),
        16,
        0x08031E
    );


    Framebuffer::draw_rect(
        SIDEBAR_W - 1,
        TOPBAR_H,
        1,
        H - TOPBAR_H,
        0x1A0855
    );


    struct Nav {
        const char* label;
        const char* sub;
        uint32_t col;
    };


    Nav items[] = {
        {
            "Dashboard",
            "Home screen",
            CLR_CYAN
        },

        {
            "Nova Browser",
            "Browse the web",
            CLR_CYAN
        },

        {
            "Nova Docs",
            "Office suite",
            CLR_PINK
        },

        {
            "Game Mode",
            "Launch games",
            CLR_PINK
        },

        {
            "Terminal",
            "Nova Shell",
            CLR_VIOLET
        },

        {
            "Nova Files",
            "NovaFS",
            CLR_DIM
        },

        {
            "Nova Store",
            "Apps",
            CLR_DIM
        },

        {
            "Settings",
            "CPU/GPU/RAM",
            CLR_DIM
        }
    };


    for (
        int i = 0;
        i < 8;
        ++i
    ) {
        int top =
            SIDEBAR_TOP +
            i *
            SIDEBAR_ROW_H;


        if (
            i == current_nav
        ) {
            Framebuffer::draw_rounded_rect(
                2,
                top + 2,
                SIDEBAR_W - 6,
                SIDEBAR_ROW_H - 4,
                12,
                0x1A0844
            );


            Framebuffer::draw_rect(
                2,
                top + 2,
                2,
                SIDEBAR_ROW_H - 4,
                CLR_VIOLET
            );
        }


        if (
            i == 5
        ) {
            Framebuffer::draw_rect(
                12,
                top - 6,
                SIDEBAR_W - 24,
                1,
                0x1A0855
            );
        }


        Framebuffer::draw_circle_aa(
            20,
            top + 18,
            4,
            items[i].col
        );


        Framebuffer::print_at(
            items[i].label,
            32,
            top + 12,
            CLR_WHITE
        );


        Framebuffer::print_at(
            items[i].sub,
            32,
            top + 24,
            CLR_GRAY
        );
    }
}


// =============================================================
// Sidebar hit test
// =============================================================

static int sidebar_hit_test(
    int mx,
    int my
) {
    if (
        mx < 0 ||
        mx >= SIDEBAR_W
    ) {
        return -1;
    }


    if (
        my <
        SIDEBAR_TOP
    ) {
        return -1;
    }


    for (
        int i = 0;
        i < 8;
        ++i
    ) {
        int top =
            SIDEBAR_TOP +
            i *
            SIDEBAR_ROW_H;


        if (
            i > 5
        ) {
            top +=
                SEPARATOR_HEIGHT;
        }


        if (
            my >= top &&
            my <
                top +
                SIDEBAR_ROW_H
        ) {
            return i;
        }
    }


    return -1;
}


// =============================================================
// Header
// =============================================================

static void draw_header() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    uint32_t W =
        fb.width;


    draw_wallpaper();

    draw_topbar();

    draw_sidebar();


    int cx =
        SIDEBAR_W +
        PADDING;


    int cw =
        (int)W -
        SIDEBAR_W -
        PADDING * 2;


    Framebuffer::draw_rounded_rect(
        cx,
        TOPBAR_H + PADDING,
        cw,
        50,
        14,
        0x0D0530
    );


    for (
        int x = cx;
        x < cx + cw;
        ++x
    ) {
        uint32_t t =
            (
                x - cx
            ) *
            255 /
            cw;


        uint32_t r =
            t;


        uint32_t g =
            229 *
            (
                255 - t
            ) /
            255;


        uint32_t b =
            (
                255 *
                (
                    255 - t
                ) +
                128 *
                t
            ) /
            255;


        Framebuffer::put_pixel(
            x,
            TOPBAR_H +
            PADDING,
            (r << 16) |
            (g << 8) |
            b
        );
    }


    Framebuffer::print_at(
        "Welcome to NovaOS",
        cx + 10,
        TOPBAR_H +
            PADDING +
            10,
        CLR_WHITE
    );


    Framebuffer::print_at(
        "Productivity | Gaming | Open Source",
        cx + 10,
        TOPBAR_H +
            PADDING +
            26,
        CLR_DIM
    );


    Framebuffer::draw_rect(
        cx,
        TOPBAR_H +
            PADDING +
            52,
        cw,
        1,
        0x1A0855
    );


    content_x =
        cx;


    content_y =
        TOPBAR_H +
        PADDING +
        62;


    cpln(
        "  Nova Shell v0.2.0 - pixel mode",
        CLR_CYAN
    );


    cpln(
        "  Type 'help' for commands.",
        CLR_DIM
    );


    cp(
        "\n",
        CLR_WHITE
    );
}


// =============================================================
// Nova Files browser
// =============================================================

#define MAX_FILE_ENTRIES 32


static char file_names[
    MAX_FILE_ENTRIES
][32];


static uint32_t file_sizes[
    MAX_FILE_ENTRIES
];


static int file_count =
    0;


static int file_selected =
    0;


static bool files_panel_active =
    false;


static bool file_viewer_active =
    false;


static int file_collect_idx =
    0;


static void collect_file_cb(
    const char* name,
    uint32_t size
) {
    if (
        file_collect_idx >=
        MAX_FILE_ENTRIES
    ) {
        return;
    }


    int i =
        0;


    while (
        name[i] &&
        i < 31
    ) {
        file_names[
            file_collect_idx
        ][i] =
            name[i];

        ++i;
    }


    file_names[
        file_collect_idx
    ][i] =
        '\0';


    file_sizes[
        file_collect_idx
    ] =
        size;


    ++file_collect_idx;
}


static void draw_file_list() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    int cx =
        SIDEBAR_W +
        PADDING;


    int cw =
        (int)fb.width -
        SIDEBAR_W -
        PADDING * 2;


    Framebuffer::draw_rounded_rect(
        cx,
        TOPBAR_H + PADDING,
        cw,
        280,
        14,
        0x0A0525
    );


    Framebuffer::print_at(
        "Nova Files",
        cx + 10,
        TOPBAR_H +
            PADDING +
            10,
        CLR_CYAN
    );


    Framebuffer::print_at(
        "Arrows: navigate | Enter: open | Esc: back",
        cx + 10,
        TOPBAR_H +
            PADDING +
            26,
        CLR_DIM
    );


    int ly =
        TOPBAR_H +
        PADDING +
        50;


    for (
        int i = 0;
        i < file_count;
        ++i
    ) {
        bool selected =
            i ==
            file_selected;


        if (selected) {
            Framebuffer::draw_rect(
                cx + 8,
                ly - 2,
                cw - 16,
                16,
                0x1A0855
            );
        }


        Framebuffer::draw_circle_aa(
            cx + 18,
            ly + 5,
            3,
            selected
                ? CLR_CYAN
                : CLR_DIM
        );


        Framebuffer::print_at(
            file_names[i],
            cx + 30,
            ly,
            selected
                ? CLR_WHITE
                : CLR_DIM
        );


        char sbuf[16];

        int si =
            0;


        uint32_t s2 =
            file_sizes[i];


        if (!s2) {
            sbuf[si++] =
                '0';
        }


        while (s2) {
            sbuf[si++] =
                '0' +
                (
                    s2 %
                    10
                );

            s2 /=
                10;
        }


        char sout[16];


        for (
            int j = 0;
            j < si;
            ++j
        ) {
            sout[j] =
                sbuf[
                    si -
                    1 -
                    j
                ];
        }


        sout[si] =
            'b';

        sout[
            si + 1
        ] =
            '\0';


        Framebuffer::print_at(
            sout,
            cx +
                cw -
                70,
            ly,
            CLR_GRAY
        );


        ly +=
            18;
    }


    if (
        file_count ==
        0
    ) {
        Framebuffer::print_at(
            "No files in NovaFS.",
            cx + 30,
            ly,
            CLR_GRAY
        );
    }
}


static void draw_file_viewer() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    int cx =
        SIDEBAR_W +
        PADDING;


    int cw =
        (int)fb.width -
        SIDEBAR_W -
        PADDING * 2;


    Framebuffer::draw_rounded_rect(
        cx,
        TOPBAR_H + PADDING,
        cw,
        280,
        14,
        0x0A0525
    );


    static char view_buf[2048];


    uint32_t view_size =
        0;


    bool found =
        NovaFSDisk::load_file(
            file_names[
                file_selected
            ],
            view_buf,
            2047,
            &view_size
        );


    Framebuffer::print_at(
        file_names[
            file_selected
        ],
        cx + 10,
        TOPBAR_H +
            PADDING +
            10,
        CLR_PINK
    );


    Framebuffer::print_at(
        "Esc: back to file list | F2: edit in Nova Docs",
        cx + 10,
        TOPBAR_H +
            PADDING +
            26,
        CLR_DIM
    );


    Framebuffer::draw_rect(
        cx + 8,
        TOPBAR_H +
            PADDING +
            44,
        cw - 16,
        1,
        0x1A0855
    );


    if (found) {
        Framebuffer::print_at(
            view_buf,
            cx + 10,
            TOPBAR_H +
                PADDING +
                56,
            CLR_WHITE
        );
    }

    else {
        Framebuffer::print_at(
            "(file not found)",
            cx + 10,
            TOPBAR_H +
                PADDING +
                56,
            CLR_RED
        );
    }
}


static void open_files_panel() {
    file_collect_idx =
        0;


    NovaFSDisk::list_files(
        collect_file_cb
    );


    file_count =
        file_collect_idx;


    file_selected =
        0;


    files_panel_active =
        true;


    file_viewer_active =
        false;


    draw_file_list();
}


// =============================================================
// Nova Docs
// =============================================================

#define DOC_BUF_SIZE 2048


static char doc_buffer[
    DOC_BUF_SIZE
];


static int doc_len =
    0;


static char doc_filename[32] =
    "untitled.txt";


static bool docs_panel_active =
    false;


static void draw_docs_editor() {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    int cx =
        SIDEBAR_W +
        PADDING;


    int cw =
        (int)fb.width -
        SIDEBAR_W -
        PADDING * 2;


    Framebuffer::draw_rounded_rect(
        cx,
        TOPBAR_H + PADDING,
        cw,
        320,
        14,
        0x0A0525
    );


    Framebuffer::print_at(
        "Nova Docs",
        cx + 10,
        TOPBAR_H +
            PADDING +
            10,
        CLR_PINK
    );


    Framebuffer::print_at(
        doc_filename,
        cx + 110,
        TOPBAR_H +
            PADDING +
            10,
        CLR_DIM
    );


    Framebuffer::print_at(
        "F2: save | Esc: back to sidebar",
        cx + 10,
        TOPBAR_H +
            PADDING +
            26,
        CLR_DIM
    );


    Framebuffer::draw_rect(
        cx + 8,
        TOPBAR_H +
            PADDING +
            42,
        cw - 16,
        1,
        0x1A0855
    );


    int ty =
        TOPBAR_H +
        PADDING +
        54;


    int tx =
        cx +
        10;


    int line =
        0;


    int draw_x =
        tx;


    int draw_y =
        ty;


    for (
        int i = 0;
        i < doc_len;
        ++i
    ) {
        char ch =
            doc_buffer[i];


        if (
            ch == '\n'
        ) {
            ++line;


            draw_x =
                tx;


            draw_y =
                ty +
                line *
                10;


            continue;
        }


        Framebuffer::draw_char(
            ch,
            draw_x,
            draw_y,
            CLR_WHITE
        );


        draw_x +=
            8;
    }


    bool cur_on =
        (
            pit_ticks /
            400
        ) %
        2 ==
        0;


    if (cur_on) {
        Framebuffer::draw_rect(
            draw_x,
            draw_y,
            6,
            9,
            CLR_PINK
        );
    }
}


static void load_doc(
    const char* name
) {
    int i =
        0;


    while (
        name[i] &&
        i < 31
    ) {
        doc_filename[i] =
            name[i];

        ++i;
    }


    doc_filename[i] =
        '\0';


    uint32_t loaded_size =
        0;


    bool ok =
        NovaFSDisk::load_file(
            name,
            doc_buffer,
            DOC_BUF_SIZE - 1,
            &loaded_size
        );


    doc_len =
        ok
            ? (int)loaded_size
            : 0;


    doc_buffer[
        doc_len
    ] =
        '\0';
}


static void save_doc() {
    NovaFSDisk::save_file(
        doc_filename,
        doc_buffer,
        (uint32_t)doc_len
    );
}


static void open_docs_panel() {
    doc_filename[0] = 'u';
    doc_filename[1] = 'n';
    doc_filename[2] = 't';
    doc_filename[3] = 'i';
    doc_filename[4] = 't';
    doc_filename[5] = 'l';
    doc_filename[6] = 'e';
    doc_filename[7] = 'd';
    doc_filename[8] = '.';
    doc_filename[9] = 't';
    doc_filename[10] = 'x';
    doc_filename[11] = 't';
    doc_filename[12] = '\0';


    doc_len =
        0;


    doc_buffer[0] =
        '\0';


    docs_panel_active =
        true;


    draw_docs_editor();
}


// =============================================================
// Generic app panel
// =============================================================

static void open_panel(
    const char* title,
    uint32_t titleColor,
    const char* line1,
    const char* line2
) {
    Framebuffer::Info& fb =
        Framebuffer::get_info();


    int cx =
        SIDEBAR_W +
        PADDING;


    int cw =
        (int)fb.width -
        SIDEBAR_W -
        PADDING * 2;


    Framebuffer::draw_rounded_rect(
        cx,
        TOPBAR_H + PADDING,
        cw,
        120,
        14,
        0x080320
    );


    Framebuffer::print_at(
        title,
        cx + 10,
        TOPBAR_H +
            PADDING +
            10,
        titleColor
    );


    Framebuffer::print_at(
        line1,
        cx + 10,
        TOPBAR_H +
            PADDING +
            32,
        CLR_DIM
    );


    Framebuffer::print_at(
        line2,
        cx + 10,
        TOPBAR_H +
            PADDING +
            48,
        CLR_GRAY
    );


    content_x =
        cx;


    content_y =
        TOPBAR_H +
        PADDING +
        72;
}


// =============================================================
// List callback
// =============================================================

static void ls_cb(
    const char* name,
    uint32_t size
) {
    cp(
        "  ",
        CLR_WHITE
    );


    cp(
        name,
        CLR_CYAN
    );


    cp(
        "  (",
        CLR_GRAY
    );


    print_num(
        size
    );


    cpln(
        " bytes)",
        CLR_GRAY
    );
}


// =============================================================
// Processes
// =============================================================

static void proc_hello() {
    cpln(
        "[hello] Hello from NovaOS!",
        CLR_GREEN
    );


    Scheduler::exit(
        0
    );
}


static void proc_counter() {
    cp(
        "[counter] ",
        CLR_YELLOW
    );


    for (
        int i = 1;
        i <= 10;
        ++i
    ) {
        char s[4];


        s[0] =
            '0' +
            i;


        s[1] =
            ' ';


        s[2] =
            '\0';


        if (
            i == 10
        ) {
            s[0] =
                '1';

            s[1] =
                '0';

            s[2] =
                ' ';

            s[3] =
                '\0';
        }


        cp(
            s,
            CLR_YELLOW
        );
    }


    cpln(
        "",
        CLR_WHITE
    );


    Scheduler::exit(
        0
    );
}


static void proc_sysinfo() {
    cpln(
        "[sysinfo] NovaOS v0.2.0",
        CLR_CYAN
    );


    cpln(
        "[sysinfo] VESA 800x600x32",
        CLR_CYAN
    );


    Scheduler::exit(
        0
    );
}


// =============================================================
// Commands
// =============================================================

static void handle_command(
    const char* cmd
) {
    if (
        streq(
            cmd,
            "help"
        )
    ) {
        cpln(
            "\n  Commands:",
            CLR_CYAN
        );


        const char* commands[] = {
            "help",
            "version",
            "about",
            "clear",
            "cpu",
            "mem",
            "uptime",
            "echo",
            "ls",
            "cat",
            "touch",
            "write",
            "rm",
            "ps",
            "run",
            "panic",
            "color"
        };


        for (
            int i = 0;
            i < 17;
            ++i
        ) {
            cp(
                "  ",
                CLR_WHITE
            );


            cpln(
                commands[i],
                CLR_GREEN
            );
        }


        cp(
            "\n",
            CLR_WHITE
        );
    }


    else if (
        streq(
            cmd,
            "version"
        )
    ) {
        cpln(
            "\n  NovaOS v0.2.0 - VESA pixel GUI\n",
            CLR_CYAN
        );
    }


    else if (
        streq(
            cmd,
            "about"
        )
    ) {
        cpln(
            "\n  NovaOS - Next-Gen OS",
            CLR_CYAN
        );


        cpln(
            "  Kernel: C/C++/ASM",
            CLR_WHITE
        );


        cpln(
            "  Display: VESA 800x600",
            CLR_WHITE
        );


        cpln(
            "  GPU: NVIDIA (roadmap)\n",
            CLR_GREEN
        );
    }


    else if (
        streq(
            cmd,
            "clear"
        )
    ) {
        draw_header();
    }


    else if (
        streq(
            cmd,
            "mem"
        )
    ) {
        cp(
            "\n  Kernel: ",
            CLR_CYAN
        );

        cpln(
            "0x100000",
            CLR_YELLOW
        );


        cp(
            "  FB:     ",
            CLR_CYAN
        );

        cpln(
            "0xFD000000",
            CLR_YELLOW
        );


        cp(
            "  RAM:    ",
            CLR_CYAN
        );

        cpln(
            "~30MB\n",
            CLR_YELLOW
        );
    }


    else if (
        streq(
            cmd,
            "cpu"
        )
    ) {
        cp(
            "\n  CPU: ",
            CLR_CYAN
        );

        cpln(
            "x86 i686 32-bit",
            CLR_WHITE
        );


        cp(
            "  Next: ",
            CLR_CYAN
        );

        cpln(
            "x86-64 + NVIDIA\n",
            CLR_GREEN
        );
    }


    else if (
        streq(
            cmd,
            "uptime"
        )
    ) {
        cp(
            "\n  Uptime: ",
            CLR_CYAN
        );


        print_num(
            pit_ticks /
            1000u
        );


        cpln(
            "s\n",
            CLR_WHITE
        );
    }


    else if (
        startswith(
            cmd,
            "echo "
        )
    ) {
        cp(
            "\n  ",
            CLR_WHITE
        );


        cpln(
            cmd + 5,
            CLR_CYAN
        );


        cp(
            "\n",
            CLR_WHITE
        );
    }


    else if (
        streq(
            cmd,
            "ls"
        )
    ) {
        cpln(
            "\n  NovaFS Files",
            CLR_CYAN
        );


        RamFS::list(
            ls_cb
        );


        cp(
            "\n",
            CLR_WHITE
        );
    }


    else if (
        startswith(
            cmd,
            "cat "
        )
    ) {
        RamFile* file =
            RamFS::find(
                cmd + 4
            );


        if (file) {
            cp(
                "\n  ",
                CLR_WHITE
            );


            cpln(
                file->data,
                CLR_WHITE
            );


            cp(
                "\n",
                CLR_WHITE
            );
        }

        else {
            cp(
                "  Not found: ",
                CLR_RED
            );


            cpln(
                cmd + 4,
                CLR_WHITE
            );
        }
    }


    else if (
        startswith(
            cmd,
            "touch "
        )
    ) {
        if (
            RamFS::create(
                cmd + 6
            )
        ) {
            cp(
                "  Created: ",
                CLR_GREEN
            );


            cpln(
                cmd + 6,
                CLR_WHITE
            );
        }

        else {
            cpln(
                "  Error",
                CLR_RED
            );
        }
    }


    else if (
        startswith(
            cmd,
            "write "
        )
    ) {
        const char* rest =
            cmd +
            6;


        int i =
            0;


        while (
            rest[i] &&
            rest[i] != ' '
        ) {
            ++i;
        }


        if (
            rest[i] ==
            ' '
        ) {
            char filename[32];


            int j =
                0;


            for (
                ;
                j < i &&
                j < 31;
                ++j
            ) {
                filename[j] =
                    rest[j];
            }


            filename[j] =
                '\0';


            const char* content =
                rest +
                i +
                1;


            uint32_t len =
                0;


            while (
                content[len]
            ) {
                ++len;
            }


            if (
                RamFS::write(
                    filename,
                    content,
                    len
                )
            ) {
                cp(
                    "  Written: ",
                    CLR_GREEN
                );


                cpln(
                    filename,
                    CLR_WHITE
                );
            }

            else {
                cpln(
                    "  File not found",
                    CLR_RED
                );
            }
        }

        else {
            cpln(
                "  Usage: write <file> <text>",
                CLR_YELLOW
            );
        }
    }


    else if (
        startswith(
            cmd,
            "rm "
        )
    ) {
        if (
            RamFS::remove(
                cmd + 3
            )
        ) {
            cp(
                "  Deleted: ",
                CLR_PINK
            );


            cpln(
                cmd + 3,
                CLR_WHITE
            );
        }

        else {
            cpln(
                "  Not found",
                CLR_RED
            );
        }
    }


    else if (
        streq(
            cmd,
            "ps"
        )
    ) {
        cpln(
            "\n  PID 1  nova-shell  RUNNING\n",
            CLR_GREEN
        );
    }


    else if (
        startswith(
            cmd,
            "run "
        )
    ) {
        const char* program =
            cmd +
            4;


        if (
            streq(
                program,
                "hello"
            )
        ) {
            Scheduler::spawn(
                "hello",
                proc_hello
            );

            proc_hello();
        }

        else if (
            streq(
                program,
                "counter"
            )
        ) {
            Scheduler::spawn(
                "counter",
                proc_counter
            );

            proc_counter();
        }

        else if (
            streq(
                program,
                "sysinfo"
            )
        ) {
            Scheduler::spawn(
                "sysinfo",
                proc_sysinfo
            );

            proc_sysinfo();
        }

        else {
            cp(
                "  Unknown: ",
                CLR_RED
            );


            cpln(
                program,
                CLR_WHITE
            );
        }
    }


    else if (
        streq(
            cmd,
            "color"
        )
    ) {
        cpln(
            "  CYAN",
            CLR_CYAN
        );

        cpln(
            "  VIOLET",
            CLR_VIOLET
        );

        cpln(
            "  PINK",
            CLR_PINK
        );
    }


    else if (
        streq(
            cmd,
            "panic"
        )
    ) {
        PANIC(
            "Manual panic from shell"
        );
    }


    else if (
        startswith(
            cmd,
            "dsave "
        )
    ) {
        const char* rest =
            cmd +
            6;


        int i =
            0;


        while (
            rest[i] &&
            rest[i] != ' '
        ) {
            ++i;
        }


        if (
            rest[i] ==
            ' '
        ) {
            char filename[32];


            int j =
                0;


            for (
                ;
                j < i &&
                j < 31;
                ++j
            ) {
                filename[j] =
                    rest[j];
            }


            filename[j] =
                '\0';


            const char* content =
                rest +
                i +
                1;


            uint32_t len =
                0;


            while (
                content[len]
            ) {
                ++len;
            }


            bool ok =
                NovaFSDisk::save_file(
                    filename,
                    content,
                    len
                );


            cp(
                "  Disk save: ",
                CLR_WHITE
            );


            cpln(
                ok
                    ? "OK"
                    : "FAIL",
                ok
                    ? CLR_GREEN
                    : CLR_RED
            );
        }

        else {
            cpln(
                "  Usage: dsave <file> <text>",
                CLR_YELLOW
            );
        }
    }


    else if (
        startswith(
            cmd,
            "dload "
        )
    ) {
        static char buf[2048];


        uint32_t size =
            0;


        bool ok =
            NovaFSDisk::load_file(
                cmd + 6,
                buf,
                2047,
                &size
            );


        if (ok) {
            cp(
                "  ",
                CLR_WHITE
            );


            cpln(
                buf,
                CLR_CYAN
            );
        }

        else {
            cpln(
                "  Not found on disk",
                CLR_RED
            );
        }
    }


    else if (
        streq(
            cmd,
            "dls"
        )
    ) {
        cpln(
            "\n  NovaFS-Disk Files",
            CLR_CYAN
        );


        NovaFSDisk::list_files(
            ls_cb
        );


        cp(
            "\n",
            CLR_WHITE
        );
    }


    else if (
        streq(
            cmd,
            "disktest"
        )
    ) {
        cpln(
            "\n  Testing ATA disk driver...",
            CLR_CYAN
        );


        static uint8_t wbuf[512];
        static uint8_t rbuf[512];


        for (
            int i = 0;
            i < 512;
            ++i
        ) {
            wbuf[i] =
                (uint8_t)(
                    i &
                    0xFF
                );
        }


        bool writeOk =
            ATA::write_sector(
                100,
                wbuf
            );


        cp(
            "  Write sector 100: ",
            CLR_WHITE
        );


        cpln(
            writeOk
                ? "OK"
                : "FAIL",
            writeOk
                ? CLR_GREEN
                : CLR_RED
        );


        bool readOk =
            ATA::read_sector(
                100,
                rbuf
            );


        cp(
            "  Read sector 100:  ",
            CLR_WHITE
        );


        cpln(
            readOk
                ? "OK"
                : "FAIL",
            readOk
                ? CLR_GREEN
                : CLR_RED
        );


        bool match =
            true;


        for (
            int i = 0;
            i < 512;
            ++i
        ) {
            if (
                rbuf[i] !=
                wbuf[i]
            ) {
                match =
                    false;

                break;
            }
        }


        cp(
            "  Data match:       ",
            CLR_WHITE
        );


        cpln(
            match
                ? "OK"
                : "MISMATCH",
            match
                ? CLR_GREEN
                : CLR_RED
        );


        cp(
            "\n",
            CLR_WHITE
        );
    }


    else if (
        cmd[0] ==
        '\0'
    ) {
    }


    else {
        cp(
            "  Unknown: ",
            CLR_RED
        );


        cpln(
            cmd,
            CLR_WHITE
        );
    }
}


// =============================================================
// Prompt
// =============================================================

static void draw_prompt() {
    cp(
        "> ",
        CLR_CYAN
    );
}


// =============================================================
// Initialization
// =============================================================

void Shell::init() {
    input_len =
        0;


    RamFS::init();

    Scheduler::init();


    draw_header();

    draw_prompt();
}


// =============================================================
// Navigation
// =============================================================

static void execute_nav(
    int hit
) {
    current_nav =
        hit;


    if (
        hit != 2
    ) {
        docs_panel_active =
            false;
    }


    if (
        hit == 0
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        draw_header();
    }


    else if (
        hit == 1
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        open_panel(
            "Nova Browser",
            CLR_CYAN,
            "Coming soon - Phase 3 roadmap.",
            "Full browser with Nova engine."
        );
    }


    else if (
        hit == 2
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        open_docs_panel();
    }


    else if (
        hit == 3
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        open_panel(
            "Game Mode",
            CLR_PINK,
            "NVIDIA GPU + Vulkan - Phase 4.",
            "DLSS, FSR, Nova Overlay."
        );
    }


    else if (
        hit == 4
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        draw_header();
    }


    else if (
        hit == 5
    ) {
        open_files_panel();
    }


    else if (
        hit == 6
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        open_panel(
            "Nova Store",
            CLR_VIOLET,
            "App marketplace - Phase 5.",
            "Native + Proton games."
        );
    }


    else if (
        hit == 7
    ) {
        files_panel_active =
            false;

        file_viewer_active =
            false;


        open_panel(
            "Settings",
            CLR_WHITE,
            "",
            ""
        );


        cpln(
            "  CPU:  x86 i686 32-bit Protected",
            CLR_DIM
        );


        cpln(
            "  GPU:  NVIDIA (roadmap)",
            CLR_DIM
        );


        cpln(
            "  RAM:  ~30MB",
            CLR_DIM
        );


        cpln(
            "  VESA: 800x600x32bpp",
            CLR_DIM
        );


        cpln(
            "  Kernel: C/C++/ASM",
            CLR_DIM
        );
    }
}


// =============================================================
// Main run loop
// =============================================================

void Shell::run() {

    static int mouse_x =
        400;


    static int mouse_y =
        300;


    static bool left_hist[3] = {
        false,
        false,
        false
    };


    // Cursor rendering state.
    //
    // This prevents us from writing the same cursor rectangle
    // to the framebuffer thousands of times per second.

    static bool cursor_initialized =
        false;


    static bool last_cursor_on =
        false;


    static int last_cursor_x =
        -1;


    static int last_cursor_y =
        -1;


    while (true) {

        // -----------------------------------------------------
        // Mouse
        // -----------------------------------------------------

        Mouse::poll();


        Mouse::State& ms =
            Mouse::get_state();


        mouse_x =
            ms.x;


        mouse_y =
            ms.y;


        if (
            mouse_x < 0
        ) {
            mouse_x =
                0;
        }


        if (
            mouse_x > 799
        ) {
            mouse_x =
                799;
        }


        if (
            mouse_y < 0
        ) {
            mouse_y =
                0;
        }


        if (
            mouse_y > 599
        ) {
            mouse_y =
                599;
        }


        // -----------------------------------------------------
        // Mouse click edge detection
        // -----------------------------------------------------

        left_hist[2] =
            left_hist[1];


        left_hist[1] =
            left_hist[0];


        left_hist[0] =
            ms.left;


        bool mouse_clicked =
            left_hist[0] &&
            !left_hist[1];


        if (
            mouse_clicked
        ) {
            int hit =
                sidebar_hit_test(
                    mouse_x,
                    mouse_y
                );


            if (
                hit >= 0
            ) {
                execute_nav(
                    hit
                );


                cursor_initialized =
                    false;
            }
        }


        // -----------------------------------------------------
        // Keyboard navigation
        // -----------------------------------------------------

        if (
            kb_head != kb_tail &&
            !docs_panel_active
        ) {
            char peek =
                kb_buf[
                    kb_tail
                ];


            if (
                input_len == 0 &&
                peek >= '1' &&
                peek <= '8'
            ) {
                kb_tail =
                    (
                        kb_tail +
                        1
                    ) %
                    256;


                execute_nav(
                    peek -
                    '1'
                );


                cursor_initialized =
                    false;
            }
        }


        // -----------------------------------------------------
        // Files panel
        // -----------------------------------------------------

        if (
            files_panel_active
        ) {
            if (
                up_pressed
            ) {
                up_pressed =
                    false;


                if (
                    !file_viewer_active &&
                    file_selected >
                    0
                ) {
                    --file_selected;

                    draw_file_list();
                }
            }


            if (
                down_pressed
            ) {
                down_pressed =
                    false;


                if (
                    !file_viewer_active &&
                    file_selected <
                    file_count -
                    1
                ) {
                    ++file_selected;

                    draw_file_list();
                }
            }


            if (
                esc_pressed
            ) {
                esc_pressed =
                    false;


                if (
                    file_viewer_active
                ) {
                    file_viewer_active =
                        false;


                    draw_file_list();
                }

                else {
                    files_panel_active =
                        false;


                    draw_header();

                    draw_prompt();


                    cursor_initialized =
                        false;
                }
            }


            if (
                f2_pressed &&
                file_viewer_active &&
                file_count >
                0
            ) {
                f2_pressed =
                    false;


                load_doc(
                    file_names[
                        file_selected
                    ]
                );


                files_panel_active =
                    false;


                file_viewer_active =
                    false;


                docs_panel_active =
                    true;


                draw_docs_editor();


                cursor_initialized =
                    false;
            }
        }


        // -----------------------------------------------------
        // Topbar refresh
        // -----------------------------------------------------

        static uint32_t last_topbar_tick =
            0;


        if (
            pit_ticks -
            last_topbar_tick >
            1000
        ) {
            last_topbar_tick =
                pit_ticks;


            draw_topbar();
        }


        // -----------------------------------------------------
        // Terminal cursor
        // -----------------------------------------------------
        //
        // Only draw when:
        //   - blink state changes
        //   - cursor moved
        //   - cursor has not been initialized yet
        //
        // Do not draw the shell cursor over Nova Files or Docs.
        // -----------------------------------------------------

        if (
            !files_panel_active &&
            !docs_panel_active
        ) {
            bool cursorOn =
                (
                    pit_ticks /
                    400
                ) %
                2 ==
                0;


            bool cursorMoved =
                content_x !=
                    last_cursor_x ||
                content_y !=
                    last_cursor_y;


            if (
                !cursor_initialized ||
                cursorOn !=
                    last_cursor_on ||
                cursorMoved
            ) {
                // Erase old cursor if it moved.

                if (
                    cursor_initialized &&
                    cursorMoved &&
                    last_cursor_x >= 0 &&
                    last_cursor_y >= 0
                ) {
                    Framebuffer::draw_rect(
                        last_cursor_x,
                        last_cursor_y,
                        6,
                        9,
                        CLR_BG
                    );
                }


                Framebuffer::draw_rect(
                    content_x,
                    content_y,
                    6,
                    9,
                    cursorOn
                        ? CLR_VIOLET
                        : CLR_BG
                );


                last_cursor_on =
                    cursorOn;


                last_cursor_x =
                    content_x;


                last_cursor_y =
                    content_y;


                cursor_initialized =
                    true;
            }
        }


        // -----------------------------------------------------
        // No queued keyboard character
        // -----------------------------------------------------

        if (
            kb_head ==
            kb_tail
        ) {
            continue;
        }


        char c =
            kb_buf[
                kb_tail
            ];


        // -----------------------------------------------------
        // Enter inside file browser
        // -----------------------------------------------------

        if (
            c == '\n' &&
            files_panel_active &&
            !file_viewer_active &&
            file_count >
            0
        ) {
            kb_tail =
                (
                    kb_tail +
                    1
                ) %
                256;


            file_viewer_active =
                true;


            draw_file_viewer();


            continue;
        }


        // -----------------------------------------------------
        // Nova Docs input
        // -----------------------------------------------------

        if (
            docs_panel_active
        ) {
            kb_tail =
                (
                    kb_tail +
                    1
                ) %
                256;


            if (
                f2_pressed
            ) {
                f2_pressed =
                    false;


                save_doc();

                continue;
            }


            if (
                esc_pressed
            ) {
                esc_pressed =
                    false;


                docs_panel_active =
                    false;


                draw_header();

                draw_prompt();


                cursor_initialized =
                    false;


                continue;
            }


            if (
                c == '\b'
            ) {
                if (
                    doc_len >
                    0
                ) {
                    --doc_len;
                }
            }

            else if (
                doc_len <
                DOC_BUF_SIZE -
                1
            ) {
                doc_buffer[
                    doc_len++
                ] =
                    c;
            }


            doc_buffer[
                doc_len
            ] =
                '\0';


            draw_docs_editor();


            continue;
        }


        // -----------------------------------------------------
        // Consume shell character
        // -----------------------------------------------------

        kb_tail =
            (
                kb_tail +
                1
            ) %
            256;


        // Remove currently visible cursor before changing text.

        if (
            cursor_initialized
        ) {
            Framebuffer::draw_rect(
                content_x,
                content_y,
                6,
                9,
                CLR_BG
            );


            cursor_initialized =
                false;
        }


        // -----------------------------------------------------
        // Enter
        // -----------------------------------------------------

        if (
            c == '\n'
        ) {
            input_buf[
                input_len
            ] =
                '\0';


            cp(
                "\n",
                CLR_WHITE
            );


            handle_command(
                input_buf
            );


            input_len =
                0;


            draw_prompt();
        }


        // -----------------------------------------------------
        // Backspace
        // -----------------------------------------------------

        else if (
            c == '\b'
        ) {
            if (
                input_len >
                0
            ) {
                --input_len;


                content_x -=
                    8;


                Framebuffer::draw_rect(
                    content_x,
                    content_y,
                    8,
                    10,
                    CLR_BG
                );
            }
        }


        // -----------------------------------------------------
        // Normal character
        // -----------------------------------------------------

        else if (
            input_len <
            255
        ) {
            input_buf[
                input_len++
            ] =
                c;


            Framebuffer::draw_char(
                c,
                content_x,
                content_y,
                CLR_WHITE
            );


            content_x +=
                8;
        }
    }
}