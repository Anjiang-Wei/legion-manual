#include <cstdio>
#include "legion.h"

using namespace Legion;

enum TaskIDs {
  TOP_LEVEL_TASK_ID,
  PRODUCER_TASK_ID,
  CONSUMER_TASK_ID,
};

enum FieldIDs {
  FIELD_A,
};

void top_level_task(const Task *task,
		    const std::vector<PhysicalRegion> &rgns,
		    Context ctx, 
		    Runtime *rt)
{
  Rect<1> rec(Point<1>(0),Point<1>(99));
  IndexSpace is = rt->create_index_space(ctx,rec);
  FieldSpace fs = rt->create_field_space(ctx);
  FieldAllocator field_allocator = rt->create_field_allocator(ctx,fs);
  FieldID fida = field_allocator.allocate_field(sizeof(int), FIELD_A);
  assert(fida == FIELD_A);


  LogicalRegion lr = rt->create_logical_region(ctx,is,fs);
  
  PhaseBarrier odd = rt->create_phase_barrier(ctx,1);
  PhaseBarrier even = rt->create_phase_barrier(ctx,1);

  for (int i = 0; i < 10; i++) {

    /* Producer task */
    TaskLauncher producer_launcher(PRODUCER_TASK_ID, TaskArgument(&i,sizeof(int)));
    producer_launcher.add_region_requirement(RegionRequirement(lr, WRITE_DISCARD, SIMULTANEOUS, lr));
    producer_launcher.add_field(0,FIELD_A);
    if (i > 0)
      producer_launcher.add_wait_barrier(odd);
    producer_launcher.add_arrival_barrier(even);
    rt->execute_task(ctx, producer_launcher);

    even = rt->advance_phase_barrier(ctx,even);

    /* Consumer task */
    TaskLauncher consumer_launcher(CONSUMER_TASK_ID, TaskArgument(NULL,0));
    consumer_launcher.add_region_requirement(RegionRequirement(lr, READ_WRITE, SIMULTANEOUS, lr));
    consumer_launcher.add_field(0,FIELD_A);
    consumer_launcher.add_wait_barrier(even);
    consumer_launcher.add_arrival_barrier(odd);
    rt->execute_task(ctx, consumer_launcher);

    odd = rt->advance_phase_barrier(ctx,odd);

    printf("Iteration %d of top level task.\n",i);
  }
  printf("Deallocating phase barriers.\n");
  rt->destroy_phase_barrier(ctx,odd);
  rt->destroy_phase_barrier(ctx,even);
}
  
void producer_task(const Task *task,
		    const std::vector<PhysicalRegion> &rgns,
		    Context ctx, Runtime *rt)
{
  int i = *((int *) task->args);
  const FieldAccessor<READ_WRITE,int,1> fa_a(rgns[0], FIELD_A);
  Rect<1> d = rt->get_index_space_domain(ctx,task->regions[0].region.get_index_space());
  for (PointInRectIterator<1> itr(d); itr(); itr++)
    {
      fa_a[*itr] = i;
    }
  printf("Wrote %d to the buffer\n",i);
}

void consumer_task(const Task *task,
		    const std::vector<PhysicalRegion> &rgns,
		    Context ctx, Runtime *rt)
{
  const FieldAccessor<READ_WRITE,int,1> fa_a(rgns[0], FIELD_A);
  Rect<1> d = rt->get_index_space_domain(ctx,task->regions[0].region.get_index_space());
  int val;
  for (PointInRectIterator<1> itr(d); itr(); itr++)
    {
      val = fa_a[*itr];
      fa_a[*itr] = 0;
    }
  printf("Found %d in the buffer\n",val);
}


int main(int argc, char **argv)
{
  Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);
  {
    TaskVariantRegistrar registrar(TOP_LEVEL_TASK_ID, "top_level_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<top_level_task>(registrar);
  }
  {
    TaskVariantRegistrar registrar(PRODUCER_TASK_ID, "producer_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<producer_task>(registrar, "producer_task");
  }
  {
    TaskVariantRegistrar registrar(CONSUMER_TASK_ID, "consumer_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<consumer_task>(registrar, "consumer_task");
  }  
  return Runtime::start(argc, argv);
}

