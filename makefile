all: poisson-blend clean

poisson-blend: main.o interface.o poisson.o
		g++ main.o interface.o poisson.o -o poisson-blend\
		-I/usr/include/opencv4/opencv -I/usr/include/opencv4\
		-lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_core\
		-I/usr/local/include/eigen3

main.o: main.cpp
		g++ -c main.cpp

interface.o: interface.cpp
		g++ -c interface.cpp

poisson.o: poisson.cpp
		g++ -c poisson.cpp

clean:
		rm -rf *.o
