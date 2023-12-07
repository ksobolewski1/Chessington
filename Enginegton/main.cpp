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

    return 0;
}
#endif
