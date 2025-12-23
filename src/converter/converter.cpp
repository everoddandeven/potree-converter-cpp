#include "converter.h"
#include "geometry/hierarchy.h"
#include "sampler/sampler_poisson.h"
#include "sampler/sampler_random.h"
#include "utils/las_utils.h"
#include "utils/chunk_utils.h"
#include <filesystem>

using namespace potree;


conversion_stats conversion_stats::compute(const std::vector<file_source>& sources) {
	vector3 min = { gen_utils::INF , gen_utils::INF , gen_utils::INF };
	vector3 max = { -gen_utils::INF , -gen_utils::INF , -gen_utils::INF };

	int64_t total_bytes = 0;
	int64_t total_points = 0;

	for(auto source : sources){
		min.x = std::min(min.x, source.min.x);
		min.y = std::min(min.y, source.min.y);
		min.z = std::min(min.z, source.min.z);
								
		max.x = std::max(max.x, source.max.x);
		max.y = std::max(max.y, source.max.y);
		max.z = std::max(max.z, source.max.z);

		total_points += source.numPoints;
		total_bytes += source.filesize;
	}


	double cube_size = (max - min).max();
	vector3 size = { cube_size, cube_size, cube_size };
	max = min + cube_size;

	std::string str_min = "[" + std::to_string(min.x) + ", " + std::to_string(min.y) + ", " + std::to_string(min.z) + "]";
	std::string str_max = "[" + std::to_string(max.x) + ", " + std::to_string(max.y) + ", " + std::to_string(max.z) + "]";
	std::string strSize = "[" + std::to_string(size.x) + ", " + std::to_string(size.y) + ", " + std::to_string(size.z) + "]";

	std::string strTotalFileSize;
	{
		int64_t KB = 1024;
		int64_t MB = 1024 * KB;
		int64_t GB = 1024 * MB;
		int64_t TB = 1024 * GB;

		if (total_bytes >= TB) {
			strTotalFileSize = gen_utils::format_number(double(total_bytes) / double(TB), 1) + " TB";
		} else if (total_bytes >= GB) {
			strTotalFileSize = gen_utils::format_number(double(total_bytes) / double(GB), 1) + " GB";
		} else if (total_bytes >= MB) {
			strTotalFileSize = gen_utils::format_number(double(total_bytes) / double(MB), 1) + " MB";
		} else {
			strTotalFileSize = gen_utils::format_number(double(total_bytes), 1) + " bytes";
		}
	}

	MINFO << "cubicAABB: {\n" << std::endl
	<< "	\"min\": " << str_min << "," << std::endl
	<< "	\"max\": " << str_max << ","  << std::endl
	<< "	\"size\": " << strSize  << std::endl
	<< "}" << std::endl
	<< "#points: " << gen_utils::format_number(total_points) << std::endl
	<< "total file size: " << strTotalFileSize << std::endl;

	// sanity check

  bool size_err = (size.x == 0.0) || (size.y == 0.0) || (size.z == 0);
  if (size_err) {
    throw std::runtime_error("invalid bounding box. at least one axis has a size of zero.");
  }

	return { min, max, total_bytes, total_points };
}

void converter::do_chunking(const file_source_container& container, const conversion_stats& stats, attributes& attrs, const std::shared_ptr<gen_utils::monitor>& monitor) {
  if (m_options.skip_chunking()) return;

  gen_utils::profiler pr("converter::do_chunking()");

  if (m_options.m_chunk_method == "LASZIP") {
    chunk_utils::chunker::do_chunking(container.m_files, m_options.m_outdir, stats.m_min, stats.m_max, m_state, attrs, monitor);
  }
  else if (m_options.m_chunk_method == "LAS_CUSTOM") {
    // TODO implement
  }
  else throw std::runtime_error("Invalid chunk method provided: " + m_options.m_chunk_method);
}

void converter::do_indexing(const file_source_container& container) {
	if (m_options.m_no_indexing) return;
	
	auto stats = conversion_stats::compute(container.m_files);
	hierarchy_indexer idxer(m_options.m_outdir, m_options);
	std::shared_ptr<sampler> smplr;

	if (m_options.m_method == "random") {
		smplr = std::make_shared<sampler_random>();
		idxer.do_indexing(m_state, smplr);
	}
	else if (m_options.m_method == "poisson") {
		smplr = std::make_shared<sampler_poisson>();
		idxer.do_indexing(m_state, smplr);
	}
	else if (m_options.m_method == "poisson_average") {
		// TODO implement sampler_poisson_average
		throw std::runtime_error("sampler_poisson_average not implemented");
	}
	else {
		MWARNING << "Unkown indexing method provided: " << m_options.m_method << std::endl;
	}
}

void converter::convert() {
  gen_utils::profiler pr("converter::convert()");
  auto cpu_info = gen_utils::get_cpu_data();

  MINFO << "threads: " << cpu_info.numProcessors << std::endl;

  auto curated_srcs = las_utils::curate_sources(m_options.m_source);

  if (m_options.m_name.empty()) m_options.m_name = curated_srcs.m_name;

  auto output_attributes = las_utils::compute_output_attributes(curated_srcs.m_files, m_options.m_attributes);

  MINFO << "output attributes: " << output_attributes.to_string() << std::endl;

  auto stats = conversion_stats::compute(curated_srcs.m_files);

  std::string target_dir = m_options.m_outdir;

  MINFO << "target directory: " << target_dir << std::endl;
  std::filesystem::create_directories(target_dir);

  m_state = std::make_shared<potree::status>();
  m_state->pointsProcessed = stats.m_total_points;
  m_state->bytesProcessed = stats.m_total_bytes;

  auto monitor = std::make_shared<gen_utils::monitor>(m_state);
  monitor->start();

  // this is the real important stuff
  do_chunking(curated_srcs, stats, output_attributes, monitor);
  do_indexing(curated_srcs);
}

