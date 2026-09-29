#include "sidebar.h"
#include "desktop.h"
#include "../widgets/button.h"
#include "../theme/colors.h"
#include "../../kernel/drivers/video/framebuffer.h"

static Button homeButton;
static Button appsButton;
static Button filesButton;
static Button terminalButton;
static Button settingsButton;

void Sidebar::init() {
    homeButton.set_bounds(8, 52, 144, 40);
    homeButton.set_label("Home");

    appsButton.set_bounds(8, 96, 144, 40);
    appsButton.set_label("Apps");

    filesButton.set_bounds(8, 140, 144, 40);
    filesButton.set_label("Files");

    terminalButton.set_bounds(8, 184, 144, 40);
    terminalButton.set_label("Terminal");

    settingsButton.set_bounds(8, 228, 144, 40);
    settingsButton.set_label("Settings");
}

void Sidebar::draw() {
    Framebuffer::draw_rect(
        0,
        40,
        160,
        560,
        NovaColors::Surface
    );

    DesktopPage page = Desktop::get_page();

    homeButton.set_selected(
        page == DesktopPage::Home
    );

    appsButton.set_selected(
        page == DesktopPage::Apps
    );

    filesButton.set_selected(
        page == DesktopPage::Files
    );

    terminalButton.set_selected(
        page == DesktopPage::Terminal
    );

    settingsButton.set_selected(
        page == DesktopPage::Settings
    );

    homeButton.draw();
    appsButton.draw();
    filesButton.draw();
    terminalButton.draw();
    settingsButton.draw();
}

void Sidebar::handle_click(
    int mouseX,
    int mouseY
) {
    if (homeButton.contains(mouseX, mouseY)) {
        Desktop::set_page(
            DesktopPage::Home
        );
    }
    else if (appsButton.contains(mouseX, mouseY)) {
        Desktop::set_page(
            DesktopPage::Apps
        );
    }
    else if (filesButton.contains(mouseX, mouseY)) {
        Desktop::set_page(
            DesktopPage::Files
        );
    }
    else if (terminalButton.contains(mouseX, mouseY)) {
        Desktop::set_page(
            DesktopPage::Terminal
        );
    }
    else if (settingsButton.contains(mouseX, mouseY)) {
        Desktop::set_page(
            DesktopPage::Settings
        );
    }
}

// ADD THIS AT THE BOTTOM
void Sidebar::handle_hover(
    int mouseX,
    int mouseY
) {
    homeButton.set_hovered(
        homeButton.contains(mouseX, mouseY)
    );

    appsButton.set_hovered(
        appsButton.contains(mouseX, mouseY)
    );

    filesButton.set_hovered(
        filesButton.contains(mouseX, mouseY)
    );

    terminalButton.set_hovered(
        terminalButton.contains(mouseX, mouseY)
    );

    settingsButton.set_hovered(
        settingsButton.contains(mouseX, mouseY)
    );

    draw();
}