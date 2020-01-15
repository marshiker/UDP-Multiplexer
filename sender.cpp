#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "swargam.pb.h"
using namespace std;

static const int BUF_SIZE=1500;

static bool run=true;
static void sigterm (int sig) {
	run = false;
}

void setSocketBufferSize(int sockFd, int bufSz) {
	int trySize=0, gotSize=0;
	socklen_t len = sizeof(socklen_t);
	trySize = bufSz+1024;
	int err = getsockopt(sockFd, SOL_SOCKET, SO_SNDBUF,( char*)&gotSize, &len);
	cout << "socket size before " << gotSize << endl;
	do {
		trySize -= 1024;
		setsockopt(sockFd, SOL_SOCKET, SO_SNDBUF, (char*)&trySize, len);
		err = getsockopt(sockFd, SOL_SOCKET, SO_SNDBUF, (char*)&gotSize, &len);
		if (err < 0) { perror("getsockopt"); break; }
	} while (gotSize < trySize);
	cout << "socket size after " << gotSize << endl;
}

bool buildSocketAndBind(sockaddr_in& sockAddr, int& sockFd, const string& ip, int port, int bufSz) {
	sockFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd < 0) {
		cout << "Error creating socket" << endl;
		return false;
	}
	
	int optval = 1;
	setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
	setSocketBufferSize(sockFd, bufSz);
	bzero((char *) &sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(ip.c_str());
	sockAddr.sin_port = htons((unsigned short)port);
	
	if (bind(sockFd, (struct sockaddr *) &sockAddr, sizeof(sockAddr)) < 0) {
		cout << "Error binding socket to port " << port << endl;
		return false;
	}
	
	return true;		
}

void buildSendToAddress(sockaddr_in& sockAddr, const string& ip, int port) {
	bzero((char *) &sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(ip.c_str());
	sockAddr.sin_port = htons((unsigned short)port);	
}

int main(int argc, char **argv) {
	//src
	int sendSockFd=-1;
	struct sockaddr_in myAddrSend;
	//dest
	struct sockaddr_in sendAddr;
	socklen_t sendAddrLen=0;
	int sendSockPortNum=-1;
	//data send
	char buf[BUF_SIZE];
	int numBytesSent=-1;

	if (argc != 7) {
		cout << 
			"Usage: ./sender <myName> <delayIntervalMicros> <myIp> <myBindPort> <outaddr ip:port> <sendBufSize>" << endl;
		exit(1);
	}
	
	// socket to use to send
	if (!buildSocketAndBind(myAddrSend, sendSockFd, argv[3], atoi(argv[4]), atoi(argv[6]))) {
		exit(1);	
	}
	
	//dest
	string outAddr(argv[argc-2]);
	size_t pos = outAddr.find(":");
	string outIp = outAddr.substr(0, pos);
	buildSendToAddress(sendAddr, outIp, atoi(outAddr.substr(pos+1).c_str()));
		
	sendAddrLen = sizeof(sendAddr);
	int delay=atoi(argv[2]);
	errno=0;
	string msg;
	msg.reserve(BUF_SIZE);
	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);
	uint64_t flushCounter=0;
	uint64_t seqNum=0;
	while (run) {
		bzero(buf, BUF_SIZE);
		msg = string(argv[1]) + "-" + to_string(++seqNum);
		bcopy(const_cast<char*>(msg.c_str()), buf, msg.size());
		
		//build new packet and send to our receiver
		numBytesSent = sendto(sendSockFd, buf, strlen(buf), 0, (struct sockaddr *) &sendAddr, sendAddrLen);
		if (numBytesSent < 0) { 
			cout << "Unexpected error in sendto ret=" << numBytesSent << " errno=" << errno << endl;
			exit(2);
		}
		
		cout << "Sent numBytes=" << numBytesSent << " msg=(" << buf << ") to " << outAddr << "\n";
		if (++flushCounter == 10000) {
			cout.flush();
			flushCounter=0;
		}
		
		if (delay > 0) usleep(delay);
	}
	
	cout.flush();
	return 0;
}