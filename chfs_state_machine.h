#include "raft_state_machine.h"
#include "rpc.h"
#include "extent_server.h"
#include <assert.h>

class chfs_command_raft : public raft_command {
public:
    enum command_type {
        CMD_NONE, 
        CMD_CRT,  
        CMD_PUT,  
        CMD_GET,  
        CMD_GETA, 
        CMD_RMV,   
    };

    struct result {
        std::chrono::system_clock::time_point start;
        extent_protocol::extentid_t id;
        std::string buf;
        extent_protocol::attr attr;
        command_type tp;

        bool done;
        std::mutex mtx;             
        std::condition_variable cv; 

        void set_attr(uint32_t type, uint32_t size, time_t ct, time_t at, time_t mt){
            attr.mtime = mt;
            attr.ctime = ct;
            attr.atime = at;
            attr.type = type;
            attr.size = size;
        }
    };
    // Lab3: your code here
    command_type cmd_tp;
    uint32_t type;
    extent_protocol::extentid_t id;
    std::string buf;
    std::shared_ptr<result> res;

    chfs_command_raft();

    chfs_command_raft(const chfs_command_raft &cmd);

    virtual ~chfs_command_raft();

    virtual int size() const override;

    virtual void serialize(char *buf, int size) const override;

    virtual void deserialize(const char *buf, int size);
};

marshall &operator<<(marshall &m, const chfs_command_raft &cmd);

unmarshall &operator>>(unmarshall &u, chfs_command_raft &cmd);

class chfs_state_machine : public raft_state_machine {
public:
    virtual ~chfs_state_machine() {
    }
    virtual void apply_log(raft_command &cmd) override;
    virtual std::vector<char> snapshot() {
        return std::vector<char>();
    }
    virtual void apply_snapshot(const std::vector<char> &) {
    }

private:
    extent_server es;
    std::mutex mtx;
    // Lab3: Your code here
    // You can add your own variables and functions here if you want.

};
