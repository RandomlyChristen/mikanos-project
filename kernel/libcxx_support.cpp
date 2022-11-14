#include <new>
#include <cerrno>
#include <exception>

std::new_handler std::get_new_handler() noexcept {
    return nullptr;
}

extern "C" int posix_memalign(void**, size_t, size_t) {
    return ENOMEM;
}

std::exception::~exception() {}
const char* std::exception::what() const noexcept {
    return "";
}