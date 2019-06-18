#include <cstdio>
#include "legion.h"

using namespace Legion;

enum TaskIDs {
  TOP_LEVEL_TASK_ID,
  INC_TASK_ID_FIELDA,
  INC_TASK_ID_FIELDB,
  INC_TASK_ID_BOTH,
  SUM_TASK_ID,
};

enum FieldIDs {
  FIELD_A,
  FIELD_B,
};

void top_level_task(const Task *task,
		    const std::vector<PhysicalRegion> &rgns,
		    Context ctx, 
		    Runtime *rt)
{
  Rect<1> rec(Point<1>(0),Point<1>(999));
  IndexSpace is = rt->create_index_space(ctx,rec);
  FieldSpace fs = rt->create_field_space(ctx);
  FieldAllocator field_allocator = rt->create_field_allocator(ctx,fs);
  FieldID fida = field_allocator.allocate_field(sizeof(int), FIELD_A);
  FieldID fidb = field_allocator.allocate_field(sizeof(int), FIELD_B);
  assert(fida == FIELD_A);
  assert(fidb == FIELD_B);

  LogicalRegion lr = rt->create_logical_region(ctx,is,fs);

  int init = 1;
  rt->fill_field(ctx,lr,lr,fida,&init,sizeof(init));
  rt->fill_field(ctx,lr,lr,fidb,&init,sizeof(init));

  TaskLauncher inc_launcher_fielda_only(INC_TASK_ID_FIELDA, TaskArgument(NULL,0));
  RegionRequirement rra(lr, READ_WRITE, ATOMIC, lr);
  rra.add_field(FIELD_A);
  inc_launcher_fielda_only.add_region_requirement(rra);

  TaskLauncher inc_launcher_fieldb_only(INC_TASK_ID_FIELDB, TaskArgument(NULL,0));
  RegionRequirement rrb(lr, READ_WRITE, EXCLUSIVE, lr);
  rrb.add_field(FIELD_B);
  inc_launcher_fieldb_only.add_region_requirement(rrb);

  TaskLauncher inc_launcher_field_both(INC_TASK_ID_BOTH, TaskArgument(NULL,0));
  RegionRequirement rrbotha(lr, READ_WRITE, ATOMIC, lr);
  rrbotha.add_field(FIELD_A);
  inc_launcher_field_both.add_region_requirement(rrbotha);
  RegionRequirement rrbothb(lr, READ_ONLY, ATOMIC, lr);
  rrbothb.add_field(FIELD_B);
  inc_launcher_field_both.add_region_requirement(rrbothb);


  //
  // Launch a bunch of tasks that will update FIELD_B
  //
  for(int i = 0; i < 100; i++) {
    inc_launcher_fieldb_only.argument = TaskArgument(&i,sizeof(i));
    rt->execute_task(ctx, inc_launcher_fieldb_only);
  }

  //
  // Now launch pairs of tasks, one of which reads from FIELD_B (and so has dependencies
  // on the previous collection of tasks) and FIELD_A, and one of which only reads from FIELD_A.
  // Both kinds of tasks write FIELD_A and have ATOMIC coherence.

  for(int i = 0; i < 100; i++) {
    inc_launcher_field_both.argument = TaskArgument(&i,sizeof(i));
    rt->execute_task(ctx, inc_launcher_field_both);
  }
  for(int i = 0; i < 100; i++) {
    inc_launcher_fielda_only.argument = TaskArgument(&i,sizeof(i));
    rt->execute_task(ctx, inc_launcher_fielda_only);
  }


  //
  // Sum up the elements of the region.
  //
  TaskLauncher sum_launcher(SUM_TASK_ID, TaskArgument(NULL,0));
  sum_launcher.add_region_requirement(RegionRequirement(lr, READ_ONLY, EXCLUSIVE, lr));
  sum_launcher.add_field(0, FIELD_A);
  rt->execute_task(ctx, sum_launcher);

  // Clean up.  IndexAllocators and FieldAllocators automatically have their resources reclaimed
  // when they go out of scope.
  rt->destroy_logical_region(ctx,lr);
  rt->destroy_field_space(ctx,fs);
  rt->destroy_index_space(ctx,is);
}

void inc_task_fielda_only(const Task *task,
			  const std::vector<PhysicalRegion> &rgns,
			  Context ctx, Runtime *rt)
{
  printf("Executing increment task %d for field A\n", *((int *) task->args));

  const FieldAccessor<READ_WRITE,int,1> acc(rgns[0], FIELD_A);
  Rect<1> dom = rt->get_index_space_domain(ctx, task->regions[0].region.get_index_space());
  for (PointInRectIterator<1>  pir(dom); pir(); pir++) 
    {
      acc[*pir] = acc[*pir] + 1;
    }
}

void inc_task_fieldb_only(const Task *task,
			  const std::vector<PhysicalRegion> &rgns,
			  Context ctx, Runtime *rt)
{
  printf("Executing increment task %d for field B\n", *((int *) task->args));

  const FieldAccessor<READ_WRITE,int,1> acc(rgns[0], FIELD_B);
  Rect<1> dom = rt->get_index_space_domain(ctx, task->regions[0].region.get_index_space());
  for (PointInRectIterator<1>  pir(dom); pir(); pir++) 
    {
      acc[*pir] = acc[*pir] + 1;
    }
}


void inc_task_field_both(const Task *task,
			 const std::vector<PhysicalRegion> &rgns,
			 Context ctx, Runtime *rt)
{
  printf("Executing increment task %d for field A += B\n", *((int *) task->args));

  const FieldAccessor<READ_WRITE,int,1> acca(rgns[0], FIELD_A);
  const FieldAccessor<READ_ONLY,int,1> accb(rgns[1], FIELD_B);
  Rect<1> dom = rt->get_index_space_domain(ctx,task->regions[0].region.get_index_space());
  for (PointInRectIterator<1>  pir(dom); pir(); pir++) 
    {
      acca[*pir] = acca[*pir] + accb[*pir];
    }
}

void sum_task(const Task *task,
		    const std::vector<PhysicalRegion> &rgns,
		    Context ctx, Runtime *rt)
{
  const FieldAccessor<READ_ONLY,int,1> acc(rgns[0], FIELD_A);
  Rect<1> dom = rt->get_index_space_domain(ctx,task->regions[0].region.get_index_space());
  int sum = 0;
  for (PointInRectIterator<1> pir(dom); pir(); pir++) 
    {
      sum += acc[*pir];
    }
  printf("The sum of the elements of field A is %d\n",sum);
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
    TaskVariantRegistrar registrar(INC_TASK_ID_FIELDA, "inc_field_A");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<inc_task_fielda_only>(registrar);
  }
  {
    TaskVariantRegistrar registrar(INC_TASK_ID_FIELDB, "inc_field_B");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<inc_task_fieldb_only>(registrar);
  }
  {
    TaskVariantRegistrar registrar(INC_TASK_ID_BOTH, "inc_both");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<inc_task_field_both>(registrar);
  }
  {
    TaskVariantRegistrar registrar(SUM_TASK_ID, "sum_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<sum_task>(registrar);
  }
  return Runtime::start(argc, argv);
}
