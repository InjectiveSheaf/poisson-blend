#include <iostream>
#include "opencv2/opencv.hpp"
#include <string>
#include <algorithm>
#include "eigen3/Eigen/Core"

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
        std::cout << "help: " << std::endl << " s = select and clone, r = return " << std::endl;
        cv::namedWindow("window");
    }

    void startHandler(){
        while(true){
            unsigned int key = cv::waitKey(15);
            if(key == 27) return;
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


