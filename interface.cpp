#include "interface.h"


Interface::Interface(cv::Mat& fg, cv::Mat& bg){
    if(fg.size[0] > bg.size[0] || fg.size[1] > bg.size[1]){
        std::cout << "image sizes does not match, please try again." << std::endl 
                  << "the dimension of overlaying image must be less than the dimension of the background"
                  << std::endl;
    }
    foreground = fg.clone();
    background = bg.clone();
    dispForeground = fg.clone();
    dispBackground = bg.clone();
    std::cout << "help:" << std::endl
              << "press s to go to the selection menu, w to write result, r to bring back all pictures and ESC to exit" << std::endl;
    cv::namedWindow("Poisson Blender");
}

// fitFormImage используется в качестве шаблона высоты. 
// Можно выбирать минимум/максимум заменой стандартной функции
// Лямбда resize приводит изображения к необходимой высоте
// Проводим это для каждого изображения с помощью for_each
// Далее соединяем изображения одной высоты по горизонтали в цикле

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

// Приводим все изображение в необходимый для вывода вид и выводим:
// Копируем изображение из шаблонного, рисуем на нём необходимым образом отмасштабированный массив точек
// Далее создаем вектор из изображений, масштабируем с помощью описанной выше hfit и выводим

void Interface::showImages(){
    dispForeground = foreground.clone();
    for(const auto p : pointVector){
        cv::Point q = p * (fittedDisp.size[0] / dispForeground.size[0]);
        cv::circle( dispForeground, q, 3, cv::Scalar(0,255,0), -1 );
    }
    std::vector<cv::Mat> dispVec = {dispForeground, dispBackground};
    fittedDisp = hfit(dispVec);
    cv::imshow("Poisson Blender", fittedDisp);
}

// 114 == r, 97 == a, 100 == d в кодировке ASCII

void Interface::selectPolygon(){
    std::cout << "Help:" << std::endl << "a = add point," << std::endl;
                            std::cout << "d = delete last point," << std::endl;
                            std::cout << "r = return back" << std::endl;
    std::cout << "when you will enter the first point again, blending will start." << std::endl;

    while(true){
        showImages();
        
        unsigned int key = cv::waitKey(15);

        if(key == 114){     
            std::cout << "Getting back to menu." << std::endl; 
            return;
        }

        if(key == 97) selectPoint(); 

        if(key == 100 && !pointVector.empty()) pointVector.pop_back(); 
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

// Оболочка для главного алгоритма: 
// Вырезаем накладываемое изображение по трафарету binaryMask в новый массив mask
// Разбиваем mask и background на каналы, которые будут объединяться в массив result
// Пропускаем каждый канал через алгоритм, получаем результирующий канал итогового изображения
// Объединяем каналы и показываем результат.

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

// 27 == esc, 115 = s, 114 = r, 119 = w в кодировке ASCII

void Interface::startHandler(){
    while(true){
        showImages();
        
        unsigned int key = cv::waitKey(15);
        if(key == 27)     return;           
        if(key == 115)    selectPolygon();  
        if(key == 114)    bringBack();      
        if(key == 119)    cv::imwrite("result.jpg", fittedDisp); 
    }
}








