
#include <chrono>
#include "TCHAR.h"
#include "pdh.h"
#include "windows.h"
#include "psapi.h"
#include <thread>
#include "gen_utils.h"

using namespace potree;
using std::chrono::high_resolution_clock;
using namespace std::chrono_literals;

static const long long start_time = high_resolution_clock::now().time_since_epoch().count();

static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;
static HANDLE self;
static bool initialized = false;

void init() {
	SYSTEM_INFO sysInfo;
	FILETIME ftime, fsys, fuser;

	GetSystemInfo(&sysInfo);
	// numProcessors = sysInfo.dwNumberOfProcessors;
	numProcessors = std::thread::hardware_concurrency();

	GetSystemTimeAsFileTime(&ftime);
	memcpy(&lastCPU, &ftime, sizeof(FILETIME));

	self = GetCurrentProcess();
	GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
	memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
	memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));

	initialized = true;
}

memory_data gen_utils::get_memory_data() {
  memory_data data;

	{
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);
		DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile;
		DWORDLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;;
		DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
		DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

		data.virtual_total = totalVirtualMem;
		data.virtual_used = virtualMemUsed;

		data.physical_total = totalPhysMem;
		data.physical_used = physMemUsed;

	}

	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		SIZE_T virtualMemUsedByMe = pmc.PrivateUsage;
		SIZE_T physMemUsedByMe = pmc.WorkingSetSize;

		static size_t virtualUsedMax = 0;
		static size_t physicalUsedMax = 0;

		virtualUsedMax = max(virtualMemUsedByMe, virtualUsedMax);
		physicalUsedMax = max(physMemUsedByMe, physicalUsedMax);

		data.virtual_usedByProcess = virtualMemUsedByMe;
		data.virtual_usedByProcess_max = virtualUsedMax;
		data.physical_usedByProcess = physMemUsedByMe;
		data.physical_usedByProcess_max = physicalUsedMax;
	}

	return data;
}

cpu_data gen_utils::get_cpu_data() {
	FILETIME ftime, fsys, fuser;
	ULARGE_INTEGER now, sys, user;
	double percent;

	if (!initialized) {
		init();
	}

	GetSystemTimeAsFileTime(&ftime);
	memcpy(&now, &ftime, sizeof(FILETIME));

	GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
	memcpy(&sys, &fsys, sizeof(FILETIME));
	memcpy(&user, &fuser, sizeof(FILETIME));
	percent = (sys.QuadPart - lastSysCPU.QuadPart) +
		(user.QuadPart - lastUserCPU.QuadPart);
	percent /= (now.QuadPart - lastCPU.QuadPart);
	percent /= numProcessors;
	lastCPU = now;
	lastUserCPU = user;
	lastSysCPU = sys;
	cpu_data data;
	data.numProcessors = numProcessors;
	data.usage = percent * 100.0;

	return data;
}

// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
// method to seperate bits from a given integer 3 positions apart
uint64_t gen_utils::split_by_3(unsigned int a) {
	uint64_t x = a & 0x1fffff; // we only look at the first 21 bits
	x = (x | x << 32) & 0x1f00000000ffff; // shift left 32 bits, OR with self, and 00011111000000000000000000000000000000001111111111111111
	x = (x | x << 16) & 0x1f0000ff0000ff; // shift left 32 bits, OR with self, and 00011111000000000000000011111111000000000000000011111111
	x = (x | x << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, and 0001000000001111000000001111000000001111000000001111000000000000
	x = (x | x << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, and 0001000011000011000011000011000011000011000011000011000100000000
	x = (x | x << 2) & 0x1249249249249249;
	return x;
}

// see https://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
uint64_t gen_utils::morton_encode(unsigned int x, unsigned int y, unsigned int z) {
	uint64_t answer = 0;
	answer |= split_by_3(x) | split_by_3(y) << 1 | split_by_3(z) << 2;
	return answer;
}

double gen_utils::now() {
	auto now = std::chrono::high_resolution_clock::now();
	long long nanosSinceStart = now.time_since_epoch().count() - start_time;
	double secondsSinceStart = double(nanosSinceStart) / 1'000'000'000.0;
	return secondsSinceStart;
}

double profile_now() {
	static LARGE_INTEGER freq;
	static int init = 0;
	LARGE_INTEGER counter;

	if (!init) {
		QueryPerformanceFrequency(&freq);
		init = 1;
	}

	QueryPerformanceCounter(&counter);
	return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
}

gen_utils::profiler::profiler(const char* name) {
	m_name = name;
	m_start = profile_now();
}

gen_utils::profiler::~profiler() {
	double ms = m_start - profile_now();
	MINFO << "[PROFILE] " << m_name << ": " << std::to_string(ms) << " ms" << std::endl;
}

gen_utils::monitor::monitor(const std::shared_ptr<potree::status>& state) {
	m_state = state;
}

void gen_utils::monitor::print() {
	auto ram = get_memory_data();
	auto CPU = get_cpu_data();
	double GB = 1024.0 * 1024.0 * 1024.0;

	double throughput = (double(m_state->pointsProcessed) / m_state->duration) / 1'000'000.0;

	double progressPass = 100.0 * m_state->progress();
	double progressTotal = (100.0 * double(m_state->currentPass - 1) + progressPass) / double(m_state->numPasses);

	std::string strProgressPass = format_number(progressPass) + "%";
	std::string strProgressTotal = format_number(progressTotal) + "%";
	std::string strTime = format_number(now()) + "s";
	std::string strDuration = format_number(m_state->duration) + "s";
	std::string strThroughput = format_number(throughput) + "MPs";

	std::string strRAM = format_number(double(ram.virtual_usedByProcess) / GB, 1)
		+ "GB (highest " + format_number(double(ram.virtual_usedByProcess_max) / GB, 1) + "GB)";
	std::string strCPU = format_number(CPU.usage) + "%";

	std::stringstream ss;
	ss << "[" << strProgressTotal << ", " << strTime << "], "
		<< "[" << m_state->name << ": " << strProgressPass 
		<< ", duration: " << strDuration 
		<< ", throughput: " << strThroughput << "]"
		<< "[RAM: " << strRAM << ", CPU: " << strCPU << "]" << std::endl;

	std::cout << ss.str() << std::flush;
}

void gen_utils::monitor::start() {
	monitor* _this = this;
	m_thread = std::thread([_this]() {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1'000ms);
		std::cout << std::endl;

		while (!_this->m_stop_requested) {
			_this->print();
			std::this_thread::sleep_for(1'000ms);
		}
	});
}

void gen_utils::monitor::stop() {
	m_stop_requested = true;
	m_thread.join();
}

size_t gen_utils::get_num_processors() {
	return get_cpu_data().numProcessors;
}

gen_utils::memory_checker::memory_checker(int64_t max_mb, double interval) {
	m_max_mb = max_mb;
	m_interval = interval;

	m_thread = std::thread([this]() {
		while(true) {
			auto mem_data = get_memory_data();
			// TODO print memory data?
			std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(m_interval * 1000)));
		}
	});
	m_thread.detach();
}


