#include "interface.h"

Interface::Interface(cv::Mat& fg, cv::Mat& bg){
    foreground = fg.clone();
    background = bg.clone();
    dispForeground = fg.clone();
    dispBackground = bg.clone();
    std::cout << "help:" << std::endl << 
            "press s to go to selection menu, r to bring back all pictures, ESC to exit" << std::endl;
    cv::namedWindow("Poisson Blender");
}



cv::Mat Interface::hfit(std::vector<cv::Mat> &images){
    const auto fitFormImage =
    std::max_element(images.begin(), images.end(),
        [](const cv::Mat& im1, const cv::Mat& im2){return im1.size[0] < im2.size[0];}
    );

    const auto resize = [&fitFormImage](cv::Mat& im){
        cv::resize(im, im, fitFormImage->size());
    };

    std::for_each(images.begin(), images.end(), resize);

    cv::Mat result = *images.begin();

    for(auto it = images.begin()+1; it != images.end(); ++it)
        cv::hconcat(result, *it, result);

    return result;
}



void Interface::showImages(){
    dispForeground = foreground.clone();
    for(const auto p : pointVector)
        cv::circle( dispForeground, p * (fittedDisp.size[0] / dispForeground.size[0]), 
                        3, cv::Scalar(0,255,0), -1 );
       
    std::vector<cv::Mat> dispVec = {dispForeground, dispBackground};
    fittedDisp = hfit(dispVec);
    cv::imshow("Poisson Blender", fittedDisp);
}

void Interface::selectPolygon(){
    std::cout << "Help:" << std::endl << "a = add point," << std::endl;
                            std::cout << "d = delete last point," << std::endl;
                            std::cout << "r = return back" << std::endl;
    std::cout << "when the first point will be entered again, Poisson blending will start." << std::endl;

    while(true){
        showImages();
        
        unsigned int key = cv::waitKey(15);
        if(key == 114){     // r
            std::cout << "Getting back to menu." << std::endl; 
            return;
        }
        if(key == 97) selectPoint(); // a
        if(key == 100 && !pointVector.empty()) pointVector.pop_back(); // d
    }
}



void Interface::selectPoint(){
    std::cout << "Enter point coordinates: x in (0, " << foreground.size[0] << "), " 
            << "y in (0, " << foreground.size[1] << ") "<< std::endl;
    int x, y;
    std::cin >> x >> y;
    if(pointVector.size() > 2 && x == pointVector[0].x && y == pointVector[0].y){
        std::cout << "Attempting to realise Poisson blending:" << std::endl;
        cv::Mat binaryMask(background.size(), background.type(), cv::Scalar(0,0,0));
        cv::fillConvexPoly(binaryMask, pointVector.data(), 
                        pointVector.size(), cv::Scalar(255,255,255));
        poissonClone(binaryMask);
        std::cout << "Done. Press r to return to main menu." << std::endl;
        return;
    }
    pointVector.push_back(cv::Point(x,y));
}



void Interface::poissonClone(cv::Mat & binaryMask){
    cv::Mat mask(background.size(), background.type(),cv::Scalar(0,0,0));
    foreground.copyTo(mask, binaryMask);
    
    std::vector<cv::Mat> maskChannels(3);
    std::vector<cv::Mat> bgChannels(3);
    cv::split(mask,maskChannels);
    cv::split(background,bgChannels);
    
    cv::Mat result;
    std::vector<cv::Mat> resChannels(3);
    
    Poisson* SimeonDenis = new Poisson();
    for(int i = 0; i < 3; i++){
        SimeonDenis->updateMatrices(maskChannels[i], bgChannels[i]);
        resChannels[i] = SimeonDenis->getResult().clone();
        // cv::imwrite("res"+std::to_string(i)+".jpg", resChannels[i]);
    }
    cv::merge(resChannels, result);
    dispBackground = result.clone();
}



void Interface::startHandler(){
    while(true){
        showImages();
        
        unsigned int key = cv::waitKey(15);
        if(key == 27)     return;           // esc
        if(key == 115)    selectPolygon();  // s
        if(key == 114)    bringBack();      // r
        if(key == 119)    cv::imwrite("result.jpg", fittedDisp); // w
    }
}







