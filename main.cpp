#include <iostream>
#include "opencv2/opencv.hpp"
#include <string>
#include <algorithm>

void m_callback(int event, int x, int y, int flags, void* param);

cv::Rect box;
bool drawing_box = false;

void draw_box(cv::Mat& img, cv::Rect box){
    cv::rectangle(img, box.tl(), box.br(), cv::Scalar(0x00,0x00,0xff));
}

void crop_box(cv::Mat& bg_image, cv::Mat& fg_image, cv::Rect box){
    cv::Mat temp = fg_image(box);
    temp.copyTo(bg_image);
}

cv::Mat hfitted_output(std::vector<cv::Mat> &images){
        const std::vector<cv::Mat>::iterator min_img = 
        std::min_element(images.begin(), images.end(),
            [](const cv::Mat& im1, const cv::Mat& im2){return im1.size[0] < im2.size[0];}
        );

        const auto resize = [&min_img](cv::Mat& im){
            cv::resize(im, im, cv::Size(im.size[1] * min_img->size[0] / im.size[0],min_img->size[0]));
        };

        std::for_each(images.begin(), images.end(), resize);

        cv::Mat result = *images.begin();

        for(auto it = images.begin()+1; it != images.end(); ++it){
            cv::hconcat(result, *it, result);
        }
        return result;
}

int main(int argc, const char** argv) {
    cv::Mat fg_img = cv::imread("foreground_im.jpg", cv::IMREAD_ANYCOLOR);
    cv::Mat bg_img = cv::imread("background_im.jpg", cv::IMREAD_ANYCOLOR);
    cv::Mat temp;
    if (bg_img.empty() || fg_img.empty()) {
        std::cout << "Error: Image cannot be loaded." << std::endl;
        system("pause");
        return -1;
    }

    const std::string winName = "Image Window";
    cv::namedWindow(winName);
    cv::setMouseCallback(winName, m_callback, (void*)&fg_img);

    while(true){
        fg_img.copyTo(temp);
        if(drawing_box) draw_box(temp,box);
        std::vector<cv::Mat> images{temp, bg_img};
        cv::imshow(winName,hfitted_output(images));
        if (cv::waitKey(15) == 27) break;
    }

    return 0;
}

void m_callback(int event, int x, int y, int flags, void* param){
    cv::Mat& image = *(cv::Mat*) param;
    switch(event){
        case cv::EVENT_MOUSEMOVE:{
            if(drawing_box){
                box.width = x - box.x;
                box.height = y - box.y;
            }
        }
        break;

        case cv::EVENT_LBUTTONDOWN:{
            drawing_box = true;
            box = cv::Rect(x,y,0,0);
        }
        break;

        case cv::EVENT_LBUTTONUP:{
            drawing_box = false;
            if(box.width < 0){
                box.x += box.width;
                box.width *= -1;
            }
            if(box.height < 0){
                box.y += box.height;
                box.height *= -1;
            }
            draw_box(image, box);
        }
        break;
    }
}

