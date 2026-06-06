#include <iostream>
#include <string>

int main() {
    const char* ptr = nullptr;
    #define USE_DVR_SUB
    #ifdef USE_DVR_SUB
        std::string formatted;
        if (true) {
            formatted = "Hello World";
            ptr = formatted.c_str();
        }
    #endif
    
    std::cout << "ptr: " << ptr << std::endl;
    return 0;
}
