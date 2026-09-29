#pragma once


enum class DesktopPage {
    Home,
    Apps,
    Files,
    Terminal,
    Settings
};


namespace Desktop {

    void init();

    void run();

    void draw();

    void set_page(
        DesktopPage page
    );

    DesktopPage get_page();
}