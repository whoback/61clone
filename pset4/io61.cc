#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.c
//    YOUR CODE HERE!

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file
{
    int fd;
    int mode;
    static constexpr off_t bufsize = 4096;
    unsigned char cbuf[bufsize];
    off_t tag;
    off_t end_tag;
    off_t pos_tag;
};

// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file *io61_fdopen(int fd, int mode)
{
    assert(fd >= 0);
    io61_file *f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}

// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file *f)
{
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}

// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    unsigned char buf[1];
    size_t pos = 0;
    size_t ch = 0;
    if(f->pos_tag == f->end_tag)
    {
        io61_fill(f);
        if(f->pos_tag == f->end_tag)
        {
            return -1;
        }
    }
    ch = 1;
    buf[0] = f->cbuf[f->pos_tag - f->tag];
    f->pos_tag += ch;
    pos += ch;
    
    return buf[0];
}
// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

ssize_t io61_read(io61_file *f, char *buf, size_t sz)
{
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);
    size_t nread = 0;
    while (nread != sz)
    {
        if (f->pos_tag == f->end_tag)
        {
            io61_fill(f);

            if (f->pos_tag == f->end_tag)
            {
                break;
            }
        }

        memcpy(buf, &f->cbuf[f->pos_tag - f->tag], sz);
        ++f->pos_tag;
        ++nread;
    }
    return nread;

    // Note: This function never returns -1 because `io61_readc`
    // does not distinguish between error and end-of-file.
    // Your final version should return -1 if a system call indicates
    // an error.
}

void io61_fill(io61_file *f)
{
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);
    f->tag = f->pos_tag = f->end_tag;

    ssize_t n = read(f->fd, f->cbuf, f->bufsize);
    if (n >= 0)
    {
        f->end_tag = f->tag + n;
    }


    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);
}

// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file *f, int ch)
{
    if (f->end_tag == f->tag + f->bufsize) {
        io61_flush(f);
    }
    size_t count = 1;

    f->cbuf[f->pos_tag - f->tag] = ch;
    f->pos_tag += count;
    f->end_tag += count;
    return 0;
}

// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    // Write cache invariant.
    assert(f->pos_tag == f->end_tag);

    /* ANSWER */
    size_t pos = 0;
    size_t ch = 0;

    while (pos < sz) {
        if (f->end_tag == f->tag + f->bufsize) {
            io61_flush(f);
        }
        if((off_t)(sz - pos) < f->bufsize - f->pos_tag + f->tag)
        {
            ch = sz - pos;
        }
        else
        {
            ch = f->bufsize - f->pos_tag + f->tag;
        }
        

        memcpy(&f->cbuf[f->pos_tag - f->tag], buf, ch);
        f->pos_tag += ch;
        f->end_tag += ch;
        pos += ch;
        buf += ch;
    }
    return pos;
}

// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    assert(f->pos_tag == f->end_tag);
    if(f->mode == O_RDONLY)
    {
        return 0;
    }
    ssize_t n = write(f->fd, f->cbuf, f->pos_tag - f->tag);
    assert((size_t) n == f->pos_tag - f->tag);
    f->tag = f->pos_tag;
    return 0;
}
// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file *f, off_t pos)
{
    off_t r = lseek(f->fd, (off_t)pos, SEEK_SET);
    if (r == (off_t)pos)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file *io61_open_check(const char *filename, int mode)
{
    int fd;
    if (filename)
    {
        fd = open(filename, mode, 0666);
    }
    else if ((mode & O_ACCMODE) == O_RDONLY)
    {
        fd = STDIN_FILENO;
    }
    else
    {
        fd = STDOUT_FILENO;
    }
    if (fd < 0)
    {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}

// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file *f)
{
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode))
    {
        return s.st_size;
    }
    else
    {
        return -1;
    }
}
