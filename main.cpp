#include "interface.h"

int main(int argc, const char** argv) {
    std::string fgName, bgName;
    std::cout << "enter names of overlaying and base images" << std::endl;
    std::cin >> fgName >> bgName;
    cv::Mat foreground = cv::imread(fgName, cv::IMREAD_ANYCOLOR);
    cv::Mat background = cv::imread(bgName, cv::IMREAD_ANYCOLOR);
    if (background.empty() || foreground.empty()) {
        std::cout << "Error: Image cannot be loaded." << std::endl;
        system("pause");
        return -1;
    }
    Interface* I = new Interface(foreground, background);
    I->startHandler();
    return 0;
}


