#include "pch.h"
#include "Enginegton.h"


#define ENGINEGTON_DLL __declspec(dllexport)

#ifdef __cplusplus
extern "C" {
#endif
    ENGINEGTON_DLL void Run(const char* path, const char* priv_path) {
        std::unique_ptr<Enginegton> engine = std::make_unique<Enginegton>(path, priv_path);
        engine->Run();
    }

#ifdef __cplusplus
}
#endif

#ifdef BUILD_CONSOLE

int main() {

    std::unique_ptr<Enginegton> engine = std::make_unique<Enginegton>("", "");

    std::vector<int> start_board = {

        9, 7, 8, 10, 11, 8, 7, 9,
        6, 6, 6, 6, -1, 6, 6, 6,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 0, 6, -1, -1, -1,
        -1, -1, -1, -1, 0, -1, -1, -1,
        0, 0, 0, -1, -1, 0, 0, 0,
        3, 1, 2, 4, 5, 2, 1, 3

    };

    std::vector<int> test_board = {

        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1
    };

    //engine->TestSearch(start_board, 1, 1, 1);

    return 0;
}
#endif