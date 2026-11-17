#ifndef FS_HPP
#define FS_HPP

#include <ctime>
#include <string>
#include <vector>

constexpr int BLOCK_SIZE = 64;
constexpr int NUM_BLOCKS = 256;
constexpr int MAX_FILE_BLOCKS = 8;

enum class NodeType { File, Dir };

// Bits de permissão estilo Unix (RWX para owner/group/other)
enum AccessOp { ACC_READ = 4, ACC_WRITE = 2, ACC_EXEC = 1 };

struct FCB {
    int inode = 0;
    std::string name;
    NodeType type = NodeType::File;
    int size = 0;
    std::time_t created = 0;
    std::time_t modified = 0;
    std::time_t accessed = 0;
    int mode = 0644;
    int owner = 1000;
    int group = 1000;
    int nblocks = 0;
    int blocks[MAX_FILE_BLOCKS]{};
};

struct Node {
    FCB meta;
    Node* parent = nullptr;
    Node* children = nullptr;  // cabeça da lista encadeada de filhos
    Node* next = nullptr;      // próximo irmão
};

class FileSystem {
public:
    FileSystem();
    ~FileSystem();

    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

    bool mkdir(const std::string& name);
    bool cd(const std::string& path);
    void ls(bool long_format = false);
    std::string pwd() const;

    bool touch(const std::string& name);
    bool write(const std::string& name, const std::string& content);
    bool cat(const std::string& name);
    bool cp(const std::string& src, const std::string& dst);
    bool mv(const std::string& src, const std::string& dst);
    bool rm(const std::string& name);
    bool chmod(const std::string& name, int mode);

    int currentUid() const { return uid_; }
    int currentGid() const { return gid_; }

private:
    char disk_[NUM_BLOCKS * BLOCK_SIZE]{};
    bool used_[NUM_BLOCKS]{};
    Node* root_ = nullptr;
    Node* cwd_ = nullptr;
    int next_inode_ = 1;
    int uid_ = 1000;
    int gid_ = 1000;

    int allocBlock();
    void freeBlock(int idx);
    void freeNodeBlocks(Node* node);
    bool writeContentToNode(Node* node, const std::string& content);
    std::string readContentFromNode(Node* node) const;

    Node* createNode(const std::string& name, NodeType type);
    void addChild(Node* parent, Node* child);
    bool removeChild(Node* parent, Node* child);
    Node* findChild(Node* parent, const std::string& name) const;
    Node* resolvePath(const std::string& path) const;
    Node* resolveParentAndName(const std::string& path, std::string& out_name) const;

    bool canAccess(const Node* node, int op) const;
    static std::string modeString(const FCB& fcb);
    std::string nodePath(const Node* node) const;
    void destroyTree(Node* node);

    bool validName(const std::string& name) const;
};

#endif
