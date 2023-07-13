// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT

#include <cpp/when.h>
#include <debug/harness.h>

class Body
{
public:
  ~Body()
  {
    Logging::cout() << "Body destroyed" << Logging::endl;
  }
};

using namespace verona::cpp;

class Counter{
public:
  u_int64_t count = 0;
};


template<typename... Args>
void test_body2(Args&... args)
{
  if(sizeof...(args) == 1){
    auto log1 = std::get<0>(std::tie(args...));
    when(log1) << [](auto) {
      std::cout << "1" << std::endl;
    };
  }

  if(sizeof...(args) == 2){
    auto log1 = std::get<0>(std::tie(args...));
    auto log2 = std::get<1>(std::tie(args...));
    when(log1, log2) << [](auto, auto) {
      std::cout << "2" << std::endl;
    };
  }

  // Add more conditions for handling additional argument cases if needed
}


void test_body()
{
  std::vector<cown_ptr<Body>> logs;
  logs.push_back(make_cown<Body>());
  logs.push_back(make_cown<Body>());


}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);

  harness.run(test_body);

  return 0;
}
