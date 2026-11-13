# Host build of the firmware core (no ESP32 needed).  make -C firmware/test_host
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -I third_party -DARDUINOJSON_USE_DOUBLE=1
SRC = test_main.cpp ../basira_fw/optics.cpp ../basira_fw/axis.cpp ../basira_fw/device.cpp ../basira_fw/protocol.cpp

run: test_host
	./test_host ../../tests/golden_vectors.json

test_host: $(SRC) ../basira_fw/*.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f test_host
.PHONY: run clean
