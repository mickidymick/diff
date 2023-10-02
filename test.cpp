#include <cstdlib>

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "xdiff/xdiff.h"

class XDiffAllocator
    : public memallocator_t
{
    public:

        XDiffAllocator()
        {
            this->priv = this;
            this->malloc = &allocate;
            this->free = &release;
            this->realloc = &reallocate;
        }

    private:
        static void * allocate(void * _this, unsigned size)
        {
            (void)_this;
            return std::malloc(size);
        }

        static void release(void * _this, void * ptr)
        {
            (void)_this;
            std::free(ptr);
        }

        static void * reallocate(void * _this, void * ptr, unsigned size)
        {
            (void)_this;
            return std::realloc(ptr, size);
        }
};

class MMFile
    : public mmfile_t
{
    public:
        MMFile(long bsize, unsigned long flags)
        {
            if (xdl_init_mmfile(this, bsize, flags) == -1)
                throw std::runtime_error("xdl_init_mmfile");
        }

        ~MMFile()
        {
            xdl_free_mmfile(this);
        }

        void load(std::string const & filename)
        {
            std::ifstream f(filename, std::ios::binary);
            if (!f.is_open())
                throw std::runtime_error("std::ifstream::open");

            f.seekg(0, std::ios::end);
            long size = static_cast<long>(f.tellg());
            f.seekg(0);

            char * buffer = static_cast<char *>(xdl_mmfile_writeallocate(this, size));
            f.read(buffer, size);
        }

        void diff(MMFile & fold, MMFile & fnew)
        {
            bdiffparam_t params;
            params.bsize = 32;

            xdemitcb_t cb;
            cb.priv = this;
            cb.outf = &MMFile::append_buffers;

            if (xdl_bdiff(&fold, &fnew, &params, &cb) == -1)
                throw std::runtime_error("xdl_bdfiff");
        }

        void store(std::string const & filename)
        {
            std::ofstream f(filename, std::ios::binary);
            if (!f.is_open())
                throw std::runtime_error("std::ofstream::open");

            long size;
            char * buffer = static_cast<char *>(xdl_mmfile_first(this, &size));
            while (buffer)
            {
                f.write(buffer, size);
                buffer = static_cast<char *>(xdl_mmfile_next(this, &size));
            }
        }

        int append_buffers(mmbuffer_t * buffers, int count)
        {
            xdl_writem_mmfile(this, buffers, count);
            return 0;
        }

    private:
        static int append_buffers(void * _this, mmbuffer_t * buffers, int count)
        {
            return static_cast<MMFile *>(_this)->append_buffers(buffers, count);
        }
};

int main(int argc, char ** argv)
try
{
    // check arguments
    if (argc != 4)
    {
        std::cerr << "Usage: \"" << argv[0] << "\" [old] [new] [patch]\n";
        return 1;
    }

    std::string name_old = argv[1];
    std::string name_new = argv[2];
    std::string name_patch = argv[3];

    // create the allocator for libxdiff
//     XDiffAllocator allocator;
//     xdl_set_allocator(&allocator);

    MMFile file_old(1024, XDL_MMF_ATOMIC);
    file_old.load(name_old);

    MMFile file_new(1024, XDL_MMF_ATOMIC);
    file_new.load(name_new);

    MMFile file_patch(1024, XDL_MMF_ATOMIC);
    file_patch.diff(file_old, file_new);
    file_patch.store(name_patch);

    return 0;
}
catch (std::exception const & ex)
{
    std::cerr << "An exception was thrown: " << ex.what() << "\n";
    return 2;
}
