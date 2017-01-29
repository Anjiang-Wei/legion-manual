#include <cstdio>
#include "legion.h"
#include "default_mapper.h"

using namespace Legion;
using namespace Legion::Mapping;
using namespace LegionRuntime::Accessor;
using namespace LegionRuntime::Arrays;

// All tasks must have a unique task id (a small integer).
// A global enum is a convenient way to assign task ids.
enum TaskID {
  TOP_LEVEL_TASK_ID,
  TASK_INC,
};

enum FieldIDs {
  FIELD_A,
};

void top_level_task(const Task *task,
		    const std::vector<PhysicalRegion> &regions,
		    Context ctx, 
		    Runtime *runtime)
{

  printf("Top level task start.\n");
  Rect<1> rec(Point<1>(0),Point<1>(9999));
  IndexSpace sis = runtime->create_index_space(ctx,Domain::from_rect<1>(rec));
  FieldSpace fs = runtime->create_field_space(ctx);
  FieldAllocator field_allocator = runtime->create_field_allocator(ctx,fs);
  FieldID fida = field_allocator.allocate_field(sizeof(int), FIELD_A);
  assert(fida == FIELD_A);

  LogicalRegion lr = runtime->create_logical_region(ctx,sis,fs);

  int init = 1;
  runtime->fill_field(ctx,lr,lr,fida,&init,sizeof(init));
  
  RegionRequirement crr(lr, READ_WRITE, ATOMIC, lr);
  crr.add_field(FIELD_A);

  for(int i = 1; i <= 100; i++) {
    TaskLauncher inclauncher(TASK_INC, TaskArgument(&i,sizeof(int)));
    inclauncher.add_region_requirement(crr);
    runtime->execute_task(ctx,inclauncher);
  }
  printf("Top level task done launching subtasks.\n");
}

void inc_task(const Task *task,
	      const std::vector<PhysicalRegion> &regions,
	      Context ctx, 
	      Runtime *runtime)
{
  printf("\tInc task %d\n", *((int *) task->args));

  RegionAccessor<AccessorType::Generic, int> acc =
    regions[0].get_field_accessor(FIELD_A).typeify<int>();

  Domain dom = runtime->get_index_space_domain(ctx,
					       task->regions[0].region.get_index_space());
  Rect<1> rect = dom.get_rect<1>();
  for (GenericPointInRectIterator<1> pir(rect); pir; pir++)
    {
      int val = acc.read(DomainPoint::from_point<1>(pir.p));
      acc.write(DomainPoint::from_point<1>(pir.p), val + 1);
    }
}

class RoundRobinMapper : public DefaultMapper {
public:
  RoundRobinMapper(MapperRuntime *rt, Machine machine, Processor local);
  virtual void select_task_options(const MapperContext ctx,
                                   const Task& task, 
                                         TaskOptions &options);
};

RoundRobinMapper::RoundRobinMapper(MapperRuntime *rt, Machine m, Processor p)
  : DefaultMapper(rt, m, p) // pass arguments through to DefaultMapper
{
}

void mapper_registration(Machine machine, Runtime *rt,
			 const std::set<Processor> &local_procs)
{
  MapperRuntime *const map_rt = rt->get_mapper_runtime();
  for (std::set<Processor>::const_iterator it = local_procs.begin();
       it != local_procs.end(); it++)
    {
      rt->replace_default_mapper(new RoundRobinMapper(map_rt, machine, *it), *it);
    }
}

void RoundRobinMapper::select_task_options(const MapperContext  ctx,
                                           const Task&          task,
                                                 TaskOptions&   options)
{
  options.initial_proc = default_get_next_global_cpu();
  options.inline_task = false;
  options.stealable = false;
  options.map_locally = false;
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
    TaskVariantRegistrar registrar(TASK_INC, "inc_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<inc_task>(registrar);
  }
  Runtime::set_registration_callback(mapper_registration);

  return Runtime::start(argc, argv);
}
