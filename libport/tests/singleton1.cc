#include <iostream>
#include "libport/singleton-ptr.hh"

struct foo
{
  void bar()
  {
    std::cout << "foo::bar()" << std::endl;
  }
};

int main()
{
  urbi::SingletonPtr<foo> test;
  test->bar();
  (*test).bar();
  return 0;
}
