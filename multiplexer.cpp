#include <iostream>
#include <string>
#include <vector>
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
#include <fcntl.h>
#include <poll.h>
#include <algorithm>
#include <deque>

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
	if (fcntl(sockFd, F_SETFL, O_NONBLOCK) < 0) {
		cout << "Error setting socket to non blocking " << errno << endl;
		return false;	
	}
	
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
	// listen
	int listenSockFd=-1;
	int listenPortNum=-1;
	struct sockaddr_in myAddrLis;
	//recv
	struct sockaddr_in recvAddr;
	socklen_t recvAddrLen=0;
	char *hostaddrp=nullptr;
	string recvIpPort;
	//send
	int sendSrcPortNum=-1;
	struct sockaddr_in myAddrSend;
	int sendSockFd=-1;
	struct sockaddr_in sendAddr;
	socklen_t sendAddrLen=0;
	int sendSockPortNum=-1;
	//data recv
	char buf[BUF_SIZE];
	int numBytesRecv=-1;
	//data send
	int numBytesSent=-1;

	if (argc < 9) {
		cout << 
			"Usage: ./multiplexer <myIp> <myListenPort> <mySendPort> <num incoming> <inaddr1 ip:port> [inaddr2]...<outaddr> <bufSz>" << endl;
		exit(1);
	}
	
	int numIn = atoi(argv[4]);
	if (argc != numIn+7) {
		cout <<
			"Usage: ./multiplexer <myIp> <myListenPort> <mySendPort> <num-incoming> <inaddr1 ip:port> [inaddr2]...<outaddr> <bufSz>" << endl;
		exit(1);	
	}
	
	vector<string> senders(numIn);
	for (int i=0; i < numIn; ++i) {
		senders.emplace_back(argv[5+i]);
	}
	
	//for server recv
	string myIp(argv[1]);
	listenPortNum = atoi(argv[2]);
	if (!buildSocketAndBind(myAddrLis, listenSockFd, myIp, listenPortNum, atoi(argv[argc-1]))) {
		exit(1);	
	}
	
	// for server send
	sendSrcPortNum = atoi(argv[3]);
	if (!buildSocketAndBind(myAddrSend, sendSockFd, myIp, sendSrcPortNum, atoi(argv[argc-1]))) {
		exit(1);	
	}
	//setSocketBufferSize(sendSockFd);
	
	string outAddr(argv[argc-2]);
	size_t pos = outAddr.find(":");
	string outIp = outAddr.substr(0, pos);
	buildSendToAddress(sendAddr, outIp, atoi(outAddr.substr(pos+1).c_str()));	
	
	recvAddrLen = sizeof(recvAddr);
	sendAddrLen = sizeof(sendAddr);
	errno=0;
	
	uint64_t flushCounter=0;
	uint64_t seqNum=0;
	string out;
	out.reserve(BUF_SIZE);
	//for poll
	struct pollfd socks[2];
	socks[0].fd = listenSockFd;
	socks[1].fd = sendSockFd;
	socks[0].events = POLLIN;
	socks[1].events = POLLOUT;
	int ret = -1;
	deque<string> dq;
	int err=0;
	
	/*	
	sigset_t mask, orig_mask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGTERM);
	sigaddset (&mask, SIGINT);
	if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
		cout << "Error sigprocmask" << endl;
		exit(2);
	}
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigterm;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		cout << "Error sigaction" << endl;
		exit(2);
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		cout << "Error sigaction" << endl;
		exit(2);
	}
	const struct timespec timeout = { tv_sec = 1, tv_nsec = 0 };
	*/
	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);
	while (run) {
		//ret = ppoll(socks, (unsigned long)2, &timeout, &orig_mask);
		ret = poll(socks, (unsigned long)2, -1);
		if (ret <= 0) {
			cout << "Unexpected error polling " << errno << endl;
			exit(2);
		}
		//should not happen
		//if (ret == 0) continue;
		
		if(((socks[0].revents&POLLHUP) == POLLHUP) ||
			((socks[0].revents&POLLERR) == POLLERR) ||
			((socks[0].revents&POLLNVAL) == POLLNVAL) ||
			((socks[1].revents&POLLHUP) == POLLHUP) ||
			((socks[1].revents&POLLERR) == POLLERR) ||
			((socks[1].revents&POLLNVAL) == POLLNVAL)) {
				cout << "Errors on socket Fds polling..closing" << endl;
				exit(2);
		} 
		
		if((socks[0].revents&POLLIN) == POLLIN) {
			bzero(buf, BUF_SIZE);
			numBytesRecv = recvfrom(listenSockFd, buf, BUF_SIZE, 0, (struct sockaddr *) &recvAddr, &recvAddrLen);
			err = errno;
			if (numBytesRecv < 0 && (err != EAGAIN) && (err != EWOULDBLOCK)) {
				cout << "Unexpected error in recvfrom ret=" << numBytesRecv << " errno=" << errno << endl;
				exit(2);
			}
			else if (numBytesRecv > 0) {
				// find sender details and drop packet if unsolicited packet		
				hostaddrp = inet_ntoa(recvAddr.sin_addr);
				if (hostaddrp == NULL) {
					cout << "Error on inet_ntoa" << endl;
					exit(2);
				}
				recvIpPort = string(hostaddrp)+":"+to_string(ntohs(recvAddr.sin_port));
				if (find(senders.begin(), senders.end(), recvIpPort) == senders.end()) {
					cout << "Dropping unsolicited packet from "	<< recvIpPort << "\n";
					continue;
				}
				dq.emplace_front(buf);
				cout << recvIpPort << " sent " << numBytesRecv << " bytes with content (" << buf << ")\n";
			}
		}
		
		if((socks[1].revents&POLLOUT) == POLLOUT && !dq.empty()) {
			//build new packet and send to our receiver
			out = to_string(++seqNum) + "-" + dq.back();
			bzero(buf, BUF_SIZE);
			bcopy(const_cast<char*>(out.c_str()), buf, out.size());
			numBytesSent = sendto(sendSockFd, buf, strlen(buf), 0, (struct sockaddr *) &sendAddr, sendAddrLen);
			if (numBytesSent < 0 && (err != EAGAIN) && (err != EWOULDBLOCK)) { 
				cout << "Unexpected error in sendto ret=" << numBytesSent << " errno=" << errno << endl;
				exit(2);
			}
			else if (numBytesSent > 0) {
				cout << "Sent numBytes=" << numBytesSent << " msg=(" << buf << ") to " << outAddr << "\n";
				dq.pop_back();
			}
			else {
				--seqNum;
			}
		}
		
		if (++flushCounter == 10000) {
			cout.flush();
			flushCounter=0;
		}
	}
	cout.flush();
	return 0;
}