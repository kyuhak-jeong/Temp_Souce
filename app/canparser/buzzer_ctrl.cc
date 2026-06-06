// Build:
// g++ -std=gnu++17 -laivmcu -o buzzerctrl buzzer_ctrl.cc

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "aivmculib.h"

using namespace AIVMCULib;

std::shared_ptr<McuMgr> mcu_mgr;

int main(int argc, char* argv[])
{
  mcu_mgr = McuMgr::create();

  if(argc < 2 || (argv[1][0]!='0' && argv[1][0]!='1')) {
    fprintf(stderr, "Usage: buzzerctrl <state>\n");
    fprintf(stderr, "   state: 0: off, 1: on\n");
    exit(EXIT_FAILURE);
  }

  const int state = atoi(argv[1]);
  const int rc = mcu_mgr->SetBuzzerOn(state);
  if(rc) {
    fprintf(stderr, "Error %d\n", rc);
  }

  return 0;
}
