#include "interface.h"

/* Имеем f(x,y) - искомую функцию на Omega и f*(x,y) - известную функцию на S
 * Помимо этого знаем градиент gradFx(x,y) = df/dx + df/dy 
 * Пока что работаем в условии того, что включение Omega \in S нестрогое,
 * т.е. коэффициент N=4 перед f_p
 * и все коэффицинты перед f_q в окрестности f_p равны -1
 * составляем матрицу A и считаем b для каждой строки
 * потом решаем систему Ax = b, далее ассоциируя значения из x со значениями в результирующем изображении.
 */

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


