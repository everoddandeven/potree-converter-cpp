#include "brotli_utils.h"
#include "brotli/encode.h"
#include <string>
#include <unordered_map>

using namespace potree;

static const int COMPRESSION_QUALITY = 6;
static const int LGWIN = BROTLI_DEFAULT_WINDOW;
static const BrotliEncoderMode ENCODER_MODE = BROTLI_DEFAULT_MODE;
static const size_t FACTOR = 1.5 * 1'000;

struct morton_code {
	uint64_t m_lower;
	uint64_t m_upper;
	uint64_t m_whatever;
	uint64_t m_index;

  static bool compare(const morton_code& a, const morton_code& b) {
    if (a.m_upper == b.m_upper) return a.m_lower < b.m_lower;
    return a.m_upper < b.m_upper;
  }

  static void sort(std::vector<morton_code>& codes) {
    std::sort(codes.begin(), codes.end(), compare);
  }
};

struct morton_compressor {
public:
  std::unordered_map<std::string, std::shared_ptr<potree::buffer>> m_buffers;
  std::vector<morton_code> m_codes;

  std::shared_ptr<potree::buffer> compress() {
    auto buffer = get_merged_buffer();
    uint8_t* input_buffer = buffer->data_u8;
    size_t input_size = buffer->size;
    size_t encoded_size = input_size * FACTOR;
    auto out_buffer = std::make_shared<potree::buffer>(encoded_size);
    uint8_t* encoded_buffer = out_buffer->data_u8;

    BROTLI_BOOL success = BROTLI_FALSE;

    for(int i = 0; i < 5; i++) {
      success = BrotliEncoderCompress(COMPRESSION_QUALITY, LGWIN, ENCODER_MODE, input_size, input_buffer, &encoded_size, encoded_buffer);

      if (success == BROTLI_FALSE) {
				encoded_size = (encoded_size + 1024) * 1.5;
				out_buffer = std::make_shared<potree::buffer>(encoded_size);
				encoded_buffer = out_buffer->data_u8;
        MWARNING << "reserved encoded_buffer size was too small. Trying again with size " + gen_utils::format_number(encoded_size) + "." << std::endl;
        continue;
      }

      break;
    }

    if (success == BROTLI_FALSE) {
      throw std::runtime_error("Conversion aborted: failed to compress node " + m_name);
    }

    auto out_ptr = std::make_shared<potree::buffer>(encoded_size);
    memcpy(out_ptr->data, encoded_buffer, encoded_size);
    return out_ptr;
  }

  static morton_compressor create(const std::shared_ptr<potree::node>& node, const attributes& attrs) {
    morton_compressor compr;
    int64_t num_points = node->numPoints;
    uint8_t* source = node->points->data_u8;

    for(const auto& attr : attrs.list) {
      int64_t bytes = attr.size * num_points;
      auto attr_offset = attrs.getOffset(attr.name);

      if (attr.is_rgb()) {
        auto mc_buffer = std::make_shared<potree::buffer>(8 * num_points);

        for(int64_t i = 0; i < num_points; i++) {
          int64_t point_offset = i * attrs.bytes;
          
          int16_t r, g, b;
          memcpy(&r, source + point_offset + attr_offset + 0, 2);
          memcpy(&g, source + point_offset + attr_offset + 2, 2);
          memcpy(&b, source + point_offset + attr_offset + 4, 2);

          auto mcoded = gen_utils::morton_encode(r, g, b);
          mc_buffer->write(&mcoded, 8);
        }

        compr.m_buffers["rgb_morton"] = mc_buffer;
      }
      else if (attr.is_position()) {
        std::vector<int32_t_point> pts;
        int32_t_point min;
        min.x = min.y = min.z = std::numeric_limits<int64_t>::max();

        for(int64_t i = 0; i < num_points; i++) {
          // MORTON
          int64_t pointOffset = i * attrs.bytes;

          int32_t XYZ[3];
          memcpy(XYZ, source + pointOffset + attr_offset, 12);

          int32_t_point pt;
          pt.x = XYZ[0];
          pt.y = XYZ[1];
          pt.z = XYZ[2];

          min.x = std::min(min.x, pt.x);
          min.y = std::min(min.y, pt.y);
          min.z = std::min(min.z, pt.z);

          pts.push_back(pt);
        }

        int64_t i = 0;

        for(auto& p : pts) {
          uint32_t mx = p.x - min.x;
          uint32_t my = p.y - min.y;
          uint32_t mz = p.z - min.z;

          uint32_t mx_l = (mx & 0x0000'ffff);
          uint32_t my_l = (my & 0x0000'ffff);
          uint32_t mz_l = (mz & 0x0000'ffff);

          uint32_t mx_h = mx >> 16;
          uint32_t my_h = my >> 16;
          uint32_t mz_h = mz >> 16;

          auto mc_l = gen_utils::morton_encode(mx_l, my_l, mz_l);
          auto mc_h = gen_utils::morton_encode(mx_h, my_h, mz_h);

          morton_code mcode;
          mcode.m_lower = mc_l;
          mcode.m_upper = mc_h;
          mcode.m_whatever = gen_utils::morton_encode(mx, my, mz);
          mcode.m_index = i;

          compr.m_codes.push_back(mcode);
          i++;
        }

        {
          auto mcbuffer = std::make_shared<potree::buffer>(16 * num_points);

          for (int i = 0; i < num_points; i++) {
            auto& mc = compr.m_codes[i];
            mcbuffer->write(&mc.m_upper, 8);
            mcbuffer->write(&mc.m_lower, 8);
          }
          
          compr.m_buffers["position_morton"] = mcbuffer;
        }

        {
          auto buffer = std::make_shared<potree::buffer>(bytes);

          for (int64_t i = 0; i < num_points; i++) {
            int64_t pointOffset = i * attrs.bytes;
            buffer->write(source + pointOffset + attr_offset, attr.size);
          }

          compr.m_buffers[attr.name] = buffer;
        }
      }
    }
  
    compr.m_name = node->name;
    compr.m_attrs = attrs;
    compr.m_num_points = num_points;
    compr.sort();

    return compr;
  }

private:
  attributes m_attrs;
  int64_t m_num_points;
  std::string m_name;

  void sort() {
    morton_code::sort(m_codes);
  }

  int64_t get_buffer_size() {
    int64_t buffer_size = 0;

    for(const auto& attr : m_attrs.list) {
      std::string name = attr.get_morton_name();
      auto& buffer = m_buffers[name];
      buffer_size += buffer->size;
    }

    return buffer_size;
  }

  std::shared_ptr<potree::buffer> get_merged_buffer() {
    int64_t buffer_size = get_buffer_size();

    auto merged_buffer = std::make_shared<potree::buffer>(buffer_size);

    for(const auto& attr : m_attrs.list) {
      auto& buffer = m_buffers[attr.get_morton_name()];
      int64_t buffer_attr_size = buffer->size / m_num_points;

      for(int i = 0; i < m_num_points; i++) {
        int src_idx = m_codes[i].m_index;
        merged_buffer->write(buffer->data_u8 + src_idx * buffer_attr_size, buffer_attr_size);
      }
    }

    return merged_buffer;
  }

};

std::shared_ptr<potree::buffer> brotli_utils::compress(const std::shared_ptr<potree::node>& node, const attributes& attrs) {
  auto num_points = node->numPoints;
  auto compr = morton_compressor::create(node, attrs);
  return compr.compress();
}
