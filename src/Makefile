default:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O3 -g -ffast-math -flto -std=c++14 -DNDEBUG'  \
		LDFLAGS='$(LDFLAGS) -flto -g' \
		leelaz

debug:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -Og -g -std=c++14' \
		LDFLAGS='$(LDFLAGS) -g' \
		leelaz

clang:
	$(MAKE) CC=clang-5.0 CXX=clang++-5.0 \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -Wno-missing-braces -O3 -ffast-math -flto -std=c++14 -DNDEBUG' \
		LDFLAGS='$(LDFLAGS) -flto -fuse-linker-plugin' \
		leelaz

DYNAMIC_LIBS = -lpthread -lz
LIBS = /cm/shared/apps/boost/gcc/1.61.0/lib/libboost_program_options.a
LIBS += /cm/shared/apps/cuda80/lib64/libOpenCL.so
LIBS += /cm/shared/apps/openblas/gcc/0.2.14/lib/libopenblas.so
USE_OPENBLAS = 0

# for Linux with OpenBLAS
CXXFLAGS += -I/cm/shared/apps/cuda80/include/CL -I/cm/shared/apps/boost/gcc/1.61.0/include -I/cm/shared/apps/openblas/gcc/0.2.14/include
# DYNAMIC_LIBS += -lopenblas
# DYNAMIC_LIBS += -lOpenCL

# for macOS (comment out the Linux part)
# LIBS += -framework Accelerate
# LIBS += -framework OpenCL
# CXXFLAGS += -I/System/Library/Frameworks/Accelerate.framework/Versions/Current/Headers

# for MKL instead of OpenBLAS
#DYNAMIC_LIBS += -lmkl_rt
#CXXFLAGS += -I/opt/intel/mkl/include
#LDFLAGS  += -L/opt/intel/mkl/lib/intel64/

CXXFLAGS += -I.
CPPFLAGS += -MD -MP

sources = Network.cpp FullBoard.cpp KoState.cpp Training.cpp \
	  TimeControl.cpp UCTSearch.cpp GameState.cpp Leela.cpp \
	  SGFParser.cpp Timing.cpp Utils.cpp FastBoard.cpp \
	  SGFTree.cpp Zobrist.cpp FastState.cpp GTP.cpp Random.cpp \
	  SMP.cpp UCTNode.cpp OpenCL.cpp TTable.cpp

objects = $(sources:.cpp=.o)
deps = $(sources:%.cpp=%.d)

-include $(deps)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

leelaz: $(objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS) $(DYNAMIC_LIBS)

clean:
	-$(RM) leelaz $(objects) $(deps)

.PHONY: clean default debug clang
