#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Sparse"
#include <random>

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
    std::vector<cv::Point> neighbours;
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
    Poisson(){
        neighbours = {cv::Point(0,1), cv::Point(1,0), cv::Point(0,-1), cv::Point(-1,0)};
    }

    /* Cоставляем уравнения:
     * Итерируем по всем пикселям p в Map f.
     * Задаем коэффициенты разреженной матрицы A и вектор-столбца b.
     * Коэффициенты: -1 в пересечении N и Omega, и 4 для p 
     */
    void setMatrixA(cv::Mat & overlayingImage){
        Omega = overlayingImage.clone();
        for(int y = 0; y < Omega.rows; ++y)
            for(int x = 0; x < Omega.cols; ++x)
                if(Omega.at<uchar>(x,y) != 0) f[cv::Point(x,y)] = Omega.at<uchar>(x,y);
        A.resize(f.size(),f.size());
        b.resize(f.size());
        for(pixelMapIter p = f.begin(); p != f.end(); ++p){
            int p_index = std::distance<pixelMapIter> (f.begin(), p);
            A.insert(p_index, p_index) = 4;
            for(auto n = neighbours.begin(); n != neighbours.end(); ++n){
                pixelMapIter q = f.find(p->first + *n);
                if(q != f.end()) A.insert(p_index, std::distance<pixelMapIter>(f.begin(), q));
            }
        }
        std::cout << "Matrix A has been builded, fsize = " << f.size() << std::endl;
    }

    void setVectorb(cv::Mat & baseImage){
        S = baseImage.clone();
        cv::namedWindow("Poisson Blender");
        cv::imshow("Poisson Blender", S);
        for(pixelMapIter p = f.begin(); p != f.end(); ++p){
            int b_temp = 0;
            int p_index = std::distance<pixelMapIter> (f.begin(), p);
            for(auto n = neighbours.begin(); n != neighbours.end(); ++n){
                pixelMapIter q = f.find(p->first + *n);
                if(q == f.end()) b_temp += S.at<uchar>(q->first);
            }
            b_temp += gradOmega(p->first);
            b(p_index) = b_temp;
        }
        std::cout << "Vector b has been builded." << std::endl;
    }

    cv::Mat getResult(){
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> LDLT(A);
        Eigen::VectorXd x = LDLT.solve(b);
        std::cout << "Solved: min->" << *(std::min_element(x.data(), x.data()+x.size())) << ", max ->" 
                << *(std::max_element(x.data(),x.data()+x.size())) << std::endl;
        cv::Mat result = S.clone();
        for(pixelMapIter p = f.begin(); p != f.end(); ++p){
            result.at<uchar>(p->first) = x(std::distance<pixelMapIter> (f.begin(), p));
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
    cv::Mat fittedDisp;
    int x1, y1, x2, y2;
public:
    Interface(cv::Mat& fg, cv::Mat& bg){
        foreground = fg.clone();
        background = bg.clone();
        dispForeground = fg.clone();
        dispBackground = bg.clone();
        std::cout << "help: " << std::endl << "s = select and clone, p = poisson clone, r = return, w = write." << std::endl;
        cv::namedWindow("Poisson Blender");
    }

    void startHandler(){
        while(true){
            hfitImages();
            cv::imshow("Poisson Blender", fittedDisp);
                    
            unsigned int key = cv::waitKey(15);
            if(key == 27)     return;
            if(key == 112)    poissonClone();
            if(key == 115)    selectClone();
            if(key == 114)    bringBack();
            if(key == 119)    cv::imwrite("result.jpg", fittedDisp);
        }
    }

    void hfitImages(){
            std::vector<cv::Mat> dispVec = {dispForeground, dispBackground};
            fittedDisp = hfit(dispVec);
    }

    void selectRectangle(cv::Mat& disp, cv::Mat& ground){
        std::cout << "input coords: x in (0," << ground.size[1] << "), y in (0," 
                << ground.size[0] << ")" << std::endl;
        std::cin >> x1 >> y1 >> x2 >> y2;
        disp = ground.clone();
        cv::rectangle(disp, cv::Rect(cv::Point(x1,y1), cv::Point(x2,y2)), cv::Scalar(0,255,0));
    }

    void selectClone(){
        selectRectangle(dispForeground, foreground);
        cv::Mat temp = foreground(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2)));
        selectRectangle(dispBackground, background);
        cv::resize(temp, temp, cv::Size(x2-x1, y2-y1));
        temp.copyTo(dispBackground(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2))));
    }

    void poissonClone(){
        // Подготавливаем область для вырезания
        selectRectangle(dispForeground, foreground);
        cv::Mat temp = foreground(cv::Rect(cv::Point(x1,y1),cv::Point(x2,y2)));
        
        selectRectangle(dispBackground, background);
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
            SimeonDenis->setMatrixA(maskChannels[i]);
            SimeonDenis->setVectorb(bgChannels[i]);
            resChannels[i] = SimeonDenis->getResult().clone();      
            cv::imwrite("res"+std::to_string(i)+".jpg", resChannels[i]);
        }
        std::random_device rd; 
        auto rng = std::default_random_engine {rd()};
        std::shuffle(resChannels.begin(), resChannels.end(), rng);
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


