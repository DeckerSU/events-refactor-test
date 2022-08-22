#ifndef EVENTS_TESTS_H
#define EVENTS_TESTS_H

#include <string>
#include <stdexcept>

namespace komodo {
     /***
     * Thrown by event constructors when it finds a problem with the input data
     */
    class parse_error : public std::logic_error
    {
    public:
        parse_error(const std::string& in) : std::logic_error(in) {}
    };
}

#endif // EVENTS_TESTS_H
