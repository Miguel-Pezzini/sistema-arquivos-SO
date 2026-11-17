# Mini-Sistema de Arquivos em Memória

Simulador didático de sistema de arquivos em **C++17**, desenvolvido para a disciplina de Sistemas Operacionais (UNIVALI).

O programa **não** usa o sistema de arquivos real do SO: tudo vive em estruturas na memória RAM, com um “disco” simulado (`char disk[]`) e alocação **indexada** de blocos.

## Compilar e executar

```bash
make
./minifs
```

Ou:

```bash
make run
```

Limpar artefatos:

```bash
make clean
```

Requisitos: `g++` com suporte a C++17 (Linux / WSL).

## Comandos do shell

| Comando | Descrição |
|---------|-----------|
| `mkdir <nome>` | Cria diretório |
| `cd <path>` | Navega (`..`, `/`, `dir/sub`) |
| `ls` / `ls -l` | Lista entradas (modo longo mostra permissões, dono, tamanho, inode) |
| `pwd` | Caminho atual |
| `touch <nome>` | Cria arquivo vazio |
| `write <nome> <texto>` | Escreve conteúdo (cria o arquivo se não existir) |
| `cat <nome>` | Lê e imprime o conteúdo |
| `cp <src> <dst>` | Copia arquivo |
| `mv <src> <dst>` | Move ou renomeia |
| `rm <nome>` | Remove arquivo ou diretório vazio |
| `chmod <octal> <nome>` | Altera permissões (ex.: `chmod 644 arq`) |
| `clear` | Limpa a tela |
| `help` | Ajuda |
| `exit` | Sai |

## Exemplo de uso

```text
user@fs:/$ mkdir docs
user@fs:/$ cd docs
user@fs:/docs$ touch nota.txt
user@fs:/docs$ write nota.txt ola mundo
user@fs:/docs$ cat nota.txt
ola mundo
user@fs:/docs$ ls -l
-rw-r--r-- 1000 1000 9 2 nota.txt
user@fs:/docs$ chmod 000 nota.txt
user@fs:/docs$ cat nota.txt
erro: permissão negada
user@fs:/docs$ chmod 644 nota.txt
user@fs:/docs$ cp nota.txt copia.txt
user@fs:/docs$ mv copia.txt ../copia.txt
user@fs:/docs$ cd ..
user@fs:/$ ls
docs/  copia.txt
```

Comparação rápida com Linux real:

- `ls -l` / `chmod` / `mkdir` / `cd` / `cat` têm o mesmo *papel* conceitual.
- Aqui os dados ficam no array `disk_`, não em partições ou inodes do kernel.

## Design e estruturas de dados

### File Control Block (FCB) e inode simulado

Cada entrada (arquivo ou diretório) tem um `FCB` com:

- `name`, `size`, `type` (arquivo/diretório)
- `created` / `modified` / `accessed` (`time_t`)
- `inode` (ID único incremental)
- `mode` (bitmask RWX)
- `owner` / `group`
- `blocks[]` — índices dos blocos no disco simulado (alocação indexada)

Isso espelha a ideia de inode/FCB: metadados separados do conteúdo.

### Árvore de diretórios + lista encadeada

```text
Node (diretório)
 ├── children → Node → next → Node → next → ...
 └── parent
```

- A hierarquia forma uma **árvore** (`parent` / filhos).
- Os filhos de um diretório são uma **lista encadeada** (`children` / `next`).
- Vantagens didáticas: nomeação hierárquica, agrupamento lógico, navegação com `cd`.

### Disco em memória e alocação indexada

- Disco: `char disk_[NUM_BLOCKS * BLOCK_SIZE]` (256 blocos × 64 bytes ≈ 16 KiB).
- Bitmap: `bool used_[NUM_BLOCKS]`.
- Cada arquivo guarda até `MAX_FILE_BLOCKS` (8) índices em `FCB.blocks[]`.
- Ao escrever, o conteúdo é fatiado em blocos; o FCB referencia esses blocos (como índices diretos de um inode).

Limite documentado: arquivo de no máximo `8 * 64 = 512` bytes.

### Controle de acesso (RWX)

Permissões no estilo Unix, três classes (owner / group / other), cada uma com bits **R=4, W=2, X=1**.

Exemplo: `0644` → `rw-r--r--`.

Antes de ler, escrever, entrar em diretório (`X`) ou criar/remover entradas no diretório (`W`+`X`), o simulador consulta o usuário atual (`uid=1000`, `gid=1000`) e nega a operação se a máscara não permitir — bloqueio obrigatório simplificado.

`chmod` só é permitido ao dono do arquivo/diretório.

## Organização do código

```text
include/fs.hpp   — tipos (FCB, Node) e classe FileSystem
src/fs.cpp       — disco, lista, operações e permissões
src/main.cpp     — shell interativo
Makefile
README.md
```

## Conceitos teóricos demonstrados

1. **Arquivo e atributos** — nome, tamanho, tipo, datas, inode, proteção.
2. **Operações** — criar, ler, escrever, copiar, mover, excluir.
3. **FCB / inode** — metadados + ponteiros para blocos.
4. **Diretórios em árvore** — eficiência de organização e paths.
5. **Proteção RWX** — bitmasks e `chmod` numérico.
6. **Alocação indexada** — conteúdo espalhado em blocos referenciados pelo FCB.

## Autoria

Trabalho avaliativo — Sistemas Operacionais (5º período).
