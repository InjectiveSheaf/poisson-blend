#include "poisson.h"


// Считаем лапласиан как поток векторного поля через данную точку, 
// Аппроксимируя суммой частных производных первого порядка
// (можно попробовать со вторым порядком, но вряд ли будет сильное различие)

float Poisson::laplace(cv::Point p, cv::Mat& img){
    return  4*img.at<float>(p)
            - img.at<float>(p + cv::Point(1,0)) - img.at<float>(p + cv::Point(-1,0))
            - img.at<float>(p + cv::Point(0,1)) - img.at<float>(p + cv::Point(0,-1));
}

// Если сосед пикселя входит в изображение и он не пустой, т.е. входит в маску - он считается соседом.
// Суммируем в цикле и выводим.

int Poisson::neighboursCount(cv::Point p){
    int N = 0;
    for(auto n : neighbours)
        if( (p+n).x >= 0         && (p+n).y >= 0         &&
            (p+n).x < Omega.cols && (p+n).y < Omega.rows && 
            Omega.at<float>(p + n) != 0 )   N++;
    return N;
}

// Мы используем упорядоченный map во многом потому, что он позволяет 
// биективно сопоставить двумерную точку cv::Point и номер неизвестной в строке матрицы А
// Взаимно-однозначное соответствие достигается благодаря:
// 1. Одинаковому порядку обхода
// 2. Функции distance, которая как раз является взаимно-однозначной
// Можно использовать unordered_map, но для этого необходимо прописать хэш-функцию, вычисляемую для каждого f
// Она тоже будет взаимно-однозначной, при этом вставка и поиск будут вычисляться за O(1), а не за O(logN)
// (позже будет пробоваться)
// Вносим в f только точки, входящие во внутренность множества
// Далее запускаем построение системы

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
    buildSLE();
}

// Система составляется следующим образом
// Итерируем по всем пикселям f_p в Map f.
// Задаем коэффициенты разреженной матрицы A: 
// При f_p стоит 4, при его соседях, если они принадлежат Omega, -1, то есть задаётся аппроксимация лапласиана
// В p-том элементе вектор-столбце b сумма по всем граничным значениям его соседей на границе
// Пройдя всех соседей, добавляем в b_p известное значение лапласиана
// И решаем в численном виде уравнение Пуассона для каждого пикселя: \nabla_d (f_p) - \nabla_b (f) = Sum
// Где nabla_d - дискретный, а nabla_b - известный граничный лапласиан, а Sum - сумма значений на границе


void Poisson::buildSLE(){
    A = Eigen::SparseMatrix<double>(f.size(), f.size());
    b = Eigen::VectorXd::Zero(f.size());
    
    // Задаем коэффициенты в пересечении окрестности и внутренности/границы множества 
    for(pixelMapIterator f_p = f.begin(); f_p != f.end(); f_p++){
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

    std::cout << "SLE has been builded in a matrix form" << std::endl;
}

// Каждая строка матрицы A будет иметь вид (0,...0,-1,0,...,0,-1,4,-1,0,...0,-1,0,...)
// Т.е., A будет очень сильно разреженной. 
// Библиотека Eigen предоставляет класс солверов для систем с разреженными матрицами.
// В качестве прямых методов решения - LL^T, LDL^T, QR и LU - разложения.
// LL^T (Холецкий) использует большое количество квадратных корней => сильная потеря в точности/скорости
// QR - разложение вращениями звучит очень хорошо, так как нам придётся занулять очень мало компонент векторов.
// LU - разложение будет проходить по циклу большое количество раз, гораздо большее, чем количество вращений.
// LDL^T - обладает очень хорошей устойчивостью для больших матриц, однако будет считаться медленней, чем QR.

// Из итогов - отбрасываем LL^T и LU
// Однако нужно протестировать на производительность и устойчивость относительно друг друга QR и LDL^T
// Сейчас используется LDL^T в предположении того, что на небольших данных производительность будет сравнима, а перенасыщение пикселей - меньше.


cv::Mat Poisson::getResult(){
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
    Eigen::VectorXd x = LDLT.solve(b);
    std::cout << "Solved: min->" << *(std::min_element(x.data(), x.data()+x.size())) << ", max ->" 
                                 << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
    
    cv::Mat result = S.clone();
    for(pixelMapIterator f_p = f.begin(); f_p != f.end(); ++f_p){
        int p = std::distance<pixelMapIterator> (f.begin(), f_p);
        float color = x(p);
        if (neighboursCount(f_p->first) != 4) continue; // Пиксели на границе мы берем из S
        if (color < 0) color = 0;
        result.at<float>(f_p->first) = color;
    } 
    
    result.convertTo(result, CV_8U);
    f.clear();
    return result;
}
