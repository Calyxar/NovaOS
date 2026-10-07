/** NovaOS — First Boot Setup */
#pragma once

namespace SetupPage {

    void init();

    // Blocks until an account has been created
    // and logged in successfully.
    bool run();

    void draw();

}