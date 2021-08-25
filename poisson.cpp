#include "poisson.h"


float Poisson::sumofFieldProj(cv::Point p, cv::Mat& img){
    return  4*img.at<float>(p)
            - img.at<float>(p + cv::Point(1,0)) - img.at<float>(p + cv::Point(-1,0))
            - img.at<float>(p + cv::Point(0,1)) - img.at<float>(p + cv::Point(0,-1));
}

int Poisson::neighboursCount(cv::Point p){
    int N = 0;
    for(auto n : neighbours)
        if(((p+n).x >= 0) && ((p+n).y >= 0) && ((p+n).x < Omega.cols) && ((p+n).y < Omega.rows) 
                        && (Omega.at<float>(p + n) != 0) ) N++;
    return N;
}


void Poisson::updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage){
    Omega = overlayingImage.clone();
    Omega.convertTo(Omega,CV_32F);

    S = baseImage.clone();
    S.convertTo(S,CV_32F);
    for(int y = 0; y < Omega.rows; ++y){
        for(int x = 0; x < Omega.cols; ++x){
            cv::Point p(x,y);
            if(Omega.at<float>(p) != 0 && neighboursCount(p) == 4) f[p] = Omega.at<float>(p);
        }
    }
    std::cout << "dgdaf";
    buildSLE();
}


 // Cоставляем уравнения:
 // Итерируем по всем пикселям p в Map f.
 // Задаем коэффициенты разреженной матрицы A и вектор-столбца b.
 // Коэффициенты: -1 в пересечении N и Omega, и 4 для p 



void Poisson::buildSLE(){
    A = Eigen::SparseMatrix<double>(f.size(), f.size());
    b = Eigen::VectorXd::Zero(f.size());
    
    // Задаем коэффициенты в пересечении окрестности и внутренности/границы множества 
    for(pixelMapIterator f_p = f.begin(); f_p != f.end(); f_p++){
        int p = std::distance<pixelMapIterator> (f.begin(), f_p);
        A.insert(p, p) = 4;
        b(p) += sumofFieldProj(f_p->first, Omega);
        
        for(auto n : neighbours){ 
            if(neighboursCount(f_p->first + n) == 4){
                pixelMapIterator f_q = f.find(f_p->first + n);
                int q = std::distance<pixelMapIterator>(f.begin(), f_q);
                A.insert(p,q) = -1;
            }
            else b(p) += S.at<float>(f_p->first + n);
        }
    }

    std::cout << "SLE has been builded in a matrix form" << std::endl;
}



cv::Mat Poisson::getResult(){
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
    Eigen::VectorXd x = LDLT.solve(b);
    std::cout << "Solved: min->" << *(std::min_element(x.data(), x.data()+x.size())) << ", max ->" 
            << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
    
    cv::Mat result = S.clone();
    for(pixelMapIterator f_p = f.begin(); f_p != f.end(); ++f_p){
        int p = std::distance<pixelMapIterator> (f.begin(), f_p);
        float color = x(p);
        if (neighboursCount(f_p->first) != 4) continue;
        if (color < 0) color = 0;
        result.at<float>(f_p->first) = color;
    } 
    
    result.convertTo(result, CV_8U);
    f.clear();
    return result;
}
