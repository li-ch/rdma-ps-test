#ifndef AMBER_COMMON_RDMA_MESSAGE_H_
#define AMBER_COMMON_RDMA_MESSAGE_H_

#include <inttypes.h>
#include <memory>

namespace amber {

#pragma pack(push, 1)
struct RDMA_Message
{
    uint16_t idx:12;
    uint16_t cmd:4;
    uint16_t send_node_id;
    uint16_t recv_node_id;
    bool request;
    int32_t priority;
    int32_t seq;

    int32_t ver;
    int32_t ver_sum;
    uint32_t blockid;
    uint32_t minibatch_size;
    uint32_t data_len;
    bool update;
    char handle_id[32];

    char msg[0];

    template<typename Deleter>
    static std::shared_ptr<RDMA_Message> GetPtr(char* buf, Deleter del)
    {
        std::shared_ptr<RDMA_Message> msg;
        msg.reset((RDMA_Message*)buf, del);
        return msg;
    }

};
#pragma pack(pop)

}   // namespace amber
#endif  // AMBER_COMMON_MESSAGE_H_
