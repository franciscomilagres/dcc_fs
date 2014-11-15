
#include "utils.h"
#include "fs.h"
#include "StringProc.h"

void cleanNode(struct inode* n) {
    n->links[0] = 0;
    n->meta = 0;
    n->mode = 0;
    n->next = 0;
    n->parent = 0;
}

void initNode(struct inode** n, size_t sz) {
    *n = (struct inode*) malloc(sz);
    cleanNode(*n);
}

int getFileSize(const char* fname) {
    FILE *fd = fopen(fname, "r");
    fseek(fd, 0, SEEK_END);
    int size = ftell(fd);
    rewind(fd);
    fclose(fd);
    return size;
}

int getLinksMaxLen(const struct superblock* sb) {
    int ans = (sb->blksz - sizeof (struct inode)) / sizeof (uint64_t);
    return ans;
}

int getFileNameMaxLen(const struct superblock* sb) {
    int ans = sb->blksz - sizeof (struct nodeinfo);
    return ans - 1;
}

int getLinksLen(const struct inode* node) {
    int len = 0;
    while (node->links[len] != 0)len++;
    return len;
}

uint64_t getNodeLastLinkBlock(const struct superblock* sb, uint64_t nodeBlock) {
    uint64_t ans = nodeBlock;
    struct inode* node = NULL;
    node = malloc(sb->blksz);
    SEEK_READ(sb, nodeBlock, node);
    while (node->next != 0) {
        ans = node->next;
        SEEK_READ(sb, node->next, node);
    }
    return ans;
}

uint64_t findFile(const struct superblock* sb, const char* fname) {
    struct inode* node, *ent;
    initNode(&node, sb->blksz);
    initNode(&ent, sb->blksz);
    struct nodeinfo *meta = (struct nodeinfo*) malloc(sizeof (sb->blksz));
    int len = 0;
    char** fileParts = getFileParts(fname, &len);

    uint64_t fileBlock = sb->root;
    SEEK_READ(sb, fileBlock, node);
    int it = 0;
    while (it < len) {
        int foundEnt = FALSE;
        int i = 0;
        do {
            while (node->links[i] != 0) {
                SEEK_READ(sb, node->links[i], ent);
                SEEK_READ(sb, ent->meta, meta);
                if (strcmp(meta->name, fileParts[it]) == 0) {
                    foundEnt = TRUE;
                    fileBlock = node->links[i];
                    it++;
                    break;
                }
                i++;
            }
            if ((foundEnt || node->next == 0))break;
            SEEK_READ(sb, node->next, node);
        } while (node->next != 0);
        if (foundEnt) {
            SEEK_READ(sb, fileBlock, node);
        } else {
            return fileBlock;
        }
    }
    return fileBlock;
}

int existsFile(const struct superblock* sb, const char* fname) {
    struct inode* dirNode = NULL;
    struct nodeinfo* meta = NULL;
    initNode(&dirNode, sb->blksz);
    meta = (struct nodeinfo*) malloc(sb->blksz);
    uint64_t dirBlock = findFile(sb, fname);
    lseek((sb)->fd, (dirBlock) * (sb)->blksz, SEEK_SET);
    read((sb)->fd, (dirNode), (sb)->blksz);
    SEEK_READ(sb, dirNode->meta, meta);
    int len = 0;
    char** fileparts = getFileParts(fname, &len);
    if (strcmp(meta->name, fileparts[len - 1]) == 0) {
        free(meta);
        free(dirNode);
        return TRUE;
    }
    free(meta);
    free(dirNode);
    return FALSE;
}

int addFileToDir(struct superblock* sb, const char* dirName,
        const uint64_t fileBlock) {
    struct inode*dirNode = NULL;
    struct nodeinfo* meta = NULL;
    dirNode = malloc(sb->blksz);
    meta = malloc(sb->blksz);
    uint64_t dirBlock = findFile(sb, dirName);
    SEEK_READ(sb, dirBlock, dirNode);
    SEEK_READ(sb, dirNode->meta, meta);
    int len = 0;
    char** fileparts = getFileParts(dirName, &len);
    if (strcmp(meta->name, fileparts[len - 1]) != 0)return 0; //dir does not exists
    if ((++meta->size) % getLinksMaxLen(sb) == 0) {
        //needs to create another link block      
        struct inode* linknode = NULL;
        linknode = malloc(sb->blksz);
        linknode->links[0] = fileBlock;
        linknode->links[1] = 0;
        uint64_t lastLinkBlock = getNodeLastLinkBlock(sb, dirBlock);
        struct inode* lastLinkNode = NULL;
        lastLinkNode = malloc(sb->blksz);
        if (lastLinkNode->next != 0) {
            exit(EXIT_FAILURE);
        }
        lastLinkNode->next = fs_get_block(sb);
        SEEK_WRITE(sb, lastLinkNode->next, linknode);
        SEEK_WRITE(sb, lastLinkBlock, lastLinkNode);
        free(linknode);
        free(lastLinkNode);
    } else {
        int linkLen = getLinksLen(dirNode);
        dirNode->links[linkLen] = fileBlock;
        dirNode->links[linkLen + 1] = 0;
    }
    SEEK_WRITE(sb, dirBlock, dirNode);
    SEEK_WRITE(sb, dirNode->meta, meta);

    free(dirNode);
    free(meta);
    return FALSE;
}

int addFileToDirBlock(struct superblock* sb, const uint64_t dirBlock,
        const uint64_t fileBlock) {
    struct inode *dirNode = NULL;
    struct nodeinfo* meta = NULL;
    dirNode = malloc(sb->blksz);
    meta = malloc(sb->blksz);
    SEEK_READ(sb, dirBlock, dirNode);
    SEEK_READ(sb, dirNode->meta, meta);
    if ((++meta->size) % getLinksMaxLen(sb) == 0) {
        //needs to create another link block      
        struct inode* linknode = NULL;
        linknode = malloc(sb->blksz);
        linknode->links[0] = fileBlock;
        linknode->links[1] = 0;
        uint64_t lastLinkBlock = getNodeLastLinkBlock(sb, dirBlock);
        struct inode* lastLinkNode = NULL;
        lastLinkNode = malloc(sb->blksz);
        if (lastLinkNode->next != 0) {
            exit(EXIT_FAILURE);
        }
        lastLinkNode->next = fs_get_block(sb);
        SEEK_WRITE(sb, lastLinkNode->next, linknode);
        SEEK_WRITE(sb, lastLinkBlock, lastLinkNode);
        free(linknode);
        free(lastLinkNode);
    } else {
        int linkLen = getLinksLen(dirNode);
        dirNode->links[linkLen] = fileBlock;
        dirNode->links[linkLen + 1] = 0;
    }
    SEEK_WRITE(sb, dirBlock, dirNode);
    SEEK_WRITE(sb, dirNode->meta, meta);

    free(dirNode);
    free(meta);
    return FALSE;
}