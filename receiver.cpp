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
	trySize = bufSz+32768;
	int err = getsockopt(sockFd, SOL_SOCKET, SO_SNDBUF,( char*)&gotSize, &len);
	cout << "socket size before " << gotSize << endl;
	do {
		trySize -= 32768;
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

int main(int argc, char **argv) {
	//local
	int lisSockFd=-1;
	struct sockaddr_in myAddrLis;
	//recv
	struct sockaddr_in recvAddr;
	socklen_t recvAddrLen=0;
	int recvSockPortNum=-1;
	//data recv
	char buf[BUF_SIZE];
	int numBytesRecv=-1;
	char *hostaddrp=nullptr;

	if (argc != 5) {
		cout << 
			"Usage: ./receiver <myIp> <myBindPort> <inaddr ip:port> <bufSz>" << endl;
		exit(1);
	}
	
	// socket to use to send
	if (!buildSocketAndBind(myAddrLis, lisSockFd, argv[1], atoi(argv[2]), atoi(argv[4]))) {
		exit(1);	
	}
	
	//dest
	string inIpPortAddr(argv[3]);
	string recvIpPort;
	recvAddrLen = sizeof(recvAddr);
	errno=0;
	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);
	int flushCounter=0;
	while (run) {
		bzero(buf, BUF_SIZE);
		numBytesRecv = recvfrom(lisSockFd, buf, BUF_SIZE, 0, (struct sockaddr *) &recvAddr, &recvAddrLen);
		if (numBytesRecv < 0) {
			cout << "Unexpected error in recvfrom ret=" << numBytesRecv << " errno=" << errno << endl;
			exit(2);
		}

		// find sender details and drop packet if unsolicited packet		
		hostaddrp = inet_ntoa(recvAddr.sin_addr);
		if (hostaddrp == NULL) {
			cout << "Error on inet_ntoa" << endl;
			exit(2);
		}
		recvIpPort = string(hostaddrp)+":"+to_string(ntohs(recvAddr.sin_port));
		//filter is unsolicited packet
		if (recvIpPort != inIpPortAddr) {
			cout << "Dropping unsolicited packet from " << recvIpPort << "\n";
			continue;	
		}
		
		cout << recvIpPort << " sent " << numBytesRecv << " bytes with content (" << buf << ")" << "\n";
		
		if (++flushCounter == 10000) {
			cout.flush();
			flushCounter=0;
		}
	}
	
	cout.flush();
	return 0;
}