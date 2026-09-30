#pragma once

namespace FilesPage {

    void init();

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