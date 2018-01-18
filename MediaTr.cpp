#include "MediaTr.h"
#include <iostream>
#include <vector>
#include <stdio.h>  
#include <stdlib.h>  
#include <errno.h>  
#include <string.h>  
#include <sys/types.h>  
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <sys/wait.h>  
#include <unistd.h>  
#include <arpa/inet.h>  
#include <fcntl.h>  
#include <sys/epoll.h>  
#include <sys/time.h>  
#include <sys/resource.h>  
#include <pthread.h>  
#include <assert.h>  

#define SO_REUSEPORT    15  

#define MAXBUF 10240  
using namespace std;
MxChan::MxChan()
{
	chanId=-1;
	listener=-1;
	sender=-1;
	recvPort=-1;
	sendPort=-1;
	epfd=-1;

	bzero(&addr, sizeof(addr));  
	bzero(&sd_addr, sizeof(sd_addr));  
	cout<<"construct Mxchan"<<endl;
}
MxChan::~MxChan()
{
	//deinit();
	cout<<"destruct Mxchan"<<endl;
}
void MxChan::deinit()
{
	if(prvd != NULL)
	{
		prvd->delSub(this);
	}
	for(auto i:subs)
	{
		i->setPrvd(0);
	}
	epoll_ctl(epfd, EPOLL_CTL_DEL, listener, NULL);
	close(listener);
	close(sender);
}
void MxChan::initSend()
{
	sender=socket(AF_INET, SOCK_DGRAM, 0);
	sd_addr.sin_family = AF_INET;
	if(sendPort==-1)
	{
		cout<<"send port is not set"<<endl;
	}else
	{
		sd_addr.sin_port = htons(sendPort);
	}
	if(sendIp.empty())
	{
		cout<<"send ip is not set"<<endl;
	}else
	{
		sd_addr.sin_addr.s_addr = inet_addr(sendIp.c_str());
	}
}
void MxChan::registerEpoll(int fd)
{
	epfd=fd;	
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev) < 0) {  
		printf("ep add recv fail\n");  
	} else {  
		printf("ep add recv OK\n");  
	} 
}
void MxChan::setSendAddr(string ip,int port)
{
	sendPort=port;
	sendIp=ip;
	sd_addr.sin_port = htons(sendPort);
	sd_addr.sin_addr.s_addr = inet_addr(sendIp.c_str());
}
void MxChan::setRecvAddr(int port)
{
	recvPort=port;
	addr.sin_port = htons(recvPort);  
}
void MxChan::initRecv()
{
	if ((listener = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {  
		perror("socket");  
		exit(1);  
	} else {  
		printf("socket OK\n");  
	}
	int opt=1;
	int ret = setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));  
	if (ret) {  
		cout<<"setsockopt addr error";
		return;
	}  

	ret = setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));  
	if (ret) {  
		cout<<"setsockopt port error";
		return;
	}
	addr.sin_family = PF_INET;  
	if(recvPort==-1)
	{
		cout<<"recv port is not set"<<endl;
	}else
	{
		addr.sin_port = htons(recvPort);  
	}
	addr.sin_addr.s_addr = INADDR_ANY; 
	//ev.events = EPOLLIN|EPOLLET;  
	ev.events = EPOLLIN;  
	ev.data.fd = listener;  
}
void MxChan::bindRecv()
{
	if (bind(listener, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1) {  
		printf("bind listener %d\n",chanId);  
	} else {  
		printf("chanid %d, IP bind %d OK\n",chanId,recvPort);  
	}
}
int MxChan::getSendfd()
{
	return sender;
}
int MxChan::getRecvfd()
{
	return listener;
}
void MxChan::write(unsigned char* buf, int len)
{
	int sendSize = sendto(sender,buf,len, 0, (struct sockaddr *) &sd_addr,sizeof(sd_addr));
	//if(sendSize < 1)
	{
		//cout<<"chid"<<chanId<<"sendSize "<<sendSize<<endl;
	}
}
void MxChan::read()
{
	unsigned char recvbuf[MAXBUF + 1];  
	int  ret=1;  
	struct sockaddr_in client_addr;  
	socklen_t cli_len=sizeof(client_addr);  

	bzero(recvbuf, MAXBUF + 1);  
	ret = recvfrom(listener, recvbuf, MAXBUF, 0, (struct sockaddr *)&client_addr, &cli_len);  
	if (ret > 0) {  
		unsigned int seq =recvbuf[2]<<8;
		seq+=recvbuf[3];
		if(seq%1000 == 1)
			cout<<"seq"<<seq<<endl;
		//TODO just test.
		//write(&recvbuf[0],ret);
		writeToSubs(&recvbuf[0],ret);
	} else {  
		printf("read err:%s  %d\n", strerror(errno), ret);  
	}
}
void MxChan::init(int id)
{
	chanId =id;
	initRecv();
	initSend();
}
void MxChan::setPrvd(MxChan* mxc)
{
	prvd = mxc;
}
void MxChan::addSub(MxChan* mxc)
{
	subs.insert(mxc);
}
void MxChan::delSub(MxChan* mxc)
{
	subs.erase(mxc);
}
int MxChan::subCnt()
{
	return subs.size();
}
void MxChan::writeToSubs(unsigned char* buf, int len)
{
	//may be iter failure
	int c=0;
	for(auto i:subs)
	{
		//cout<<"count"<<c<<" ";
		i->write(buf,len);
		c++;
	}
	//cout<<endl;
}

MxChan* MxChan::getPrvd()
{
	return prvd;
}
/////////////////////////////
MxServ::MxServ()
{
	fd_cnt=0;
	epfd =-1;
	chanId=-1;
	epfd = epoll_create(MAXEPOLLSIZE);  //et: 100 //TODO tmp here
}
MxServ::~MxServ()
{
	epfd=-1;
}
void MxServ::run()
{
	while (1) {  
		cout<<"while begin"<<epfd<<endl;
		fd_cnt= epoll_wait(epfd, events, 10000, -1);  
		cout<<"while end"<<epfd<<endl;
		if (fd_cnt == -1) {  
			printf("epoll_wait error\n");  
			break;  
		} 
		cout<<"fd_cnt"<<fd_cnt<<endl;
		for (int i = 0; i < fd_cnt; i++) {  
			int tfd =events[i].data.fd;
			MxChan* tm=r_map[tfd];
			tm->read();
		} 
	}
}
int MxServ::openChan(string sip,int sport, int rport)
{
	chanId++;
	MxChan* mxc=new MxChan();
	mxc->setSendAddr(sip,sport);
	mxc->setRecvAddr(rport);
	mxc->init(chanId);
	mxc->bindRecv();
	mxc->registerEpoll(epfd);
	int fd= mxc->getRecvfd();
	r_map[fd]=mxc;
	id_map[chanId]=mxc;
	return chanId;
}
void MxServ::closeChan(int id)
{
	MxChan* mxc =id_map[id];
	if(mxc != 0)
	{
		mxc->deinit();
		delete mxc;
		mxc =0;
		id_map[id]=0;
	}
}
void MxServ::addSub(int prvd, int sub)
{
	cout<<"prvd"<<prvd<<"sub"<<sub<<endl;
	MxChan* p=id_map[prvd];
	MxChan* s=id_map[sub];
	if(p !=0 && s!=0)
	{
		p->addSub(s);
		s->setPrvd(p);
	}
}
void MxServ::rmSub(int prvd, int sub)
{
	MxChan* p=id_map[prvd];
	MxChan* s=id_map[sub];
	if(p !=0 && s!=0)
	{
		p->delSub(s);
		s->setPrvd(0);
	}
}
void MxServ::chgPrvd(int prvd, int sub)
{
	MxChan* p=id_map[prvd];
	MxChan* s=id_map[sub];
	if(p !=0 && s!=0)
	{
		MxChan* old_p =s->getPrvd();
		old_p->delSub(s);
		p->addSub(s);
		s->setPrvd(p);
	}
}

//reference to http://blog.csdn.net/dog250/article/details/50557570
int main(int argc, char **argv)  
{
	MxServ mxs;
	int prid =mxs.openChan("192.168.1.206",8000,9000);//sip sp rp
	int scnt=10;	//1 transmit 10
	vector<int> sid(scnt);
	for(int i=0;i<scnt;i++)
	{
		sid[i]=mxs.openChan("192.168.1.206",8001+i,8999);//no need to recv.
		mxs.addSub(prid,sid[i]);
	}
	mxs.run();
	return 1;
}

