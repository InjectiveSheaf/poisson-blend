#ifndef POISSON_H
#define POISSON_H

#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Sparse"
#include <random>

// Класс, отвечающий за главный алгоритм всея программы
// Omega - область, значения функции f для которой мы хотим найти
// S - окружающая её область
// Граничные условия заданы так, что Omega = S на границе Omega, 
// Причем мы считаем, что d Omega < Omega (вложение границы в множество)
// A, b - разреженная матрица и вектор справа в системе Ax = b, которую нам необходимо решить
// neighbours - вектор относительных координат соседей точки (в окрестности фон Неймана радиуса 1)
// PointComparator - структура, необходимая для упорядочивания точек. 
// Порядок типичный - прироритет отдаём горизонтальной оси X, если по X значения одинаковые - сравниваем Y
// f определяет отображение из Omega -> R, которое мы хотим найти по граничным условиям
// Определяем итератор pixelMapIterator, которым будем не раз пользоваться
//
// Метод laplace выдаёт лапласиан той точки, что нас интересует - он добавляется в b для всех точек
// neighboursCount считает непустых соседей у точки, используя данные из Omega (маски) и neighbours
// 
// Далее перейдем от вспомогательных методов к интерфейсу: 
// updateMatrices обновляет отображение f (стоит переименовать)
// buildSLE строит систему по f
// getResult возвращает найденное решение в виде одноканального изображения (CV_8UC1)

class Poisson{
private:
    cv::Mat Omega;
    cv::Mat S;
    Eigen::SparseMatrix<double> A;
    Eigen::VectorXd b;
    std::vector<cv::Point> neighbours{cv::Point(0,1), cv::Point(1,0), cv::Point(0,-1), cv::Point(-1,0)};
    
    struct PointComparator{
        bool operator () (const cv::Point& a, const cv::Point& b) const{ 
            return (a.x < b.x) || (a.x == b.x && a.y < b.y);
        }
    };
    
    std::map<cv::Point, float, PointComparator> f;
    
    typedef std::map<cv::Point, float, PointComparator>::iterator pixelMapIterator;

    float laplace(cv::Point p, cv::Mat &img);
    
    int neighboursCount(cv::Point p); 

    void buildSLE();
public:
    
    void updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage);

    cv::Mat getResult();
};


#endif
