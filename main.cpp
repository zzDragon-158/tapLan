#include "tapLanServer.hpp"

int main() {
    TapLanServer tLS(9993);
    tLS.start();
    while (1);
    return 0;
}
