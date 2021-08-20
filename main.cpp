#include <iostream>
#include <string>
#include <algorithm>
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Core/eigen.hpp"


cv::Mat hfit(std::vector<cv::Mat> &images){
        const std::vector<cv::Mat>::iterator min_img = 
        std::min_element(images.begin(), images.end(),
            [](const cv::Mat& im1, const cv::Mat& im2){return im1.size[0] < im2.size[0];}
        );

        const auto resize = [&min_img](cv::Mat& im){
            cv::resize(im, im, min_img->size());
        };

        std::for_each(images.begin(), images.end(), resize);

        cv::Mat result = *images.begin();

        for(auto it = images.begin()+1; it != images.end(); ++it){
            cv::hconcat(result, *it, result);
        }
        return result;
}
/* Имеем f(x,y) - искомую функцию на Omega и f*(x,y) - известную функцию на S
 * Помимо этого знаем градиент gradFx(x,y) = df/dx + df/dy 
 * Пока что работаем в условии того, что включение Omega \in S нестрогое,
 * т.е. коэффициент N=4 перед f \forall N,
 * и коэффициенты в проколотой окрестности f существуют и равны -1
 * т.е. слева в строке стоит вектор вида (...,-1,...,-1,4,-1,...,-1,...)
 * справа же можем посчитать сумму значений в окрестности и градиент при помощи разностной аппроксимации
 * тогда, составляем матрицу из векторов, а значения справа - 
 * суммируя результаты функций getRightSum и getGradient для каждой строки.
 */

class PoissonSolver{ // Для одного канала, в интерфейсе получим для всех трёх и сложим
private:
    Eigen::Matrix Omega;
    Eigen::Matrix S;
    unsigned int p; // количество пикселей в накладываемом изображении
    
    struct Neighbourhood{
        int left, right, bottom, top;
        Neighbourhood(int x, int y){
            left = x-1 + y*Omega.rows();
            right = x+1 + y*Omega.rows();
            top = (y+1)*Omega.rows() + x;
            botton = (y-1)*Omega.rows() + x;
        }
        bool isNeighbour(int a, int b){
            int position = a + b*Omega.rows();
            if(position == left || position == right || position == top || position == bottom) return true;
            else return false;
        }
    };

    double getRightSum(int x, int y){
        double Sum = 0;
        for(int dx = -1; dx <= 1; dx++)
            for(int dy = -1; dy <= 1; dy++)
                Sum += S(x+dx,y+dx);
        Sum += (Omega(x+1,y) - Omega(x-1,y))/2;
        Sum += (Omega(x,y+1) - Omega(x,y-1))/2;
        return Sum;
    }

    Eigen::Vector getMatrixRow(int x, int y){
        Eigen::Vector row = Eigen::Vector::Zero(p);
        row(y*Omega.rows() + x) = 4;
        for(int dx = -1; dx <= 1; dx++)
            for(int dy = -1; dy <= 1; dy++)
                if(Neighbourhood(x,y).isNeighbour(x+dx,y+dy)) row((y+dy)*Omega.rows()+x+dx) = -1;
    }
public:
/* Для того, чтобы пользоваться в дальнейшем произвольными формами, а не только прямоугольниками
 * Используем следующее(не оптимизированное) представление через маску:
 * 1. Omega = 0 там, где мы её не знаем,
 * 2. Для удобства предполагаем, что dim(Omega) = dim(S).
 */ 
    PoissonSolver(Eigen::Matrix& omega, Eigen::Mat& s){
        Omega = omega;
        S = s;
        p = Omega.cols()*Omega.rows();
    }

    Eigen::Matrix getResult(){
        // Будущие левая и правая часть
        Eigen::Matrix A = Eigen::Matrix::Zero(p,p); 
        Eigen::Vector b = Eigen::Vector::Zero(p);
        for(int x = 0; x < Omega.cols(); x++){
            for(int y = 0; y < Omega.rows(); y++){
                Eigen::Vector row = Eigen::Vector::Zero(p);
                double right
                if(Omega(x,y) != 0){
                Eigen::Vector row = Eigen::Vector::Zero(p);
                row[p]
                }
            }
        }
    }
};

class Interface{
private:
    cv::Mat foreground;
    cv::Mat background;
    cv::Mat dispForeground;
    cv::Mat dispBackground;
    int x1, y1, x2, y2;
public:
    Interface(cv::Mat& fg, cv::Mat& bg){
        foreground = fg.clone();
        background = bg.clone();
        dispForeground = fg.clone();
        dispBackground = bg.clone();
        std::cout << "help: " << std::endl << " s = select and clone, r = return " << std::endl;
        cv::namedWindow("window");
    }

    void startHandler(){
        while(true){
            unsigned int key = cv::waitKey(15);
            if(key == 27) return;
            if(key == 115) selectClone();
            if(key == 114) bringBack();
            std::vector<cv::Mat> images{dispForeground, dispBackground};
            cv::imshow("window", hfit(images));
        }
    }

    void aim(cv::Mat& disp, cv::Mat& ground){
        std::cout << "input coords: x in (0," << ground.size[1] << "), y in (0," 
                << ground.size[0] << ")" << std::endl;
        std::cin >> x1 >> y1 >> x2 >> y2;
        disp = ground.clone();
        cv::rectangle(disp, cv::Rect(cv::Point(x1,y1), cv::Point(x2,y2)), cv::Scalar(0,255,0));
    }

    void selectClone(){
        aim(dispForeground, foreground);
        cv::Mat temp = foreground(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2)));
        aim(dispBackground, background);
        cv::resize(temp, temp, cv::Size(x2-x1, y2-y1));
        temp.copyTo(dispBackground(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2))));
    }

    void poissonClone(){
        aim(dispForeground, foreground);
        cv::Mat Omega = 
    }

    void bringBack(){
        std::cout << "bringing all back" << std::endl;
        dispForeground = foreground.clone();
        dispBackground = background.clone();
    }
};

int main(int argc, const char** argv) {
    cv::Mat fg_img = cv::imread("foreground_im.jpg", cv::IMREAD_ANYCOLOR);
    cv::Mat bg_img = cv::imread("background_im.jpg", cv::IMREAD_ANYCOLOR);
    if (bg_img.empty() || fg_img.empty()) {
        std::cout << "Error: Image cannot be loaded." << std::endl;
        system("pause");
        return -1;
    }
    Interface I(fg_img, bg_img);
    I.startHandler();
    return 0;
}


