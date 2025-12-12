#include "common/scale_offset.h"
#include "las/las_info.h"
#include "las_utils.h"
#include "gen_utils.h"
#include "string_utils.h"
#include "attribute_utils.h"
#include <unordered_map>
#include <execution>

using namespace potree;

std::vector<attribute> las_utils::parse_extra_attributes(const las_header& header) {
	std::vector<attribute> attributes;

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

				attributes.push_back(xyz);
			}
		}
	}

	return attributes;
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
