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

class Poisson{
private:
    cv::Mat Omega;
    cv::Mat S;
    Eigen::SparseMatrix<double> A;
    Eigen::VectorXd b;
    std::vector<cv::Point> neighbours;
    
    struct PointComparator{
        bool operator () (const cv::Point& a, const cv::Point& b) const{ 
            return (a.x < b.x) || (a.x == b.x && a.y < b.y);
        }
    };

    std::map<cv::Point, uchar, PointComparator> f;
    typedef std::map<cv::Point, uchar, PointComparator>::iterator pixelMapIter;

    double sumofFieldProj(cv::Point p);
public:
    Poisson(){
        neighbours = {cv::Point(0,1), cv::Point(1,0), cv::Point(0,-1), cv::Point(-1,0)};
    }

    void updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage);

    void buildSLE();

    cv::Mat getResult();
};


#endif
