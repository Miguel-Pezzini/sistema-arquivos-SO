#include "fs.hpp"

#include <iostream>
#include <sstream>
#include <string>

static void printHelp() {
    std::cout
        << "Comandos:\n"
        << "  mkdir <nome>           cria diretório\n"
        << "  cd <path>              muda de diretório\n"
        << "  ls [-l]                lista entradas\n"
        << "  pwd                    mostra caminho atual\n"
        << "  touch <nome>           cria arquivo vazio\n"
        << "  write <nome> <texto>   escreve conteúdo no arquivo\n"
        << "  cat <nome>             mostra conteúdo\n"
        << "  cp <src> <dst>         copia arquivo\n"
        << "  mv <src> <dst>         move/renomeia\n"
        << "  rm <nome>              remove arquivo ou dir vazio\n"
        << "  chmod <octal> <nome>   altera permissões (ex: 644)\n"
        << "  clear                  limpa a tela\n"
        << "  help                   esta ajuda\n"
        << "  exit                   sai\n";
}

static int parseOctal(const std::string& s, bool& ok) {
    ok = false;
    if (s.empty()) {
        return 0;
    }
    int value = 0;
    for (char c : s) {
        if (c < '0' || c > '7') {
            return 0;
        }
        value = value * 8 + (c - '0');
    }
    ok = true;
    return value;
}

int main() {
    FileSystem fs;
    std::cout << "Mini-Sistema de Arquivos (memória, alocação indexada)\n"
              << "Digite 'help' para ver os comandos.\n";

    std::string line;
    while (true) {
        std::cout << "user@fs:" << fs.pwd() << "$ " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "exit" || cmd == "quit") {
            break;
        }
        if (cmd == "help") {
            printHelp();
            continue;
        }
        if (cmd == "clear") {
            std::cout << "\033[2J\033[H" << std::flush;
            continue;
        }
        if (cmd == "pwd") {
            std::cout << fs.pwd() << "\n";
            continue;
        }
        if (cmd == "ls") {
            std::string opt;
            iss >> opt;
            fs.ls(opt == "-l");
            continue;
        }
        if (cmd == "cd") {
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cerr << "uso: cd <path>\n";
            } else {
                fs.cd(path);
            }
            continue;
        }
        if (cmd == "mkdir") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cerr << "uso: mkdir <nome>\n";
            } else {
                fs.mkdir(name);
            }
            continue;
        }
        if (cmd == "touch") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cerr << "uso: touch <nome>\n";
            } else {
                fs.touch(name);
            }
            continue;
        }
        if (cmd == "write") {
            std::string name;
            iss >> name;
            std::string content;
            std::getline(iss, content);
            if (!content.empty() && content[0] == ' ') {
                content.erase(0, 1);
            }
            if (name.empty()) {
                std::cerr << "uso: write <nome> <texto>\n";
            } else {
                fs.write(name, content);
            }
            continue;
        }
        if (cmd == "cat") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cerr << "uso: cat <nome>\n";
            } else {
                fs.cat(name);
            }
            continue;
        }
        if (cmd == "cp") {
            std::string src, dst;
            iss >> src >> dst;
            if (src.empty() || dst.empty()) {
                std::cerr << "uso: cp <src> <dst>\n";
            } else {
                fs.cp(src, dst);
            }
            continue;
        }
        if (cmd == "mv") {
            std::string src, dst;
            iss >> src >> dst;
            if (src.empty() || dst.empty()) {
                std::cerr << "uso: mv <src> <dst>\n";
            } else {
                fs.mv(src, dst);
            }
            continue;
        }
        if (cmd == "rm") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cerr << "uso: rm <nome>\n";
            } else {
                fs.rm(name);
            }
            continue;
        }
        if (cmd == "chmod") {
            std::string mode_str, name;
            iss >> mode_str >> name;
            bool ok = false;
            int mode = parseOctal(mode_str, ok);
            if (!ok || name.empty()) {
                std::cerr << "uso: chmod <octal> <nome>   (ex: chmod 644 arq)\n";
            } else {
                fs.chmod(name, mode);
            }
            continue;
        }

        std::cerr << "comando desconhecido: '" << cmd << "' (digite help)\n";
    }

    return 0;
}
