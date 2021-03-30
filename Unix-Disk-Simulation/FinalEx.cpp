#include <iostream>
#include <vector>
#include <map>
#include <assert.h>
#include <string.h>
#include <string>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <queue>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;
#define DISK_SIZE 256
void decToBinary(int n,char &c)
{
    c='\0';
    // array to store binary number
    int binaryNum[8];

    // counter for binary array
    int i = 0;
    while (n > 0)
    {
        // storing remainder in binary array
        binaryNum[i] = n % 2;
        n = n / 2;
        i++;
    }

    // printing binary array in reverse order
    for (int j = i - 1; j >= 0; j--)
    {
        if (binaryNum[j] == 1)
            c = c | 1u << j;
    }
}

// #define SYS_CALL
// ============================================================================
class fsInode
{
    int fileSize;
    int blocks_in_use;
    int *directBlocks;
    int singleInDirect;
    int num_of_direct_blocks;
    int block_size; 
    bool block_full; // A boolean var, true only if the last block delt with has been filled.
    bool direct_blocks_full; // A boolean var, true only if all direct entries are taken.
    int last_direct; // A var to hold the last direct block delt with.
    int last_in_direct; // A var to hold the last single-indirect delt wilth.
    int *inDirectBlocks; // An array for further manipulations on the single-indirect blocks.

public:
    fsInode(int _block_size, int _num_of_direct_blocks) // fsInode Ctor.
    {
        fileSize = 0;
        block_full = true;
        direct_blocks_full = false;
        last_direct = -1;
        last_in_direct = -1;
        block_size = _block_size;
        num_of_direct_blocks = _num_of_direct_blocks;
        inDirectBlocks = new int[block_size];
        assert(inDirectBlocks);
        for (int i = 0; i < block_size; i++)
        {
            inDirectBlocks[i] = -1;
        }
        directBlocks = new int[num_of_direct_blocks];
        assert(directBlocks);
        for (int i = 0; i < num_of_direct_blocks; i++)
        {
            directBlocks[i] = -1;
        }
        singleInDirect = -1;
    }
    void setIthInDirectBlock(int i, int block) // A setter for the i'th cell in the in
    {
        inDirectBlocks[i] = block;
    }
    void setSingleIndirectBlock(int block) // A setter for the indirect block.
    {
        this->singleInDirect = block;
    }
    bool isInDirectBlockFull() // A getter to know if there's a block that is unfilled to it's maximum yet.
    {
        if(this->getWritten() % block_size == 0)
            return true;
        else
            return false;
    }
    void setLastInDirect(int x) { this->last_in_direct = x; } // A setter to the last indirect block delt with.
    int getLastInDirect() { return this->last_in_direct; } // A getter of the last indirect block delt with.
    void setBlockFull(bool y) { block_full = y; } // A setter for the boolean condition if the block is full.
    bool IsBlockFull() { return this->block_full; } // A getter of the boolean condition if the block is full.
    bool AreDirectBlocksFull() { return this->direct_blocks_full; } // A getter of the boolean condition to know if all direct blocks have been filled.
    void FillDirectBlocks() { this->direct_blocks_full = true; } // A setter of the boolean condition to update that the direct blocks have been filled to maximum.
    void setIth_directBlock(int block, int i) { this->directBlocks[i] = block; } // A setter of the i'th cell of the diret blocks array. 
    int getIth_directBlock(int i) { return directBlocks[i]; } // A getter of the i'th cell of the direct blocks array. 
    void setWritten(int x) { this->fileSize += x; } // A setter to update the size of a file.
    int *getDirectBlocks() { return this->directBlocks; } // A getter to a pointer of the direct blocks array.
    void setLastDirectBlock(int x) { this->last_direct = x; } // A setter to set the last direct block delt with.
    int getLastDirect() { return this->last_direct; } // A getter to get the last direct block delt with.
    int getWritten() // A getter of the file's size.
    { 
        return this->fileSize; 
    }
    int getInDirectBlock() { return this->singleInDirect; } // A getter of the InDirect block number.
    ~fsInode()
    {
        delete directBlocks;
        delete inDirectBlocks;
    }
};

// ============================================================================
class FileDescriptor
{
    pair<string, fsInode *> file;
    bool inUse;

public:
    FileDescriptor(string FileName, fsInode *fsi)
    {
        file.first = FileName;
        file.second = fsi;
        inUse = true;
    }
    void setInode(fsInode* fsi) // A setter to set the fsInode.
    {
        file.second=fsi;
    }
    void setFileName(string name) // A setter to set the file's name.
    {
         file.first = name;
    }
    string getFileName() // A getter of the file's name.
    {
        return file.first;
    }

    fsInode *getInode() // A getter of a file's fsInode.
    {

        return file.second;
    }

    bool isInUse() // A getter of the boolean condition to know if the file is open.
    {
        return (inUse);
    } 
    void setInUse(bool _inUse) // A setter to update the boolean condition if the file's open.
    { 
        inUse = _inUse;
    }
};

#define DISK_SIM_FILE "DISK_SIM_FILE.txt"
// ============================================================================
class fsDisk
{
    FILE *sim_disk_fd;

    bool is_formated;

    // BitVector - "bit" (int) vector, indicate which block in the disk is free
    //              or not.  (i.e. if BitVector[0] == 1 , means that the
    //             first block is occupied.
    int BitVectorSize;
    int *BitVector;
    queue<int> deleted_fds; // A queue to hold the deleted and unused file descriptors.
    // Unix directories are lists of association structures,
    // each of which contains one filename and one inode number.
    map<string, fsInode *> MainDir;

    // OpenFileDescriptors --  when you open a file,
    // the operating system creates an entry to represent that file
    // This entry number is the file descriptor.
    vector<FileDescriptor> OpenFileDescriptors;
    int used_blocks; // A counter of the used blocks of the disk.
    int direct_entries;
    int block_size;
    int open_file_count;
    int max_size;

public:
    // ------------------------------------------------------------------------
    fsDisk()
    {
        sim_disk_fd = fopen(DISK_SIM_FILE, "r+");
        assert(sim_disk_fd);
        for (int i = 0; i < DISK_SIZE; i++)
        {
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
            open_file_count = 0;
            used_blocks = 0;
        }
        fflush(sim_disk_fd);
    }
    ~fsDisk()
    {
        free(sim_disk_fd);
        free(BitVector);
        for(auto it=begin(MainDir); it!=end(MainDir);++it)
        {
            it->second->~fsInode();
            MainDir.erase(it);
        }
        MainDir.clear();
        MainDir.~map();
        for(int i =0 ;i < OpenFileDescriptors.size();i++)
        {
            OpenFileDescriptors.at(i).getInode()->~fsInode();
        }
        OpenFileDescriptors.clear();
        OpenFileDescriptors.~vector();
    }
    void initDisk() // A function to initialize the disk.
    {
        for (int i = 0; i < DISK_SIZE; i++)
        {
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
        }
    }
    int getBitVectorFreeBlocks()
    {
        int x=0;
        for(int i = 0;i<BitVectorSize;i++)
        {
            if(BitVector[i]==0) x++;
        }
        return x;
    }
    // ------------------------------------------------------------------------
    void listAll()
    {
        int i = 0;
        for (auto it = begin(OpenFileDescriptors); it != end(OpenFileDescriptors); ++it)
        {
            cout << "index: " << i << ": FileName: " << it->getFileName() << " , isInUse: " << it->isInUse() << endl;
            i++;
        }
        char bufy;
        cout << "Disk content: '";
        for (i = 0; i < DISK_SIZE; i++)
        {
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = fread(&bufy, 1, 1, sim_disk_fd);
            cout << bufy;
        }
        cout << "'" << endl;
    }

    // ------------------------------------------------------------------------
    void fsFormat(int blockSize = 4, int direct_Entries_ = 3)
    {
        this->direct_entries = direct_Entries_;
        this->block_size = blockSize;
        for (int i = 0; i < this->OpenFileDescriptors.size(); i++) // If there's any data in the current disk, delete it.
        {
            this->OpenFileDescriptors.front().getInode()->~fsInode();
            delete (this->OpenFileDescriptors.front().getInode());
        }
        this->MainDir.~map(); // Destroy any data if exists already in the map. (format the map)
        this->OpenFileDescriptors.clear(); // Clear the OpenFileDescriptors vector.
        this->initDisk(); // Initialize the disk.
        this->BitVectorSize = DISK_SIZE / this->block_size; // Caclculate the amount of blocks according to the given data.
        this->BitVector = (int *)malloc(sizeof(int) * BitVectorSize); // Assert the BitVector array.
        assert(BitVector);
        for (int i = 0; i < this->BitVectorSize; i++)
        {
            this->BitVector[i] = 0;
        }
        this->max_size = (this->direct_entries + this->block_size) * this->block_size; // A calculation of the maximum file size according to the given data.
        this->open_file_count = 0;
        this->is_formated = true;
        cout << "FORMAT DISC: There are now " << DISK_SIZE / blockSize << " blocks." << endl;
    }

    // ------------------------------------------------------------------------
    int CreateFile(string fileName)
    {
        int fd; // An integer to return the fd asserted.
        if (this->is_formated == false) // If the disk was not yet formated, print an error message and exit.
        {
            cout << "Disk not formated." << endl;
            return -1;
        }
        if(!this->deleted_fds.empty()) // As long as there are free fd's anywhere inside the vector.
        {
            fd = this->deleted_fds.front(); // Take from the queue the fd to be asserted. (According to FIFO)
            deleted_fds.pop(); // Pop.
            fsInode *new_inode = new fsInode(this->block_size, this->direct_entries); // Create a new inode.
            auto it = begin(OpenFileDescriptors); // Initialize an iterator to the begining of the vector.
            for(int i = 0;i <fd;i++) // Forward it to the fd that we need to assert to the file that's requested to be opened.
            {
                ++it;
            }
            it->setInode(new_inode); // Set the new fsInode.
            it->setFileName(fileName); // Set the new filename.
            it->setInUse(true); // Set the boolean condition to update that the file has been openend.
            pair<string, fsInode *> new_file; // Define a new pair, to be inserted into MainDir.
            new_file.first = fileName; // Set the file's name.
            new_file.second = new_inode; // Set the fsInode of the file.
            this->MainDir.insert(new_file); // Insert into MainDir.
            this->open_file_count++;
            return fd;
        }
        else // If there's no any free fd's anywhere in the OpenFileDescriptors vector.
        {
            fsInode *new_inode = new fsInode(this->block_size, this->direct_entries); // Ceate a new fsInode.
            FileDescriptor new_fd = FileDescriptor(fileName, new_inode); // Create a new FileDescriptor with the last created fsInode and given file's name.
            new_fd.setInUse(true); // Update the boolean condition of the file to true, so it'll be open.
            this->OpenFileDescriptors.push_back(new_fd); // Push the last fd to the end of the vector, since there are no fd's free inbetween.
            pair<string, fsInode *> new_file; 
            new_file.first = fileName;
            new_file.second = new_inode;
            this->MainDir.insert(new_file);
            this->open_file_count++;
            return this->open_file_count - 1;
        }
        
    }

    // ------------------------------------------------------------------------
    int OpenFile(string fileName)
    {
        bool found = false;
        int fd = 0;
        for (auto it1 = begin(this->OpenFileDescriptors); it1 != end(this->OpenFileDescriptors); ++it1)
        {
            if (it1->getFileName().compare(fileName) == 0)
            {
                found = true;
                if (it1->isInUse())
                {
                    cout << "The file is already open." << endl;
                    return fd;
                }
                else
                {
                    it1->setInUse(true);
                    this->open_file_count++;
                    return fd;
                }
            }
            fd++;
        }
        if (!found)
        {
            cout << "ERROR: File not found." << endl;
            return -1;
        }
    }

    // ------------------------------------------------------------------------
    string CloseFile(int fd)
    {
        if (fd > this->OpenFileDescriptors.size() - 1)
            return "-1";
        else if (this->OpenFileDescriptors.at(fd).isInUse()) // If the file is open, close it.
        {
            this->open_file_count--;
            this->OpenFileDescriptors.at(fd).setInUse(false);
            return this->OpenFileDescriptors.at(fd).getFileName();
        }
        else if (this->OpenFileDescriptors.at(fd).isInUse() == false) // If file is already close, return a message.
        {
            cout << "The requested fd's is closed already." << endl;
            return this->OpenFileDescriptors.at(fd).getFileName();
        }
        else
        {
            cout << "File does not exist." << endl;
            return "-1";
        }
    }
    // ------------------------------------------------------------------------
    int WriteToFile(int fd, char *buf, int len)
    {
        bool to_break=false;
        char* reading = (char*)malloc(sizeof(char)); // An array to read to.
        assert(reading); 
        char* writing;
        char test = '\0';
        char dectoBin = '\0';
        int last_block; // Last block delt with.
        int offset;
        int seek = 0;
        int count_inDirect = 0;
        int to_write = 0;
        int indirect_write;
        fsInode *the_node = this->OpenFileDescriptors.at(fd).getInode();
        offset = the_node->getWritten() % block_size;
        if (fd > this->OpenFileDescriptors.size() - 1)
        {
            cout << "ERROR: Given file descriptor does not exist." << endl;
            return -1;
        }
        if (len <= 0)
        {
            cout << "ERROR: Length of given string is 0 (or illegal)." << endl;
            return -1;
        }
        if(offset!=0)
        {
            if((this->getBitVectorFreeBlocks()*block_size+(block_size-offset))<len)
            {
                cout << "ERROR: There's no enough space in the disk." << endl;
                return -1;
            }
        }
        if(offset==0)
        {
            if(this->getBitVectorFreeBlocks()*block_size<len)
            {
                cout << "ERROR: There's no enough space in the disk." << endl;
                return -1;
            }
        }
        if (the_node->getWritten() + len > (direct_entries + block_size) * block_size)
        {
            cout << "ERROR: Not enough space in the file." << endl;
            return -1;
        }
        if(!this->OpenFileDescriptors.at(fd).isInUse()) 
        {
            cout << "ERROR: File is closed." << endl;
            return -1;
        }
        if(!this->is_formated)
        {
            cout << "ERROR: Disk not formated." << endl;
            return -1;
        }
        if (the_node->getWritten() == block_size * direct_entries)
        {
            the_node->FillDirectBlocks();
        }
        while (len > 0 && !the_node->IsBlockFull()) // As long as there's anything left to write and also there's space available in the last block written to.
        {
            if (!the_node->AreDirectBlocksFull()) // As long as there's still place in the direct blocks.
            { 
                offset = the_node->getWritten() % block_size; // Calculate the offset inside the last block written to.
                to_write = block_size - offset; // Calculate how many characters to write,
                last_block = the_node->getLastDirect(); // Get the last direct block written to.
                if(last_block > direct_entries-1) last_block = direct_entries -1;
                seek = the_node->getIth_directBlock(last_block) * block_size + offset; // Calculate the seek inside the disk.
                lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                if(write(sim_disk_fd->_fileno, buf, to_write)==-1)
                {
                    cout << "ERROR: Could not write." << endl;
                }
                buf += to_write; // Forward the pointer.
                the_node->setWritten(to_write); // Update the size of the file.
                if (offset + to_write == block_size) // If the last data written has filled the block, update the boolean condition.
                {
                    the_node->setBlockFull(true);
                }
                if(the_node->getLastDirect() == this->direct_entries-1 && (the_node->getWritten()/(direct_entries*block_size))==1) // If there's no space left in the direct blocks, update the boolean condition.
                {
                    the_node->FillDirectBlocks();
                }
                len -= to_write; // Update the length left to write.
            }
        }
        while (len > 0 && the_node->IsBlockFull()) // If there's still data to write and the last block delt with is full.
        {
            if (!the_node->AreDirectBlocksFull()&&the_node->getWritten() < block_size*direct_entries) // Check if there's still space in the direct blocks.
            {
                last_block = the_node->getLastDirect(); // Get the last direct block written to.
                for (int i = 0; i < BitVectorSize; i++) // Scan the BitVector for an available block to write to.
                {
                    if (BitVector[i] == 0)
                    {
                        if (last_block < direct_entries - 1)
                        {
                            BitVector[i] = 1; // Set the block to used.
                            the_node->setIth_directBlock(i, last_block + 1); // Update the i'th cell of the directBlocks array.
                            the_node->setLastDirectBlock(last_block + 1); // Update the last block written to.
                            seek = i * block_size; // Calculate the seek to the block asserted.
                            break;
                        }
                    }
                }
                if (len >= block_size) // If there's more than block-size to write, then we can only write block_size and fill the block
                {
                    to_write = block_size;
                    the_node->setBlockFull(true);
                }
                else // Else, there's len left to be written, and the last block written to has not been filled.
                {
                    to_write = len;
                    the_node->setBlockFull(false);
                }
                if(the_node->getLastDirect() == this->direct_entries-1 && (the_node->getWritten()/(direct_entries*block_size))==1) // If the direct blocks have been filled, update the boolean condition.
                {
                    the_node->FillDirectBlocks();
                }            
                lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                if(write(sim_disk_fd->_fileno, buf, to_write)==-1)
                {
                    cout << "ERROR: Could not write." << endl;
                }
                buf += to_write; // Forward the pointer.
                the_node->setWritten(to_write); // Update the size of the file.
                len -= to_write; // Update the length left to be written.
            }
            else
            {
                if (the_node->isInDirectBlockFull()) // If there's no space in any block to fill.
                {
                    for (int i = 0; i < BitVectorSize; i++) // Scan the BitVector for a free block and assert it to be the singleInDirect block.
                    {                                       // Also, if we got to this situation, assert the first inDirect data block, since we'll need it certainly.
                        last_block = the_node->getLastInDirect(); 
                        if (BitVector[i] == 0 && the_node->getLastInDirect()==-1)
                        {
                            BitVector[i] = 1;
                            the_node->setSingleIndirectBlock(i); // Update the block that was asserted to be the singleInDirect block.
                            for (int j = i + 1; j < BitVectorSize; j++)
                            {
                                if (BitVector[j] == 0 && count_inDirect < block_size) // If the block is free, and we did not already assert block-size number of indirect data blocks.
                                {
                                    BitVector[j] = 1;
                                    decToBinary(j, dectoBin);
                                    seek = i * block_size; // Calculate the seek inside the indirect block to write the first data indirect block number to the disk.
                                    lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                                    writing = &dectoBin;
                                    if(write(sim_disk_fd->_fileno,writing,sizeof(char))==-1) // Write the asserted indirect block number to the disk.
                                    {
                                        cout << "ERROR: Could not write." << endl;
                                    }                                  
                                    the_node->setLastInDirect(count_inDirect); // Set the last inDirect block delt with.
                                    count_inDirect++;
                                    break;
                                }
                            }
                            break;
                        }
                        else if(the_node->getLastInDirect()!=-1) // If we already asserted the first data block of the single-in-direct blocks
                        {
                            for (int j = 0; j < BitVectorSize; j++) // Scan through the BitVector for an available block to assert.
                            {                                       // If found an available block, write it to the single-in-direct block with the right offset in the disk.
                                if (BitVector[j] == 0 && count_inDirect < block_size)
                                {
                                    BitVector[j] = 1;
                                    decToBinary(j, dectoBin);
                                    seek = the_node->getInDirectBlock()*block_size+(the_node->getLastInDirect()+1);
                                    lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                                    writing = &dectoBin;
                                    if(write(sim_disk_fd->_fileno,writing,sizeof(char))==-1)
                                    {
                                        cout << "ERROR: Could not write." << endl;
                                    } 
                                    the_node->setLastInDirect(the_node->getLastInDirect()+1);
                                    count_inDirect++;
                                    to_break=true;
                                }
                                if(to_break)
                                {
                                    to_break=false;
                                    break;
                                } 
                            }
                            break;
                        }
                        
                    }
                    if (len >= block_size)
                    {
                        to_write = block_size;
                    }
                    if (len < block_size)
                    {
                        to_write = len;
                    }
                    seek = the_node->getInDirectBlock()*block_size+(the_node->getLastInDirect()); // Calculate the seek needed to read the correct data block of the single-in-direct.
                    if(the_node->getLastInDirect()==0 && the_node->getWritten() >= (direct_entries+1)*block_size)
                    {
                        seek = the_node->getInDirectBlock()*block_size+(the_node->getLastInDirect()+1);
                    } 
                    lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                    if(read(sim_disk_fd->_fileno,reading,sizeof(char))==-1) // Read the single-in-direct data block from the disk,
                    {
                        cout << "ERROR: Could not read." << endl;
                    }
                    indirect_write=(int)reading[0]; // Cast the char to int.
                    seek = indirect_write*block_size; // Calculate the seek to the single-in-direct data block.
                    lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                    if(write(sim_disk_fd->_fileno,buf,to_write)==-1) // Write to the correct single-in-direct data block.
                    {
                        cout << "ERROR: Could not write." << endl;
                    }
                    len-=to_write;
                    buf+=to_write;
                    the_node->setWritten(to_write);
                }
                else // If there's any space left to write in the last written single-in-direct block.
                {
                    offset = the_node->getWritten() % block_size; // Calculate the offset inside the block.
                    to_write = block_size - offset; 
                    last_block = the_node->getLastInDirect();
                    seek = the_node->getInDirectBlock()*block_size+(last_block);
                    lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                    if(read(sim_disk_fd->_fileno,reading,sizeof(char))==-1)
                    {
                        cout << "ERROR: Could not read." << endl;
                    }
                    indirect_write=(int)reading[0];
                    seek = indirect_write*block_size+offset;
                    lseek(sim_disk_fd->_fileno, seek, SEEK_SET);
                    if(write(sim_disk_fd->_fileno,buf,to_write)==-1)
                    {
                        cout << "ERROR: Could not write." << endl;
                    }
                    len-=to_write;
                    buf+=to_write;
                    the_node->setWritten(to_write);
                    
                }
            }
        }
    }
    // ------------------------------------------------------------------------
    int DelFile(string FileName)
    {
        int cast;
        char* reading = (char*) malloc (sizeof(char));
        assert(reading);
        int offset;
        int seek;
        int singleBlocks;
        int taken=0;
        bool found = false;
        for (auto it = begin(this->MainDir); it != end(this->MainDir); ++it) // Scan through MainDir for the file requested to be deleted, if exists than delete it from MainDir.
        {
            if(it->first.compare(FileName)==0) 
            {
                found = true;
                this->MainDir.erase(it);
                break;
            }
        }
        if(!found) 
        {
            cout << "ERROR: There is no file named: " << FileName << "." << endl;
            return -1;
        }
        for(int i = 0; i < this->OpenFileDescriptors.size();i++) // Scan the OpenFileDescriptors vector fo the requested file to be deleted.
        {
            
            if(this->OpenFileDescriptors.at(i).getFileName().compare(FileName)==0)
            {
                this->OpenFileDescriptors.at(i).setFileName(""); // Delete the file's name.
                singleBlocks = ceil((this->OpenFileDescriptors.at(i).getInode()->getWritten()-(direct_entries*block_size))/block_size); // Calculate how many single indirect data blocks the file has.
                this->deleted_fds.push(i); // After we delete this file, the fd will become available for other files to get it so push it to the queue.
                this->OpenFileDescriptors.at(i).setInUse(false); // Update the boolean condition to close the file if opened.
                int* temp = this->OpenFileDescriptors.at(i).getInode()->getDirectBlocks(); // Get the directBlocks array of the file.
                for(int j = 0; j < direct_entries;j++)
                {
                    if(temp[j]!=-1) taken++;
                }
                for(int k=0;k<taken;k++) // Go to the correct blocks in BitVector and update their status to available.
                {
                    BitVector[temp[k]]=0;
                }
                for(int z = 0; z< singleBlocks ;z++) // Calculate the seek of the single indirect block to get to the data blocks of it and update their status in the BitVector to available.
                {
                    seek = this->OpenFileDescriptors.at(i).getInode()->getInDirectBlock()*block_size + z;
                    lseek(sim_disk_fd->_fileno,seek,SEEK_SET);
                    if(read(sim_disk_fd->_fileno,reading,sizeof(char))==-1)
                    {
                        cout << "ERROR: Could not read." << endl;
                    }
                    cast = (int)reading[0];
                    BitVector[cast] = 0;
                }

                BitVector[this->OpenFileDescriptors.at(i).getInode()->getInDirectBlock()]=0; // Update the singleInDirect block to available.           
                this->OpenFileDescriptors.at(i).getInode()->~fsInode();
                return i;
            }
        }
        
    }
    // ------------------------------------------------------------------------
    int ReadFromFile(int fd, char *buf, int len)
    {
        fsInode* the_node = this->OpenFileDescriptors.at(fd).getInode();
        if(fd > this->OpenFileDescriptors.size()-1)
        {
            cout << "ERROR:Given FD is illegal." << endl;
            return -1;
        }
        if(!this->is_formated)
        {
            cout << "ERROR: Disk is not formated." << endl;
            return -1;
        }
        for(int i = 0; i < strlen(buf);i++)
        {
            buf[i] = '\0';
        }
        int already_read = 0;
        int direct_read = 0; // Direct blocks read.
        int single_read = 0; // InDirect blocks read.
        int temp;
        int singleBlock = the_node->getInDirectBlock();
        int to_read; 
        int seek;
        int* directBlocks = (int*) malloc(sizeof(int)*direct_entries);
        assert(directBlocks);
        directBlocks=the_node->getDirectBlocks();
        char* copy = (char*)malloc(sizeof(char)*block_size);
        assert(copy);
        char* singleRead = (char*) malloc(sizeof(char));
        assert(singleRead);
        if(len>the_node->getWritten()) // If the requested length to read is larget than the size of the file, than read all of the file's content.
        {
            len = the_node->getWritten();
        }
        while(len>0) 
        {
            while(direct_read<direct_entries) // If there's data left to read from the direct blocks.
            {
                seek=the_node->getIth_directBlock(direct_read)*block_size; // Calculate the seek of the i'th direct block.
                if(len>=block_size) // If the length requested to read is more than block-size, then read an entire block.
                {
                    to_read=block_size;
                }
                else // Else, read what's left of requested len.
                {
                    to_read=len;
                    copy = (char*)malloc(sizeof(char)*len);
                    assert(copy);
                }
                lseek(sim_disk_fd->_fileno,seek,SEEK_SET);
                already_read+=read(sim_disk_fd->_fileno,copy,to_read);
                strcat(buf,copy); // Append what is read to the buf.
                direct_read++;
                len-=to_read;
                if(len==0) break;
            }
            if(len>0)
            {
                while(single_read<block_size) // If there're more single indirect data blocks to read from.
                {
                    seek = (singleBlock*block_size)+single_read; // Calculate the seek to a single indirect block, then read the correct data block from the disk.
                    lseek(sim_disk_fd->_fileno,seek,SEEK_SET);
                    if(read(sim_disk_fd->_fileno,singleRead,sizeof(char))==-1)
                    {
                        cout << "ERROR: Could not read." << endl;
                    }
                    
                    temp = (int)singleRead[0];
                    seek = temp*block_size; // Calculate the seek to single indirect data block to read from.
                    if(len>=block_size)
                    {
                        to_read=block_size;
                    }
                    else
                    {
                        to_read=len;
                        copy = (char*)malloc(sizeof(char)*len);
                        assert(copy);
                    }
                    lseek(sim_disk_fd->_fileno,seek,SEEK_SET);
                    already_read+=read(sim_disk_fd->_fileno,copy,to_read);
                    strcat(buf,copy);
                    single_read++;
                    len-=to_read;
                    if(len==0) break;
                }
            }
            
        }  
        return already_read; 

    }
   
    ///
};

int main()
{
    int blockSize;
    int direct_entries;
    string fileName;
    char str_to_write[DISK_SIZE];
    char str_to_read[DISK_SIZE];
    int size_to_read;
    int _fd;
    

    fsDisk *fs = new fsDisk();
    int cmd_;
    while (1)
    {
        cin >> cmd_;
        switch (cmd_)
        {
        case 0: // exit
            delete fs;
            exit(0);
            break;

        case 1: // list-file
            fs->listAll();
            break;

        case 2: // format
            cin >> blockSize;
            cin >> direct_entries;
            fs->fsFormat(blockSize, direct_entries);
            break;

        case 3: // creat-file
            cin >> fileName;
            _fd = fs->CreateFile(fileName);
            cout << "CreateFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 4: // open-file
            cin >> fileName;
            _fd = fs->OpenFile(fileName);
            if (_fd != -1)
                cout << "OpenFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 5: // close-file
            cin >> _fd;
            fileName = fs->CloseFile(_fd);
            cout << "CloseFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 6: // write-file
            cin >> _fd;
            cin >> str_to_write;
            fs->WriteToFile(_fd, str_to_write, strlen(str_to_write));
            break;

        case 7: // read-file
            cin >> _fd;
            cin >> size_to_read;
            fs->ReadFromFile(_fd, str_to_read, size_to_read);
            cout << "ReadFromFile: " << str_to_read << endl;
            break;

        case 8: // delete file
            cin >> fileName;
            _fd = fs->DelFile(fileName);
            cout << "DeletedFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;
        default:
            break;
        }
    }
}
