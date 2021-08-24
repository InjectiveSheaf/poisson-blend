#ifndef INTERFACE_H
#define INTERFACE_H

#include "poisson.h"

class Interface{
private:
    cv::Mat foreground;
    cv::Mat background;
    cv::Mat dispForeground;
    cv::Mat dispBackground;
    cv::Mat fittedDisp;
    int x1, y1, x2, y2;
    std::vector<cv::Point> pointVector;

    cv::Mat hfit(std::vector<cv::Mat> &images);

    void showImages();

    void selectPolygon();

    void selectPoint();

    void poissonClone(cv::Mat & binaryMask);

    void bringBack(){
        std::cout << "bringing all back" << std::endl;
        dispForeground = foreground.clone();
        dispBackground = background.clone();
    }

public:
    Interface(cv::Mat& fg, cv::Mat& bg);

    void startHandler();
};

#endif
