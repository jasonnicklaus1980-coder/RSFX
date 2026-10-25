# RSFX build. `make test` = x86 host test (ASan/UBSan); `make arm` = 32-bit ARM MPC plugin.
CXX      ?= g++
ARMCXX   ?= arm-linux-gnueabihf-g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -fPIC -fvisibility=hidden -fno-exceptions -fno-rtti
ARMFLAGS := -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard
SRC      := src/rsfx.cpp
HDRS     := $(wildcard src/*.h)

all: test arm

build/rsfx_host.so: $(SRC) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -g -fsanitize=address,undefined -shared -o $@ $(SRC)

build/host_test: tests/host_test.cpp src/vst2.h src/algos.h
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -g -fsanitize=address,undefined -o $@ $< -ldl

build/list_algos: tools/list_algos.cpp $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -o $@ $<

test: build/rsfx_host.so build/host_test
	ASAN_OPTIONS=detect_leaks=1 ./build/host_test ./build/rsfx_host.so

docs: build/list_algos
	./build/list_algos > docs/ALGORITHMS.md

arm: build/arm/rsfx.so

build/arm/rsfx.so: $(SRC) $(HDRS)
	@mkdir -p build/arm
	$(ARMCXX) $(CXXFLAGS) $(ARMFLAGS) -shared -static-libstdc++ -static-libgcc -Wl,--no-undefined -Wl,-z,defs \
	    -o $@ $(SRC) -lm
	arm-linux-gnueabihf-strip --strip-unneeded $@ 2>/dev/null || true

skin:
	python3 tools/make_skin.py
	python3 tools/check_skin.py

# previews of the skin with the plugin's real state, e.g. `make preview PROGRAM=79`
PROGRAM ?= 15
preview: build/dump_state
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -fPIC -shared -fvisibility=hidden -o build/rsfx_prev.so $(SRC)
	./build/dump_state ./build/rsfx_prev.so $(PROGRAM) -3 > build/state.json
	python3 tools/make_skin.py --preview build/state.json build/preview.png

build/dump_state: tests/dump_state.cpp src/vst2.h
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -o $@ $< -ldl

build/compat_test: tests/compat_test.cpp src/vst2.h
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -o $@ $< -ldl

# compare against an older build: make compat OLD=/path/to/old/rsfx.so (x86 builds of both)
compat: build/compat_test
	$(CXX) $(CXXFLAGS) -shared -o build/rsfx_new.so $(SRC)
	./build/compat_test $(OLD) ./build/rsfx_new.so

clean:
	rm -rf build dist

.PHONY: all test arm docs skin preview compat clean
