all: poisson

poisson: main.o 
	g++ main.o -o poisson \
    -I/usr/include/opencv4/opencv \
    -I/usr/include/opencv4 \
    -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_core \
    -I/usr/local/include/eigen3
