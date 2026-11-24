#include "control.h"
#include "cli.h"

int main(int argc, char* argv[]) {
    ControlSettings settings;
    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }

    return RunControl(&settings);
}
