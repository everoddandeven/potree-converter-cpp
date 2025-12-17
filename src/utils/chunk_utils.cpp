#include <filesystem>
#include "common/node.h"
#include "chunk_utils.h"
#include "utils/task.h"
#include "utils/file_utils.h"
#include "utils/attribute_utils.h"
#include "utils/string_utils.h"
#include "las_utils.h"

using namespace potree;

static const int64_t MAX_POINTS_PER_CHUNK = 10'000'000;

void write_metadata(const std::string& path, const vector3& min, const vector3& max, const attributes& attrs) {
	json js;

	js["min"] = { min.x, min.y, min.z };
	js["max"] = { max.x, max.y, max.z };

	js["attributes"] = {};
	for (const auto& attribute : attrs.list) {

		json js_attr;
		js_attr["name"] = attribute.name;
		js_attr["size"] = attribute.size;
		js_attr["numElements"] = attribute.numElements;
		js_attr["elementSize"] = attribute.elementSize;
		js_attr["description"] = attribute.description;
		js_attr["type"] = attribute_utils::get_name(attribute.type);

		if (attribute.numElements == 1) {
			js_attr["min"] = std::vector<double>{ attribute.min.x };
			js_attr["max"] = std::vector<double>{ attribute.max.x };
			js_attr["scale"] = std::vector<double>{ attribute.scale.x };
			js_attr["offset"] = std::vector<double>{ attribute.offset.x };
		} else if (attribute.numElements == 2) {
			js_attr["min"] = std::vector<double>{ attribute.min.x, attribute.min.y};
			js_attr["max"] = std::vector<double>{ attribute.max.x, attribute.max.y};
			js_attr["scale"] = std::vector<double>{ attribute.scale.x, attribute.scale.y};
			js_attr["offset"] = std::vector<double>{ attribute.offset.x, attribute.offset.y};
		} else if (attribute.numElements == 3) {
			js_attr["min"] = std::vector<double>{ attribute.min.x, attribute.min.y, attribute.min.z };
			js_attr["max"] = std::vector<double>{ attribute.max.x, attribute.max.y, attribute.max.z };
			js_attr["scale"] = std::vector<double>{ attribute.scale.x, attribute.scale.y, attribute.scale.z };
			js_attr["offset"] = std::vector<double>{ attribute.offset.x, attribute.offset.y, attribute.offset.z };
		}

		bool emptyHistogram = true;
		for(int i = 0; i < attribute.histogram.size(); i++){
			if(attribute.histogram[i] != 0){
				emptyHistogram = false;
			}
		}

		if(attribute.size == 1 && !emptyHistogram){
			json jsHistogram = attribute.histogram;

			js_attr["histogram"] = jsHistogram;
		}

		js["attributes"].push_back(js_attr);
	}

	js["scale"] = std::vector<double>({
		attrs.posScale.x, 
		attrs.posScale.y, 
		attrs.posScale.z});

	js["offset"] = std::vector<double>({
		attrs.posOffset.x,
		attrs.posOffset.y,
		attrs.posOffset.z });

	string content = js.dump(4);

	file_utils::write_text(path, content);
}

void for_xyz(int64_t size, std::function<void(int64_t, int64_t, int64_t)> callback) {
  for(int64_t x = 0; x < size; x++) {
    for(int64_t y = 0; y < size; y++) {
      for(int64_t z = 0; z < size; z++) {
        callback(x, y, z);
      }
    }
  }
}

struct refine_task : public task {
  int64_t start = 0;
  int64_t size = 0;
  int64_t numPoints = 0;
};

struct grid_info {
  int64_t m_high;
  int64_t m_low;
  int64_t m_size_low;
  int64_t m_size_high;
  int64_t m_level_low;
  int64_t m_level_max;
  int64_t m_level_high;
  std::vector<int64_t> m_grid_high;
  std::vector<int64_t> m_grid_low;
  int64_t m_max_points_per_chunk = 5'000'000;
  std::vector<potree::node> m_nodes;
};

void process_grid(grid_info& info, int64_t x, int64_t y, int64_t z) {
  int64_t size_high = info.m_size_high;
  int64_t idx_low = x + y * info.m_size_low + z * info.m_size_low * info.m_size_low;
  int64_t sum = 0;
  int64_t max = 0;
  bool mergeable = true;
 
  // loop through the 8 enclosed cells of the higher detailed grid
  for(int64_t j = 0; j < 8; j++) {
    int64_t ox = (j & 0b100) >> 2;
    int64_t oy = (j & 0b010) >> 1;
    int64_t oz = (j & 0b001) >> 0;

    int64_t nx = 2 * x + ox;
    int64_t ny = 2 * y + oy;
    int64_t nz = 2 * z + oz;
    int64_t index_high = nx + ny * size_high + nz * size_high * size_high;

    int64_t value = info.m_grid_high[index_high];

    if (value == -1) {
      mergeable = false;
    } else {
      sum += value;
    }

    max = std::max(max, value);
  }

  if (!mergeable || sum > info.m_max_points_per_chunk) {
    // finished chunks
    for (int64_t j = 0; j < 8; j++) {
      int64_t ox = (j & 0b100) >> 2;
      int64_t oy = (j & 0b010) >> 1;
      int64_t oz = (j & 0b001) >> 0;
      int64_t nx = 2 * x + ox;
      int64_t ny = 2 * y + oy;
      int64_t nz = 2 * z + oz;

      int64_t index_high = nx + ny * size_high + nz * size_high * size_high;
      int64_t value = info.m_grid_high[index_high];

      if (value > 0) {
        std::string nodeID = chunk_utils::build_id(info.m_level_high, size_high, nx, ny, nz);
        potree::node node(nodeID, value);
        node.x = nx;
        node.y = ny;
        node.z = nz;
        node.size = pow(2, (info.m_level_max - info.m_level_high));
        info.m_nodes.push_back(node);
      }
    }

    // invalidate the field to show the parent that nothing can be merged with it
    info.m_grid_low[idx_low] = -1;
  }
  else {
    info.m_grid_low[idx_low] = sum;
  }
}

struct node_lookup_table {
public:
  int64_t m_grid_size;
  std::vector<int32_t> m_grid;

  static node_lookup_table create(std::vector<std::atomic_int32_t>& grid, int64_t grid_size) {
    gen_utils::profiler pr("node_lookup_table::create()");
    grid_info info;
    info.m_grid_high.reserve(grid.size());
    info.m_grid_high.insert(info.m_grid_high.begin(), grid.begin(), grid.end());
    info.m_level_max= int64_t(log2(grid_size));

		// - evaluate counting grid in "image pyramid" fashion
		// - merge smaller cells into larger ones
		// - unmergeable cells are resulting chunks; push them to "nodes" array.
    for(info.m_level_low = info.m_level_max - 1; info.m_level_low >= 0; info.m_level_low--) {
      info.m_level_high = info.m_level_low + 1;
      info.m_size_high = pow(2, info.m_level_high);
      info.m_size_low = pow(2, info.m_level_low);
      std::vector<int64_t> grid_low(info.m_size_low * info.m_size_low * info.m_size_low, 0);

      // loop through all cells of the lower detail target grid, 
      // and for each cell through the 8 enclosed cells of the higher level grid.
      for_xyz(info.m_size_low, [&info](int64_t x, int64_t y, int64_t z){
        process_grid(info, x, y, z);
      });

      info.m_grid_high = info.m_grid_low;
    }

    // - create lookup table
		// - loop through nodes, add pointers to node/chunk for all enclosed cells in LUT.
    std::vector<int32_t> lut(grid_size * grid_size * grid_size, -1);
    for(size_t i = 0; i < info.m_nodes.size(); i++) {
      auto& node = info.m_nodes[i];

      for_xyz(node.size, [&node, &lut, grid_size, i](int64_t ox, int64_t oy, int64_t oz) {
      	int64_t x = node.size * node.x + ox;
				int64_t y = node.size * node.y + oy;
				int64_t z = node.size * node.z + oz;
				int64_t index = x + y * grid_size + z * grid_size * grid_size;
				lut[index] = i;
      });
    }

    return {grid_size, lut};
  }

};

struct distribution_task : public task {
  std::string path;
  int64_t maxBatchSize;
  int64_t batchSize;
  int64_t firstPoint;
  node_lookup_table* lut;
  vector3 scale;
  vector3 offset;
  vector3 min;
  vector3 max;
  attributes inputAttributes;
};

struct point_distributor {
public:
  std::vector<file_source> m_sources;
  vector3 m_min;
  vector3 m_max;
  std::string m_target_dir;
  node_lookup_table m_lut;
  std::shared_ptr<status> m_state;
  attributes m_out_attributes;
  std::shared_ptr<gen_utils::monitor> m_monitor;
  // end params
  std::vector<potree::node> m_nodes;
  std::unique_ptr<task_pool> m_pool;

  void distribute() {
    gen_utils::profiler pr("point_distributor::distribute()");
    size_t num_processors = gen_utils::get_num_processors();
    m_state->pointsProcessed = 0;
    m_state->bytesProcessed = 0;
    m_state->duration = 0;
    auto writer = std::make_shared<concurrent_writer>(num_processors, m_state);
    init_processor();
    m_pool = std::make_unique<task_pool>(num_processors, m_processor);
    process_sources();
    m_pool->close();
    writer->join();
  }

private:
  std::function<void(std::shared_ptr<task>)> m_processor;

  void init_processor() {
    throw std::runtime_error("not implemented");
  }

  void process_sources() {
		for (auto& source : m_sources) {

			laszip_POINTER laszip_reader;
			laszip_header* header;
			{
				laszip_BOOL request_reader = 1;
				laszip_BOOL is_compressed = string_utils::iends_with(source.path, ".laz") ? 1 : 0;

				laszip_create(&laszip_reader);
				laszip_request_compatibility_mode(laszip_reader, request_reader);
				laszip_open_reader(laszip_reader, source.path.c_str(), &is_compressed);
				laszip_get_header_pointer(laszip_reader, &header);
			}

			int64_t numPoints = std::max(uint64_t(header->number_of_point_records), header->extended_number_of_point_records);
			int64_t pointsLeft = numPoints;
			int64_t maxBatchSize = 1'000'000;
			int64_t numRead = 0;

			std::vector<file_source> tmpSources = { source };
      std::vector<std::string> tmpAttr;
			attributes inputAttributes = las_utils::compute_output_attributes(tmpSources, tmpAttr);

			while (pointsLeft > 0) {

				int64_t numToRead;
				if (pointsLeft < maxBatchSize) {
					numToRead = pointsLeft;
					pointsLeft = 0;
				} else {
					numToRead = maxBatchSize;
					pointsLeft = pointsLeft - maxBatchSize;
				}

				auto task = std::make_shared<distribution_task>();
				task->maxBatchSize = maxBatchSize;
				task->batchSize = numToRead;
				task->lut = &m_lut;
				task->firstPoint = numRead;
				task->path = source.path;
				//task->scale = { header->x_scale_factor, header->y_scale_factor, header->z_scale_factor };
				task->scale = m_out_attributes.posScale;
				task->offset = m_out_attributes.posOffset;
				task->min = m_min;
				task->max = m_max;
				task->inputAttributes = inputAttributes;

				m_pool->add(task);

				numRead += numToRead;
			}

			laszip_close_reader(laszip_reader);
			laszip_destroy(laszip_reader);

		}
  }
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

std::string chunk_utils::build_id(int level, int grid_size, int64_t x, int64_t y, int64_t z) {
  std::string id = "r";

  int currentGridSize = grid_size;
  int lx = x;
  int ly = y;
  int lz = z;

  for (int i = 0; i < level; i++) {

    int index = 0;

    if (lx >= currentGridSize / 2) {
      index = index + 0b100;
      lx = lx - currentGridSize / 2;
    }

    if (ly >= currentGridSize / 2) {
      index = index + 0b010;
      ly = ly - currentGridSize / 2;
    }

    if (lz >= currentGridSize / 2) {
      index = index + 0b001;
      lz = lz - currentGridSize / 2;
    }

    id = id + std::to_string(index);
    currentGridSize = currentGridSize / 2;
  }

  return id;
}

void chunk_utils::add_buckets(const std::vector<potree::node>& nodes, const std::vector<std::shared_ptr<potree::buffer>>& buckets, const std::shared_ptr<concurrent_writer>& writer, const std::string& target_dir) {
  if (writer == nullptr) throw std::runtime_error("Cannot add new buckets: concurrent writer is null");
  
  for(int nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++){

    if (buckets[nodeIndex]->size == 0) {
      continue;
    }

    auto& node = nodes[nodeIndex];
    std::string path = target_dir + "/chunks/" + node.id + ".bin";
    auto buffer = buckets[nodeIndex];

    writer->write(path, buffer);
  }

}

void chunk_utils::chunker::do_chunking(const std::vector<file_source>& sources, const std::string& target_dir, const vector3& min, const vector3& max, const std::shared_ptr<status>& state, attributes& out_attrs, const std::shared_ptr<gen_utils::monitor>& monitor) {
 gen_utils::profiler pr("chunker::do_chunking()");

 int64_t tmp = state->pointsTotal / 20;
 int grid_size = 512; // default
  if (state->pointsTotal < 100'000'000) {
    grid_size = 128;
  }
  else if (state->pointsTotal < 500'000'000) {
    grid_size = 256;
  }

  state->currentPass = 1;

  { // prepare/clean target directories
    string dir = target_dir + "/chunks";
    std::filesystem::create_directories(dir);

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      std::filesystem::remove(entry);
    }
  }

  las_utils::cell_point_counter pt_ctr(sources, min, max, grid_size, state, out_attrs, monitor);
  auto grid = pt_ctr.count();

  {
    node_lookup_table lut = node_lookup_table::create(grid, grid_size);
    state->currentPass = 2;
    point_distributor pt_dtr;
    pt_dtr.m_sources = sources;
    pt_dtr.m_min = min;
    pt_dtr.m_max = max;
    pt_dtr.m_target_dir = target_dir;
    pt_dtr.m_lut = lut;
    pt_dtr.m_state = state;
    pt_dtr.m_out_attributes = out_attrs;
    pt_dtr.m_monitor = monitor;

    // distribute points
    pt_dtr.distribute();
  }

  std::string metadataPath = target_dir + "/chunks/metadata.json";
  double cubeSize = (max - min).max();
  vector3 size = { cubeSize, cubeSize, cubeSize };
  write_metadata(metadataPath, min, min + cubeSize, out_attrs);
}
