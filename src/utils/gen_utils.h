#pragma once
#include <limits>
#include <string>
#include <locale>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include "common/memory_data.h"
#include "common/cpu_data.h"

#define MINFO std::cout << "INFO: "
#define MERROR std::cout << "ERROR(" << __FILE__ << ":" << __LINE__ << "): "
#define MWARNING std::cout << "WARNING: "

namespace potree {
namespace gen_utils {

  static const double INF = std::numeric_limits<double>::infinity();

  class punct_facet : public std::numpunct<char> {
  protected:
    virtual char do_decimal_point() const override { return '.'; };
    virtual char do_thousands_sep() const override { return '\''; };
    virtual std::string do_grouping() const override { return "\3"; }

    virtual std::string do_truename() const override { return "true"; }
    virtual std::string do_falsename() const override { return "false"; }
  };

  template<class T>
  static inline std::string format_number(T number, int decimals = 0) {
    std::stringstream ss;
    ss.imbue(std::locale(std::locale(), new punct_facet));
    ss << std::fixed;
    ss << std::setprecision(decimals);
    ss << number;
    return ss.str();
  };

  template<typename T>
  static inline T read_value(std::vector<uint8_t>& buffer, int offset) {
    T value;
    memcpy(&value, buffer.data() + offset, sizeof(T));
    return value;
  };
  
  uint64_t split_by_3(unsigned int a);
  uint64_t morton_encode(unsigned int x, unsigned int y, unsigned int z);

  memory_data get_memory_data();
  cpu_data get_cpu_data();
}
}