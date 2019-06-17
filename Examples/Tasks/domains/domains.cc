#include <cstdio>
#include "legion.h"

// A macro to suppress compiler warnings about an unused variable.
# define USEVAR(x) true ? x : x

using namespace Legion;

enum TaskID {
  TOP_LEVEL_TASK_ID,
};

void print_point(DomainPoint d) {
  printf("The point is <");
  for(int i = 0; i <= d.dim - 1; i++) 
    printf("%lld,",d[i]);
  if (d.dim > 0) 
    printf("%lld>\n",d[d.dim-1]);
}

void top_level_task(const Task *task,
                      const std::vector<PhysicalRegion> &regions,
                      Context ctx, HighLevelRuntime *runtime)
{
  //
  // Point operations
  //
  Point<1> one(1);
  print_point(one);

  Point<1> two(2);
  print_point(two);

  print_point(Point<1>(one + two));

  coord_t source[3];
  source[0] = 0;
  source[1] = 0;
  Point<2> zero(source);
  print_point(zero);

  Point<2> zeroes(0,0);
  Point<2> twos(2,2);
  Point<2> threes(3,3);
  Point<3> fours(4,4,4);

  printf("The point is <%lld,%lld>\n",twos[0],twos[1]);
  printf("The point is <%lld,%lld>\n",threes[0],threes[1]);
  printf("The point is <%lld,%lld,%lld>\n",fours[0],fours[1],fours[2]);
  print_point(Point<2> (twos + threes));
  print_point(Point<2> (twos * threes));
  printf("The dot product is %lld\n",twos.dot(threes));
  
  assert(twos == twos);
  assert(twos != threes);

  //
  // Rect operations
  //
  Rect<2> big(zeroes,threes);
  Rect<2> small(twos,threes);
  assert(big != small);
  assert(big.contains(small));
  assert(small.overlaps(big));
  assert(small.intersection(big) == small);

  //
  // Conversion to Domain and DomainPoint
  //
  Domain bigdomain = big;
  DomainPoint dtwos = twos;

  USEVAR(bigdomain);
  USEVAR(dtwos); 

}

int main(int argc, char **argv)
{
  Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);
  {
    TaskVariantRegistrar registrar(TOP_LEVEL_TASK_ID, "top_level_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<top_level_task>(registrar);
  }
  return Runtime::start(argc, argv);
}
