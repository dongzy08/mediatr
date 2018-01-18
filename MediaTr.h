#ifndef _MEDIATR_H_
#define _MEDIATR_H_
#include <string>
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <sys/wait.h>  
#include <unistd.h>  
#include <arpa/inet.h>  
#include <fcntl.h>  
#include <sys/epoll.h>  
#include <set>  
#include <unordered_map>
#define MAXEPOLLSIZE 100
using namespace std;
class MxChan;
class MxServ
{
	public:
		MxServ();
		~MxServ();
		void run();
		int openChan(string sip,int sport, int rport);
		void closeChan(int id);
		void addSub(int prvd, int sub);
		void rmSub(int prvd, int sub);
		void chgPrvd(int prvd, int sub);
	private:
		int chanId;
		int fd_cnt;
		int epfd;  //et: 100
		struct epoll_event events[MAXEPOLLSIZE];  
		unordered_map<int,MxChan*> r_map;	//recvfd,*MxChan, for epoll wait
		unordered_map<int,MxChan*> id_map;	//chanId,*MxChan, for search mxchan
};
class MxChan
{
	public:
		MxChan();
		~MxChan();
		void init(int id);
		void setSendAddr(string ip,int port);
		void setRecvAddr(int port);
		void bindRecv();
		void registerEpoll(int fd);
		int getRecvfd();
		int getSendfd();
		void read();
		void write(unsigned char* buf, int len);//local sender

		void addSub(MxChan* mxc);
		void delSub(MxChan* mxc);
		void setPrvd(MxChan* mxc);
		int subCnt();
		void deinit();
		MxChan* getPrvd();
	private:

		void initSend();
		void initRecv();
		void writeToSubs(unsigned char* buf, int len);
		int chanId;
		int epfd;
		struct epoll_event ev;  

		int listener;
		int sender;
		struct sockaddr_in addr;
		struct sockaddr_in sd_addr;
		int recvPort;
		int sendPort;
		string sendIp;

		set<MxChan*> subs;
		MxChan* prvd;
};
#endif //_MEDIATR_H_


