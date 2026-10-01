#pragma once


namespace FilesPage {

    void init();

    // Process keyboard/editor state.
    // Returns true if something changed
    // and the page needs to be redrawn.
    bool update();

    void draw();

    void handle_hover(
        int mouseX,
        int mouseY
    );

    void handle_click(
        int mouseX,
        int mouseY
    );
}