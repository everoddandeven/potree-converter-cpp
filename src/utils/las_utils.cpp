#include "common/scale_offset.h"
#include "common/point.h"
#include "common/bounding_box.h"
#include "common/node.h"
#include "las/las_info.h"
#include "las_utils.h"
#include "gen_utils.h"
#include "string_utils.h"
#include "file_utils.h"
#include "attribute_utils.h"
#include <unordered_map>
#include <execution>

using namespace potree;

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