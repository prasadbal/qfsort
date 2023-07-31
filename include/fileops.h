#pragma once
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WINDOWS)
#include <io.h>
#else
static const int	O_SEQUENTIAL = 0;
static const int	O_BINARY=0;
#include <unistd.h>
#endif


namespace fileops {
#if defined(_WIN32)
static const auto DefaultMode = _S_IREAD | _S_IWRITE;

template<typename ...T>
auto open(T&& ...args) {
    return _open(args...);
}

template<typename ...T>
auto lseek(T&& ...args) {
    return _lseeki64(args...);
}

template<typename ...T>
auto close(T&& ...args) {
    return _close(args...);
}

template<typename ...T>
auto read(T&& ...args) {
    return _read(args...);
}

template<typename ...T>
auto write(T&& ...args) {
    return _write(args...);
}

#else
static const auto DefaultMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

template<typename ...T>
auto open(T&& ...args) {
    return ::open(args...);
}

template<typename ...T>
auto lseek(T&& ...args) {
    return ::lseek(args...);
}

template<typename ...T>
auto close(T&& ...args) {
    return ::close(args...);
}

template<typename ...T>
auto read(T&& ...args) {
    return ::read(args...);
}

template<typename ...T>
auto write(T&& ...args) {
    return ::write(args...);
}

#endif

}
