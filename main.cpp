#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Sparse"

cv::Mat hfit(std::vector<cv::Mat> &images){
        const std::vector<cv::Mat>::iterator fitImg = 
        std::max_element(images.begin(), images.end(),
            [](const cv::Mat& im1, const cv::Mat& im2){return im1.size[0] < im2.size[0];}
        );

        const auto resize = [&fitImg](cv::Mat& im){
            cv::resize(im, im, fitImg->size());
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
 * т.е. коэффициент N=4 перед f_p
 * и все коэффицинты перед f_q в окрестности f_p равны -1
 * составляем матрицу A и считаем b для каждой строки
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

    std::map<cv::Point, uchar, PointComparator> f;
    typedef std::map<cv::Point, uchar, PointComparator>::iterator pixelMapIter;

    double gradOmega(cv::Point p){
        return (Omega.at<uchar>(p + cv::Point(1,0)) + Omega.at<uchar>(p + cv::Point(-1,0))
                + Omega.at<uchar>(p + cv::Point(0,1)) + Omega.at<uchar>(p + cv::Point(0,-1)) 
                )-4*Omega.at<uchar>(p);
    }
public:
    void updateMatrices(cv::Mat & overlayingImage, cv::Mat & baseImage){
        f.clear();
        Omega = overlayingImage.clone();
        S = baseImage.clone();
        for(int y = 0; y < Omega.rows; ++y){
            for(int x = 0; x < Omega.cols; ++x){
                cv::Point p(x,y);
                if(Omega.at<uchar>(p) != 0){
                    f[p] = Omega.at<uchar>(p);
                }
            }
        } 
        A.resize(f.size(), f.size());
        b.resize(f.size());
    }
    
/* Cоставляем уравнения:
 * Итерируем по всем пикселям p в Map f.
 * Задаем коэффициенты разреженной матрицы A и вектор-столбца b.
 * Коэффициенты: -1 в пересечении N и Omega, и 4 для p 
 */
    void buildSLE(){    
        for(pixelMapIter p = f.begin(); p != f.end(); ++p){
            int p_index = std::distance<pixelMapIter> (f.begin(), p);
            int b_temp = 0;
            
            A.insert(p_index, p_index) = 4; // рассматриваемый пиксель
            std::vector<cv::Point> neighbours{cv::Point(0,1), cv::Point(1,0), cv::Point(0,-1), cv::Point(-1,0)};
            // задаем коэффициенты для пересечении окрестности и внутренности/границы множества
            for(const auto n : neighbours){
                pixelMapIter q = f.find(p->first + n);
                if(q != f.end()){ 
                    int q_index = std::distance<pixelMapIter> (f.begin(), q);
                    A.insert(p_index, q_index) = -1;
                }
                else b_temp += S.at<uchar>(p->first + n); // если нет, то это граница Omega
            }

            b_temp += gradOmega(p->first);
            b(p_index) = b_temp;
            // std::cout << "SLE building: " << p_dist << " of " << f.size() << std::endl;
        }
        std::cout << "SLE has been builded in a matrix form" << std::endl;
    }

    cv::Mat getResult(){
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
        Eigen::VectorXd x = LDLT.solve(b);
        std::cout << *(std::min_element(x.data(), x.data()+x.size())) << " <- min, max ->" << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
        cv::Mat result(S.size(), S.type(), cv::Scalar(0,0,0));
        for(pixelMapIter p = f.begin(); p != f.end(); ++p){
            result.at<uchar>(p->first) = uchar(x(std::distance<pixelMapIter> (f.begin(), p)));
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
        std::cout << "help: " << std::endl << "s = select and clone, p = poisson clone, r = return, w = write." << std::endl;
        cv::namedWindow("window");
    }

    void startHandler(){
        while(true){
            unsigned int key = cv::waitKey(15);
            if(key == 27) return;
            if(key == 112) poissonClone();
            if(key == 115) selectClone();
            if(key == 114) bringBack();
            if(key == 119) writeImage();
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
        cv::Mat mask(background.size(),background.type(),cv::Scalar(0,0,0));
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
            SimeonDenis->buildSLE();
            resChannels[i] = SimeonDenis->getResult().clone();
            dispBackground = result.clone();
        }
        cv::merge(resChannels,result);
        dispBackground = result.clone();
    }

    void bringBack(){
        std::cout << "bringing all back" << std::endl;
        dispForeground = foreground.clone();
        dispBackground = background.clone();
    }

    void writeImage(){
        cv::imwrite("result.jpg", dispBackground);
    }
};

int main(int argc, const char** argv) {
    std::string fgName, bgName;
    std::cout << "enter names of overlaying and base images" << std::endl;
    std::cin >> fgName >> bgName;
    cv::Mat foreground = cv::imread(fgName, cv::IMREAD_ANYCOLOR);
    cv::Mat background = cv::imread(bgName, cv::IMREAD_ANYCOLOR);
    if (background.empty() || foreground.empty()) {
        std::cout << "Error: Image cannot be loaded." << std::endl;
        system("pause");
        return -1;
    }
    Interface I(foreground, background);
    I.startHandler();
    return 0;
}


