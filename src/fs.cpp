#include "fs.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

FileSystem::FileSystem() {
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        used_[i] = false;
    }
    std::memset(disk_, 0, sizeof(disk_));

    root_ = new Node();
    root_->meta.inode = next_inode_++;
    root_->meta.name = "/";
    root_->meta.type = NodeType::Dir;
    root_->meta.mode = 0755;
    root_->meta.owner = uid_;
    root_->meta.group = gid_;
    std::time_t now = std::time(nullptr);
    root_->meta.created = root_->meta.modified = root_->meta.accessed = now;
    root_->parent = root_;  // /.. == /
    cwd_ = root_;
}

FileSystem::~FileSystem() {
    destroyTree(root_);
    root_ = nullptr;
    cwd_ = nullptr;
}

void FileSystem::destroyTree(Node* node) {
    if (!node) {
        return;
    }
    Node* child = node->children;
    while (child) {
        Node* next = child->next;
        destroyTree(child);
        child = next;
    }
    freeNodeBlocks(node);
    delete node;
}

bool FileSystem::validName(const std::string& name) const {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    if (name.find('/') != std::string::npos) {
        return false;
    }
    return true;
}

int FileSystem::allocBlock() {
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        if (!used_[i]) {
            used_[i] = true;
            std::memset(disk_ + i * BLOCK_SIZE, 0, BLOCK_SIZE);
            return i;
        }
    }
    return -1;
}

void FileSystem::freeBlock(int idx) {
    if (idx >= 0 && idx < NUM_BLOCKS) {
        used_[idx] = false;
        std::memset(disk_ + idx * BLOCK_SIZE, 0, BLOCK_SIZE);
    }
}

void FileSystem::freeNodeBlocks(Node* node) {
    if (!node || node->meta.type != NodeType::File) {
        return;
    }
    for (int i = 0; i < node->meta.nblocks; ++i) {
        freeBlock(node->meta.blocks[i]);
        node->meta.blocks[i] = 0;
    }
    node->meta.nblocks = 0;
    node->meta.size = 0;
}

Node* FileSystem::createNode(const std::string& name, NodeType type) {
    Node* node = new Node();
    node->meta.inode = next_inode_++;
    node->meta.name = name;
    node->meta.type = type;
    node->meta.mode = (type == NodeType::Dir) ? 0755 : 0644;
    node->meta.owner = uid_;
    node->meta.group = gid_;
    std::time_t now = std::time(nullptr);
    node->meta.created = node->meta.modified = node->meta.accessed = now;
    return node;
}

void FileSystem::addChild(Node* parent, Node* child) {
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
    parent->meta.modified = std::time(nullptr);
}

bool FileSystem::removeChild(Node* parent, Node* child) {
    if (!parent || !child) {
        return false;
    }
    Node* prev = nullptr;
    Node* cur = parent->children;
    while (cur) {
        if (cur == child) {
            if (prev) {
                prev->next = cur->next;
            } else {
                parent->children = cur->next;
            }
            cur->next = nullptr;
            parent->meta.modified = std::time(nullptr);
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

Node* FileSystem::findChild(Node* parent, const std::string& name) const {
    if (!parent) {
        return nullptr;
    }
    for (Node* c = parent->children; c; c = c->next) {
        if (c->meta.name == name) {
            return c;
        }
    }
    return nullptr;
}

Node* FileSystem::resolvePath(const std::string& path) const {
    if (path.empty()) {
        return nullptr;
    }
    if (path == "/") {
        return root_;
    }

    Node* cur = (path[0] == '/') ? root_ : cwd_;
    std::string token;
    std::stringstream ss(path);
    // Se path absoluto, o primeiro token após split por '/' é vazio
    while (std::getline(ss, token, '/')) {
        if (token.empty() || token == ".") {
            continue;
        }
        if (token == "..") {
            if (cur != root_) {
                cur = cur->parent;
            }
            continue;
        }
        if (cur->meta.type != NodeType::Dir) {
            return nullptr;
        }
        cur = findChild(cur, token);
        if (!cur) {
            return nullptr;
        }
    }
    return cur;
}

Node* FileSystem::resolveParentAndName(const std::string& path, std::string& out_name) const {
    if (path.empty() || path == "/") {
        return nullptr;
    }

    std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        out_name = path;
        return cwd_;
    }

    out_name = path.substr(slash + 1);
    if (out_name.empty()) {
        return nullptr;
    }

    std::string parent_path = path.substr(0, slash);
    if (parent_path.empty()) {
        return root_;
    }
    return resolvePath(parent_path);
}

bool FileSystem::canAccess(const Node* node, int op) const {
    if (!node) {
        return false;
    }
    int shift = 0;
    if (uid_ == node->meta.owner) {
        shift = 6;
    } else if (gid_ == node->meta.group) {
        shift = 3;
    } else {
        shift = 0;
    }
    int bits = (node->meta.mode >> shift) & 7;
    return (bits & op) == op;
}

std::string FileSystem::modeString(const FCB& fcb) {
    std::string s;
    s += (fcb.type == NodeType::Dir) ? 'd' : '-';
    int mode = fcb.mode;
    const char* rwx = "rwx";
    for (int shift = 6; shift >= 0; shift -= 3) {
        int bits = (mode >> shift) & 7;
        for (int i = 0; i < 3; ++i) {
            s += (bits & (4 >> i)) ? rwx[i] : '-';
        }
    }
    return s;
}

std::string FileSystem::nodePath(const Node* node) const {
    if (!node || node == root_) {
        return "/";
    }
    std::vector<std::string> parts;
    const Node* cur = node;
    while (cur && cur != root_) {
        parts.push_back(cur->meta.name);
        cur = cur->parent;
    }
    std::string path;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        path += "/";
        path += *it;
    }
    return path.empty() ? "/" : path;
}

bool FileSystem::writeContentToNode(Node* node, const std::string& content) {
    freeNodeBlocks(node);

    if (content.empty()) {
        node->meta.size = 0;
        node->meta.modified = std::time(nullptr);
        return true;
    }

    int needed = static_cast<int>((content.size() + BLOCK_SIZE - 1) / BLOCK_SIZE);
    if (needed > MAX_FILE_BLOCKS) {
        std::cerr << "erro: conteúdo maior que o limite do arquivo ("
                  << MAX_FILE_BLOCKS * BLOCK_SIZE << " bytes)\n";
        return false;
    }

    for (int i = 0; i < needed; ++i) {
        int b = allocBlock();
        if (b < 0) {
            std::cerr << "erro: disco cheio\n";
            freeNodeBlocks(node);
            return false;
        }
        node->meta.blocks[i] = b;
        node->meta.nblocks++;

        std::size_t offset = static_cast<std::size_t>(i) * BLOCK_SIZE;
        std::size_t chunk = std::min(static_cast<std::size_t>(BLOCK_SIZE), content.size() - offset);
        std::memcpy(disk_ + b * BLOCK_SIZE, content.data() + offset, chunk);
    }

    node->meta.size = static_cast<int>(content.size());
    node->meta.modified = std::time(nullptr);
    return true;
}

std::string FileSystem::readContentFromNode(Node* node) const {
    std::string out;
    out.reserve(static_cast<std::size_t>(node->meta.size));
    int remaining = node->meta.size;
    for (int i = 0; i < node->meta.nblocks && remaining > 0; ++i) {
        int b = node->meta.blocks[i];
        int chunk = std::min(BLOCK_SIZE, remaining);
        out.append(disk_ + b * BLOCK_SIZE, chunk);
        remaining -= chunk;
    }
    return out;
}

// ---- operações públicas ----

bool FileSystem::mkdir(const std::string& name) {
    std::string leaf;
    Node* parent = resolveParentAndName(name, leaf);
    if (!parent) {
        std::cerr << "erro: caminho inválido\n";
        return false;
    }
    if (!validName(leaf)) {
        std::cerr << "erro: nome inválido\n";
        return false;
    }
    if (parent->meta.type != NodeType::Dir) {
        std::cerr << "erro: destino não é diretório\n";
        return false;
    }
    if (!canAccess(parent, ACC_WRITE | ACC_EXEC)) {
        std::cerr << "erro: permissão negada\n";
        return false;
    }
    if (findChild(parent, leaf)) {
        std::cerr << "erro: já existe '" << leaf << "'\n";
        return false;
    }

    Node* dir = createNode(leaf, NodeType::Dir);
    addChild(parent, dir);
    return true;
}

bool FileSystem::cd(const std::string& path) {
    Node* target = resolvePath(path);
    if (!target) {
        std::cerr << "erro: diretório não encontrado\n";
        return false;
    }
    if (target->meta.type != NodeType::Dir) {
        std::cerr << "erro: não é um diretório\n";
        return false;
    }
    if (!canAccess(target, ACC_EXEC)) {
        std::cerr << "erro: permissão negada\n";
        return false;
    }
    cwd_ = target;
    cwd_->meta.accessed = std::time(nullptr);
    return true;
}

void FileSystem::ls(bool long_format) {
    if (!canAccess(cwd_, ACC_READ)) {
        std::cerr << "erro: permissão negada\n";
        return;
    }
    cwd_->meta.accessed = std::time(nullptr);

    for (Node* c = cwd_->children; c; c = c->next) {
        if (long_format) {
            std::cout << modeString(c->meta) << " "
                      << c->meta.owner << " "
                      << c->meta.group << " "
                      << c->meta.size << " "
                      << c->meta.inode << " "
                      << c->meta.name;
            if (c->meta.type == NodeType::Dir) {
                std::cout << "/";
            }
            std::cout << "\n";
        } else {
            std::cout << c->meta.name;
            if (c->meta.type == NodeType::Dir) {
                std::cout << "/";
            }
            std::cout << "  ";
        }
    }
    if (!long_format && cwd_->children) {
        std::cout << "\n";
    }
}

std::string FileSystem::pwd() const {
    return nodePath(cwd_);
}

bool FileSystem::touch(const std::string& name) {
    std::string leaf;
    Node* parent = resolveParentAndName(name, leaf);
    if (!parent) {
        std::cerr << "erro: caminho inválido\n";
        return false;
    }
    if (!validName(leaf)) {
        std::cerr << "erro: nome inválido\n";
        return false;
    }
    if (parent->meta.type != NodeType::Dir) {
        std::cerr << "erro: destino não é diretório\n";
        return false;
    }
    if (!canAccess(parent, ACC_WRITE | ACC_EXEC)) {
        std::cerr << "erro: permissão negada\n";
        return false;
    }

    Node* existing = findChild(parent, leaf);
    if (existing) {
        if (existing->meta.type != NodeType::File) {
            std::cerr << "erro: '" << leaf << "' é um diretório\n";
            return false;
        }
        existing->meta.accessed = existing->meta.modified = std::time(nullptr);
        return true;
    }

    Node* file = createNode(leaf, NodeType::File);
    addChild(parent, file);
    return true;
}

bool FileSystem::write(const std::string& name, const std::string& content) {
    Node* node = resolvePath(name);
    if (!node) {
        // cria automaticamente se não existir (como echo >)
        if (!touch(name)) {
            return false;
        }
        node = resolvePath(name);
        if (!node) {
            return false;
        }
    }
    if (node->meta.type != NodeType::File) {
        std::cerr << "erro: não é um arquivo\n";
        return false;
    }
    if (!canAccess(node, ACC_WRITE)) {
        std::cerr << "erro: permissão negada\n";
        return false;
    }
    return writeContentToNode(node, content);
}

bool FileSystem::cat(const std::string& name) {
    Node* node = resolvePath(name);
    if (!node) {
        std::cerr << "erro: arquivo não encontrado\n";
        return false;
    }
    if (node->meta.type != NodeType::File) {
        std::cerr << "erro: não é um arquivo\n";
        return false;
    }
    if (!canAccess(node, ACC_READ)) {
        std::cerr << "erro: permissão negada\n";
        return false;
    }
    node->meta.accessed = std::time(nullptr);
    std::cout << readContentFromNode(node) << "\n";
    return true;
}

bool FileSystem::cp(const std::string& src, const std::string& dst) {
    Node* source = resolvePath(src);
    if (!source) {
        std::cerr << "erro: origem não encontrada\n";
        return false;
    }
    if (source->meta.type != NodeType::File) {
        std::cerr << "erro: cp só copia arquivos\n";
        return false;
    }
    if (!canAccess(source, ACC_READ)) {
        std::cerr << "erro: permissão negada na origem\n";
        return false;
    }

    std::string leaf;
    Node* parent = resolveParentAndName(dst, leaf);
    if (!parent || !validName(leaf)) {
        std::cerr << "erro: destino inválido\n";
        return false;
    }
    if (!canAccess(parent, ACC_WRITE | ACC_EXEC)) {
        std::cerr << "erro: permissão negada no destino\n";
        return false;
    }

    std::string content = readContentFromNode(source);
    Node* dest = findChild(parent, leaf);
    if (dest) {
        if (dest->meta.type != NodeType::File) {
            std::cerr << "erro: destino é um diretório\n";
            return false;
        }
        if (!canAccess(dest, ACC_WRITE)) {
            std::cerr << "erro: permissão negada no destino\n";
            return false;
        }
        return writeContentToNode(dest, content);
    }

    Node* copy = createNode(leaf, NodeType::File);
    addChild(parent, copy);
    if (!writeContentToNode(copy, content)) {
        removeChild(parent, copy);
        delete copy;
        return false;
    }
    return true;
}

bool FileSystem::mv(const std::string& src, const std::string& dst) {
    Node* source = resolvePath(src);
    if (!source || source == root_) {
        std::cerr << "erro: origem inválida\n";
        return false;
    }

    Node* old_parent = source->parent;
    if (!canAccess(old_parent, ACC_WRITE | ACC_EXEC)) {
        std::cerr << "erro: permissão negada na origem\n";
        return false;
    }

    std::string leaf;
    Node* new_parent = resolveParentAndName(dst, leaf);
    if (!new_parent || !validName(leaf)) {
        std::cerr << "erro: destino inválido\n";
        return false;
    }
    if (!canAccess(new_parent, ACC_WRITE | ACC_EXEC)) {
        std::cerr << "erro: permissão negada no destino\n";
        return false;
    }

    // Evita mover diretório para dentro de si mesmo
    if (source->meta.type == NodeType::Dir) {
        Node* walk = new_parent;
        while (walk && walk != root_) {
            if (walk == source) {
                std::cerr << "erro: não é possível mover para dentro de si mesmo\n";
                return false;
            }
            walk = walk->parent;
        }
    }

    Node* conflict = findChild(new_parent, leaf);
    if (conflict && conflict != source) {
        std::cerr << "erro: já existe '" << leaf << "'\n";
        return false;
    }

    removeChild(old_parent, source);
    source->meta.name = leaf;
    addChild(new_parent, source);
    source->meta.modified = std::time(nullptr);
    return true;
}

bool FileSystem::rm(const std::string& name) {
    Node* node = resolvePath(name);
    if (!node || node == root_) {
        std::cerr << "erro: não encontrado ou inválido\n";
        return false;
    }
    Node* parent = node->parent;
    if (!canAccess(parent, ACC_WRITE | ACC_EXEC)) {
        std::cerr << "erro: permissão negada\n";
        return false;
    }
    if (node->meta.type == NodeType::Dir && node->children != nullptr) {
        std::cerr << "erro: diretório não está vazio\n";
        return false;
    }

    // Se estiver no cwd ou abaixo, sobe para o pai
    Node* walk = cwd_;
    while (walk && walk != root_) {
        if (walk == node) {
            cwd_ = parent;
            break;
        }
        walk = walk->parent;
    }

    removeChild(parent, node);
    freeNodeBlocks(node);
    delete node;
    return true;
}

bool FileSystem::chmod(const std::string& name, int mode) {
    Node* node = resolvePath(name);
    if (!node) {
        std::cerr << "erro: não encontrado\n";
        return false;
    }
    // Só o dono (ou o usuário atual se for o dono) altera permissões
    if (uid_ != node->meta.owner) {
        std::cerr << "erro: permissão negada (apenas o dono)\n";
        return false;
    }
    if (mode < 0 || mode > 0777) {
        std::cerr << "erro: modo inválido (use 0..777 octal)\n";
        return false;
    }
    node->meta.mode = mode;
    node->meta.modified = std::time(nullptr);
    return true;
}
