//// Copyright Microsoft and Project Verona Contributors.
//// SPDX-License-Identifier: MIT
//#include <cpp/when.h>
//#include <debug/harness.h>
//#include <verona.h>
//
//using namespace verona::rt;
//using namespace verona::cpp;
//
//
//struct ExternalSource;
//
//
//struct Poller : VCown<Poller>
//{
//  std::shared_ptr<ExternalSource> es;
//};
//
//struct ExternalSource
//{
//  Poller* p;
//  std::atomic<bool> notifications_on;
//  Notification* n;
//
//  ExternalSource(Poller* p_) : p(p_), notifications_on(false)
//  {
//    Cown::acquire(p);
//  }
//
//  ~ExternalSource()
//  {
//    Logging::cout() << "~ExternalSource" << Logging::endl;
//  }
//
//  void main_es()
//  {
//
//
//    while (1){
//      sleep(1);
//      when() << [](){
//        printf("hey\n");
//      };
//    }
//
//    schedule_lambda(Scheduler::remove_external_event_source);
//  }
//
//
//};
//
//
//
//void test(SystematicTestHarness* harness)
//{
//  auto& alloc = ThreadAlloc::get();
//  auto* p = new (alloc) Poller();
//  auto es = std::make_shared<ExternalSource>(p);
//  p->es = es;
//
//
//  schedule_lambda<YesTransfer>(p, [=]() {
//    // Start IO Thread
//    Scheduler::add_external_event_source();
//    harness->external_thread([=]() {   es->main_es(); });
//  });
//}
//