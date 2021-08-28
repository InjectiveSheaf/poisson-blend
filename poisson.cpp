#include "poisson.h"


// Считаем лапласиан как поток векторного поля через данную точку, 
// Аппроксимируя суммой частных производных первого порядка


float Poisson::laplace(cv::Point p, cv::Mat& img){
    return  4*img.at<float>(p)
            - img.at<float>(p + cv::Point(1,0)) - img.at<float>(p + cv::Point(-1,0))
            - img.at<float>(p + cv::Point(0,1)) - img.at<float>(p + cv::Point(0,-1));
}


int Poisson::neighboursCount(cv::Point p){
    int N = 0;
    for(auto n : neighbours){
        bool outOfBorders = (p+n).x < 0  && (p+n).y < 0 && (p+n).x >= Omega.cols && (p+n).y >= Omega.rows;
        if( !outOfBorders && Omega.at<float>(p + n) != 0 ) N++;
    }
    return N;
}


// Вносим в f только точки, входящие во внутренность множества
// Далее запускаем построение системы


void Poisson::updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage){
    baseImage.convertTo(S, CV_32F);
    overlayingImage.convertTo(Omega, CV_32F);
    
    for(int y = 0; y < Omega.rows; ++y){
        for(int x = 0; x < Omega.cols; ++x){
            cv::Point p(x,y);
            if(Omega.at<float>(p) != 0 && neighboursCount(p) == 4) f[p] = Omega.at<float>(p);
        }
    }

    buildSLE();
}


// Система составляется следующим образом
// Итерируем по всем пикселям f_p в f.
// Задаем коэффициенты разреженной матрицы A: 
// При f_p стоит 4, при его соседях, если они принадлежат Omega, -1, то есть задаётся аппроксимация лапласиана
// В p-том элементе вектор-столбце b сумма по всем граничным значениям его соседей на границе
// Пройдя всех соседей, добавляем в b_p известное значение лапласиана
// И решаем в численном виде уравнение Пуассона для каждого пикселя: \nabla_d (f_p) - \nabla_b (f) = Sum
// Где nabla_d - дискретный, а nabla_b - известный граничный лапласиан, а Sum - сумма значений на границе


void Poisson::buildSLE(){
    A = Eigen::SparseMatrix<double>(f.size(), f.size());
    b = Eigen::VectorXd::Zero(f.size());
    
    for(pixelMapIterator f_p = f.begin(); f_p != f.end(); ++f_p){
        int p = std::distance<pixelMapIterator> (f.begin(), f_p);
        A.insert(p, p) = 4;
        
        for(auto n : neighbours){ 
            if(neighboursCount(f_p->first + n) == 4){
                pixelMapIterator f_q = f.find(f_p->first + n);
                int q = std::distance<pixelMapIterator>(f.begin(), f_q);
                A.insert(p,q) = -1;
            }
            else b(p) += S.at<float>(f_p->first + n);
        }

        b(p) += laplace(f_p->first, Omega);
    }

    std::cout << "SLE has been builded" << std::endl;
}


// Каждая строка матрицы A будет иметь вид (0,...0,-1,0,...,0,-1,4,-1,0,...0,-1,0,...)
// Т.е., A будет очень сильно разреженной. 
// Библиотека Eigen предоставляет класс солверов для систем с разреженными матрицами.
// В качестве прямых методов решения - LL^T, LDL^T, QR и LU - разложения.
// LL^T (Холецкий) использует большее количество умножений + корни => потеря в скорости
// QR - сильно замедлено из-за необходимости сжимать матрицу при помощи makeCompressed 
// LU - разложение будет проходить по циклу большее количество раз, чем при использовании QR.
// Остаётся LDL^T, обладающий очень хорошей устойчивостью и довольно быстрый.


cv::Mat Poisson::getResult(){
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
    Eigen::VectorXd x = LDLT.solve(b);

    std::cout << "Solved: min->" << *(std::min_element(x.data(), x.data()+x.size())) << ", max ->" 
                                 << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
    
    cv::Mat result = S.clone();

    for(pixelMapIterator f_p = f.begin(); f_p != f.end(); ++f_p){
        int p = std::distance<pixelMapIterator> (f.begin(), f_p);
        float color = x(p);
        if (neighboursCount(f_p->first) == 4) result.at<float>(f_p->first) = color;
    } 
    
    result.convertTo(result, CV_8U);
    f.clear();
    return result;
}


