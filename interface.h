#ifndef INTERFACE_H
#define INTERFACE_H

#include "poisson.h"


// Простой интерфейс, позволяющий визуализировать создание маски.
//
// foreground, background - вставляемое и фоновые изображения, соответственно 
// получаемые при создании экземляра интерфейса.
// dispForeground, dispBackground - изображения, которые выводятся на экран в showImages.
// fittedDisp - конкатенированные dispForeground и dispForeground, приведённые к одной высоте при помощи hfit.
// x1, y1, x2, y2 - локальные переменные, использующиеся для ввода точек маски в selectPoint.
// pointVector - вектор точек многоугольника, получаемых в selectPolygon, 
// выпуклая оболочка которых является маской.
// метод bringBack возвращает изображения в исходный вид. 
// метод startHandler запускает обработчик событий.

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
