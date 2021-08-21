#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Core"
#include "eigen3/Eigen/Cholesky"

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
/*
template<typename Derived>
class Poisson{ // Для одного канала, в интерфейсе получим для всех трёх и сложим
private:
    // Eigen::Matrix<typename Derived::cellType, Derived::Rows, Derived::Columns> Omega; // Omega \in S 
    // Eigen::Matrix<typename Derived::cellType, Derived::Rows, Derived::Columns> S; 
    Eigen::MatrixXd Omega, S;
    bool isNeighbour(cv::Point p, cv::Point q){
        int l1_norm = std::abs(p.x - q.x) + std::abs(p.y - q.y);
        return l1_norm == 1;
    }
    double grad(cv::Point p){
        double grad_x = Omega(p.x+1,p.y) - Omega(p.x-1,p.y);
        double grad_y = Omega(p.x,p.y+1) - Omega(p.x,p.y-1);
        return grad_x + grad_y;
    }
public:
 // Для того, чтобы пользоваться в дальнейшем произвольными формами, а не только прямоугольниками
 // Используем представление через маску:
 // 1. Omega = 0 вне области вставки
 // 2. Для удобства предполагаем, что dim(Omega) = dim(S).
 // 
    Poisson();
    void loadMatrices(Eigen::MatrixBase<Derived> & _Omega, Eigen::MatrixBase<Derived> & _S){
    //void loadMatrices(Eigen::MatrixXd & _Omega, Eigen::MatrixXd & _S){
        Omega = _Omega;
        S = _S;
    }

    Eigen::Matrix<typename Derived::cellType, Derived::Rows, Derived::Columns> solve(){ 
    // Eigen::MatrixXd solve(){
        int k = Omega.size() - std::count(Omega.reshaped().begin(),Omega.reshaped().end(), 0); // количество пикселей
        // int k = (Omega.array() != 0).count();
        Eigen::MatrixXd A;
        Eigen::VectorXd b;
        std::unordered_map<cv::Point, double> f; // f: Omega -> R
        
        // Заполняем наше отображение
        for(int x = 0; x < Omega.cols(); x++){
            for(int y = 0; y < Omega.rows(); y++){
                if(Omega(x,y) == 0) continue; // пиксели вне маски нас не интересуют
                else f[cv::Point(x,y)] = Omega(x,y);
            }
        }
        // Теперь составим для каждого пикселя уравнение и решим СЛАУ
        for(const auto &[p_key,p_val] : f){ // p имеет тип cv::Point
            Eigen::VectorXd lhs;
            double rhs = 0;
            // Так как мы не меняем f, порядок обхода сохраняется и СЛАУ правильно составится 
            for(const auto &[q_key,q_val] : f){ 
                if(isNeighbour(q_key,p_key)) lhs << -1; // Коэффициенты для соседей в пересечении с Omega
                if(q_key == p_key) lhs << 4; // Пиксель, у которого мы смотрим окрестность
                else lhs << 0; // Пиксели вне окрестности p
            }
            A << lhs;
            // Считаем правую часть
            std::vector<cv::Point> neighbours = {(0,1),(1,0),(0,-1),(-1,0)};
            for(const auto nbs : neighbours){
                if(!f.count(p_key+nbs)) rhs += S((p_key+nbs).x,(p_key+nbs).y);
            }
            rhs += grad(p_key);
        }
    }
}; 
*/

class Poisson{
private:
    cv::Mat Omega;
    cv::Mat S;
    Eigen::MatrixXd A;
    Eigen::VectorXd b;
    
    struct PointComparator{
        bool operator () (const cv::Point& a, const cv::Point& b) const{ 
            return (a.x < b.x) || (a.x == b.x && a.y < b.y);
        }
    };

    typedef std::map<cv::Point, uchar, PointComparator>::iterator pixelmapIter;
    std::map<cv::Point, uchar, PointComparator> f;
    
    bool isNeighbour(cv::Point p, cv::Point q){
        int l1_norm = std::abs(p.x - q.x) + std::abs(p.y - q.y);
        return l1_norm == 1;
    }

    double gradOmega(cv::Point p){
        double grad_x = Omega.at<char>(p + cv::Point(1,0)) - Omega.at<char>(p + cv::Point(-1,0));
        double grad_y = Omega.at<char>(p + cv::Point(0,1)) - Omega.at<char>(p + cv::Point(0,-1));
        return (grad_x + grad_y)/2;
    }
public:
    void updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage){
        Omega = overlayingImage;
        S = baseImage;
        f.clear();
    }
    
    void doAlgorithm(){
        // Заполняем контейнер пикселями
        for(int y = 0; y < Omega.rows; ++y){
            for(int x = 0; x < Omega.cols; ++x){
                cv::Point p(x,y);
                if(Omega.at<uchar>(p) != 0){
                    f[p] = Omega.at<uchar>(p);
                }
            }
        }
        A.resize(f.size(),f.size());
        b.resize(f.size());
        /* Теперь составляем уравнения
           p_key и q_key - имеют тип cv::Point, по ним идёт итерация
           Так как мы не меняем f, то порядок обхода сохраняется и СЛАУ составится правильно
           Вектор-строка lhs и скаляр rhs - это left-hand и right-hand side соответственно
           Коэффициенты: -1 в пересечении (N /\ Omega), 4 для рассматриваемого пикселя и 0 иначе */
        for(pixelmapIter p = f.begin(); p != f.end(); ++p){
            Eigen::VectorXd lhs(f.size());
            double rhs = 0;
            std::cout << "initialised" << std::endl;
        // Считаем левую часть
            for(pixelmapIter q = f.begin(); q != f.end(); ++q){ 
                int dist = std::distance<pixelmapIter> (f.begin(), q);
                if(isNeighbour(q->first, p->first)) lhs(dist) = -1 ; 
                else if(q->first == p->first) lhs(dist) = 4 ; 
                else lhs(dist) = 0 ; 
            }
            A.row(std::distance<pixelmapIter> (f.begin(), p)) = lhs;
            std::cout << "left-hand ok ";
        // Считаем правую часть: 
        // если точка p+n не содержится в Omega, но p в Omega есть, то она содержится на границе
            std::vector<cv::Point> neighbours{cv::Point(0,1),cv::Point(1,0),cv::Point(0,-1),cv::Point(-1,0)};
            for(const auto n : neighbours){
                if(!f.count(p->first + n)) rhs += S.at<uchar>(p->first + n);
            }
            rhs += gradOmega(p->first);
            b << rhs;
            std::cout << "right-hand ok" << std::endl;
        }
    }
    cv::Mat getResult(){
        Eigen::VectorXd x;
        cv::Mat result(S.size(), S.type(), cv::Scalar(0,0,0));
        /* SparceMatrix<double> A
         * Eigen::SimplicialLDLT<SparseMatrix<double>> Solver;
         * Solver.compute(A);
         * x = Solver.solve(b);
         */ 
        for(pixelmapIter p = f.begin(); p != f.end(); ++p){
            result.at<uchar>(p->first) = x(std::distance<pixelmapIter> (f.begin(), p));
        }
        for(int y = 0; y < result.rows; ++y){
            for(int x = 0; x < result.cols; ++x){
                cv::Point p(x,y);
                if(Omega.at<uchar>(p) == 0){
                    result.at<uchar>(p) = S.at<uchar>(p);
                }
            }
        }
        return result;
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
        std::cout << "help: " << std::endl << " s = select and clone, p = poisson clone, r = return " << std::endl;
        cv::namedWindow("window");
    }

    void startHandler(){
        while(true){
            unsigned int key = cv::waitKey(15);
            if(key == 27) return;
            if(key == 112) poissonClone();
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
        // Подготавливаем область для вырезания
        aim(dispForeground, foreground);
        cv::Mat temp = foreground(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2)));
        aim(dispBackground, background);
        cv::resize(temp,temp, cv::Size(x2-x1, y2-y1));
        cv::Mat mask(foreground.size(),foreground.type(),cv::Scalar(0,0,0));
        temp.copyTo(mask(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2))));
        // Имеем маску с изображением, теперь разобьем всё на каналы 
        std::vector<cv::Mat> maskChannels(3);
        std::vector<cv::Mat> bgChannels(3);
        cv::split(mask,maskChannels);
        cv::split(background,bgChannels);
        // И поканально пропустим в алгоритм
        cv::Mat result;
        std::vector<cv::Mat> resChannels(3);
        Poisson* SimeonDenis = new Poisson();

        for(int i = 0; i < 3; i++){
            SimeonDenis->updateMatrices(maskChannels[i], bgChannels[i]);
            SimeonDenis->doAlgorithm();
            resChannels[i] = SimeonDenis->getResult().clone();
        }
        
        cv::merge(resChannels,result);
        dispBackground = result.clone();
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


