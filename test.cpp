#include "rdma.h"
#include <cstring>
#include <iostream>
#include <sys/time.h>
#include <unistd.h> 
#include "buf_list.h"
#include <chrono>
using namespace std;
using namespace amber;
using namespace amber::rdma;

const int max_packet_size = (1 << 20);
const int SND_NUM = 1e5;
const int CLI_NUM = 2;
const int SVR_NUM = 2;
const int QP_NUM = 2;
const int SERVER_SLEEP = 600;


const char* svr_ip[] = {"172.16.18.197","172.16.18.198"};//{"100.88.65.7" ,"100.88.65.8"};
const char* svr_port[] = {"51234", "53214"};
const char* cli_ip[] = {"172.16.18.205","172.16.18.207"};//{"100.88.65.9" ,"100.88.65.10"}; 

int sz;


void Client()
{
    RDMA_Client cli(SVR_NUM * QP_NUM);
    BufferList* pool[SVR_NUM * QP_NUM];
    for (int i = 0; i < QP_NUM; ++i)
    {
        for (int j = 0; j < SVR_NUM; ++j)
	{
	    int idx;
	    pool[idx = i * SVR_NUM + j] = new BufferList(sizeof(RDMA_Message) + max_packet_size, 200);
            cli.Connect(svr_ip[j], svr_port[i], (void *)pool[idx]->Head(), pool[idx]->Capcity());
	    puts("Client Connect Success");
	    printf("pool[idx = %d]\n", idx);
	}
    }

    sleep(3);

    puts("Client Start Sending");
    //auto t1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < SVR_NUM * SND_NUM * QP_NUM; ++i)
    {

	int idx = i % (QP_NUM * SVR_NUM);
        shared_ptr<RDMA_Message> f;
        auto buf = pool[idx]->GetFreeBuf();
	if (buf == nullptr)
        {
            --i;
            continue;
        }
	//auto t2 = std::chrono::high_resolution_clock::now();
 	//auto diff = std::chrono::duration_cast<std::chrono::microseconds>(t2-t1);
	//if (diff.count()>0) printf("Mem aquired in %ld us\n",diff.count());
	//t1 = t2;
	//puts("Acquired Mem");
        f.reset((RDMA_Message*)buf, [&pool, idx](RDMA_Message* msg){pool[idx]->FreeBuf((char*)msg);});
        f.get()->idx = i % (QP_NUM * SVR_NUM);
        f.get()->data_len = max_packet_size;
        sprintf(f.get()->msg, "Test rdma message seq[%d]", i);
	//puts("Client Message");
	if ((i + 1) % (SND_NUM / 5) == 0)
        {
            printf("Send Msg %d %d\n", f->idx, i);
        }
        cli.Send(f, idx);
	//puts("Msg Sent");
    }

    puts("Client done");
    sleep(300);
}

int num;
void Handle(shared_ptr<RDMA_Message>& msg)
{
    if (++num % (SND_NUM / 10) == 0)
    {
        printf("Recv Msg idx[%d] %d, %d, (%s), addr: %p\n", msg.get()->idx, msg.get()->data_len, sz++, msg.get()->msg, msg.get());
    }
}

void Server()
{
    RDMA_Server svr(CLI_NUM, QP_NUM, bind(Handle, placeholders::_1));

    for (int i = 0; i < QP_NUM; ++i) {
        svr.Listen(svr_port[i], sizeof(RDMA_Message) + max_packet_size);
	puts("Server Listen Success");
    }

    for (int i = 0; i < CLI_NUM; ++i)
    {
        for (int j = 0; j < QP_NUM; ++j)
        {
            svr.Accept(j);
	    puts("Server Accept Success");
        }
    }
    puts("Sleeps");
    sleep(SERVER_SLEEP);
}

int main(int argc, char* argv[])
{
    if (argc == 1)
    {   
        puts("Client");
        Client();
    }   
    else
    {   
        puts("Server");
        Server();
    }   
    puts("End");
    return 0;
}
