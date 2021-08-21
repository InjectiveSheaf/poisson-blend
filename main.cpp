#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Sparse"

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
 * и коэффициенты в проколотой окрестности f существуют и равны -1 => матрица будет разреженной
 * т.е. слева в строке стоит вектор вида (...,-1,...,-1,4,-1,...,-1,...)
 * справа же можем посчитать сумму значений в окрестности и градиент при помощи разностной аппроксимации
 * тогда, составляем матрицу A из векторов, а значения справа в b - считая для каждой строки
 * потом решаем систему Ax = b, далее ассоциируя значения из x со значениями в результирующем изображении.
 */

class Poisson{
private:
    cv::Mat Omega;
    cv::Mat S;
    Eigen::SparseMatrix<double> A;
    Eigen::VectorXd b;
    
    struct PointComparator{
        bool operator () (const cv::Point& a, const cv::Point& b) const{ 
            return (a.x < b.x) || (a.x == b.x && a.y < b.y);
        }
    };

    typedef std::map<cv::Point, uchar, PointComparator>::iterator pixelmapIter;
    std::map<cv::Point, uchar, PointComparator> f;
    
    int norm_l1(cv::Point p, cv::Point q){
        return std::abs(p.x - q.x) + std::abs(p.y - q.y);
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
        for(int y = 0; y < Omega.rows; ++y){
            for(int x = 0; x < Omega.cols; ++x){
                cv::Point p(x,y);
                if(Omega.at<uchar>(p) != 0){
                    f[p] = Omega.at<uchar>(p);
                }
            }
        }
        A.resize(f.size(), f.size());
        b = Eigen::VectorXd::Zero(f.size());
    }
    
    void doAlgorithm(){
        /* Cоставляем уравнения:
           p_key и q_key - имеют тип cv::Point, по ним идёт итерация
           Так как мы не меняем f, то порядок обхода сохраняется и СЛАУ составится правильно
           Коэффициенты: -1 в пересечении (N /\ Omega), 4 для рассматриваемого пикселя и 0 иначе */
        for(pixelmapIter p = f.begin(); p != f.end(); ++p){
        // Считаем левую часть
            int p_dist = std::distance<pixelmapIter> (f.begin(), p);
            for(pixelmapIter q = f.begin(); q != f.end(); q++){ 
                int q_dist = std::distance<pixelmapIter> (f.begin(), q);
                if(norm_l1(p->first, q->first) == 1) A.insert(p_dist, q_dist) = -1;
                if(norm_l1(p->first, q->first) == 0) A.insert(p_dist, q_dist) = 4;
            }
        // Считаем правую часть: 
        // если точка p+n не содержится в Omega, но p в Omega есть, то она содержится на границе
            int rhs = 0;
            std::vector<cv::Point> neighbours{cv::Point(0,1),cv::Point(1,0),cv::Point(0,-1),cv::Point(-1,0)};
            for(const auto n : neighbours){
                if(!f.count(p->first + n)) rhs += S.at<uchar>(p->first + n);
            }
            rhs += gradOmega(p->first);
            b(p_dist) = rhs;
        }
        std::cout << "SLE has been builded in matrix form" << std::endl;
    }

    cv::Mat getResult(){
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
        Eigen::VectorXd x = LDLT.solve(b);
        
        cv::Mat result(S.size(), S.type(), cv::Scalar(0,0,0));
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
            cv::merge(resChannels,result);
            dispBackground = result.clone();
        }
        cv::imwrite("result.jpg", result);   
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


