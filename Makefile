CXX=g++
RM=rm -f
PROGS=multiplexer sender receiver

BIN_DIR=/usr/local/bin
LIBS_DIR=/usr/local/lib
M_SRCS=multiplexer.cpp swargam.pb.cc
S_SRCS=sender.cpp swargam.pb.cc
R_SRCS=receiver.cpp swargam.pb.cc
LIBS=-L$(LIBS_DIR) -lprotobuf
INCLUDES=/usr/local/include

all: protos multiplexer sender receiver

protos:
	$(BIN_DIR)/protoc --cpp_out=. -I$(INCLUDES) --proto_path=. swargam.proto

multiplexer: multiplexer.cpp 
	$(BIN_DIR)/protoc --cpp_out=. -I$(INCLUDES) --proto_path=. swargam.proto
	$(CXX) -I$(INCLUDES) $^ swargam.pb.cc -o $@ $(LIBS)

sender: sender.cpp
	$(BIN_DIR)/protoc --cpp_out=. -I$(INCLUDES) --proto_path=. swargam.proto
	$(CXX) -I$(INCLUDES) $^ swargam.pb.cc -o $@ $(LIBS)

receiver: receiver.cpp
	$(BIN_DIR)/protoc --cpp_out=. -I$(INCLUDES) --proto_path=. swargam.proto
	$(CXX) -I$(INCLUDES) $^ swargam.pb.cc -o $@ $(LIBS)

clean:
	$(RM) $(PROGS)
