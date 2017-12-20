#include <iostream>
#include <thread>

#include <vector>

#include "dataacqctl.h"

using std::endl;

int main()
{
	DataAcqCtl ctl;    // = DataAcqCtl::Instance();
	ctl.openDevice();
	ctl.init();

	vector<uint32_t> fids(2000);
	int A;
	uint32_t curFrameId = 0;
	for(uint i=0; i<100; i++)
	{
	//	curFrameId = ctl.waitFrameValid(curFrameId, 1000);
	//	fids[i] = curFrameId;
	//	ctl.grabData();
		ctl.switchBank();
		ctl.getFrameNumber();

		this_thread::sleep_for(chrono::microseconds(2000));

		A = i;
	}

	for(const auto & elem : fids)
	{
		cout << elem << ", ";
	}

	cout << endl;
	return 0;
}
