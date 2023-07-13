// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT

#include <cpp/when.h>
#include <debug/harness.h>

using namespace verona::cpp;

class Observer;

class Subject
{
  std::string str = "aaaaaaaaaaaaaaaaaaaaa";
public:
  typename cown_ptr<Observer>::weak observer;

  void test(){
    printf("hey from subject\n");
  }

  ~Subject()
  {
    Logging::cout() << "Subject destroyed" << Logging::endl;
  }
};

class Observer
{
public:
  cown_ptr<Subject> subject;

  ~Observer()
  {
    Logging::cout() << "Observer destroyed" << Logging::endl;
  }
};

void test_body()
{
  // Create two objects a subject and an observer, then form a cycle with a weak
  // reference to break the cycle. This example tests promoting a weak reference
  // to a strong reference.

  Logging::cout() << "test_build()" << Logging::endl;

  auto subject = make_cown<Subject>();
  auto observer = make_cown<Observer>();


  ActualCown<Subject> * allocated_cown = subject.allocated_cown;
  if ((allocated_cown != nullptr) && allocated_cown->acquire_strong_from_weak()){
    cown_ptr<Subject> cownPtr = cown_ptr<Subject>(allocated_cown);
    when(cownPtr) <<[](acquired_cown<Subject> acquiredCown){
        acquiredCown->test();
    };
  }



  //while (1);
  //Cown::release(ThreadAlloc::get(),x);
  //check leaks like this program (if u delete release then it says error)
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  //Scheduler::set_detect_leaks(true);
  harness.run(test_body);


  return 0;
}
