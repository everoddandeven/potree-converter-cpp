#include <filesystem>
#include "common/node.h"
#include "chunk_utils.h"
#include "gen_utils.h"
#include "utils/task.h"
#include "utils/file_utils.h"
#include "utils/attribute_utils.h"
#include "utils/string_utils.h"

using namespace potree;

static const int64_t MAX_POINTS_PER_CHUNK = 10'000'000;

struct refine_task : public task {
  int64_t start = 0;
  int64_t size = 0;
  int64_t numPoints = 0;
};

std::shared_ptr<chunks> chunk_utils::load_chunks(const std::string& path_in) {
  gen_utils::profiler pr("chunk_utils::load_chunks");

  std::string chunkDirectory = path_in + "/chunks";
  json js = file_utils::read_json(chunkDirectory + "/metadata.json");

  vector3 min = {
    js["min"][0].get<double>(),
    js["min"][1].get<double>(),
    js["min"][2].get<double>()
  };

  vector3 max = {
    js["max"][0].get<double>(),
    js["max"][1].get<double>(),
    js["max"][2].get<double>()
  };

  std::vector<attribute> attributeList;
  auto jsAttributes = js["attributes"];
  for (auto jsAttribute : jsAttributes) {
    std::string name = jsAttribute["name"];
    std::string description = jsAttribute["description"];
    int size = jsAttribute["size"];
    int numElements = jsAttribute["numElements"];
    int elementSize = jsAttribute["elementSize"];

    attribute_type type = attribute_utils::get_type(jsAttribute["type"]);
    attribute attribute(name, size, numElements, elementSize, type);

    attributeList.push_back(attribute);
  }

  double scaleX = js["scale"][0];
  double scaleY = js["scale"][1];
  double scaleZ = js["scale"][2];

  double offsetX = js["offset"][0];
  double offsetY = js["offset"][1];
  double offsetZ = js["offset"][2];

  attributes attrs(attributeList);
  attrs.posScale = { scaleX, scaleY, scaleZ };
  attrs.posOffset = { offsetX, offsetY, offsetZ };


  auto toID = [](std::string filename) -> std::string {
    std::string strID = string_utils::replace(filename, "chunk_", "");
    return string_utils::replace(strID, ".bin", "");
  };

  std::vector<std::shared_ptr<potree::chunk>> chunksToLoad;
  for (const auto& entry : std::filesystem::directory_iterator(chunkDirectory)) {
    std::string filename = entry.path().filename().string();
    std::string chunkID = toID(filename);

    if (!string_utils::iends_with(filename, ".bin")) {
      continue;
    }

    auto chunk = std::make_shared<potree::chunk>();
    chunk->m_file = entry.path().string();
    chunk->m_id = chunkID;

    bounding_box box = { min, max };

    for (int i = 1; i < chunkID.size(); i++) {
      // this feels so wrong...
      int index = chunkID[i] - '0';
      box = box.child_of(index);
    }

    chunk->min = box.min;
    chunk->max = box.max;

    chunksToLoad.push_back(chunk);
  }

  auto chunks = std::make_shared<potree::chunks>(chunksToLoad, min, max);
  chunks->m_attributes = attrs;

  return chunks;
}

void chunk_utils::refine_chunk(const std::shared_ptr<chunk>& chunk, const attributes& attrs) {
  gen_utils::profiler pr("chunk_utils::refine_chunk");
  MINFO << "Refining large chunk file: " << chunk->m_file << std::endl;

  std::vector<std::vector<uint8_t>> chunk_parts;

  int64_t grid_size = 128;
  std::vector<std::atomic_int32_t> counters(grid_size * grid_size * grid_size);

  task_pool pool(16, [&chunk, &chunk_parts, &attrs, &counters, grid_size](std::shared_ptr<task> t) {
    auto task = std::static_pointer_cast<refine_task>(t);
    std::vector<uint8_t> points = file_utils::read_binary(chunk->m_file, task->start, task->size);

    auto min = chunk->min;
    auto size = chunk->max - min;
    auto scale = attrs.posScale;
    auto offset = attrs.posOffset;
    auto bpp = attrs.bytes;

    auto gridIndexOf = [&points, bpp, scale, offset, min, size, grid_size](int64_t pointIndex) {

      int64_t pointOffset = pointIndex * bpp;
      int32_t* xyz = reinterpret_cast<int32_t*>(&points[0] + pointOffset);

      double x = (xyz[0] * scale.x) + offset.x;
      double y = (xyz[1] * scale.y) + offset.y;
      double z = (xyz[2] * scale.z) + offset.z;

      int64_t ix = double(grid_size) * (x - min.x) / size.x;
      int64_t iy = double(grid_size) * (y - min.y) / size.y;
      int64_t iz = double(grid_size) * (z - min.z) / size.z;

      ix = std::max(0ll, std::min(ix, grid_size - 1));
      iy = std::max(0ll, std::min(iy, grid_size - 1));
      iz = std::max(0ll, std::min(iz, grid_size - 1));

      int64_t index = gen_utils::morton_encode(iz, iy, ix);

      return index;
    };

    for(int64_t i = 0; i < task->numPoints; i++){
      auto index = gridIndexOf(i);
      counters[index]++;
    }

    chunk_parts.push_back(std::move(points));
  }); 
}

void chunk_utils::refine(const std::string& target_dir, const status& state) {
  gen_utils::profiler pr("chunk_utils::refine");
  
  auto chunks = load_chunks(target_dir);
  int bytes = chunks->m_attributes.bytes;
  int64_t max_file_size = MAX_POINTS_PER_CHUNK * bytes;

  std::vector<std::shared_ptr<chunk>> too_large_chunks;

  for(auto& chunk : chunks->m_list) {
    auto file_size = std::filesystem::file_size(chunk->m_file);
    if (file_size > max_file_size) too_large_chunks.push_back(chunk);
  }

  for(auto& chunk : too_large_chunks) {
    refine_chunk(chunk, chunks->m_attributes);
  }
}
