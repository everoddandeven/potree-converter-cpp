#include "common/scale_offset.h"
#include "common/point.h"
#include "common/bounding_box.h"
#include "common/node.h"
#include "common/file_source.h"
#include "las/las_info.h"
#include "las_utils.h"
#include "gen_utils.h"
#include "string_utils.h"
#include "file_utils.h"
#include "attribute_utils.h"
#include <unordered_map>
#include <execution>
#include <filesystem>

using namespace potree;

static const size_t NUM_PROCESSORS = gen_utils::get_num_processors();

struct morton_comparer {
	vector3 m_min;
	vector3 m_max; // TODO unused variable

	morton_comparer(const vector3& min, const vector3& max) {
		m_min = min;
		m_max = max;
	}

	bool operator() (const point& a, const point& b) {
		int32_t factor = 100;
		uint64_t mc1 = gen_utils::morton_encode(
			(a.x + m_min.x) * factor, 
			(a.y + m_min.y) * factor, 
			(a.z + m_min.z) * factor
		);
		uint64_t mc2 = gen_utils::morton_encode(
			(b.x + m_min.x) * factor,
			(b.y + m_min.y) * factor,
			(b.z + m_min.z) * factor
		);
		return mc1 < mc2;
	}
};

struct point_cloud_sorter {
public:
	vector3 min;
	vector3 max;
	int64_t num_points;

	void sort() {
		double coordinates[3];
		points.clear();
		points.reserve(num_points);

		// read points
		for(int64_t i = 0; i < num_points; i++) {
			laszip_read_point(reader);
			laszip_get_coordinates(reader, coordinates);
		
			colored_point p;
			p.x = coordinates[0];
			p.y = coordinates[1];
			p.z = coordinates[2];
			p.r = point->rgb[0];
			p.g = point->rgb[1];
			p.b = point->rgb[2];

			points.push_back(p);
		}

		// sort array
		std::sort(points.begin(), points.end(), morton_comparer{min, max});
	}

	void save(const std::string& target) {
		las_utils::save(header, points, target);
	}

	static point_cloud_sorter load(const std::string& path) {
		laszip_BOOL compressed = string_utils::iends_with(path, ".laz") ? 1 : 0;
		laszip_BOOL request = 1;
		laszip_POINTER reader;
		laszip_header* header;
		laszip_point* point;

		laszip_create(&reader);
		laszip_request_compatibility_mode(reader, request);
		laszip_open_reader(reader, path.c_str(), &compressed);
		laszip_get_header_pointer(reader, &header);
		laszip_get_point_pointer(reader, &point);
		
		point_cloud_sorter sorter;
		sorter.reader = reader;
		sorter.header = header;
		sorter.num_points = (header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records);
		sorter.min = {
			header->min_x,
			header->min_y,
			header->min_z
		};
		sorter.max = {
			header->max_x,
			header->max_y,
			header->max_z
		};

		return sorter;
	}

private:
	laszip_POINTER reader;
	laszip_point* point;
	laszip_header* header;
	std::vector<colored_point> points;

	point_cloud_sorter() {}

	double get_largest() const { return (max - min).max(); }

	vector3 get_size() const {
		double l = get_largest();
		return { l, l, l };
	}

	vector3 get_max() const {
		return min + get_size();
	}
};

las_utils::cell_point_counter::cell_point_counter(
	const std::vector<file_source>& sources, const vector3& min, const vector3& max,
	int64_t grid_size, const std::shared_ptr<status>& state, attributes& out_attributes, const std::shared_ptr<gen_utils::monitor>& monitor
) {
	m_sources = sources;
	m_min = min;
	m_max = max;
	m_grid_size = grid_size;		
	std::vector<std::atomic_int32_t> grid(grid_size * grid_size * grid_size);
	m_grid = std::move(grid);
	m_out_attributes = out_attributes;
	m_monitor = monitor;
	m_state = state;
	init_processor();
	m_pool = std::make_unique<task_pool>(NUM_PROCESSORS, m_processor);
}

std::vector<std::atomic_int32_t> las_utils::cell_point_counter::count() {
	gen_utils::profiler pr("cell_point_counter::count()");
	MINFO << "START COUNTING" << std::endl;

	m_t_start = gen_utils::now();
	assembly_sources();

	m_pool->wait();
	m_pool->close();

	return std::move(m_grid);
}

void las_utils::cell_point_counter::init_processor() {
	m_processor = [this](std::shared_ptr<potree::task> t) {
		auto task = std::static_pointer_cast<point_count_task>(t);
		if (task == nullptr) {
			MERROR << "cell_point_counter::count(): task is not point_count_task" << std::endl;
			return;
		}

		std::string path = task->path;
		int64_t start = task->firstByte;
		int64_t numBytes = task->numBytes;
		int64_t numToRead = task->numPoints;
		int64_t bpp = task->bpp;
		vector3 min = task->min;
		vector3 max = task->max;

		MINFO << "counting " << std::filesystem::path(task->path).filename().string() 
			<< ", first point: " << gen_utils::format_number(task->firstPoint)
			<< ", num points: " << gen_utils::format_number(task->numPoints) << std::endl;

		thread_local unique_ptr<void, void(*)(void*)> buffer(nullptr, free);
		thread_local int64_t bufferSize = -1;

		// sanity checks
		if(numBytes < 0){
			MERROR << "invalid malloc size: " + gen_utils::format_number(numBytes) << std::endl;
		}

		if (bufferSize < numBytes){
			buffer.reset(malloc(numBytes));
			bufferSize = numBytes;
		}

		laszip_POINTER reader;
		{
			laszip_BOOL is_compressed = string_utils::iends_with(path, ".laz") ? 1 : 0;
			laszip_BOOL request_reader = 1;

			laszip_create(&reader);
			laszip_request_compatibility_mode(reader, request_reader);
			laszip_open_reader(reader, path.c_str(), &is_compressed);
			laszip_seek_point(reader, task->firstPoint);
		}

		double cubeSize = (max - min).max();
		vector3 size = { cubeSize, cubeSize, cubeSize };
		max = min + cubeSize;

		double d_grid_size = double(this->m_grid_size);

		double coordinates[3];

		auto posScale = this->m_out_attributes.posScale;
		auto posOffset = this->m_out_attributes.posOffset;

		for (int i = 0; i < numToRead; i++) {
			int64_t pointOffset = i * bpp;

			laszip_read_point(reader);
			laszip_get_coordinates(reader, coordinates);

			{
				// transfer las integer coordinates to new scale/offset/box values
				double x = coordinates[0];
				double y = coordinates[1];
				double z = coordinates[2];

				int32_t X = int32_t((x - posOffset.x) / posScale.x);
				int32_t Y = int32_t((y - posOffset.y) / posScale.y);
				int32_t Z = int32_t((z - posOffset.z) / posScale.z);

				double ux = (double(X) * posScale.x + posOffset.x - min.x) / size.x;
				double uy = (double(Y) * posScale.y + posOffset.y - min.y) / size.y;
				double uz = (double(Z) * posScale.z + posOffset.z - min.z) / size.z;

				bool inBox = ux >= 0.0 && uy >= 0.0 && uz >= 0.0;
				inBox = inBox && ux <= 1.0 && uy <= 1.0 && uz <= 1.0;

				if (!inBox) {
					MERROR << "encountered point outside bounding box." << std::endl
					<< "box.min: " << min.to_string() << std::endl
					<< "box.max: " << max.to_string() << std::endl
					<< "point: " << vector3(x, y, z).to_string() << std::endl
					<< "file: " << path << std::endl
					<< "PotreeConverter requires a valid bounding box to operate." << std::endl
					<< "Please try to repair the bounding box, e.g. using lasinfo with the -repair_bb argument." << std::endl;

					throw std::runtime_error("encountered point outside bounding box");
				}

				int64_t ix = int64_t(std::min(d_grid_size * ux, d_grid_size - 1.0));
				int64_t iy = int64_t(std::min(d_grid_size * uy, d_grid_size - 1.0));
				int64_t iz = int64_t(std::min(d_grid_size * uz, d_grid_size - 1.0));

				int64_t index = ix + iy * m_grid_size + iz * m_grid_size * m_grid_size;

				m_grid[index]++;
			}

		}

		laszip_close_reader(reader);
		laszip_destroy(reader);

		static int64_t pointsProcessed = 0;
		pointsProcessed += task->numPoints;

		m_state->name = "COUNTING";
		m_state->pointsProcessed = pointsProcessed;
		m_state->duration = gen_utils::now() - m_t_start;
	};
}

void las_utils::cell_point_counter::assembly_sources() {
	gen_utils::profiler pr("cell_point_counter::assembly_sources()");
	for (const auto& source : m_sources) {
		laszip_POINTER laszip_reader;
		laszip_header* header;
		
		{
			laszip_create(&laszip_reader);
			laszip_BOOL request_reader = 1;
			laszip_BOOL is_compressed = string_utils::iends_with(source.path, ".laz") ? 1 : 0;
			laszip_request_compatibility_mode(laszip_reader, request_reader);
			laszip_open_reader(laszip_reader, source.path.c_str(), &is_compressed);
			laszip_get_header_pointer(laszip_reader, &header);
		}
		
		int64_t bpp = header->point_data_record_length;
		int64_t numPoints = std::max(uint64_t(header->number_of_point_records), header->extended_number_of_point_records);
		int64_t pointsLeft = numPoints;
		int64_t batchSize = 1'000'000;
		int64_t numRead = 0;

		while (pointsLeft > 0) {

			int64_t numToRead;
			if (pointsLeft < batchSize) {
				numToRead = pointsLeft;
				pointsLeft = 0;
			} 
			else {
				numToRead = batchSize;
				pointsLeft = pointsLeft - batchSize;
			}
			
			int64_t firstByte = header->offset_to_point_data + numRead * bpp;
			int64_t numBytes = numToRead * bpp;

			auto task = std::make_shared<point_count_task>();
			task->path = source.path;
			task->totalPoints = numPoints;
			task->firstPoint = numRead;
			task->firstByte = firstByte;
			task->numBytes = numBytes;
			task->numPoints = numToRead;
			task->bpp = header->point_data_record_length; 
			//task->scale = { header->x_scale_factor, header->y_scale_factor, header->z_scale_factor };
			//task->offset = { header->x_offset, header->y_offset, header->z_offset };
			task->min = m_min;
			task->max = m_max;

			m_pool->add(task);

			numRead += batchSize;
		}

		laszip_close_reader(laszip_reader);
		laszip_destroy(laszip_reader);
	}
}

std::vector<attribute> las_utils::parse_extra_attributes(const las_header& header) {
	std::vector<attribute> attrs;

	for (auto& vlr : header.vlrs) {
		if (vlr.record_id == 4) {
			auto extraData = vlr.data;

			constexpr int recordSize = 192;
			int numExtraAttributes = extraData.size() / recordSize;

			for (int i = 0; i < numExtraAttributes; i++) {

				int offset = i * recordSize;
				uint8_t type = gen_utils::read_value<uint8_t>(extraData, offset + 2);
				uint8_t options = gen_utils::read_value<uint8_t>(extraData, offset + 3);
				
				char chrName[32];
				memcpy(chrName, extraData.data() + offset + 4, 32);
				std::string name(chrName);

				vector3 aScale = {1.0, 1.0, 1.0};
				vector3 aOffset = {0.0, 0.0, 0.0};
				if((options & 0b01000) != 0){
					memcpy(&aScale, extraData.data() + offset + 112, 24);
				}
				if((options & 0b10000) != 0){
					memcpy(&aOffset, extraData.data() + offset + 136, 24);
				}

				char chrDescription[32];
				memcpy(chrDescription, extraData.data() + offset + 160, 32);
				std::string description(chrDescription);

				auto info = las_info::parse(type);
				std::string typeName = attribute_utils::get_name(info.type);
				int elementSize = attribute_utils::get_size(info.type);

				int size = info.num_elements * elementSize;
				attribute xyz(name, size, info.num_elements, elementSize, info.type);
				xyz.description = description;
				xyz.scale = aScale;
				xyz.offset = aOffset;

				attrs.push_back(xyz);
			}
		}
	}

	return attrs;
}

std::vector<attribute> las_utils::compute_output_attributes(const las_header& header) {
	auto format = header.pointDataFormat;

	attribute xyz("position", 12, 3, 4, attribute_type::INT32);
	attribute intensity("intensity", 2, 1, 2, attribute_type::UINT16);
	attribute returns("returns", 1, 1, 1, attribute_type::UINT8);
	attribute returnNumber("return number", 1, 1, 1, attribute_type::UINT8);
	attribute numberOfReturns("number of returns", 1, 1, 1, attribute_type::UINT8);
	attribute classification("classification", 1, 1, 1, attribute_type::UINT8);
	attribute scanAngleRank("scan angle rank", 1, 1, 1, attribute_type::UINT8);
	attribute userData("user data", 1, 1, 1, attribute_type::UINT8);
	attribute pointSourceId("point source id", 2, 1, 2, attribute_type::UINT16);
	attribute gpsTime("gps-time", 8, 1, 8, attribute_type::DOUBLE);
	attribute rgb("rgb", 6, 3, 2, attribute_type::UINT16);
	attribute wavePacketDescriptorIndex("wave packet descriptor index", 1, 1, 1, attribute_type::UINT8);
	attribute byteOffsetToWaveformData("byte offset to waveform data", 8, 1, 8, attribute_type::UINT64);
	attribute waveformPacketSize("waveform packet size", 4, 1, 4, attribute_type::UINT32);
	attribute returnPointWaveformLocation("return point waveform location", 4, 1, 4, attribute_type::FLOAT);
	attribute XYZt("XYZ(t)", 12, 3, 4, attribute_type::FLOAT);
	attribute classificationFlags("classification flags", 1, 1, 1, attribute_type::UINT8);
	attribute scanAngle("scan angle", 2, 1, 2, attribute_type::INT16);

	std::vector<attribute> list;

	if (format == 0) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId };
	} else if (format == 1) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime };
	} else if (format == 2) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, rgb };
	} else if (format == 3) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb };
	} else if (format == 4) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime,
			wavePacketDescriptorIndex, byteOffsetToWaveformData, waveformPacketSize, returnPointWaveformLocation,
			XYZt
		};
	} else if (format == 5) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classification, scanAngleRank, userData, pointSourceId, gpsTime, rgb,
			wavePacketDescriptorIndex, byteOffsetToWaveformData, waveformPacketSize, returnPointWaveformLocation,
			XYZt
		};
	} else if (format == 6) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classificationFlags, classification, userData, scanAngle, pointSourceId, gpsTime };
	} else if (format == 7) {
		list = { xyz, intensity, returnNumber, numberOfReturns, classificationFlags, classification, userData, scanAngle, pointSourceId, gpsTime, rgb };
	} else {
		throw std::runtime_error("ERROR: currently unsupported LAS format: " + int(format));
	}

	std::vector<attribute> extraAttributes = parse_extra_attributes(header);

	list.insert(list.end(), extraAttributes.begin(), extraAttributes.end());

	return list;
}

attributes las_utils::compute_output_attributes(std::vector<file_source>& sources, std::vector<std::string>& requested_attributes) {
	// TODO: a bit wasteful to iterate over source files and load headers twice
	vector3 scaleMin = { gen_utils::INF, gen_utils::INF, gen_utils::INF };
	//vector3 offset = { gen_utils::INF, gen_utils::INF, gen_utils::INF};
	vector3 min = { gen_utils::INF, gen_utils::INF, gen_utils::INF };
	vector3 max = { -gen_utils::INF, -gen_utils::INF, -gen_utils::INF };
	vector3 scale, offset;

	std::vector<attribute> fullAttributeList;
	std::unordered_map<std::string, int> acceptedAttributeNames;

	// compute scale and offset from all sources
	try {
		std::mutex mtx;
		auto parallel = std::execution::par;
		for_each(parallel, sources.begin(), sources.end(), [&mtx, &sources, &scaleMin, &min, &max, requested_attributes, &fullAttributeList, &acceptedAttributeNames](file_source source) {

			auto header = las_header::load(source.path);

			std::vector<attribute> att = compute_output_attributes(header);

			mtx.lock();

			for (auto& attribute : att) {
				bool alreadyAdded = acceptedAttributeNames.find(attribute.name) != acceptedAttributeNames.end();

				if (!alreadyAdded) {
					fullAttributeList.push_back(attribute);
					acceptedAttributeNames[attribute.name] = 1;
				}
			}

			scaleMin.x = std::min(scaleMin.x, header.scale.x);
			scaleMin.y = std::min(scaleMin.y, header.scale.y);
			scaleMin.z = std::min(scaleMin.z, header.scale.z);

			min.x = std::min(min.x, header.min.x);
			min.y = std::min(min.y, header.min.y);
			min.z = std::min(min.z, header.min.z);

			max.x = std::max(max.x, header.max.x);
			max.y = std::max(max.y, header.max.y);
			max.z = std::max(max.z, header.max.z);

			mtx.unlock();
		});

		auto scaleOffset = scale_offset::compute(min, max, scaleMin);
		scale = scaleOffset.scale;
		offset = scaleOffset.offset;

		if (scaleMin.x != scale.x || scaleMin.y != scale.y || scaleMin.z != scale.z) {
			MWARNING << "scale/offset/bounding box were adjusted. "
				<< "new scale: " << scale.to_string() << ", "
				<< "new offset: " << offset.to_string() << std::endl;
		}
	} catch (const std::exception& ex) { }

	// filter down to optionally specified attributes
	std::vector<attribute> filteredAttributeList = fullAttributeList;
	if (requested_attributes.size() > 0) {
		auto should = requested_attributes;
		auto is = fullAttributeList;

		// always add position
		should.insert(should.begin(), { "position" });

		std::vector<attribute> filtered;

		for (std::string name : should) {
			auto it = find_if(is.begin(), is.end(), [name](auto& value) {
				return value.name == name;
				});

			if (it != is.end()) {
				filtered.push_back(*it);
			}
		}

		filteredAttributeList = filtered;
	} 

	attributes attrs(filteredAttributeList);
	attrs.posScale = scale;
	attrs.posOffset = offset;
	return attrs;
}

void process_points(const std::vector<colored_point>& points, const laszip_POINTER& writer, const std::string& target) {
	laszip_point* point_ptr;

	laszip_open_writer(writer, target.c_str(), false);
	laszip_get_point_pointer(writer, &point_ptr);

	// write points
	double coordinates[3];
	for(const auto& point : points) {
		// write coordinates
		coordinates[0] = point.x;
		coordinates[1] = point.y;
		coordinates[2] = point.z;

		// write color
		point_ptr->rgb[0] = point.r;
		point_ptr->rgb[1] = point.g;
		point_ptr->rgb[2] = point.b;

		laszip_set_coordinates(writer, coordinates);
		laszip_write_point(writer);
	}

	// close writer
	laszip_close_writer(writer);
	laszip_destroy(writer);
}

void las_utils::save(const laszip_header* header, const std::vector<colored_point>& points, const std::string& target) {
	// create writer
	laszip_POINTER writer;
	laszip_create(&writer);
	laszip_set_header(writer, header);
	process_points(points, writer, target);
}

void las_utils::save(const std::string& target, const point_level& points, const vector3& min, const vector3& max)
{
	laszip_POINTER writer;
	laszip_header* header;

	laszip_create(&writer);
	laszip_get_header_pointer(writer, &header);

	header->version_major = 1;
	header->version_minor = 4;
	header->header_size = 375;
	header->offset_to_point_data = header->header_size;
	header->point_data_format = 2;
	header->point_data_record_length = 26;
	header->number_of_point_records = points.size();
	header->x_scale_factor = 0.001;
	header->y_scale_factor = 0.001;
	header->z_scale_factor = 0.001;
	header->x_offset = 0.0;
	header->y_offset = 0.0;
	header->z_offset = 0.0;
	header->min_x = min.x;
	header->min_y = min.y;
	header->min_z = min.z;
	header->max_x = max.x;
	header->max_y = max.y;
	header->max_z = max.z;

	header->extended_number_of_point_records = points.size();
	process_points(points, writer, target);
}

void las_utils::to_laz(const std::string& potree_path) {
	json metadata = file_utils::read_json(potree_path + "/metadata.json");
	auto bbox = bounding_box::parse(metadata["boundingBox"]);
	auto attrs = node::parse_attributes(metadata);
	auto root = node::load_hierarchy(potree_path, metadata);
	point_levels levels(10);

	root->traverse([&levels, potree_path, attrs](node* node, int level){
		std::vector<colored_point>& points = levels[level];
		auto buffer = file_utils::read_binary(potree_path + "/octree.bin", node->byteOffset, node->byteSize);
		int bpp = attrs.bytes;
		int num_points = buffer.size() / bpp;

		int64_t rgbOffset = 0;
		int64_t rgbOffsetFind = 0;
		for (const auto& attribute : attrs.list) {
			if (attribute.name == "rgb") {
				rgbOffset = rgbOffsetFind;
				break;
			}

			rgbOffsetFind += attribute.size;
		}

		for (int64_t i = 0; i < num_points; i++) {
			int64_t pointOffset = i * bpp;
			
			int32_t ix = gen_utils::read_value<int32_t>(buffer, pointOffset + 0);
			int32_t iy = gen_utils::read_value<int32_t>(buffer, pointOffset + 4);
			int32_t iz = gen_utils::read_value<int32_t>(buffer, pointOffset + 8);

			double x = double(ix) * attrs.posScale.x + attrs.posOffset.x;
			double y = double(iy) * attrs.posScale.y + attrs.posOffset.y;
			double z = double(iz) * attrs.posScale.z + attrs.posOffset.z;

			uint16_t r = gen_utils::read_value<uint16_t>(buffer, pointOffset + rgbOffset + 0);
			uint16_t g = gen_utils::read_value<uint16_t>(buffer, pointOffset + rgbOffset + 2);
			uint16_t b = gen_utils::read_value<uint16_t>(buffer, pointOffset + rgbOffset + 4);

			colored_point point;
			point.x = x;
			point.y = y;
			point.z = z;
			point.r = r;
			point.g = g;
			point.b = b;

			points.push_back(point);
		}
	});

	int level = -1;
	for(const auto& points : levels) {
		++level;
		if (points.empty()) continue;
		std::string target = potree_path + "/level_" + std::to_string(level) + ".laz";
		save(target, points, bbox.min, bbox.max);
	}

}

std::vector<las_utils::attribute_handler> las_utils::create_attribute_handlers(
	laszip_header* header, uint8_t* data, laszip_point* point, 
	attributes& inputAttributes, attributes& outputAttributes
) {
	std::vector<las_utils::attribute_handler> handlers;

	int attributeOffset = 0;

	// reset min/max, we're writing the values to a per-thread copy anyway
	for (auto& attribute : outputAttributes.list) {
		attribute.min = { gen_utils::INF, gen_utils::INF, gen_utils::INF };
		attribute.max = { -gen_utils::INF, -gen_utils::INF, -gen_utils::INF };
	}

	{ // STANDARD LAS ATTRIBUTES

		int offsetRGB = outputAttributes.getOffset("rgb");
		attribute* attributeRGB = outputAttributes.get("rgb");
		auto rgb = [data, point, header, offsetRGB, attributeRGB](int64_t offset) {
			if (offsetRGB >= 0) {

				uint16_t rgb[] = { 0, 0, 0 };
				memcpy(rgb, &point->rgb, 6);
				memcpy(data + offset + offsetRGB, rgb, 6);
				
				attributeRGB->min.x = std::min(attributeRGB->min.x, double(rgb[0]));
				attributeRGB->min.y = std::min(attributeRGB->min.y, double(rgb[1]));
				attributeRGB->min.z = std::min(attributeRGB->min.z, double(rgb[2]));

				attributeRGB->max.x = std::max(attributeRGB->max.x, double(rgb[0]));
				attributeRGB->max.y = std::max(attributeRGB->max.y, double(rgb[1]));
				attributeRGB->max.z = std::max(attributeRGB->max.z, double(rgb[2]));
			}
		};

		int offsetIntensity = outputAttributes.getOffset("intensity");
		attribute* attributeIntensity = outputAttributes.get("intensity");
		auto intensity = [data, point, header, offsetIntensity, attributeIntensity](int64_t offset) {
			memcpy(data + offset + offsetIntensity, &point->intensity, 2);

			attributeIntensity->min.x = std::min(attributeIntensity->min.x, double(point->intensity));
			attributeIntensity->max.x = std::max(attributeIntensity->max.x, double(point->intensity));
		};

		int offsetReturnNumber = outputAttributes.getOffset("return number");
		attribute* attributeReturnNumber = outputAttributes.get("return number");
		auto returnNumber = [data, point, header, offsetReturnNumber, attributeReturnNumber](int64_t offset) {
			uint8_t value = point->return_number;

			memcpy(data + offset + offsetReturnNumber, &value, 1);

			attributeReturnNumber->min.x = std::min(attributeReturnNumber->min.x, double(value));
			attributeReturnNumber->max.x = std::max(attributeReturnNumber->max.x, double(value));
		};

		int offsetNumberOfReturns = outputAttributes.getOffset("number of returns");
		attribute* attributeNumberOfReturns = outputAttributes.get("number of returns");
		auto numberOfReturns = [data, point, header, offsetNumberOfReturns, attributeNumberOfReturns](int64_t offset) {
			uint8_t value = point->number_of_returns;

			memcpy(data + offset + offsetNumberOfReturns, &value, 1);

			attributeNumberOfReturns->min.x = std::min(attributeNumberOfReturns->min.x, double(value));
			attributeNumberOfReturns->max.x = std::max(attributeNumberOfReturns->max.x, double(value));
		};

		int offsetScanAngleRank = outputAttributes.getOffset("scan angle rank");
		attribute* attributeScanAngleRank = outputAttributes.get("scan angle rank");
		auto scanAngleRank = [data, point, header, offsetScanAngleRank, attributeScanAngleRank](int64_t offset) {
			memcpy(data + offset + offsetScanAngleRank, &point->scan_angle_rank, 1);

			attributeScanAngleRank->min.x = std::min(attributeScanAngleRank->min.x, double(point->scan_angle_rank));
			attributeScanAngleRank->max.x = std::max(attributeScanAngleRank->max.x, double(point->scan_angle_rank));
		};

		int offsetScanAngle= outputAttributes.getOffset("scan angle");
		attribute* attributeScanAngle = outputAttributes.get("scan angle");
		auto scanAngle = [data, point, header, offsetScanAngle, attributeScanAngle](int64_t offset) {
			memcpy(data + offset + offsetScanAngle, &point->extended_scan_angle, 2);

			attributeScanAngle->min.x = std::min(attributeScanAngle->min.x, double(point->extended_scan_angle));
			attributeScanAngle->max.x = std::max(attributeScanAngle->max.x, double(point->extended_scan_angle));
		};

		int offsetUserData = outputAttributes.getOffset("user data");
		attribute* attributeUserData = outputAttributes.get("user data");
		auto userData = [data, point, header, offsetUserData, attributeUserData](int64_t offset) {
			memcpy(data + offset + offsetUserData, &point->user_data, 1);

			attributeUserData->min.x = std::min(attributeUserData->min.x, double(point->user_data));
			attributeUserData->max.x = std::max(attributeUserData->max.x, double(point->user_data));
		};

		int offsetClassification = outputAttributes.getOffset("classification");
		attribute* attributeClassification = outputAttributes.get("classification");
		auto classification = [data, point, header, offsetClassification, attributeClassification](int64_t offset) {
			
			uint8_t value = 0;
			if (point->extended_classification > 31){
				value = point->extended_classification;
			}else{
				value = point->classification;
			}

			data[offset + offsetClassification] = value;
			attributeClassification->histogram[value]++;

			attributeClassification->min.x = std::min(attributeClassification->min.x, double(value));
			attributeClassification->max.x = std::max(attributeClassification->max.x, double(value));
		};

		int offsetSourceId = outputAttributes.getOffset("point source id");
		attribute* attributePointSourceId = outputAttributes.get("point source id");
		auto pointSourceId = [data, point, header, offsetSourceId, attributePointSourceId](int64_t offset) {
			memcpy(data + offset + offsetSourceId, &point->point_source_ID, 2);

			attributePointSourceId->min.x = std::min(attributePointSourceId->min.x, double(point->point_source_ID));
			attributePointSourceId->max.x = std::max(attributePointSourceId->max.x, double(point->point_source_ID));
		};

		int offsetGpsTime= outputAttributes.getOffset("gps-time");
		attribute* attributeGpsTime = outputAttributes.get("gps-time");
		auto gpsTime = [data, point, header, offsetGpsTime, attributeGpsTime](int64_t offset) {
			memcpy(data + offset + offsetGpsTime, &point->gps_time, 8);

			attributeGpsTime->min.x = std::min(attributeGpsTime->min.x, point->gps_time);
			attributeGpsTime->max.x = std::max(attributeGpsTime->max.x, point->gps_time);
		};

		int offsetClassificationFlags = outputAttributes.getOffset("classification flags");
		attribute* attributeClassificationFlags = outputAttributes.get("classification flags");
		auto classificationFlags = [data, point, header, offsetClassificationFlags, attributeClassificationFlags](int64_t offset) {
			uint8_t value = point->extended_classification_flags;

			memcpy(data + offset + offsetClassificationFlags, &value, 1);

			attributeClassificationFlags->min.x = std::min(attributeClassificationFlags->min.x, double(point->extended_classification_flags));
			attributeClassificationFlags->max.x = std::max(attributeClassificationFlags->max.x, double(point->extended_classification_flags));
		};

		unordered_map<string, function<void(int64_t)>> mapping = {
			{"rgb", rgb},
			{"intensity", intensity},
			{"return number", returnNumber},
			{"number of returns", numberOfReturns},
			{"classification", classification},
			{"scan angle rank", scanAngleRank},
			{"scan angle", scanAngle},
			{"user data", userData},
			{"point source id", pointSourceId},
			{"gps-time", gpsTime},
			{"classification flags", classificationFlags},
		};

		for (auto& attribute : inputAttributes.list) {

			attributeOffset += attribute.size;

			if (attribute.name == "position") {
				continue;
			}

			bool standardMappingExists = mapping.find(attribute.name) != mapping.end();
			bool isIncludedInOutput = outputAttributes.get(attribute.name) != nullptr;
			if (standardMappingExists && isIncludedInOutput) {
				handlers.push_back(mapping[attribute.name]);
			}
		}
	}

	{ // EXTRA ATTRIBUTES

		// mapping from las format to index of first extra attribute
		// +1 for all formats with returns, which is split into return number and number of returns
		unordered_map<int, int> formatToExtraIndex = {
			{0, 8},
			{1, 9},
			{2, 9},
			{3, 10},
			{4, 14},
			{5, 15},
			{6, 10},
			{7, 11},
		};

		bool noMapping = formatToExtraIndex.find(header->point_data_format) == formatToExtraIndex.end();
		if (noMapping) {
			string msg = "ERROR: las format not supported: " + gen_utils::format_number(header->point_data_format) + "\n";
			cout << msg;

			exit(123);
		}

		// handle extra bytes individually to compute per-attribute information
		int firstExtraIndex = formatToExtraIndex[header->point_data_format];
		int sourceOffset = 0;

		int attributeOffset = 0;
		for (int i = 0; i < firstExtraIndex; i++) {
			attributeOffset += inputAttributes.list[i].size;
		}

		for (int i = firstExtraIndex; i < inputAttributes.list.size(); i++) {
			attribute& inputAttribute = inputAttributes.list[i];
			attribute* attribute = outputAttributes.get(inputAttribute.name);
			int targetOffset = outputAttributes.getOffset(inputAttribute.name);

			int attributeSize = inputAttribute.size;

			if (attribute != nullptr) {
				auto handleAttribute = [data, point, header, attributeSize, attributeOffset, sourceOffset, attribute](int64_t offset) {
					memcpy(data + offset + attributeOffset, point->extra_bytes + sourceOffset, attributeSize);

					std::function<double(uint8_t*)> f;

					// TODO: shouldn't use DOUBLE as a unifying type
					// it won't work with uint64_t and int64_t
					if (attribute->type == attribute_type::INT8) {
						f = gen_utils::read_double<int8_t>;
					} else if (attribute->type == attribute_type::INT16) {
						f = gen_utils::read_double<int16_t>;
					} else if (attribute->type == attribute_type::INT32) {
						f = gen_utils::read_double<int32_t>;
					} else if (attribute->type == attribute_type::INT64) {
						f = gen_utils::read_double<int64_t>;
					} else if (attribute->type == attribute_type::UINT8) {
						f = gen_utils::read_double<uint8_t>;
					} else if (attribute->type == attribute_type::UINT16) {
						f = gen_utils::read_double<uint16_t>;
					} else if (attribute->type == attribute_type::UINT32) {
						f = gen_utils::read_double<uint32_t>;
					} else if (attribute->type == attribute_type::UINT64) {
						f = gen_utils::read_double<uint64_t>;
					} else if (attribute->type == attribute_type::FLOAT) {
						f = gen_utils::read_double<float>;
					} else if (attribute->type == attribute_type::DOUBLE) {
						f = gen_utils::read_double<double>;
					}

					if (attribute->numElements == 1) {
						double x = f(point->extra_bytes + sourceOffset);

						attribute->min.x = std::min(attribute->min.x, x);
						attribute->max.x = std::max(attribute->max.x, x);
					} else if (attribute->numElements == 2) {
						double x = f(point->extra_bytes + sourceOffset + 0 * attribute->elementSize);
						double y = f(point->extra_bytes + sourceOffset + 1 * attribute->elementSize);

						attribute->min.x = std::min(attribute->min.x, x);
						attribute->min.y = std::min(attribute->min.y, y);
						attribute->max.x = std::max(attribute->max.x, x);
						attribute->max.y = std::max(attribute->max.y, y);

					} else if (attribute->numElements == 3) {
						double x = f(point->extra_bytes + sourceOffset + 0 * attribute->elementSize);
						double y = f(point->extra_bytes + sourceOffset + 1 * attribute->elementSize);
						double z = f(point->extra_bytes + sourceOffset + 2 * attribute->elementSize);

						attribute->min.x = std::min(attribute->min.x, x);
						attribute->min.y = std::min(attribute->min.y, y);
						attribute->min.z = std::min(attribute->min.z, z);
						attribute->max.x = std::max(attribute->max.x, x);
						attribute->max.y = std::max(attribute->max.y, y);
						attribute->max.z = std::max(attribute->max.z, z);
					}


				};

				handlers.push_back(handleAttribute);
				attributeOffset += attribute->size;
			}

			sourceOffset += inputAttribute.size;
		}

	}
	
	return handlers;
}