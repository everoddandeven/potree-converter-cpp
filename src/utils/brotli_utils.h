#pragma once

#include "common/node.h"

namespace potree {
namespace brotli_utils {

  std::shared_ptr<potree::buffer> compress(const std::shared_ptr<potree::node>& node, const attributes& attrs);

}
}