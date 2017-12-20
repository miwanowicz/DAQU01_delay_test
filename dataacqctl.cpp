#include "dataacqctl.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

#define MEMPATH "/dev/mem"

using namespace std;

//std::unique_ptr<DataAcqCtl> DataAcqCtl::instance;
//std::once_flag DataAcqCtl::onceFlag;

DataAcqCtl::~DataAcqCtl()
{
	closeDevice();
}

void DataAcqCtl::openDevice() {
	if((fd = open(MEMPATH, O_RDWR | O_SYNC )) == -1)
		throw runtime_error("could not access device memory!");
	state = State::Open;
}

void DataAcqCtl::init() {
	if(state != State::Open)
		throw runtime_error("device not open or has already been initialized!");

	map_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ADR_START_BASE & ~MAP_MASK);

	if (!memoryIsOk())
		throw runtime_error("Error mapping data memory");

	acqStartTime = chrono::high_resolution_clock::now();
	configFpgaRegAddrStruct();
	state = State::Initialized;
}

bool DataAcqCtl::memoryIsOk()
{
	return !(map_base == (void*) (-1) );
}

void DataAcqCtl::closeDevice() {
	switch (state) {
	case State::Closed: 					// easy, nothing to do ...
			break;
	case State::Open:						// close file handle to memory
			close(fd);
			break;
case State::Initialized:					// deallocate memory and close file handle
			munmap(map_base, MAP_SIZE);
			close(fd);
			break;
	}
	state = State::Closed;
}

/**
 * Read frame number registers and return result
 */
uint32_t DataAcqCtl::getFrameNumber()
{
	unsigned long read_result1, read_result2;
	unsigned int frame_no;

	read_result1 = *((unsigned short *) fpgaCtlRegisters.frame_number_msb);
	read_result2 = *((unsigned short *) fpgaCtlRegisters.frame_number_lsb);

    printf("Frame number: 0x%X / 0x%X\n ", read_result1 , read_result2);
    frame_no  = read_result1;
    frame_no *= 0xFFFF;
    frame_no += read_result2;

	return frame_no;
}

/**
 * Dummy implementation of data grabbing function... does not really do any data grabbing but simply switches bank
 */
void DataAcqCtl::grabData()
{
	switchBank();
}

/**
 * Blocks program execution until new frame id is observed. Returns new frame id.
 */
uint32_t DataAcqCtl::waitFrameValid(const uint32_t & curFrameId, const uint32_t  & timeoutInMillis)
{
	if(curFrameId==0)
		switchBank();
	uint32_t newFrameId;
	const auto absoluteTimeout = steady_clock::now() + milliseconds(timeoutInMillis);
	for( newFrameId = getFrameNumber(); newFrameId==curFrameId ; newFrameId = getFrameNumber())
	{
		this_thread::sleep_for(chrono::microseconds(50));
		if( steady_clock::now()>absoluteTimeout )
		{
			throw runtime_error("Frame acquisition timeout! Is the frame trigger running?");
		}
	}
	return newFrameId;
}

void DataAcqCtl::switchBank()
{
		unsigned long read_result,temp;
		unsigned short data;

		read_result  = *((unsigned short *) fpgaCtlRegisters.bank_control );
		temp = read_result & 0x1;

		if (temp == 1) // if last bit in bank control '1'
		{
			 data = read_result & 0xFFF0;
			 *((unsigned short *) fpgaCtlRegisters.bank_control)  = data;  // init default value
			 printf("switch bank: 0 \n ");
		}
		else // if last bit in bank control is '0'
		{
			data =  read_result | 0x0001;
			*((unsigned short *)fpgaCtlRegisters.bank_control)  = data;  // init default value
			printf("switch bank: 1 \n ");
		}
}

void DataAcqCtl::configFpgaRegAddrStruct()	{
	fpgaCtlRegisters.bank_control = map_base + ((ADR_SETUP_BASE + ADR_SETUP_BANK_CHANGE_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.pulse_delay = map_base + ((ADR_SETUP_BASE + ADR_SETUP_PULSE_DELAY_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.avg_cnt =  map_base + ((ADR_SETUP_BASE + ADR_SETUP_AVRG_NO_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.end_conv_cnt = map_base + ((ADR_SETUP_BASE + ADR_SETUP_END_CNV_CNT_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.lmbd_max_cnt =  map_base + ((ADR_SETUP_BASE + ADR_SETUP_NO_LAMBDA_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.start_bg_avg =  map_base + ((ADR_SETUP_BASE + ADR_SETUP_START_BACK_AVRG_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.stop_bg_avg =  map_base + ((ADR_SETUP_BASE  + ADR_SETUP_STOP_BACK_AVRG_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.start_pulse_avg = map_base + ((ADR_SETUP_BASE + ADR_SETUP_START_PULSE_AVRG_OFFSET)& MAP_MASK);
	fpgaCtlRegisters.stop_pulse_avg =  map_base + ((ADR_SETUP_BASE + ADR_SETUP_STOP_PULSE_AVRG_OFFSET)& MAP_MASK);

	fpgaCtlRegisters.frame_number_lsb =  map_base + (ADR_START_BASE_FRAME_NR_LSB_OFFSET & MAP_MASK);
	fpgaCtlRegisters.frame_number_msb =  map_base;
	fpgaCtlRegisters.pulse_avg_value =  map_base + (ADR_START_BASE_PULSE_AVG_VAL_OFFSET & MAP_MASK);
	fpgaCtlRegisters.bg_avg_value =  map_base + (ADR_START_BASE_BG_AVG_VAL_OFFSET & MAP_MASK);
	fpgaCtlRegisters.rd_average_number =  map_base +  (ADR_START_BASE_AVG_NO_OFFSET & MAP_MASK);
}

