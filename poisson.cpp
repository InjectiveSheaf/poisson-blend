#include "poisson.h"

double Poisson::sumofFieldProj(cv::Point p){
    return (Omega.at<uchar>(p + cv::Point(1,0)) + Omega.at<uchar>(p + cv::Point(-1,0))
            + Omega.at<uchar>(p + cv::Point(0,1)) + Omega.at<uchar>(p + cv::Point(0,-1)) 
            -4*Omega.at<uchar>(p));
}

void Poisson::updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage){
    f.clear();
    Omega = overlayingImage.clone();
    S = baseImage.clone();
    for(int y = 0; y < Omega.rows; ++y)
        for(int x = 0; x < Omega.cols; ++x)
            if(Omega.at<uchar>(cv::Point(x,y)) != 0)
                f[cv::Point(x,y)] = Omega.at<uchar>(cv::Point(x,y));
    A.resize(f.size(), f.size());
    b.resize(f.size());
}

/* Cоставляем уравнения:
 * Итерируем по всем пикселям p в Map f.
 * Задаем коэффициенты разреженной матрицы A и вектор-столбца b.
 * Коэффициенты: -1 в пересечении N и Omega, и 4 для p 
 */

void Poisson::buildSLE(){
    for(pixelMapIter p = f.begin(); p != f.end(); ++p){
        int p_index = std::distance<pixelMapIter> (f.begin(), p);
        int b_temp = 0;

/* задаем коэффициенты для пересечении окрестности и внутренности/границы множества 
 * Если пиксель содержится в f, то он принадлежит внутренности Omega, иначе - границе */
        A.insert(p_index, p_index) = 4; 
        for(auto n : neighbours){
            pixelMapIter q = f.find(p->first + n); // здесь можно ускорить
            int q_index = std::distance<pixelMapIter> (f.begin(), q);
            if(q != f.end()) A.insert(p_index, q_index) = -1;
            else b_temp += S.at<uchar>(p->first + n);
        }

        b_temp += sumofFieldProj(p->first)/4; // почему 4?
        b(p_index) = b_temp;
    }
    std::cout << "SLE has been builded in a matrix form" << std::endl;
}

cv::Mat Poisson::getResult(){
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
    Eigen::VectorXd x = LDLT.solve(b);
    std::cout << "Solved: min->" << *(std::min_element(x.data(), x.data()+x.size())) << ", max ->" 
            << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
    cv::Mat result = S.clone();
    for(pixelMapIter p = f.begin(); p != f.end(); ++p){
        int color = x(std::distance<pixelMapIter> (f.begin(), p));
        result.at<uchar>(p->first) = color;
    } 
    return result;
}
