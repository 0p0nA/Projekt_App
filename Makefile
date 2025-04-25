CXX = g++
CXXFLAGS = -std=c++17 `wx-config --cxxflags`
LDFLAGS = `wx-config --libs` -lcurl -lws2_32 -lwldap32 -lcrypt32
SRC = test2.cpp
OUT = app.exe

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

run: all
	./$(OUT)

clean:
	rm -f $(OUT)


