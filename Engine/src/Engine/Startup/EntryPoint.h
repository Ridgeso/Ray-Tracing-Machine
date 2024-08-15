#pragma once
#ifdef RT_CLIENT

#include "Engine/Startup/Startup.h"

int32_t main(int argc, char* argv[])
{
	return RT::Main(argc, argv);
}

#endif // RT_CLIENT
