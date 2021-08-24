#include "poisson.h"

float Poisson::sumofFieldProj(cv::Point p){
    return 4*Omega.at<float>(p) -
            ( Omega.at<float>(p + cv::Point(1,0)) + Omega.at<float>(p + cv::Point(-1,0))
            + Omega.at<float>(p + cv::Point(0,1)) + Omega.at<float>(p + cv::Point(0,-1)) );
}

void Poisson::updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage){
    f.clear();
    Omega = overlayingImage.clone();
    Omega.convertTo(Omega,CV_32F);

    S = baseImage.clone();
    S.convertTo(S,CV_32F);
    for(int y = 0; y < Omega.rows; ++y)
        for(int x = 0; x < Omega.cols; ++x)
            if(Omega.at<float>(cv::Point(x,y)) != 0)
                f[cv::Point(x,y)] = Omega.at<float>(cv::Point(x,y));
    A.resize(f.size(), f.size());
    b.resize(f.size());
    
    buildSLE();
}

/* Cоставляем уравнения:
 * Итерируем по всем пикселям p в Map f.
 * Задаем коэффициенты разреженной матрицы A и вектор-столбца b.
 * Коэффициенты: -1 в пересечении N и Omega, и 4 для p 
 */

void Poisson::buildSLE(){
    for(pixelMapIterator p = f.begin(); p != f.end(); ++p){
        int p_index = std::distance<pixelMapIterator> (f.begin(), p);
        int b_temp = 0;

/* задаем коэффициенты для пересечении окрестности и внутренности/границы множества 
 * Если пиксель содержится в f, то он принадлежит внутренности Omega, иначе - границе */

        A.insert(p_index, p_index) = 4; 
        for(auto n : neighbours){
            pixelMapIterator q = f.find(p->first + n); // здесь можно ускорить
            int q_index = std::distance<pixelMapIterator> (f.begin(), q);
            if(q != f.end()) A.insert(p_index,q_index) = -1;
            else b_temp += S.at<float>(p->first + n);
        }

        b_temp += sumofFieldProj(p->first); // 
        b(p_index) = b_temp;
    }
    std::cout << "SLE has been builded in a matrix form" << std::endl;
}

cv::Mat Poisson::getResult(){
    Eigen::SparseLU<Eigen::SparseMatrix<double>> LDLT(A);
    Eigen::VectorXd x = LDLT.solve(b);
    std::cout << "Solved: min->" << *(std::min_element(x.data(), x.data()+x.size())) << ", max ->" 
            << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
    cv::Mat result = S.clone();
    for(pixelMapIterator p = f.begin(); p != f.end(); ++p){
        float color = x(std::distance<pixelMapIterator> (f.begin(), p));
        result.at<float>(p->first) = std::abs(color);
    } 
    result.convertTo(result, CV_8U);
    return result;
}
