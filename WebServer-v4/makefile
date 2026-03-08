CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g -pthread

TARGET = server

# 源文件列表
SRCS = main.cpp \
       config.cpp \
       webserver.cpp \
       http/http_conn.cpp \
       timer/timer.cpp \
       log/log.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -rf $(TARGET) *.o *.log